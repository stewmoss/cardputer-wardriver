#include "UploadManager.h"
#include "CsvFileMaintenance.h"
#include "Logger.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <algorithm>
#include <string.h>

namespace
{
    String basenameOf(const String &path)
    {
        int slash = path.lastIndexOf('/');
        if (slash >= 0 && slash + 1 < (int)path.length())
        {
            return path.substring(slash + 1);
        }
        return path;
    }

    int parseCsvIndex(const String &filename)
    {
        if (!filename.startsWith(CSV_PREFIX) || !filename.endsWith(".csv"))
        {
            return -1;
        }
        String numeric = filename.substring(strlen(CSV_PREFIX), filename.length() - 4);
        if (numeric.length() == 0)
        {
            return -1;
        }
        for (size_t index = 0; index < numeric.length(); ++index)
        {
            char digit = numeric.charAt(index);
            if (digit < '0' || digit > '9')
            {
                return -1;
            }
        }
        return numeric.toInt();
    }

    uint32_t countCsvRecords(fs::FS &fs, const String &path)
    {
        File file = fs.open(path, FILE_READ);
        if (!file)
        {
            return 0;
        }

        uint8_t buffer[256];
        uint32_t newlineCount = 0;
        while (file.available())
        {
            int bytesRead = file.read(buffer, sizeof(buffer));
            if (bytesRead <= 0)
            {
                break;
            }
            for (int index = 0; index < bytesRead; ++index)
            {
                if (buffer[index] == '\n')
                {
                    newlineCount++;
                }
            }
        }
        file.close();
        return (newlineCount > 2) ? (newlineCount - 2) : 0;
    }

    bool anyKeyboardKeyPressed()
    {
        return M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
    }
}

UploadManager::UploadManager()
    : phase(UploadPhase::DONE), cancelRequested(false), scanAttempted(false), uploadConnected(false),
      mode(Mode::AUTO), rebootOnExit(false),
      phaseStartMs(0), lastProgressDrawMs(0), currentIndex(0), retryMode(false),
      display(nullptr), alerts(nullptr), wifi(nullptr), cfg(nullptr),
      scanCfg(nullptr), uploader(false)
{
    memset(&stats, 0, sizeof(stats));
}

void UploadManager::begin(Display *displayRef,
                          AlertManager *alertsRef,
                          WiFiManager *wifiRef,
                          const UploadConfig *uploadCfg,
                          const ScanConfig *scanConfig,
                          bool superDebug)
{
    display = displayRef;
    alerts = alertsRef;
    wifi = wifiRef;
    cfg = uploadCfg;
    scanCfg = scanConfig;
    uploader.setSuperDebug(superDebug);
    this->superDebug = superDebug;
    if (cfg != nullptr)
    {
        uploader.setCredentials(cfg->wigle_api_name, cfg->wigle_api_token);
    }
}

bool UploadManager::shouldRun() const
{
    return cfg != nullptr &&
           cfg->wigle_upload_enabled &&
           cfg->auto_upload &&
           cfg->upload_ssid.length() > 0 &&
           cfg->wigle_api_name.length() > 0 &&
           cfg->wigle_api_token.length() > 0;
}

void UploadManager::enter(Mode m)
{
    cancelRequested = false;
    scanAttempted = false;
    uploadConnected = false;
    mode = m;
    rebootOnExit = false;
    currentIndex = 0;
    retryMode = false;
    phaseStartMs = millis();
    lastProgressDrawMs = 0;
    lastErrorMessage = "";
    maxIndexPath = "";
    pendingFiles.clear();
    results.clear();
    memset(&stats, 0, sizeof(stats));

    purgeTempDir();
    if (!ensureDirectories())
    {
        lastErrorMessage = "Upload directories failed";
        phase = UploadPhase::ERROR_SCREEN;
        return;
    }

    CsvFileMaintenance::cleanupStaleZeroBytePlaceholders(SD);

    pendingFiles = discoverPendingFiles();
    stats.totalFiles = (uint16_t)pendingFiles.size();
    if (pendingFiles.empty())
    {
        if (isManual())
        {
            logger.debugPrintln("[Upload] manual: no pending files");
            lastErrorMessage = "Nothing to upload";
            phase = UploadPhase::ERROR_SCREEN;
            return;
        }
        logger.debugPrintln("[Upload] No pending CSV files; skipping");
        phase = UploadPhase::DONE;
        return;
    }

    if (isManual())
    {
        if (superDebug)
        {
            const char *modeName = (mode == Mode::MANUAL_SHUTDOWN) ? "SHUTDOWN" : "REBOOT";
            logger.debugPrintln(String("[Upload] manual upload requested (mode=") + modeName +
                                ", pending=" + String((int)pendingFiles.size()) + ")");
        }

        if (alerts != nullptr)
        {
            alerts->setBuzzer(BEEP_FREQ, SHORT_TONE_MS);
        }
    }
    else
    {
        logger.debugPrintln(String("[Upload] enter; pending=") + String((int)pendingFiles.size()));
    }

    if (display != nullptr)
    {
        display->showUploadScanning(cfg->upload_ssid);
    }
    phase = UploadPhase::SCAN_FOR_HOME;
}

