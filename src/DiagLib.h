// ===============================
// DIAGLIB - https://github.com/imp1338/diaglib
// VERSION - 1.0 (INITIAL RELEASE)
// ===============================

#pragma once

#ifdef DIAGLIB_EXPORTS
#define DIAGLIB_API __declspec(dllexport)
#else
#define DIAGLIB_API __declspec(dllimport)
#endif

#include <windows.h>
#include <stdint.h>

enum DIAG_LOG_TARGET {
    DIAG_LOG_CONSOLE = 0x01,
    DIAG_LOG_FILE = 0x02,
    DIAG_LOG_DEBUGGER = 0x04,
    DIAG_LOG_ALL = 0xFF
};

enum DIAG_CALLBACK_TYPE {
    DIAG_CALLBACK_ON_EXCEPTION = 0x01,
    DIAG_CALLBACK_ON_THREAD = 0x02,
    DIAG_CALLBACK_ON_PERFORMANCE = 0x04,
    DIAG_CALLBACK_ON_MEMORY_LEAK = 0x08,
    DIAG_CALLBACK_ALL = 0xFF
};

enum DIAG_SEVERITY {
    DIAG_SEVERITY_INFO = 0,
    DIAG_SEVERITY_WARNING = 1,
    DIAG_SEVERITY_ERROR = 2,
    DIAG_SEVERITY_CRITICAL = 3
};

struct DIAG_EXCEPTION_INFO {
    DWORD code;
    PVOID address;
    DWORD threadId;
    DWORD processId;
    char moduleName[256];
    char symbolName[512];
    DWORD64 offset;
    char fileName[256];
    DWORD lineNumber;
    char registerInfo[1024];
    char stackTrace[4096];
    int severity;
};

struct DIAG_PERFORMANCE_INFO {
    DWORD64 memoryUsageMB;
    DWORD64 peakMemoryMB;
    DWORD handleCount;
    DWORD threadCount;
    DWORD cpuUsagePercent;
    DWORD uptimeSeconds;
};

typedef void (*DIAG_EXCEPTION_CALLBACK)(const DIAG_EXCEPTION_INFO* info, void* userData);
typedef void (*DIAG_PERFORMANCE_CALLBACK)(const DIAG_PERFORMANCE_INFO* info, void* userData);

#ifdef __cplusplus
extern "C" {
#endif

    DIAGLIB_API int DiagLib_Initialize(void);
    DIAGLIB_API int DiagLib_InitializeEx(const char* logFilePath, int logTargets,
        BOOL enablePerformanceMonitor, DWORD performanceIntervalMs);
    DIAGLIB_API void DiagLib_Shutdown(void);
    DIAGLIB_API void DiagLib_RegisterExceptionCallback(DIAG_EXCEPTION_CALLBACK callback,
        int types, void* userData);
    DIAGLIB_API void DiagLib_RegisterPerformanceCallback(DIAG_PERFORMANCE_CALLBACK callback, void* userData);
    DIAGLIB_API void DiagLib_LogMessage(int severity, const char* format, ...);
    DIAGLIB_API DIAG_PERFORMANCE_INFO DiagLib_GetPerformanceMetrics(void);
    DIAGLIB_API const char* DiagLib_GetLastError(void);
    DIAGLIB_API void DiagLib_SetMinSeverity(int severity);
    DIAGLIB_API BOOL DiagLib_GenerateCrashDump(const char* dumpPath);
    DIAGLIB_API void DiagLib_EnableVerboseLogging(BOOL enable);

#ifdef __cplusplus
}
#endif