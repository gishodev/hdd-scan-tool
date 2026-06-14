#include "scanner.h"
#include <winioctl.h>
#include <ntddstor.h>
#include <cstdio>
#include <vector>
#include <string>

// Definitions of global variables
CRITICAL_SECTION g_ScanStatsCS;
ScanStats g_ScanStats;
BlockStatus g_Grid[GRID_SIZE];
double g_GridLatency[GRID_SIZE];
std::vector<double> g_SpeedHistory;

Language g_CurrentLanguage = LANG_EN;

static HANDLE g_hScanThread = NULL;
static CRITICAL_SECTION g_LogCS;
static std::vector<std::wstring> g_LogEntries;

// Translation matrix (UTF-8 formatted)
const char* g_Translations[IDS_MAX][3] = {
    // English, Japanese, Chinese Simplified
    { "TARGET DRIVE:", "対象ドライブ:", "目标驱动器:" },
    { "BLOCK SIZE:", "ブロックサイズ:", "块大小:" },
    { "DIAGNOSTICS & STATS", "診断と統計情報", "诊断与统计" },
    { "SURFACE GRID MAP", "サーフェスグリッドマップ", "表面网格图" },
    { "SPEED BENCHMARK (MB/s)", "速度ベンチマーク (MB/s)", "速度基准 (MB/s)" },
    { "SURFACE LOG DETAILS", "サーフェスログ詳細", "表面日志详情" },
    
    // Buttons
    { "Start Scan", "スキャン開始", "开始扫描" },
    { "Pause", "一時停止", "暂停" },
    { "Resume", "再開", "恢复" },
    { "Stop", "停止", "停止" },
    
    // Stats labels
    { "Status", "ステータス", "状态" },
    { "Capacity", "容量", "容量" },
    { "Progress", "進捗状況", "进度" },
    { "Blocks", "ブロック数", "数据块" },
    { "Speed (Cur/Avg)", "速度 (現在/平均)", "速度 (当前/平均)" },
    { "Healthy (<150ms)", "正常 (<150ms)", "良好 (<150ms)" },
    { "Slow (>150ms)", "遅延 (>150ms)", "缓慢 (>150ms)" },
    { "Very Slow (>500ms)", "極遅 (>500ms)", "极慢 (>500ms)" },
    { "Bad Sectors", "不良セクター数", "损坏扇区" },
    { "Elapsed Time", "経過時間", "已用时间" },
    { "Est. Remaining", "予想残り時間", "估计剩余时间" },
    
    // Status values
    { "Idle", "待機中", "空闲" },
    { "Scanning...", "スキャン中...", "正在扫描..." },
    { "Paused", "一時停止中", "已暂停" },
    { "Completed", "完了", "已完成" },
    { "Error", "エラー", "错误" },
    
    // Tooltips & details
    { "Unscanned", "未スキャン", "未扫描" },
    { "Scanning", "スキャン中", "正在扫描" },
    { "Healthy", "正常", "良好" },
    { "Slow (>150ms)", "遅延 (>150ms)", "缓慢 (>150ms)" },
    { "Very Slow (>500ms)", "極遅 (>500ms)", "极慢 (>500ms)" },
    { "Bad Sector", "不良セクター", "损坏扇区" },
    { "Hover over cells for sector details", "セルにカーソルを合わせるとセクターの詳細が表示されます", "将鼠标悬停在网格上以查看扇区详情" },
    { "Awaiting scan data...", "スキャンデータの待機中...", "等待扫描数据..." },
    
    // Logs
    { "Application initialized successfully.", "アプリケーションが正常に初期化されました。", "应用程序初始化成功。" },
    { "WARNING: No physical drives detected! Running as administrator is required.", "警告: 物理ドライブが検出されませんでした！管理者として実行する必要があります。", "警告: 未检测到物理驱动器！需要以管理员权限运行。" },
    { "Detected %d physical drives.", "%d 個の物理ドライブを検出しました。", "检测到 %d 个物理驱动器。" },
    { "Opening device %s...", "デバイス %s を開いています...", "正在打开设备 %s..." },
    { "ERROR: Cannot open drive %s. Error code: %u. Make sure you are running as Administrator.", "エラー: ドライブ %s を開けません。エラーコード: %u。管理者として実行しているか確認してください。", "错误: 无法打开驱动器 %s。错误代码: %u。请确保您正以管理员身份运行。" },
    { "ERROR: Unable to get drive capacity.", "エラー: ドライブ容量を取得できません。", "错误: 无法获取驱动器容量。" },
    { "Scan started. Drive capacity: %.2f GB. Block size: %u KB. Total blocks to scan: %I64u", "スキャンを開始しました。ドライブ容量: %.2f GB。ブロックサイズ: %u KB。スキャン総ブロック数: %I64u", "扫描已开始。驱动器容量: %.2f GB。块大小: %u KB。总扫描块数: %I64u" },
    { "ERROR: Failed to allocate aligned buffer for scanning.", "エラー: スキャン用のアライメントバッファの割り当てに失敗しました。", "错误: 无法为扫描分配对齐的缓冲区。" },
    { "Failed: Out of memory", "失敗: メモリ不足", "失败: 内存不足" },
    { "Failed: Open error %u", "失敗: オープンエラー %u", "失败: 打开错误 %u" },
    { "Failed: Disk capacity error", "失敗: ディスク容量エラー", "失败: 磁盘容量错误" },
    { "Scan abort requested by user.", "ユーザーによってスキャンが中止されました。", "用户请求中止扫描。" },
    { "Scan paused.", "スキャンが一時停止されました。", "扫描已暂停。" },
    { "Scan resumed.", "スキャンが再開されました。", "扫描已恢复。" },
    { "Bad Sector detected at LBA %I64u (Offset %I64u) - Error: %u", "LBA %I64u (オフセット %I64u) で不良セクターを検出 - エラー: %u", "在 LBA %I64u (偏移量 %I64u) 检测到损坏扇区 - 错误: %u" },
    { "Warning: Very Slow Sector (>500ms) at LBA %I64u - %0.1f ms", "警告: LBA %I64u で極遅セクター (>500ms) - %0.1f ms", "警告: LBA %I64u 处有极慢扇区 (>500ms) - %0.1f 毫秒" },
    { "Info: Slow Sector (>150ms) at LBA %I64u - %0.1f ms", "情報: LBA %I64u で遅延セクター (>150ms) - %0.1f ms", "信息: LBA %I64u 处有缓慢扇区 (>150ms) - %0.1f 毫秒" },
    { "Scan completed successfully.", "スキャンが正常に完了しました。", "扫描已成功完成。" },
    { "Initializing scan thread...", "スキャンスレッドを初期化しています...", "正在初始化扫描线程..." },
    { "Aborting...", "中止中...", "正在中止..." },
    { "Seek Error at block %I64u (Offset %I64u). Error: %u", "ブロック %I64u (オフセット %I64u) でシークエラー。エラー: %u", "数据块 %I64u (偏移量 %I64u) 处寻道错误。错误: %u" }
};