bool UploadManager::tick()
{
    if (phase == UploadPhase::DONE)
    {
        return true;
    }

    // Manual-reboot cancellation: show cancelled screen, beep, reboot.
    // Manual-shutdown cancellation: clean up and return to caller (no reboot).
    if (cancelRequested && isManual() &&
        phase != UploadPhase::COMPLETE_SUMMARY &&
        phase != UploadPhase::ERROR_SCREEN)
    {
        if (mode == Mode::MANUAL_REBOOT)
        {
            if (superDebug)
            {
                logger.debugPrintln("[Upload] manual upload cancelled by user; rebooting");
            }
            cancelBeep();
            disconnectHomeNetwork();
            if (display != nullptr)
            {
                display->showUploadCancelled();
            }
            delay(1500);
            ESP.restart();
            return true; // unreachable
        }
        // MANUAL_SHUTDOWN: cancel cleanly and let caller relock.
        if (superDebug)
        {
            logger.debugPrintln("[Upload] shutdown manual upload cancelled by user");
        }
        cancelBeep();
        disconnectHomeNetwork();
        phase = UploadPhase::DONE;
        return true;
    }

    if (cancelRequested &&
        phase != UploadPhase::UPLOADING &&
        phase != UploadPhase::COMPLETE_SUMMARY &&
        phase != UploadPhase::ERROR_SCREEN)
    {
        cancelBeep();
        disconnectHomeNetwork();
        phase = UploadPhase::DONE;
        return true;
    }

    switch (phase)
    {
    case UploadPhase::SCAN_FOR_HOME:
        phaseScanForHome();
        break;
    case UploadPhase::FOUND_COUNTDOWN:
        phaseFoundCountdown();
        break;
    case UploadPhase::LOW_BATTERY_PROMPT:
        phaseLowBatteryPrompt();
        break;
    case UploadPhase::UPLOADING:
        phaseUploading();
        break;
    case UploadPhase::COMPLETE_SUMMARY:
        phaseCompleteSummary();
        break;
    case UploadPhase::ERROR_SCREEN:
        phaseErrorScreen();
        break;
    case UploadPhase::DONE:
        return true;
    }

    if (rebootOnExit && phase == UploadPhase::DONE && mode == Mode::MANUAL_REBOOT)
    {
        disconnectHomeNetwork();
        cancelBeep();
        delay(1500);
        ESP.restart();
    }

    return phase == UploadPhase::DONE;
}

void UploadManager::requestCancel()
{
    cancelRequested = true;
    if (superDebug)
    {
        logger.debugPrintln("[Upload] Cancel requested");
    }
}

