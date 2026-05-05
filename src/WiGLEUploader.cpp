#include "WiGLEUploader.h"
#include "Config.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <new>

namespace
{
    const char *UPLOAD_BOUNDARY = "----WardriverBoundaryPF40CDA7UXrKCDs";

    String basenameOf(const String &path)
    {
        int slash = path.lastIndexOf('/');
        if (slash >= 0 && slash + 1 < (int)path.length())
        {
            return path.substring(slash + 1);
        }
        return path;
    }

    bool waitForClientData(WiFiClientSecure &client, uint32_t timeoutMs)
    {
        uint32_t start = millis();
        while (client.connected() && !client.available() && (millis() - start) < timeoutMs)
        {
            delay(10);
        }
        return client.available() > 0;
    }
}

WiGLEUploader::WiGLEUploader(bool superDebug)
{
    this->superDebug = superDebug;
}

void WiGLEUploader::setSuperDebug(bool enabled)
{
    superDebug = enabled;
}

void WiGLEUploader::setCredentials(const String &name, const String &token)
{
    apiName = name;
    apiToken = token;
    rebuildAuthHeader();
}

String WiGLEUploader::getLastErrorMessage() const
{
    return lastError;
}

void WiGLEUploader::rebuildAuthHeader()
{
    String plain = apiName + ":" + apiToken;
    size_t encodedLen = 0;
    mbedtls_base64_encode(nullptr, 0, &encodedLen,
                          reinterpret_cast<const unsigned char *>(plain.c_str()),
                          plain.length());

    unsigned char *encoded = new (std::nothrow) unsigned char[encodedLen + 1];
    if (encoded == nullptr)
    {
        authHeaderCached = "";
        return;
    }

    size_t actualLen = 0;
    int ret = mbedtls_base64_encode(encoded, encodedLen, &actualLen,
                                    reinterpret_cast<const unsigned char *>(plain.c_str()),
                                    plain.length());
    if (ret == 0)
    {
        encoded[actualLen] = '\0';
        authHeaderCached = String("Basic ") + reinterpret_cast<char *>(encoded);
    }
    else
    {
        authHeaderCached = "";
    }
    delete[] encoded;
}

String WiGLEUploader::mapHttpCodeToMessage(int httpCode)
{
    if (httpCode == 401 || httpCode == 403)
    {
        return "Auth failed - check API name/token";
    }
    if (httpCode == 429)
    {
        return "WiGLE rate limited the upload";
    }
    if (httpCode >= 500)
    {
        return String("WiGLE server error ") + String(httpCode);
    }
    if (httpCode >= 400)
    {
        return String("WiGLE rejected upload ") + String(httpCode);
    }
    return String("HTTP ") + String(httpCode);
}