std::wstring Utf8ToWstring(const char* utf8Str) {
    if (!utf8Str) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, buf.data(), len);
    return std::wstring(buf.data());
}

std::wstring GetStr(StringID id) {
    if (id < 0 || id >= IDS_MAX) return L"";
    return Utf8ToWstring(g_Translations[id][g_CurrentLanguage]);
}

std::wstring GetBlockSizeStr(int index) {
    switch (index) {
        case 0:
            return g_CurrentLanguage == LANG_JA ? L"512 バイト (遅い)" :
                   (g_CurrentLanguage == LANG_ZH ? L"512 字节 (慢)" : L"512 Bytes (Slow)");
        case 1:
            return g_CurrentLanguage == LANG_JA ? L"4 KB (標準クラスター)" :
                   (g_CurrentLanguage == LANG_ZH ? L"4 KB (标准簇)" : L"4 KB (Standard Cluster)");
        case 2:
            return g_CurrentLanguage == LANG_JA ? L"64 KB (推奨)" :
                   (g_CurrentLanguage == LANG_ZH ? L"64 KB (平衡)" : L"64 KB (Balanced)");
        case 3:
            return g_CurrentLanguage == LANG_JA ? L"256 KB (高速)" :
                   (g_CurrentLanguage == LANG_ZH ? L"256 KB (快速)" : L"256 KB (Fast)");
        case 4:
            return g_CurrentLanguage == LANG_JA ? L"1 MB (極超高速)" :
                   (g_CurrentLanguage == LANG_ZH ? L"1 MB (极速)" : L"1 MB (High Speed)");
        default:
            return L"";
    }
}