void UploadManager::phaseScanForHome()
{
    if (!isManual())
    {
        if (scanAttempted)
        {
            return;
        }
        scanAttempted = true;

        if (superDebug)
        {
            logger.debugPrintln(String("[Upload] scanning for SSID '") + cfg->upload_ssid + "'");
        }
        if (display != nullptr)
        {
            display->showUploadScanning(cfg->upload_ssid);
        }

        bool found = wifi != nullptr && wifi->isSsidVisible(cfg->upload_ssid, UPLOAD_HOME_SCAN_TIMEOUT_MS);
        if (!found)
        {
            logger.debugPrintln("[Upload] Home SSID not found; skipping");
            disconnectHomeNetwork();
            phase = UploadPhase::DONE;
            return;
        }

        logger.debugPrintln("[Upload] Home SSID found");
        doubleBeep();
        phaseStartMs = millis();
        phase = UploadPhase::FOUND_COUNTDOWN;
        return;
    }

    // Manual mode: keep scanning until SSID found or user cancels.
    if (display != nullptr)
    {
        display->showUploadScanning(cfg->upload_ssid);
    }
    if (cancelRequested)
    {
        return; // tick() handles the cancel/reboot path
    }
    bool found = wifi != nullptr && wifi->isSsidVisible(cfg->upload_ssid, UPLOAD_HOME_SCAN_TIMEOUT_MS);
    if (cancelRequested)
    {
        return;
    }
    if (!found)
    {
        // Stay in this phase — try again next tick.
        return;
    }
    logger.debugPrintln("[Upload] manual: home SSID found");
    doubleBeep();
    phaseStartMs = millis();
    phase = UploadPhase::FOUND_COUNTDOWN;
}

void UploadManager::phaseFoundCountdown()
{
    uint32_t elapsed = millis() - phaseStartMs;
    int secondsRemaining = (elapsed >= UPLOAD_FOUND_COUNTDOWN_MS)
                               ? 0
                               : (int)((UPLOAD_FOUND_COUNTDOWN_MS - elapsed + 999UL) / 1000UL);
    if (display != nullptr)
    {
        display->showUploadCountdown((int)pendingFiles.size(), secondsRemaining);
    }

    if (elapsed < UPLOAD_FOUND_COUNTDOWN_MS)
    {
        return;
    }

    int batteryPct = M5Cardputer.Power.getBatteryLevel();
    if (!isManual() && batteryPct >= 0 && batteryPct < MIN_UPLOAD_BATTERY_PCT)
    {
        phaseStartMs = millis();
        phase = UploadPhase::LOW_BATTERY_PROMPT;
        return;
    }
    if (isManual() && batteryPct >= 0 && batteryPct < MIN_UPLOAD_BATTERY_PCT)
    {
        logger.debugPrintln(String("[Upload] manual: low battery (") + String(batteryPct) + "%); proceeding anyway");
    }

    stats.startMs = millis();
    if (alerts != nullptr)
    {
        alerts->setStatusScanning();
    }
    phase = UploadPhase::UPLOADING;
}

void UploadManager::phaseLowBatteryPrompt()
{
    uint32_t elapsed = millis() - phaseStartMs;
    int secondsRemaining = (elapsed >= UPLOAD_LOW_BATT_TIMEOUT_MS)
                               ? 0
                               : (int)((UPLOAD_LOW_BATT_TIMEOUT_MS - elapsed + 999UL) / 1000UL);
    int batteryPct = M5Cardputer.Power.getBatteryLevel();
    if (display != nullptr)
    {
        display->showUploadLowBattery(batteryPct, secondsRemaining);
    }

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
    {
        Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
        if (keys.enter)
        {
            stats.startMs = millis();
            if (alerts != nullptr)
            {
                alerts->setStatusScanning();
            }
            phase = UploadPhase::UPLOADING;
            return;
        }
    }

    if (elapsed >= UPLOAD_LOW_BATT_TIMEOUT_MS)
    {
        logger.debugPrintln("[Upload] Low battery prompt timed out; skipping");
        phase = UploadPhase::DONE;
    }
}

