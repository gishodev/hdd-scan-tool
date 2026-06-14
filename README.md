# 💾 HDD Surface Scanner & Diagnostics (Native x86)

A standalone, lightweight, high-performance native Windows Utility to inspect Hard Disk Drive (HDD) surfaces for bad sectors, response latencies, and reading performance benchmarks. 

Written in pure C++ using the Win32 API, GDI+, and high-precision timing, this tool operates directly on raw disk storage interfaces and compiles to a single statically-linked binary (`HddScanner.exe`) with **zero external runtime DLL dependencies**.

---

## 🚀 Key Features

* **Low-Level Direct IO Engine:** Opens physical drives (`\\.\PhysicalDriveX`) with `FILE_FLAG_NO_BUFFERING` to bypass Windows filesystem cache layers and read raw sector blocks directly.
* **Direct Sector Alignment:** Allocates page-aligned buffers via `VirtualAlloc` to satisfy unbuffered sector boundary constraints.
* **High-Precision Latency Profiling:** Measures sub-millisecond block response times using hardware-level high-resolution performance counters (`QueryPerformanceCounter`).
* **Progressive Sector Grid Map:** Dynamically paints a grid of $80 \times 40$ blocks (3,200 grid items) reflecting the drive's health status in real-time:
  * 🟩 **Healthy** (< 150ms latency)
  * 🟨 **Slow** (> 150ms latency)
  * 🟧 **Very Slow** (> 500ms latency)
  * 🟥 **Bad Sector** (Read failure / CRC / hardware error)
* **Interactive Tooltips:** Hovering over any cell in the grid displays the exact logical block address (LBA) sector range and the peak latency recorded in that block.
* **Speed Performance Chart:** Tracks a rolling moving-average transfer speed (MB/s) and draws a smooth line chart with alpha-blended area curves.
* **Monospaced Event Logging:** Lists precise details of bad sectors, delays, thread operations, and diagnostic results in a clean, scrollable technical console.
* **Flicker-Free Double Buffering:** Directs all painting instructions to an off-screen memory device context (DC) to prevent screen flickering, coupled with optimized region repaint cycles (near-0% idle CPU overhead).
* **Multi-Language Support (UTF-8):** Includes fully localized UI labels, headings, combobox options, and thread log entries in **English**, **Japanese (日本語)**, and **Chinese Simplified (简体中文)**, switchable dynamically at runtime.
* **Embedded App Icon & Manifest:** Features a custom high-contrast embedded icon (resolutions up to 256x256) and an execution manifest requesting UAC administrator privileges (essential for low-level storage access).

---

## 📁 Project Structure

* **`main.cpp`**: Core Win32 window initialization, event message pump (`WndProc`), GDI+ double-buffered paint logic, and card layouts.
* **`scanner.h` / `scanner.cpp`**: Low-level read loops, device ioctl commands, performance timing queries, multithread state controllers, and localized UTF-8 translation arrays.
* **`resource.h` / `resource.rc`**: Compilation configurations to bind application icons and UAC manifests.
* **`HddScanner.manifest`**: Application manifest requesting execution as administrator (`requireAdministrator`) and enabling flat visual controls (Common Controls v6.0).
* **`build.bat`**: Compilation script to automatically detect and initialize the Visual Studio compiler environment and compile the application.

---

## 🛠️ Build Instructions

### Prerequisites
* A Windows operating system.
* **Microsoft Visual C++ Build Tools** (e.g., installed via Visual Studio 2015, 2017, 2019, or 2022).
* **Workload:** "Desktop development with C++" must be selected in the Visual Studio installer.

### Compiling
1. Open a Command Prompt or PowerShell terminal.
2. Navigate to the project root directory.
3. Run the automated build script:
   ```cmd
   build.bat
   ```
This script will locate your local Visual Studio C++ compiler, compile the resources (`rc.exe`), compile all C++ sources with the static library linker (`/MT`), apply optimizations (`/O2`), specify proper character interpretation (`/utf-8`), and output a single standalone executable `HddScanner.exe`.

---

## 🛡️ Usage & Safety Guidelines

> [!WARNING]
> **Administrator Privileges Required:** Physical drive reading requires raw access rights. If launched without elevation, the operating system will fail to open the device. The application manifest will automatically trigger a User Account Control (UAC) prompt on launch.
> 
> **Read-Only Access:** The scanner uses `GENERIC_READ` attributes and performs **read-only** sequential checks. It does not write to, modify, format, or destroy any data on target physical drives, making it safe to inspect operational system disks.

1. Launch `HddScanner.exe` (approve the UAC elevation prompt).
2. Select your target drive from the dropdown list (shows index, product model, and capacity).
3. Select your desired sequential block size (defaults to **64 KB** which is balanced; smaller block sizes yield finer grid resolution but take longer to scan).
4. Click **Start Scan** to begin. 
5. Use **Pause** / **Resume** or **Stop** at any time.
6. Hover over any block in the grid to inspect the corresponding LBA range.
7. Switch language targets in the bottom-right selector as needed.

---

## 📄 License

This project is open-source and licensed under the **[MIT License](LICENSE)**.