int WiGLEUploader::uploadFile(fs::FS &fs,
                              const String &localPath,
                              const String &remoteFilename,
                              const char *mimeType,
                              ProgressCb progress)
{
    lastError = "";

    if (authHeaderCached.length() == 0)
    {
        lastError = "Missing WiGLE credentials";
        return -1;
    }

    File file = fs.open(localPath, FILE_READ);
    if (!file)
    {
        lastError = "Upload file open failed";
        return -1;
    }

    uint32_t fileSize = (uint32_t)file.size();
    String uploadName = remoteFilename.length() > 0 ? basenameOf(remoteFilename) : basenameOf(localPath);
    String preamble = String("--") + UPLOAD_BOUNDARY + "\r\n" +
                      "Content-Disposition: form-data; name=\"file\"; filename=\"" + uploadName + "\"\r\n" +
                      "Content-Type: " + mimeType + "\r\n\r\n";
    String trailer = String("\r\n--") + UPLOAD_BOUNDARY + "--\r\n";
    uint32_t contentLength = (uint32_t)preamble.length() + fileSize + (uint32_t)trailer.length();

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(UPLOAD_HTTP_TIMEOUT_S * 1000UL);

    if (superDebug)
    {
        logger.debugPrintln(String("[Upload] POST ") + WIGLE_API_UPLOAD_PATH + " HTTP/1.1");
        logger.debugPrintln(String("[Upload] Host: ") + WIGLE_API_HOST + "");
        logger.debugPrintln("[Upload] Authorization: Basic ***");
        logger.debugPrintln(String("[Upload] Content-Type: multipart/form-data; boundary=") + UPLOAD_BOUNDARY);
    }

    if (!client.connect(WIGLE_API_HOST, WIGLE_API_PORT))
    {
        file.close();
        lastError = "Could not connect to WiGLE";
        return -1;
    }

    client.print(String("POST ") + WIGLE_API_UPLOAD_PATH + " HTTP/1.1\r\n");
    client.print(String("Host: ") + WIGLE_API_HOST + "\r\n");
    client.print(String("Authorization: ") + authHeaderCached + "\r\n");
    client.print(String("Content-Type: multipart/form-data; boundary=") + UPLOAD_BOUNDARY + "\r\n");
    client.print(String("Content-Length: ") + String(contentLength) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    client.print(preamble);

    uint8_t *buffer = new (std::nothrow) uint8_t[UPLOAD_GZIP_CHUNK_BYTES];
    if (buffer == nullptr)
    {
        file.close();
        client.stop();
        lastError = "Upload buffer unavailable";
        return -1;
    }

    if (superDebug)
    {
        logger.debugPrintln("[Upload] File Upload Started");
    }

    uint32_t sent = 0;
    while (file.available())
    {
        int bytesRead = file.read(buffer, UPLOAD_GZIP_CHUNK_BYTES);
        if (bytesRead <= 0)
        {
            delete[] buffer;
            file.close();
            client.stop();
            lastError = "Upload file read failed";
            return -1;
        }

        size_t written = client.write(buffer, bytesRead);
        if (written != (size_t)bytesRead)
        {
            delete[] buffer;
            file.close();
            client.stop();
            lastError = "Upload write failed";
            return -1;
        }

        sent += (uint32_t)written;
        if (progress && !progress(sent, fileSize))
        {
            delete[] buffer;
            file.close();
            client.stop();
            lastError = "Cancelled by user";
            return -2;
        }
    }
    if (superDebug)
    {
        logger.debugPrintln("[Upload] Complete");
    }

    delete[] buffer;
    file.close();
    client.print(trailer);

    if (!waitForClientData(client, UPLOAD_HTTP_TIMEOUT_S * 1000UL))
    {
        client.stop();
        lastError = "No response from WiGLE";
        return -1;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();

    if (superDebug)
    {
        logger.debugPrintln("[Upload] Response Received: " + statusLine);
    }

    int firstSpace = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
    int httpCode = 0;
    if (firstSpace >= 0)
    {
        String code = (secondSpace > firstSpace) ? statusLine.substring(firstSpace + 1, secondSpace)
                                                 : statusLine.substring(firstSpace + 1);
        httpCode = code.toInt();
    }

    while (client.connected() || client.available())
    {
        String header = client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0)
        {
            break;
        }
    }

    String body;
    uint32_t bodyStart = millis();
    while ((client.connected() || client.available()) && (millis() - bodyStart) < UPLOAD_HTTP_TIMEOUT_S * 1000UL)
    {
        while (client.available())
        {
            body += (char)client.read();
            bodyStart = millis();
        }
        if (!client.available())
        {
            delay(10);
        }
    }
    client.stop();

    if (httpCode != 200)
    {
        lastError = mapHttpCodeToMessage(httpCode);
        return httpCode;
    }

    DynamicJsonDocument doc(768);

    if (superDebug)
    {
        logger.debugPrintln("[Upload] Body Received: " + body);
    }

    DeserializationError error = deserializeJson(doc, body);
    if (!error && doc["success"].is<bool>() && !doc["success"].as<bool>())
    {
        if (doc["message"].is<const char *>())
        {
            lastError = doc["message"].as<String>();
        }
        else
        {
            lastError = "WiGLE returned success=false";
        }
        return 200;
    }

    return 200;
}