void UploadManager::phaseUploading()
{
    if (!uploadConnected)
    {
        if (!connectHomeNetwork())
        {
            lastErrorMessage = String("Could not connect to ") + cfg->upload_ssid;
            phase = UploadPhase::ERROR_SCREEN;
            return;
        }
        uploadConnected = true;
    }

    if (currentIndex >= pendingFiles.size())
    {
        disconnectHomeNetwork();
        recomputeStats();
        stats.endMs = millis();
        phase = UploadPhase::COMPLETE_SUMMARY;
        return;
    }

    String csvPath = pendingFiles[currentIndex];
    String csvName = basenameOf(csvPath);

    // ── Sweep-count pre-scan ─────────────────────────────────────────────
    uint16_t sweepsFound = 0;
    if (cfg->min_sweeps_threshold > 0)
    {
        if (display != nullptr)
        {
            display->showUploadSweepScanning(csvName,
                                             (uint32_t)currentIndex + 1,
                                             stats.totalFiles);
        }
        if (!countSweepsUpToThreshold(csvPath, cfg->min_sweeps_threshold, sweepsFound))
        {
            // Treat as standard failure
            UploadFileResult result;
            result.filename = csvPath;
            result.success = false;
            result.skippedThin = false;
            result.httpCode = 0;
            result.errorMessage = "Sweep scan: open failed";
            result.bytesOriginal = 0;
            result.bytesCompressed = 0;
            result.recordCount = 0;
            results.push_back(result);
            currentIndex++;
            return;
        }
        if (sweepsFound < cfg->min_sweeps_threshold)
        {
            logger.debugPrintln(String("[Upload] sweep scan done: ") + csvName +
                                " sweeps=" + String(sweepsFound) + " -> THIN");

            UploadFileResult result;
            result.filename = csvPath;
            result.success = false;
            result.skippedThin = true;
            result.httpCode = 0;
            result.errorMessage = "";
            result.bytesOriginal = 0;
            result.bytesCompressed = 0;
            result.recordCount = 0;            
            // Quarantine the file (no-op if it's already in thin dir).
            moveToThin(csvPath);
            results.push_back(result);
            currentIndex++;
            return;
        }
        else
        {
            logger.debugPrintln(String("[Upload] sweep scan done: ") + csvName +
                                " sweeps>=" + String(cfg->min_sweeps_threshold) + " -> proceed");
        }
    }

    File csvFile = SD.open(csvPath, FILE_READ);
    uint32_t originalBytes = csvFile ? (uint32_t)csvFile.size() : 0;
    if (csvFile)
    {
        csvFile.close();
    }

    UploadFileResult result;
    result.filename = csvPath;
    result.success = false;
    result.skippedThin = false;
    result.httpCode = 0;
    result.errorMessage = "";
    result.bytesOriginal = originalBytes;
    result.bytesCompressed = 0;
    result.recordCount = 0;

    String uploadPath = csvPath;
    String uploadName = csvName;
    const char *mimeType = "text/csv";
    bool removeTemp = false;

    if (cfg->compress_before_upload)
    {
        String gzPath = String(UPLOAD_TEMP_DIR) + "/" + csvName + ".gz";
        GzipCompressor::Result compressed = compressor.compress(SD, csvPath, gzPath);
        if (!compressed.success)
        {
            result.errorMessage = compressed.errorMessage;
            results.push_back(result);
            currentIndex++;
            return;
        }

        result.bytesOriginal = compressed.bytesIn;
        result.bytesCompressed = compressed.bytesOut;
        result.recordCount = compressed.recordCount;
        uploadPath = gzPath;
        uploadName = csvName + ".gz";
        mimeType = "application/gzip";
        removeTemp = true;
        if (superDebug)
        {
            logger.debugPrintln(String("[Upload] compress: ") + String(compressed.bytesIn) + " -> " +
                                String(compressed.bytesOut) + " records=" + String(compressed.recordCount));
        }
    }
    else
    {
        result.bytesCompressed = originalBytes;
        result.recordCount = countCsvRecords(SD, csvPath);
    }

    File uploadFile = SD.open(uploadPath, FILE_READ);
    uint32_t uploadBytes = uploadFile ? (uint32_t)uploadFile.size() : 0;
    if (uploadFile)
    {
        uploadFile.close();
    }

    lastProgressDrawMs = 0;
    if (display != nullptr)
    {
        display->showUploadProgress(uploadName,
                                    (uint32_t)currentIndex + 1,
                                    stats.totalFiles,
                                    0,
                                    uploadBytes,
                                    0);
    }

    uint32_t uploadStartMs = millis();
    int httpCode = uploader.uploadFile(
        SD,
        uploadPath,
        uploadName,
        mimeType,
        [this, uploadName, uploadStartMs](uint32_t sent, uint32_t total) -> bool
        {
            uint32_t now = millis();
            if (display != nullptr && (lastProgressDrawMs == 0 || now - lastProgressDrawMs >= 250UL || sent >= total))
            {
                display->showUploadProgress(uploadName,
                                            (uint32_t)currentIndex + 1,
                                            stats.totalFiles,
                                            sent,
                                            total,
                                            (now - uploadStartMs) / 1000UL);
                lastProgressDrawMs = now;
            }
            return !cancelRequested;
        });

    result.httpCode = httpCode;
    if (httpCode == 200 && uploader.getLastErrorMessage().length() == 0)
    {
        result.success = true;
        moveOrDeleteAfterUpload(csvPath,
                                removeTemp ? uploadPath : String(),
                                removeTemp ? uploadName : String());
        removeTemp = false;
    }
    else
    {
        result.errorMessage = uploader.getLastErrorMessage();
        if (result.errorMessage.length() == 0)
        {
            result.errorMessage = String("HTTP ") + String(httpCode);
        }
        logger.debugPrintln(String("[Upload] failed ") + csvName + ": " + result.errorMessage);
    }

    if (removeTemp)
    {
        SD.remove(uploadPath);
    }

    results.push_back(result);

    if (httpCode == 401 || httpCode == 403 || cancelRequested)
    {
        currentIndex = pendingFiles.size();
    }
    else
    {
        currentIndex++;
    }
}