// Internal helpers
static ULONGLONG GetDiskSize(HANDLE hDevice) {
    GET_LENGTH_INFORMATION lengthInfo = { 0 };
    DWORD bytesReturned = 0;
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL)) {
        return lengthInfo.Length.QuadPart;
    }
    
    DISK_GEOMETRY_EX geometry = { 0 };
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geometry, sizeof(geometry), &bytesReturned, NULL)) {
        return geometry.DiskSize.QuadPart;
    }
    
    return 0;
}

std::vector<DriveInfo> EnumerateDrives() {
    std::vector<DriveInfo> drives;
    
    // Initialize Critical Sections if not already done
    static bool csInitialized = false;
    if (!csInitialized) {
        InitializeCriticalSection(&g_ScanStatsCS);
        InitializeCriticalSection(&g_LogCS);
        csInitialized = true;
    }

    for (int i = 0; i < 16; ++i) {
        wchar_t drivePath[64];
        swprintf_s(drivePath, L"\\\\.\\PhysicalDrive%d", i);
        
        HANDLE hDevice = CreateFileW(
            drivePath,
            0, // Read-write attributes query only
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hDevice != INVALID_HANDLE_VALUE) {
            ULONGLONG size = GetDiskSize(hDevice);
            if (size > 0) {
                DriveInfo info;
                info.index = i;
                info.path = drivePath;
                
                // Get Drive Model Name
                STORAGE_PROPERTY_QUERY query = {};
                query.PropertyId = StorageDeviceProperty;
                query.QueryType = PropertyStandardQuery;
                
                BYTE outBuf[2048] = { 0 };
                DWORD bytesReturned = 0;
                std::wstring modelName = L"";
                
                if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), outBuf, sizeof(outBuf), &bytesReturned, NULL)) {
                    STORAGE_DEVICE_DESCRIPTOR* desc = (STORAGE_DEVICE_DESCRIPTOR*)outBuf;
                    std::string vendor = "";
                    std::string product = "";
                    
                    if (desc->VendorIdOffset > 0 && desc->VendorIdOffset < bytesReturned) {
                        vendor = (char*)(outBuf + desc->VendorIdOffset);
                    }
                    if (desc->ProductIdOffset > 0 && desc->ProductIdOffset < bytesReturned) {
                        product = (char*)(outBuf + desc->ProductIdOffset);
                    }
                    
                    // Helper to trim spaces
                    auto TrimString = [](std::string& s) {
                        while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
                        while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
                    };
                    TrimString(vendor);
                    TrimString(product);
                    
                    std::string combined = vendor;
                    if (!combined.empty() && !product.empty()) combined += " ";
                    combined += product;
                    
                    if (!combined.empty()) {
                        int len = MultiByteToWideChar(CP_ACP, 0, combined.c_str(), -1, NULL, 0);
                        if (len > 0) {
                            std::vector<wchar_t> wbuf(len);
                            MultiByteToWideChar(CP_ACP, 0, combined.c_str(), -1, wbuf.data(), len);
                            modelName = wbuf.data();
                        }
                    }
                }
                
                if (modelName.empty()) {
                    modelName = L"Physical Drive " + std::to_wstring(i);
                }
                
                info.model = modelName;
                info.sizeBytes = size;
                drives.push_back(info);
            }
            CloseHandle(hDevice);
        }
    }
    return drives;
}

