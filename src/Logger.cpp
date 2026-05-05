#include "Logger.h"
#include <SD.h>
#include <time.h>

// Global singleton instance
Logger logger;

Logger::Logger()
    : config(nullptr), debugFile(DEBUG_LOG_FILENAME), sdReady(false)
{
}

bool Logger::begin()
{
    sdReady = true;
    Serial.println("[Logger] Initialized");
    return true;
}

void Logger::setConfig(AppConfig *cfg)
{
    config = cfg;
    if (config && config->debug.enabled)
    {
        Serial.println("[Logger] Debug file logging enabled");
    }
}

void Logger::debugPrint(const char *msg)
{
    Serial.print(msg);
    writeToFile(msg, false);
}

void Logger::debugPrintln(const char *msg)
{
    Serial.println(msg);
    writeToFile(msg, true);
}

void Logger::debugPrintln(const String &msg)
{
    debugPrintln(msg.c_str());
}

void Logger::debugPrintln()
{
    Serial.println();
    writeToFile("", true);
}

bool Logger::logStatsCSV(const String& csvRow)
{
    if (!sdReady || !config || !config->debug.system_stats_enabled)
        return false;
    
    if (!ensureLogDirectory())
        return false;

    String statsFile = String(DEBUG_LOG_DIR) + "/stats.csv";
    bool writeHeader = !SD.exists(statsFile);

    File f = SD.open(statsFile, FILE_APPEND);
    if (!f)
        return false;

    if (writeHeader)
    {
        f.print("timestamp,epoch_s,type,uptime_s,mem_used,mem_free,heap_size,heap_used,heap_free,min_heap_free,max_alloc_heap,psram_size,psram_free,total_sweeps,max_stations_per_sweep,total_stations_seen,unique_stations,last_scan_count,bssid_set_count,bssid_set_est_bytes,batt_pct,batt_mv,is_charging,temp_c,cpu_mhz,task_hwm,tot_tasks,max_loop_ms\n");
    }

    f.print(csvRow);
    f.print("\n");
    f.close();
    
    return true;
}

String Logger::getDebugLogFile()
{
    return debugFile;
}

bool Logger::ensureLogDirectory()
{
    if (SD.exists(DEBUG_LOG_DIR))
        return true;
    if (!SD.mkdir(DEBUG_LOG_DIR))
    {
        // Use Serial directly to avoid infinite recursion through debugPrintln -> writeToFile -> ensureLogDirectory
        Serial.println("[Logger] Failed to create debug log directory");
        return false;
    }
    return true;
}

bool Logger::clearDebugLog()
{
    if (!sdReady)
        return false;
    if (!ensureLogDirectory())
        return false;

    File f = SD.open(debugFile, FILE_WRITE);
    if (!f)
        return false;
    f.close();
    debugPrintln("[Logger] Debug log cleared");
    return true;
}

void Logger::writeToFile(const char *msg, bool newline)
{
    if (!sdReady || !config || !config->debug.enabled)
        return;

    if (!ensureLogDirectory())
        return;

    File f = SD.open(debugFile, FILE_APPEND);
    if (!f)
        return;

    // Prepend timestamp
    time_t now = time(nullptr);
    if (now > 100000L)
    {
        struct tm t;
        localtime_r(&now, &t);
        char ts[23]; // 22 chars + 1 null terminator
        strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", &t);
        f.print(ts);
    }
    else
    {
        f.print("[----/--/-- --:--:--] ");
    }

    if (newline)
    {
        f.println(msg);
    }
    else
    {
        f.print(msg);
    }
    f.close();
}