void UploadManager::phaseCompleteSummary()
{
    recomputeStats();
    if (display != nullptr)
    {
        bool retryAvail = stats.failCount > 0 && !retryMode;
        Display::ExitMode exitMode;
        switch (mode)
        {
        case Mode::MANUAL_REBOOT:
            exitMode = Display::ExitMode::REBOOT;
            break;
        case Mode::MANUAL_SHUTDOWN:
            exitMode = Display::ExitMode::RETURN_TO_SHUTDOWN;
            break;
        case Mode::AUTO:
        default:
            exitMode = Display::ExitMode::RETURN;
            break;
        }
        display->showUploadComplete(stats, retryAvail, exitMode);
    }

    if (!anyKeyboardKeyPressed())
    {
        return;
    }

    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    // Retry path (auto mode only): `R` re-runs failed uploads in the
    // current session without rebooting.
    if (!isManual() && stats.failCount > 0 && !retryMode)
    {
        for (auto key : keys.word)
        {
            if (key == 'r' || key == 'R')
            {
                setupRetry();
                return;
            }
        }
    }

    // Any other key (or ENT) — beep then exit. Reboot/return is decided
    // by tick() based on `mode`.
    doubleBeep();
    if (mode == Mode::MANUAL_REBOOT || mode == Mode::AUTO)
    {
        rebootOnExit = (mode == Mode::MANUAL_REBOOT);
    }
    phase = UploadPhase::DONE;
}

void UploadManager::phaseErrorScreen()
{
    if (alerts != nullptr)
    {
        alerts->setStatusError();
    }
    if (display != nullptr)
    {
        Display::ExitMode exitMode;
        switch (mode)
        {
        case Mode::MANUAL_REBOOT:
            exitMode = Display::ExitMode::REBOOT;
            break;
        case Mode::MANUAL_SHUTDOWN:
            exitMode = Display::ExitMode::RETURN_TO_SHUTDOWN;
            break;
        case Mode::AUTO:
        default:
            exitMode = Display::ExitMode::RETURN;
            break;
        }
        display->showUploadError(lastErrorMessage, exitMode);
    }

    if (anyKeyboardKeyPressed() || M5Cardputer.BtnA.wasPressed())
    {
        disconnectHomeNetwork();
        if (mode == Mode::MANUAL_REBOOT)
        {
            rebootOnExit = true;
        }
        phase = UploadPhase::DONE;
    }
}

