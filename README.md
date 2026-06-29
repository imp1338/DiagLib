# DiagLib

**DiagLib** is a lightweight Windows diagnostics library for catching crashes, logging errors, and monitoring performance — all with minimal setup.

## Features

- **Crash Handling** – Catches access violations, division by zero, stack overflows, and more.  
- **Detailed Reports** – Logs stack traces, register dumps, and source locations (if debug symbols are available).  
- **Performance Metrics** – Tracks memory usage, handle count, and CPU time.  
- **Simple API** – Just a few functions to initialize, log, and shut down.  
- **Color Console Output** – Uses ANSI colors for readable logs (works in modern Windows terminals).  
- **Static or Dynamic Library** – Build as a `.lib` or `.dll` via CMake.

## Building

### Prerequisites
- Windows 7 or later  
- Visual Studio 2019/2022 (with C++ tools) or any CMake-compatible generator  
- CMake 3.10+

### Steps

```bash
git clone https://github.com/imp1338/diaglib.git
cd diaglib
mkdir build && cd build
cmake ..                     # static library (default)
# or cmake .. -DBUILD_SHARED_LIBS=ON   # for DLL
cmake --build . --config Release
```

The library will be in `build/lib/`, and the example executable in `build/bin/`.

## Usage

Include `DiagLib.h`, link against the library, and call the initialization function at program start.

```cpp
#include "DiagLib.h"

int main() {
    DiagLib_Initialize();   // starts the crash handler and logging

    DiagLib_LogMessage(DIAG_SEVERITY_INFO, "Hello from my app!");

    // Your code here...

    DiagLib_Shutdown();
    return 0;
}
```

### Advanced Setup

```cpp
// Custom log file, enable performance monitoring
DiagLib_InitializeEx("my_log.log", DIAG_LOG_CONSOLE | DIAG_LOG_FILE, TRUE, 5000);

// Register a callback for crashes
void MyCrashHandler(const DIAG_EXCEPTION_INFO* info, void*) {
    printf("Crash: 0x%08X at %p\n", info->code, info->address);
}
DiagLib_RegisterExceptionCallback(MyCrashHandler, DIAG_CALLBACK_ON_EXCEPTION, nullptr);
```

## Example

The `example/` folder contains a full test suite that demonstrates all features. Run it to see DiagLib in action and test crash handling.

## API Reference

| Function | Description |
|----------|-------------|
| `DiagLib_Initialize()` | Initialize with default settings (log to console). |
| `DiagLib_InitializeEx(logPath, targets, enablePerf, interval)` | Full initialization. |
| `DiagLib_Shutdown()` | Clean up resources. |
| `DiagLib_LogMessage(severity, format, ...)` | Log a message. |
| `DiagLib_SetMinSeverity(severity)` | Filter log messages by severity. |
| `DiagLib_RegisterExceptionCallback(callback, types, userData)` | Handle crashes. |
| `DiagLib_GetPerformanceMetrics()` | Get current memory/handle usage. |
| `DiagLib_GenerateCrashDump(path)` | Write a minidump manually. |

## Images

<img width="870" height="822" alt="image" src="https://github.com/user-attachments/assets/088db817-2d53-4979-ba96-12d01b52fbc9" />
<img width="892" height="762" alt="image" src="https://github.com/user-attachments/assets/a67435d0-7fed-4902-b029-a4c861c79a09" />
<img width="928" height="744" alt="image" src="https://github.com/user-attachments/assets/7b722d94-c412-4bef-888b-670e1aacf692" />