void AddLogEntry(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    wchar_t buffer[1024];
    vswprintf_s(buffer, format, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[64];
    swprintf_s(timeBuf, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    std::wstring entry = timeBuf;
    entry += buffer;

    EnterCriticalSection(&g_LogCS);
    g_LogEntries.push_back(entry);
    // Keep log buffer reasonable
    if (g_LogEntries.size() > 500) {
        g_LogEntries.erase(g_LogEntries.begin());
    }
    LeaveCriticalSection(&g_LogCS);
}

std::vector<std::wstring> GetLogEntries() {
    EnterCriticalSection(&g_LogCS);
    std::vector<std::wstring> logs = g_LogEntries;
    LeaveCriticalSection(&g_LogCS);
    return logs;
}

void ClearLog() {
    EnterCriticalSection(&g_LogCS);
    g_LogEntries.clear();
    LeaveCriticalSection(&g_LogCS);
}

void GetSharedStats(ScanStats* pStatsOut) {
    EnterCriticalSection(&g_ScanStatsCS);
    *pStatsOut = g_ScanStats;
    LeaveCriticalSection(&g_ScanStatsCS);
}

DWORD WINAPI ScanThreadProc(LPVOID lpParam) {
    int driveIndex = (int)(ULONG_PTR)lpParam;
    
    wchar_t drivePath[64];
    swprintf_s(drivePath, L"\\\\.\\PhysicalDrive%d", driveIndex);
    
    AddLogEntry(GetStr(IDS_LOG_OPENING_DEVICE).c_str(), drivePath);
    
    // Open physical drive with NO BUFFERING to bypass system cache
    HANDLE hDevice = CreateFileW(
        drivePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        AddLogEntry(GetStr(IDS_LOG_ERR_OPEN_DRIVE).c_str(), drivePath, err);
        
        EnterCriticalSection(&g_ScanStatsCS);
        g_ScanStats.isRunning = false;
        swprintf_s(g_ScanStats.currentStatusText, L"Failed: Open error %u", err);
        LeaveCriticalSection(&g_ScanStatsCS);
        return 0;
    }
    
    ULONGLONG diskSize = GetDiskSize(hDevice);
    if (diskSize == 0) {
        AddLogEntry(GetStr(IDS_LOG_ERR_CAPACITY).c_str());
        CloseHandle(hDevice);
        EnterCriticalSection(&g_ScanStatsCS);
        g_ScanStats.isRunning = false;
        swprintf_s(g_ScanStats.currentStatusText, L"Failed: Disk capacity error");
        LeaveCriticalSection(&g_ScanStatsCS);
        return 0;
    }
    
    DWORD blockSize = 0;
    EnterCriticalSection(&g_ScanStatsCS);
    blockSize = g_ScanStats.blockSizeBytes;
    g_ScanStats.totalBytes = diskSize;
    g_ScanStats.totalBlocks = diskSize / blockSize;
    ULONGLONG totalBlocks = g_ScanStats.totalBlocks;
    g_ScanStats.startTime = GetTickCount64();
    LeaveCriticalSection(&g_ScanStatsCS);
    
    AddLogEntry(GetStr(IDS_LOG_SCAN_STARTED).c_str(), 
        (double)diskSize / (1024.0 * 1024.0 * 1024.0), blockSize / 1024, totalBlocks);

    // Allocate sector-aligned buffer for direct IO reading
    void* readBuffer = VirtualAlloc(NULL, blockSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!readBuffer) {
        AddLogEntry(GetStr(IDS_LOG_ERR_ALLOC).c_str());
        CloseHandle(hDevice);
        EnterCriticalSection(&g_ScanStatsCS);
        g_ScanStats.isRunning = false;
        swprintf_s(g_ScanStats.currentStatusText, L"Failed: Out of memory");
        LeaveCriticalSection(&g_ScanStatsCS);
        return 0;
    }
    
    LARGE_INTEGER qpcFreq;
    QueryPerformanceFrequency(&qpcFreq);
    
    ULONGLONG lastBytesScanned = 0;
    ULONGLONG lastSpeedCheckTime = GetTickCount64();

    for (ULONGLONG blockIndex = 0; blockIndex < totalBlocks; ++blockIndex) {
        // Handle pause & stop flags
        while (true) {
            EnterCriticalSection(&g_ScanStatsCS);
            bool stopReq = g_ScanStats.requestStop;
            bool paused = g_ScanStats.isPaused;
            LeaveCriticalSection(&g_ScanStatsCS);
            
            if (stopReq) {
                AddLogEntry(GetStr(IDS_LOG_ABORT_USER).c_str());
                goto scan_cleanup;
            }
            if (paused) {
                Sleep(50);
                continue;
            }
            break;
        }

        ULONGLONG offset = blockIndex * blockSize;
        LARGE_INTEGER liOffset;
        liOffset.QuadPart = (LONGLONG)offset;
        
        // Seek to correct offset
        if (!SetFilePointerEx(hDevice, liOffset, NULL, FILE_BEGIN)) {
            DWORD err = GetLastError();
            AddLogEntry(GetStr(IDS_LOG_SEEK_ERROR).c_str(), blockIndex, offset, err);
        }
        
        // Map block index to grid cell index
        int cellIndex = (int)((blockIndex * GRID_SIZE) / totalBlocks);
        if (cellIndex >= GRID_SIZE) cellIndex = GRID_SIZE - 1;

        // Show active scanning cursor
        BlockStatus oldCellStatus = STATUS_UNSCANNED;
        EnterCriticalSection(&g_ScanStatsCS);
        oldCellStatus = g_Grid[cellIndex];
        if (oldCellStatus == STATUS_UNSCANNED) {
            g_Grid[cellIndex] = STATUS_SCANNING;
        }
        LeaveCriticalSection(&g_ScanStatsCS);
        
        // Perform read operations with high-precision latency profiling
        LARGE_INTEGER qpcStart, qpcEnd;
        DWORD bytesRead = 0;
        
        QueryPerformanceCounter(&qpcStart);
        BOOL readSuccess = ReadFile(hDevice, readBuffer, blockSize, &bytesRead, NULL);
        QueryPerformanceCounter(&qpcEnd);
        
        double readMs = (double)(qpcEnd.QuadPart - qpcStart.QuadPart) * 1000.0 / qpcFreq.QuadPart;
        BlockStatus resultStatus = STATUS_HEALTHY;
        
        if (!readSuccess || bytesRead < blockSize) {
            resultStatus = STATUS_BAD;
            DWORD err = GetLastError();
            AddLogEntry(GetStr(IDS_LOG_BAD_SECTOR).c_str(), blockIndex * (blockSize / 512), offset, err);
        } else {
            if (readMs > 500.0) {
                resultStatus = STATUS_VERY_SLOW;
                AddLogEntry(GetStr(IDS_LOG_VERY_SLOW_SECTOR).c_str(), blockIndex * (blockSize / 512), readMs);
            } else if (readMs > 150.0) {
                resultStatus = STATUS_SLOW;
                AddLogEntry(GetStr(IDS_LOG_SLOW_SECTOR).c_str(), blockIndex * (blockSize / 512), readMs);
            }
        }
        
        // Update stats and cell grid state
        EnterCriticalSection(&g_ScanStatsCS);
        
        // Keep worst status for this grid cell
        if (resultStatus == STATUS_BAD) {
            if (g_Grid[cellIndex] != STATUS_BAD) {
                g_Grid[cellIndex] = STATUS_BAD;
                g_ScanStats.badSectors++;
            }
        } else if (resultStatus == STATUS_VERY_SLOW) {
            if (g_Grid[cellIndex] < STATUS_VERY_SLOW) {
                g_Grid[cellIndex] = STATUS_VERY_SLOW;
                g_ScanStats.verySlowSectors++;
            }
        } else if (resultStatus == STATUS_SLOW) {
            if (g_Grid[cellIndex] < STATUS_SLOW) {
                g_Grid[cellIndex] = STATUS_SLOW;
                g_ScanStats.slowSectors++;
            }
        } else {
            if (g_Grid[cellIndex] == STATUS_SCANNING || g_Grid[cellIndex] == STATUS_UNSCANNED) {
                g_Grid[cellIndex] = STATUS_HEALTHY;
            }
        }
        
        if (readMs > g_GridLatency[cellIndex]) {
            g_GridLatency[cellIndex] = readMs;
        }
        
        g_ScanStats.bytesScanned += bytesRead;
        g_ScanStats.blocksScanned = blockIndex + 1;
        g_ScanStats.currentLBA = (blockIndex + 1) * (blockSize / 512);
        
        // Periodic speed sample and remaining time calculations
        ULONGLONG now = GetTickCount64();
        g_ScanStats.elapsedMs = now - g_ScanStats.startTime;
        
        ULONGLONG timeDiff = now - lastSpeedCheckTime;
        if (timeDiff >= 1000) {
            ULONGLONG bytesDiff = g_ScanStats.bytesScanned - lastBytesScanned;
            double instantSpeed = (double)bytesDiff / (1024.0 * 1024.0) / ((double)timeDiff / 1000.0);
            
            g_ScanStats.currentSpeedMBs = instantSpeed;
            g_ScanStats.averageSpeedMBs = (double)g_ScanStats.bytesScanned / (1024.0 * 1024.0) / ((double)g_ScanStats.elapsedMs / 1000.0);
            
            lastBytesScanned = g_ScanStats.bytesScanned;
            lastSpeedCheckTime = now;
            
            // Limit speed history buffer to graph resolution
            g_SpeedHistory.push_back(instantSpeed);
            if (g_SpeedHistory.size() > 60) {
                g_SpeedHistory.erase(g_SpeedHistory.begin());
            }
        }
        
        LeaveCriticalSection(&g_ScanStatsCS);
    }
    
    AddLogEntry(GetStr(IDS_LOG_SCAN_COMPLETED).c_str());
    
    EnterCriticalSection(&g_ScanStatsCS);
    swprintf_s(g_ScanStats.currentStatusText, L"Scan Completed");
    LeaveCriticalSection(&g_ScanStatsCS);

scan_cleanup:
    VirtualFree(readBuffer, 0, MEM_RELEASE);
    CloseHandle(hDevice);
    
    EnterCriticalSection(&g_ScanStatsCS);
    g_ScanStats.isRunning = false;
    LeaveCriticalSection(&g_ScanStatsCS);
    
    return 0;
}

void StartScan(int driveIndex, DWORD blockSizeBytes) {
    EnterCriticalSection(&g_ScanStatsCS);
    if (g_ScanStats.isRunning) {
        LeaveCriticalSection(&g_ScanStatsCS);
        return;
    }
    
    // Reset scanner states
    g_ScanStats.totalBytes = 0;
    g_ScanStats.bytesScanned = 0;
    g_ScanStats.totalBlocks = 0;
    g_ScanStats.blocksScanned = 0;
    g_ScanStats.badSectors = 0;
    g_ScanStats.slowSectors = 0;
    g_ScanStats.verySlowSectors = 0;
    g_ScanStats.currentSpeedMBs = 0.0;
    g_ScanStats.averageSpeedMBs = 0.0;
    g_ScanStats.startTime = GetTickCount64();
    g_ScanStats.elapsedMs = 0;
    g_ScanStats.currentLBA = 0;
    g_ScanStats.blockSizeBytes = blockSizeBytes;
    g_ScanStats.isRunning = true;
    g_ScanStats.isPaused = false;
    g_ScanStats.requestStop = false;
    swprintf_s(g_ScanStats.currentStatusText, L"Scanning...");
    
    for (int i = 0; i < GRID_SIZE; ++i) {
        g_Grid[i] = STATUS_UNSCANNED;
        g_GridLatency[i] = 0.0;
    }
    g_SpeedHistory.clear();
    LeaveCriticalSection(&g_ScanStatsCS);
    
    ClearLog();
    AddLogEntry(GetStr(IDS_LOG_INIT_THREAD).c_str());

    g_hScanThread = CreateThread(
        NULL,
        0,
        ScanThreadProc,
        (LPVOID)(ULONG_PTR)driveIndex,
        0,
        NULL
    );
}

void StopScan() {
    EnterCriticalSection(&g_ScanStatsCS);
    if (g_ScanStats.isRunning) {
        g_ScanStats.requestStop = true;
        swprintf_s(g_ScanStats.currentStatusText, L"Aborting...");
    }
    LeaveCriticalSection(&g_ScanStatsCS);
}

void PauseScan() {
    EnterCriticalSection(&g_ScanStatsCS);
    if (g_ScanStats.isRunning && !g_ScanStats.isPaused) {
        g_ScanStats.isPaused = true;
        swprintf_s(g_ScanStats.currentStatusText, L"Paused");
        AddLogEntry(GetStr(IDS_LOG_PAUSED).c_str());
    }
    LeaveCriticalSection(&g_ScanStatsCS);
}

void ResumeScan() {
    EnterCriticalSection(&g_ScanStatsCS);
    if (g_ScanStats.isRunning && g_ScanStats.isPaused) {
        g_ScanStats.isPaused = false;
        swprintf_s(g_ScanStats.currentStatusText, L"Scanning...");
        AddLogEntry(GetStr(IDS_LOG_RESUMED).c_str());
    }
    LeaveCriticalSection(&g_ScanStatsCS);
}