std::vector<String> UploadManager::discoverPendingFiles()
{
    struct PendingFile
    {
        String path;
        int index;
    };

    std::vector<PendingFile> files;
    File dir = SD.open(CSV_DIR);
    if (!dir || !dir.isDirectory())
    {
        if (dir)
        {
            dir.close();
        }
        return std::vector<String>();
    }

    File entry;
    while ((entry = dir.openNextFile()))
    {
        if (!entry.isDirectory() && entry.size() > 0)
        {
            String name = basenameOf(entry.name());
            int fileIndex = parseCsvIndex(name);
            if (fileIndex >= 0)
            {
                String path = String(CSV_DIR) + "/" + name;
                files.push_back({path, fileIndex});
            }
        }
        entry.close();
    }
    dir.close();

    std::sort(files.begin(), files.end(), [](const PendingFile &left, const PendingFile &right)
              { return left.index < right.index; });

    std::vector<String> paths;
    paths.reserve(files.size());
    if (!files.empty())
    {
        maxIndexPath = files.back().path;
    }
    for (const PendingFile &file : files)
    {
        paths.push_back(file.path);
    }

    // Optionally include quarantined thin files at the end of the list.
    if (cfg != nullptr && cfg->retry_thin_files && cfg->min_sweeps_threshold > 0 &&
        SD.exists(UPLOAD_THIN_DIR))
    {
        File thinDir = SD.open(UPLOAD_THIN_DIR);
        if (thinDir && thinDir.isDirectory())
        {
            int added = 0;
            File entry;
            while ((entry = thinDir.openNextFile()))
            {
                if (!entry.isDirectory() && entry.size() > 0)
                {
                    String name = basenameOf(entry.name());
                    int fileIndex = parseCsvIndex(name);
                    if (fileIndex >= 0)
                    {
                        paths.push_back(String(UPLOAD_THIN_DIR) + "/" + name);
                        added++;
                    }
                }
                entry.close();
            }
            thinDir.close();
            if (added > 0)
            {
                logger.debugPrintln(String("[Upload] retry_thin_files=true; including ") +
                                    String(added) + " file(s) from " + UPLOAD_THIN_DIR);
            }
        }
    }
    else if (cfg != nullptr && cfg->retry_thin_files && cfg->min_sweeps_threshold == 0)
    {
        logger.debugPrintln("[Upload] retry_thin_files=true but threshold=0 — ignoring");
    }

    return paths;
}

bool UploadManager::moveOrDeleteAfterUpload(const String &csvPath,
                                            const String &uploadedPath,
                                            const String &uploadedFilename)
{
    String filename = basenameOf(csvPath);
    bool hasSeparateUploadArtifact = uploadedPath.length() > 0 && uploadedPath != csvPath;
    String archiveFilename = uploadedFilename.length() > 0 ? basenameOf(uploadedFilename) : basenameOf(uploadedPath);
    bool removedOriginal = false;
    bool archivedUpload = true;

    if (cfg->delete_after_upload)
    {
        removedOriginal = SD.remove(csvPath);
        if (hasSeparateUploadArtifact && SD.exists(uploadedPath))
        {
            SD.remove(uploadedPath);
        }
    }
    else if (hasSeparateUploadArtifact)
    {
        String destPath = String(UPLOAD_DIR) + "/" + archiveFilename;
        if (SD.exists(destPath))
        {
            SD.remove(destPath);
        }
        archivedUpload = SD.rename(uploadedPath, destPath);
        if (!archivedUpload)
        {
            logger.debugPrintln(String("[Upload] archive gzip rename failed: ") + archiveFilename);
        }
        else
        {
            removedOriginal = SD.remove(csvPath);
            if (!removedOriginal)
            {
                logger.debugPrintln(String("[Upload] warning: original remains after gzip archive: ") + filename);
            }
        }
    }
    else
    {
        String destPath = String(UPLOAD_DIR) + "/" + filename;
        if (SD.exists(destPath))
        {
            SD.remove(destPath);
        }
        removedOriginal = SD.rename(csvPath, destPath);
        if (!removedOriginal)
        {
            logger.debugPrintln(String("[Upload] archive rename failed; deleting original: ") + filename);
            removedOriginal = SD.remove(csvPath);
        }
    }

    if (removedOriginal)
    {
        String finalizedName = (!cfg->delete_after_upload && hasSeparateUploadArtifact && archivedUpload)
                                   ? archiveFilename
                                   : filename;
        logger.debugPrintln(String("[Upload] uploaded file finalized: ") + finalizedName);
        if (csvPath == maxIndexPath)
        {
            writePlaceholderFor(filename);
        }
    }
    else
    {
        logger.debugPrintln(String("[Upload] warning: original remains after upload: ") + filename);
    }

    return removedOriginal && archivedUpload;
}

