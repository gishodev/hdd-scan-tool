#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <vector>
#include <string>

// Visual Grid Constants
#define GRID_COLS 80
#define GRID_ROWS 40
#define GRID_SIZE (GRID_COLS * GRID_ROWS)

// Sector block status categories
enum BlockStatus {
    STATUS_UNSCANNED = 0,
    STATUS_SCANNING,
    STATUS_HEALTHY,    // < 50ms
    STATUS_SLOW,       // 50ms - 150ms
    STATUS_VERY_SLOW,  // 150ms - 500ms
    STATUS_BAD         // Read error / timeout (> 500ms)
};

// Information about detected physical drives
struct DriveInfo {
    int index;
    std::wstring path;
    std::wstring model;
    ULONGLONG sizeBytes;
};

// Shared state for background scanner thread
struct ScanStats {
    ULONGLONG totalBytes;
    ULONGLONG bytesScanned;
    ULONGLONG totalBlocks;
    ULONGLONG blocksScanned;
    ULONGLONG badSectors;
    ULONGLONG slowSectors;
    ULONGLONG verySlowSectors;
    double currentSpeedMBs;
    double averageSpeedMBs;
    ULONGLONG startTime;
    ULONGLONG elapsedMs;
    ULONGLONG currentLBA;
    DWORD blockSizeBytes;
    
    // Thread flags
    bool isRunning;
    bool isPaused;
    bool requestStop;
    
    // Status text
    wchar_t currentStatusText[256];
};

// Global shared variables (extern)
extern CRITICAL_SECTION g_ScanStatsCS;
extern ScanStats g_ScanStats;
extern BlockStatus g_Grid[GRID_SIZE];
extern double g_GridLatency[GRID_SIZE];
extern std::vector<double> g_SpeedHistory;

// Localization Definitions
enum Language {
    LANG_EN = 0,
    LANG_JA,
    LANG_ZH
};

enum StringID {
    IDS_TARGET_DRIVE,
    IDS_BLOCK_SIZE,
    IDS_DIAGNOSTICS_TITLE,
    IDS_SURFACE_GRID_TITLE,
    IDS_SPEED_BENCHMARK_TITLE,
    IDS_SURFACE_LOG_TITLE,
    
    // Buttons
    IDS_BTN_START,
    IDS_BTN_PAUSE,
    IDS_BTN_RESUME,
    IDS_BTN_STOP,
    
    // Stats labels
    IDS_STAT_STATUS,
    IDS_STAT_CAPACITY,
    IDS_STAT_PROGRESS,
    IDS_STAT_BLOCKS,
    IDS_STAT_SPEED,
    IDS_STAT_HEALTHY,
    IDS_STAT_SLOW,
    IDS_STAT_VERY_SLOW,
    IDS_STAT_BAD_SECTORS,
    IDS_STAT_ELAPSED,
    IDS_STAT_REMAINING,
    
    // Status values
    IDS_VAL_IDLE,
    IDS_VAL_SCANNING,
    IDS_VAL_PAUSED,
    IDS_VAL_COMPLETED,
    IDS_VAL_ERROR,
    
    // Tooltips & details
    IDS_GRID_UNSCANNED,
    IDS_GRID_SCANNING,
    IDS_GRID_HEALTHY,
    IDS_GRID_SLOW,
    IDS_GRID_VERY_SLOW,
    IDS_GRID_BAD,
    IDS_GRID_HOVER_PROMPT,
    IDS_GRID_AWAITING,
    
    // Logs
    IDS_LOG_INIT_SUCCESS,
    IDS_LOG_NO_DRIVES,
    IDS_LOG_DETECTED_DRIVES,
    IDS_LOG_OPENING_DEVICE,
    IDS_LOG_ERR_OPEN_DRIVE,
    IDS_LOG_ERR_CAPACITY,
    IDS_LOG_SCAN_STARTED,
    IDS_LOG_ERR_ALLOC,
    IDS_LOG_ERR_OUT_OF_MEM,
    IDS_LOG_ERR_OPEN_CODE,
    IDS_LOG_ERR_DISK_CAPACITY,
    IDS_LOG_ABORT_USER,
    IDS_LOG_PAUSED,
    IDS_LOG_RESUMED,
    IDS_LOG_BAD_SECTOR,
    IDS_LOG_VERY_SLOW_SECTOR,
    IDS_LOG_SLOW_SECTOR,
    IDS_LOG_SCAN_COMPLETED,
    IDS_LOG_INIT_THREAD,
    IDS_LOG_ABORTING,
    IDS_LOG_SEEK_ERROR,
    IDS_MAX
};

extern Language g_CurrentLanguage;

// Localization Helpers
std::wstring Utf8ToWstring(const char* utf8Str);
std::wstring GetStr(StringID id);
std::wstring GetBlockSizeStr(int index);

// Function Declarations
std::vector<DriveInfo> EnumerateDrives();
DWORD WINAPI ScanThreadProc(LPVOID lpParam);
void StartScan(int driveIndex, DWORD blockSizeBytes);
void StopScan();
void PauseScan();
void ResumeScan();
void GetSharedStats(ScanStats* pStatsOut);
void AddLogEntry(const wchar_t* format, ...);
std::vector<std::wstring> GetLogEntries();
void ClearLog();