bool UploadManager::ensureDirectories()
{
    if (!SD.exists(UPLOAD_DIR) && !SD.mkdir(UPLOAD_DIR))
    {
        return false;
    }
    if (!SD.exists(UPLOAD_TEMP_DIR) && !SD.mkdir(UPLOAD_TEMP_DIR))
    {
        return false;
    }
    return true;
}

bool UploadManager::ensureThinDir()
{
    if (SD.exists(UPLOAD_THIN_DIR))
    {
        return true;
    }
    return SD.mkdir(UPLOAD_THIN_DIR);
}

bool UploadManager::moveToThin(const String &csvPath)
{
    String name = basenameOf(csvPath);
    String dest = String(UPLOAD_THIN_DIR) + "/" + name;
    if (csvPath == dest)
    {
        // Already in thin dir (came in via retry_thin_files).
        return true;
    }
    if (!ensureThinDir())
    {
        logger.debugPrintln(String("[Upload] moveToThin: cannot create ") + UPLOAD_THIN_DIR);
        return false;
    }
    if (SD.exists(dest))
    {
        SD.remove(dest);
    }
    bool moved = SD.rename(csvPath, dest);
    if (moved)
    {
        logger.debugPrintln(String("[Upload] moveToThin: ") + name);
        if (csvPath == maxIndexPath)
        {
            writePlaceholderFor(name);
        }
    }
    else
    {
        logger.debugPrintln(String("[Upload] moveToThin: rename failed for ") + name);
    }
    return moved;
}

void UploadManager::purgeTempDir()
{
    if (!SD.exists(UPLOAD_TEMP_DIR))
    {
        return;
    }

    File dir = SD.open(UPLOAD_TEMP_DIR);
    if (!dir || !dir.isDirectory())
    {
        if (dir)
        {
            dir.close();
        }
        return;
    }

    File entry;
    while ((entry = dir.openNextFile()))
    {
        if (!entry.isDirectory())
        {
            String path = String(UPLOAD_TEMP_DIR) + "/" + basenameOf(entry.name());
            entry.close();
            SD.remove(path);
        }
        else
        {
            entry.close();
        }
    }
    dir.close();
}

bool UploadManager::connectHomeNetwork()
{
    if (wifi == nullptr || cfg == nullptr || scanCfg == nullptr)
    {
        return false;
    }

    uploader.setCredentials(cfg->wigle_api_name, cfg->wigle_api_token);
    return wifi->connectSTA(cfg->upload_ssid,
                            cfg->upload_password,
                            UPLOAD_STA_CONNECT_TIMEOUT_MS,
                            scanCfg->wifi_country_code);
}

void UploadManager::disconnectHomeNetwork()
{
    if (wifi != nullptr)
    {
        wifi->disconnectSTA();
    }
    uploadConnected = false;
}

void UploadManager::doubleBeep()
{
    if (alerts == nullptr)
    {
        return;
    }
    ToneNote notes[] = {{BEEP_FREQ, 80}, {0, 80}, {BEEP_FREQ, 80}};
    alerts->playSequence(notes, 3);
}

void UploadManager::cancelBeep()
{
    if (alerts != nullptr)
    {
        alerts->setBuzzer(BOOP_FREQ, SHORT_TONE_MS);
    }
}

void UploadManager::writePlaceholderFor(const String &uploadedFilename)
{
    String placeholder = String(CSV_DIR) + "/" + basenameOf(uploadedFilename);
    File file = SD.open(placeholder, FILE_WRITE);
    if (file)
    {
        file.close();
        logger.debugPrintln(String("[Upload] placeholder written: ") + placeholder);
        CsvFileMaintenance::cleanupStaleZeroBytePlaceholders(SD);
    }
    else
    {
        logger.debugPrintln(String("[Upload] placeholder failed: ") + placeholder);
    }
}

void UploadManager::recomputeStats()
{
    stats.successCount = 0;
    stats.failCount = 0;
    stats.thinCount = 0;
    stats.bytesOriginal = 0;
    stats.bytesCompressed = 0;
    stats.recordCount = 0;

    for (const UploadFileResult &result : results)
    {
        if (result.success)
        {
            stats.successCount++;
        }
        else if (result.skippedThin)
        {
            stats.thinCount++;
        }
        else
        {
            stats.failCount++;
        }
        stats.bytesOriginal += result.bytesOriginal;
        stats.bytesCompressed += result.bytesCompressed;
        stats.recordCount += result.recordCount;
    }
}

void UploadManager::setupRetry()
{
    std::vector<String> retryFiles;
    std::vector<UploadFileResult> keptResults;

    for (const UploadFileResult &result : results)
    {
        if (result.success || result.skippedThin)
        {
            keptResults.push_back(result);
        }
        else
        {
            retryFiles.push_back(result.filename);
        }
    }

    if (retryFiles.empty())
    {
        return;
    }

    pendingFiles = retryFiles;
    results = keptResults;
    currentIndex = 0;
    retryMode = true;
    cancelRequested = false;
    uploadConnected = false;
    stats.startMs = millis();
    phase = UploadPhase::UPLOADING;
    logger.debugPrintln(String("[Upload] retrying failed files: ") + String((int)pendingFiles.size()));
}

// ── Sweep scan helper ────────────────────────────────────────────────────────

bool UploadManager::countSweepsUpToThreshold(const String &csvPath, uint16_t threshold, uint16_t &sweepsFound)
{
    File f = SD.open(csvPath, FILE_READ);
    if (!f)
    {
        sweepsFound = 0;
        return false;
    }
    if (superDebug)
    {
        logger.debugPrintln(String("[Upload] sweep scan begin: ") + basenameOf(csvPath) +
                            " (threshold=" + String(threshold) + ")");
    }

    char buf[UPLOAD_SWEEP_LINE_BUF_BYTES];
    String prevFirstSeen;
    sweepsFound = 0;

    while (f.available())
    {
        size_t n = f.readBytesUntil('\n', buf, sizeof(buf) - 1);
        buf[n] = '\0';
        // Strip trailing CR
        if (n > 0 && buf[n - 1] == '\r')
        {
            buf[n - 1] = '\0';
        }
        if (n == 0)
        {
            continue;
        }
        // Skip header rows: pre-header begins with "WigleWifi", header begins with "MAC,"
        if (buf[0] == 'W' || buf[0] == 'M' || buf[0] == '#')
        {
            continue;
        }
        String line(buf);
        String firstSeen = extractFirstSeenField(line);
        if (firstSeen.length() == 0)
        {
            continue;
        }
        if (firstSeen != prevFirstSeen)
        {
            prevFirstSeen = firstSeen;
            sweepsFound++;
            if (sweepsFound >= threshold)
            {
                // Early exit: file passes — no need to read further.
                break;
            }
        }
    }

    f.close();
    return true;
}

String UploadManager::extractFirstSeenField(const String &line)
{
    // Walk to the 4th field (index 3): MAC, SSID, AuthMode, FirstSeen, ...
    int fieldIndex = 0;
    int fieldStart = 0;
    bool inQuotes = false;
    int len = (int)line.length();
    for (int i = 0; i < len; i++)
    {
        char c = line.charAt(i);
        if (c == '"')
        {
            // Toggle quotes (CSV "" escape handled implicitly: the second " toggles back on)
            inQuotes = !inQuotes;
            continue;
        }
        if (c == ',' && !inQuotes)
        {
            if (fieldIndex == 3)
            {
                String f = line.substring(fieldStart, i);
                f.trim();
                if (f.length() >= 2 && f.charAt(0) == '"' && f.charAt(f.length() - 1) == '"')
                {
                    f = f.substring(1, f.length() - 1);
                }
                return f;
            }
            fieldIndex++;
            fieldStart = i + 1;
        }
    }
    if (fieldIndex == 3)
    {
        String f = line.substring(fieldStart);
        f.trim();
        if (f.length() >= 2 && f.charAt(0) == '"' && f.charAt(f.length() - 1) == '"')
        {
            f = f.substring(1, f.length() - 1);
        }
        return f;
    }
    return String();
}
