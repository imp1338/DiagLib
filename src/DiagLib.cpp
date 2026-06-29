// ===============================
// DIAGLIB - https://github.com/imp1338/diaglib
// VERSION - 1.0 (INITIAL RELEASE)
// ===============================

#define DIAGLIB_EXPORTS
#include "DiagLib.h"
#include <dbghelp.h>
#include <psapi.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <algorithm>

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BRIGHT_RED "\033[91m"
#define COLOR_BRIGHT_GREEN "\033[92m"
#define COLOR_BRIGHT_YELLOW "\033[93m"
#define COLOR_BRIGHT_BLUE "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN "\033[96m"
#define COLOR_BRIGHT_WHITE "\033[97m"
#define COLOR_BOLD "\033[1m"
#define COLOR_DIM "\033[2m"
#define COLOR_UNDERLINE "\033[4m"
#define COLOR_BLINK "\033[5m"
#define COLOR_REVERSE "\033[7m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN "\033[46m"
#define BG_BRIGHT_RED "\033[101m"
#define BG_BRIGHT_GREEN "\033[102m"
#define BG_BRIGHT_YELLOW "\033[103m"

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

static class DiagnosticsLibrary* g_diagnosticsInstance = nullptr;
static std::mutex g_instanceMutex;

static const DWORD g_noisyExceptions[] = 
{
    0x40010006,  // EXCEPTION_WX86_BREAKPOINT
    0x40010008,  // EXCEPTION_WX86_SINGLE_STEP
    0x406D1388,  // MSC exception
};

static bool IsNoisyException(DWORD code) 
{
    for (int i = 0; i < sizeof(g_noisyExceptions) / sizeof(DWORD); i++) 
    {
        if (code == g_noisyExceptions[i]) 
            return true;
    }
    return false;
}

class DiagnosticsLibrary 
{
private:
    PVOID vectoredHandler;
    HANDLE monitorThread;
    bool running;
    bool initialized;
    std::string logPath;
    int logTargets;
    int minSeverity;
    std::string lastError;
    bool verboseLogging;
    bool hasLoggedIssue;

    struct ExceptionCallback 
    {
        DIAG_EXCEPTION_CALLBACK callback;
        int types;
        void* userData;
    };

    std::vector<ExceptionCallback> exceptionCallbacks;
    std::vector<DIAG_PERFORMANCE_CALLBACK> performanceCallbacks;

    std::mutex callbackMutex;
    std::mutex logMutex;
    FILE* logFile;
    static thread_local bool isHandlingException;

    std::vector<std::string> logBuffer;
    bool logFileOpened;

    static void InvalidParamHandler(const wchar_t* expr, const wchar_t* func, const wchar_t* file, unsigned int line, uintptr_t reserved) 
    {
        RaiseException(STATUS_INVALID_PARAMETER, 0, 0, NULL);
    }

public:
    DiagnosticsLibrary() : vectoredHandler(nullptr), monitorThread(nullptr), running(false), initialized(false), logFile(nullptr), logTargets(DIAG_LOG_CONSOLE), minSeverity(DIAG_SEVERITY_INFO), verboseLogging(true), hasLoggedIssue(false), logFileOpened(false)  { }

    ~DiagnosticsLibrary() 
    {
        Shutdown();
    }

    int Initialize(const char* logFilePath, int targets, BOOL enablePerfMonitor, DWORD perfInterval) 
    {
        if (initialized) 
            return 0;

        EnableVirtualTerminalAndUnicode();
        EnableANSIColors();

        _controlfp_s(NULL, 0, _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_INVALID);

        _set_invalid_parameter_handler(InvalidParamHandler);

        if (logFilePath) {
            logPath = logFilePath;
        }
        else {
            logPath = "dump.log";
        }

        logTargets = targets & ~DIAG_LOG_FILE;
        hasLoggedIssue = false;
        logFileOpened = false;

        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) 
        {
            lastError = "Failed to initialize symbols";
            return -1;
        }

        vectoredHandler = AddVectoredExceptionHandler(1, VectoredHandler);
        if (!vectoredHandler) 
        {
            lastError = "Failed to install VEH";
            return -2;
        }

        if (enablePerfMonitor)
        {
            running = true;
            monitorThread = CreateThread(nullptr, 0, MonitorThreadProc, this, 0, nullptr);
        }

        initialized = true;
        return 0;
    }

    void Shutdown() 
    {
        if (!initialized) 
            return;

        if (hasLoggedIssue && logFile)
        {
            FlushLogBuffer();
            time_t now = time(nullptr);
            fprintf(logFile, "=== DIAGNOSTICS SESSION ENDED: %s\n", ctime(&now));
            fclose(logFile);
            logFile = nullptr;
        }

        running = false;

        if (monitorThread) 
        {
            WaitForSingleObject(monitorThread, 2000);
            CloseHandle(monitorThread);
            monitorThread = nullptr;
        }

        if (vectoredHandler) 
        {
            RemoveVectoredExceptionHandler(vectoredHandler);
            vectoredHandler = nullptr;
        }

        SymCleanup(GetCurrentProcess());

        initialized = false;
    }

    void RegisterExceptionCallback(DIAG_EXCEPTION_CALLBACK callback, int types, void* userData) 
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        exceptionCallbacks.push_back({ callback, types, userData });
    }

    void RegisterPerformanceCallback(DIAG_PERFORMANCE_CALLBACK callback, void* userData)
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        performanceCallbacks.push_back(callback);
    }

    void LogMessage(int severity, const char* format, ...) 
    {
        if (severity < minSeverity) return;

        std::lock_guard<std::mutex> lock(logMutex);

        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);

        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);

        const char* severityStr = GetSeverityString(severity);

        switch (severity)
        {
        case DIAG_SEVERITY_CRITICAL:
            printf("%s[%s%s%s] %s[%sCRITICAL%s]%s %s%s%s\n",
                COLOR_BRIGHT_CYAN,
                COLOR_WHITE, timestamp, COLOR_BRIGHT_CYAN, COLOR_RESET,
                COLOR_BRIGHT_RED, COLOR_RESET, COLOR_BOLD, COLOR_RESET,
                COLOR_WHITE, buffer, COLOR_RESET);
            break;
        case DIAG_SEVERITY_ERROR:
            printf("%s%s[%s%s%s]%s %s%s%s %s%s%s\n",
                COLOR_BRIGHT_RED, COLOR_BOLD,
                COLOR_BRIGHT_WHITE, timestamp, COLOR_RESET, COLOR_BRIGHT_RED,
                COLOR_BRIGHT_RED, "[ERROR]", COLOR_RESET,
                COLOR_BRIGHT_RED, buffer, COLOR_RESET);
            break;
        case DIAG_SEVERITY_WARNING:
            printf("%s%s[%s%s%s]%s %s%s%s %s%s%s\n",
                COLOR_BRIGHT_YELLOW, COLOR_BOLD,
                COLOR_WHITE, timestamp, COLOR_RESET, COLOR_BRIGHT_YELLOW,
                COLOR_BRIGHT_YELLOW, "[WARN]", COLOR_RESET,
                COLOR_BRIGHT_YELLOW, buffer, COLOR_RESET);
            break;
        default:
            printf("%s[%s%s%s] %s[%sINFO%s]%s %s%s%s\n",
                COLOR_BRIGHT_CYAN,
                COLOR_WHITE, timestamp, COLOR_BRIGHT_CYAN, COLOR_RESET,
                COLOR_BRIGHT_GREEN, COLOR_RESET, COLOR_BOLD, COLOR_RESET,
                COLOR_WHITE, buffer, COLOR_RESET);
            break;
        }


        if (hasLoggedIssue) 
        {
            if (logFile) {
                fprintf(logFile, "[%s] [%s] %s\n", timestamp, severityStr, buffer);
                fflush(logFile);
            }
        }
        else if (severity >= DIAG_SEVERITY_WARNING)
        {
            OpenLogFile();
            hasLoggedIssue = true;
            FlushLogBuffer();
            if (logFile) {
                fprintf(logFile, "[%s] [%s] %s\n", timestamp, severityStr, buffer);
                fflush(logFile);
            }
        }
        else
        {
            if (logBuffer.size() < 100) {
                std::string entry = std::string("[") + timestamp + "] [" + severityStr + "] " + buffer;
                logBuffer.push_back(entry);
            }
        }

        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
    }

    const char* GetLastError() 
    {
        return lastError.c_str();
    }

    void SetMinSeverity(int severity) 
    {
        minSeverity = severity;
    }

    void EnableVerboseLogging(BOOL enable) 
    {
        verboseLogging = enable;
    }

    DIAG_PERFORMANCE_INFO GetPerformanceMetrics() 
    {
        DIAG_PERFORMANCE_INFO metrics = { 0 };

        HANDLE process = GetCurrentProcess();
        PROCESS_MEMORY_COUNTERS_EX pmc = { 0 };
        pmc.cb = sizeof(pmc);

        if (GetProcessMemoryInfo(process, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            metrics.memoryUsageMB = pmc.WorkingSetSize / (1024 * 1024);
            metrics.peakMemoryMB = pmc.PeakWorkingSetSize / (1024 * 1024);
        }

        GetProcessHandleCount(process, &metrics.handleCount);

        return metrics;
    }

private:
    static bool EnableVirtualTerminalAndUnicode() 
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;

        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            dwMode |= DISABLE_NEWLINE_AUTO_RETURN;

            if (SetConsoleMode(hOut, dwMode)) {
                SetConsoleOutputCP(CP_UTF8);
                SetConsoleCP(CP_UTF8);
                return true;
            }
        }

        return false;
    }

    static bool EnableANSIColors() 
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;

        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            return SetConsoleMode(hOut, dwMode);
        }

        return false;
    }

    void OpenLogFile() 
    {
        if (logFileOpened) return;

        size_t lastSlash = logPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            std::string dir = logPath.substr(0, lastSlash);
            CreateDirectoryA(dir.c_str(), NULL);
        }

        fopen_s(&logFile, logPath.c_str(), "a");
        if (logFile) {
            logFileOpened = true;
            time_t now = time(nullptr);
            fprintf(logFile, "\n\n");
            fprintf(logFile, "========================================\n");
            fprintf(logFile, "    DIAGNOSTICS LIBRARY - CRASH REPORT\n");
            fprintf(logFile, "========================================\n");
            fprintf(logFile, "Session Started: %s", ctime(&now));
            fprintf(logFile, "Process ID: %d\n", GetCurrentProcessId());
            fprintf(logFile, "========================================\n\n");
            fflush(logFile);
        }
    }

    void FlushLogBuffer() 
    {
        if (!logFile) return;

        if (!logBuffer.empty()) {
            fprintf(logFile, "----------------------------------------\n");
            fprintf(logFile, "         PRE-CRASH ACTIVITY LOG\n");
            fprintf(logFile, "----------------------------------------\n\n");
            for (const auto& entry : logBuffer) {
                fprintf(logFile, "%s\n", entry.c_str());
            }
            fprintf(logFile, "\n========================================\n\n");
            logBuffer.clear();
        }
    }

    static LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS exceptionInfo) 
    {
        if (isHandlingException) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

        if (IsNoisyException(code)) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP) {
            if (!IsDebuggerPresent()) {
                return EXCEPTION_CONTINUE_SEARCH;
            }
        }

        if (g_diagnosticsInstance && g_diagnosticsInstance->initialized) {
            isHandlingException = true;
            g_diagnosticsInstance->HandleException(exceptionInfo);
            isHandlingException = false;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    void HandleException(PEXCEPTION_POINTERS exceptionInfo)
    {
        DIAG_EXCEPTION_INFO info = { 0 };

        info.code = exceptionInfo->ExceptionRecord->ExceptionCode;
        info.address = exceptionInfo->ExceptionRecord->ExceptionAddress;
        info.threadId = GetCurrentThreadId();
        info.processId = GetCurrentProcessId();
        info.severity = GetSeverityForException(info.code);

        ResolveSymbol((DWORD64)info.address, info.symbolName, sizeof(info.symbolName), info.moduleName, sizeof(info.moduleName), &info.offset, info.fileName, sizeof(info.fileName), &info.lineNumber);
        FormatRegistersFull(exceptionInfo->ContextRecord, info.registerInfo, sizeof(info.registerInfo));
        FormatStackTraceFull(exceptionInfo->ContextRecord, info.stackTrace, sizeof(info.stackTrace));

        char logBuffer[2048];

        LogMessage(DIAG_SEVERITY_CRITICAL, "========================================");
        LogMessage(DIAG_SEVERITY_CRITICAL, "         !!! EXCEPTION CAUGHT !!!");
        LogMessage(DIAG_SEVERITY_CRITICAL, "========================================");

        snprintf(logBuffer, sizeof(logBuffer), "Exception Code: 0x%08X (%s)", info.code, GetExceptionName(info.code));
        LogMessage(DIAG_SEVERITY_CRITICAL, logBuffer);

        snprintf(logBuffer, sizeof(logBuffer), "Exception Address: %p", info.address);
        LogMessage(DIAG_SEVERITY_CRITICAL, logBuffer);

        snprintf(logBuffer, sizeof(logBuffer), "In Module: %s", info.moduleName);
        LogMessage(DIAG_SEVERITY_CRITICAL, logBuffer);

        snprintf(logBuffer, sizeof(logBuffer), "At Function: %s+0x%llX", info.symbolName, info.offset);
        LogMessage(DIAG_SEVERITY_CRITICAL, logBuffer);

        Sleep(3000);
        printf("\n\n");
        if (info.fileName[0]) {
            snprintf(logBuffer, sizeof(logBuffer), "Source Location: %s line %d", info.fileName, info.lineNumber);
            LogMessage(DIAG_SEVERITY_INFO, logBuffer);
        }

        snprintf(logBuffer, sizeof(logBuffer), "Thread ID: %d | Process ID: %d", info.threadId, info.processId);
        LogMessage(DIAG_SEVERITY_INFO, logBuffer);

        LogMessage(DIAG_SEVERITY_INFO, "----------------------------------------");
        LogMessage(DIAG_SEVERITY_INFO, "REGISTER DUMP");
        LogMessage(DIAG_SEVERITY_INFO, "----------------------------------------");

        char* line = strtok(info.registerInfo, "\n");
        while (line) {
            LogMessage(DIAG_SEVERITY_INFO, line);
            line = strtok(nullptr, "\n");
        }

        LogMessage(DIAG_SEVERITY_INFO, "----------------------------------------");
        LogMessage(DIAG_SEVERITY_INFO, "STACK TRACE");
        LogMessage(DIAG_SEVERITY_INFO, "----------------------------------------");

        line = strtok(info.stackTrace, "\n");
        while (line) {
            LogMessage(DIAG_SEVERITY_INFO, line);
            line = strtok(nullptr, "\n");
        }

        LogMessage(DIAG_SEVERITY_INFO, "========================================");

        std::lock_guard<std::mutex> lock(callbackMutex);
        for (auto& entry : exceptionCallbacks) {
            if (entry.types & DIAG_CALLBACK_ON_EXCEPTION) {
                entry.callback(&info, entry.userData);
            }
        }
    }

    const char* GetExceptionName(DWORD code)
    {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS VIOLATION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INTEGER DIVIDE BY ZERO";
        case EXCEPTION_STACK_OVERFLOW: return "STACK OVERFLOW";
        case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
        case EXCEPTION_SINGLE_STEP: return "SINGLE STEP";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "IN PAGE ERROR";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY BOUNDS EXCEEDED";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLOAT DENORMAL OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLOAT DIVIDE BY ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT: return "FLOAT INEXACT RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION: return "FLOAT INVALID OPERATION";
        case EXCEPTION_FLT_OVERFLOW: return "FLOAT OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK: return "FLOAT STACK CHECK";
        case EXCEPTION_FLT_UNDERFLOW: return "FLOAT UNDERFLOW";
        default: return "UNKNOWN EXCEPTION";
        }
    }

    static DWORD WINAPI MonitorThreadProc(LPVOID param)
    {
        DiagnosticsLibrary* self = (DiagnosticsLibrary*)param;

        while (self->running) {
            DIAG_PERFORMANCE_INFO metrics = self->GetPerformanceMetrics();

            if (metrics.memoryUsageMB > 500) {
                self->LogMessage(DIAG_SEVERITY_WARNING,
                    "High memory usage : % llu MB", metrics.memoryUsageMB);
            }

            if (metrics.handleCount > 1000) {
                self->LogMessage(DIAG_SEVERITY_WARNING,
                    "High handle count: %d", metrics.handleCount);
            }

            std::lock_guard<std::mutex> lock(self->callbackMutex);
            for (auto& callback : self->performanceCallbacks) {
                callback(&metrics, nullptr);
            }

            Sleep(10000);
        }
        return 0;
    }

    const char* GetSeverityString(int severity)
    {
        switch (severity) {
        case DIAG_SEVERITY_INFO: return "INFO";
        case DIAG_SEVERITY_WARNING: return "WARN";
        case DIAG_SEVERITY_ERROR: return "ERROR";
        case DIAG_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
        }
    }

    WORD GetColorForSeverity(int severity) 
    {
        switch (severity) {
        case DIAG_SEVERITY_CRITICAL: return 12;
        case DIAG_SEVERITY_ERROR: return 12;
        case DIAG_SEVERITY_WARNING: return 14;
        default: return 7;
        }
    }

    int GetSeverityForException(DWORD code)
    {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_STACK_OVERFLOW:
            return DIAG_SEVERITY_CRITICAL;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return DIAG_SEVERITY_ERROR;
        default:
            return DIAG_SEVERITY_INFO;
        }
    }

    void ResolveSymbol(DWORD64 address, char* symbolName, DWORD symbolSize, char* moduleName, DWORD moduleSize, DWORD64* offset, char* fileName, DWORD fileSize, DWORD* lineNumber)
    {
        HANDLE process = GetCurrentProcess();

        char buffer[sizeof(SYMBOL_INFO) + 256] = { 0 };
        PSYMBOL_INFO sym = (PSYMBOL_INFO)buffer;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;

        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, sym)) {
            strncpy_s(symbolName, symbolSize, sym->Name, _TRUNCATE);
            *offset = displacement;
        }
        else {
            strcpy_s(symbolName, symbolSize, "<unknown>");
        }

        IMAGEHLP_MODULE64 moduleInfo = { 0 };
        moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
        if (SymGetModuleInfo64(process, address, &moduleInfo)) {
            strncpy_s(moduleName, moduleSize, moduleInfo.ModuleName, _TRUNCATE);
        }
        else {
            strcpy_s(moduleName, moduleSize, "<unknown>");
        }

        IMAGEHLP_LINE64 lineInfo = { 0 };
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo)) {
            strncpy_s(fileName, fileSize, lineInfo.FileName, _TRUNCATE);
            *lineNumber = lineInfo.LineNumber;
        }
        else {
            fileName[0] = 0;
            *lineNumber = 0;
        }
    }

    void FormatRegistersFull(CONTEXT* ctx, char* buffer, DWORD bufferSize) 
    {
#ifdef _M_X64
        snprintf(buffer, bufferSize,
            "============================================================\n"
            "x64 Register Dump\n"
            "============================================================\n"
            "RAX = %-16p  RBX = %-16p\n"
            "RCX = %-16p  RDX = %-16p\n"
            "RSP = %-16p  RBP = %-16p\n"
            "RSI = %-16p  RDI = %-16p\n"
            "R8  = %-16p  R9  = %-16p\n"
            "R10 = %-16p  R11 = %-16p\n"
            "R12 = %-16p  R13 = %-16p\n"
            "R14 = %-16p  R15 = %-16p\n"
            "RIP = %-16p\n"
            "------------------------------------------------------------\n"
            "Segment Registers:\n"
            "CS = 0x%04X  DS = 0x%04X  ES = 0x%04X  FS = 0x%04X  GS = 0x%04X  SS = 0x%04X\n"
            "Flags: 0x%08X\n"
            "============================================================",
            (PVOID)ctx->Rax, (PVOID)ctx->Rbx,
            (PVOID)ctx->Rcx, (PVOID)ctx->Rdx,
            (PVOID)ctx->Rsp, (PVOID)ctx->Rbp,
            (PVOID)ctx->Rsi, (PVOID)ctx->Rdi,
            (PVOID)ctx->R8, (PVOID)ctx->R9,
            (PVOID)ctx->R10, (PVOID)ctx->R11,
            (PVOID)ctx->R12, (PVOID)ctx->R13,
            (PVOID)ctx->R14, (PVOID)ctx->R15,
            (PVOID)ctx->Rip, ctx->SegCs, ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs, ctx->SegSs, ctx->EFlags);
#else
        snprintf(buffer, bufferSize,
            "============================================================\n"
            "x86 Register Dump\n"
            "============================================================\n"
            "EAX = %-8p  EBX = %-8p  ECX = %-8p  EDX = %-8p\n"
            "ESP = %-8p  EBP = %-8p  ESI = %-8p  EDI = %-8p\n"
            "EIP = %-8p\n"
            "------------------------------------------------------------\n"
            "Segment Registers:\n"
            "CS = 0x%04X  DS = 0x%04X  ES = 0x%04X  FS = 0x%04X  GS = 0x%04X  SS = 0x%04X\n"
            "Flags: 0x%08X\n"
            "============================================================",
            (PVOID)ctx->Eax, (PVOID)ctx->Ebx, (PVOID)ctx->Ecx, (PVOID)ctx->Edx,
            (PVOID)ctx->Esp, (PVOID)ctx->Ebp, (PVOID)ctx->Esi, (PVOID)ctx->Edi,
            (PVOID)ctx->Eip, ctx->SegCs, ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs, ctx->SegSs, ctx->EFlags);
#endif
    }

    void FormatStackTraceFull(CONTEXT* ctx, char* buffer, DWORD bufferSize) 
    {
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        STACKFRAME64 frame = { 0 };
        DWORD machine;

#ifdef _M_X64
        frame.AddrPC.Offset = ctx->Rip;
        frame.AddrStack.Offset = ctx->Rsp;
        frame.AddrFrame.Offset = ctx->Rbp;
        machine = IMAGE_FILE_MACHINE_AMD64;
#else
        frame.AddrPC.Offset = ctx->Eip;
        frame.AddrStack.Offset = ctx->Esp;
        frame.AddrFrame.Offset = ctx->Ebp;
        machine = IMAGE_FILE_MACHINE_I386;
#endif

        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;

        char* pos = buffer;
        size_t remaining = bufferSize;
        int frameNum = 0;

        int written = snprintf(pos, remaining,
            "============================================================\n"
            "Stack Trace (max 32 frames)\n"
            "============================================================\n");
        if (written > 0) {
            pos += written;
            remaining -= written;
        }

        while (remaining > 0 && frameNum < 32) {
            if (!StackWalk64(machine, process, thread, &frame, ctx, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
                break;
            }

            if (frame.AddrPC.Offset == 0) break;

            char symbol[512] = "<unknown>";
            char module[256] = "<unknown>";
            DWORD64 offset = 0;
            char fileName[MAX_PATH] = "";
            DWORD lineNumber = 0;

            char symBuffer[sizeof(SYMBOL_INFO) + 256] = { 0 };
            PSYMBOL_INFO sym = (PSYMBOL_INFO)symBuffer;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = 255;

            DWORD64 displacement = 0;
            if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, sym)) {
                strncpy_s(symbol, sizeof(symbol), sym->Name, _TRUNCATE);
                offset = displacement;
            }

            IMAGEHLP_MODULE64 moduleInfo = { 0 };
            moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
            if (SymGetModuleInfo64(process, frame.AddrPC.Offset, &moduleInfo)) {
                strncpy_s(module, sizeof(module), moduleInfo.ModuleName, _TRUNCATE);
            }

            IMAGEHLP_LINE64 lineInfo = { 0 };
            lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &lineInfo)) {
                strncpy_s(fileName, sizeof(fileName), lineInfo.FileName, _TRUNCATE);
                lineNumber = lineInfo.LineNumber;
            }

            if (fileName[0] != 0) {
                written = snprintf(pos, remaining, "[%2d] 0x%016llX  %s!%s + 0x%llX\n" "      >> %s:%d\n", frameNum, frame.AddrPC.Offset, module, symbol, offset, fileName, lineNumber);
            }
            else {
                written = snprintf(pos, remaining, "[%2d] 0x%016llX  %s!%s + 0x%llX\n", frameNum, frame.AddrPC.Offset, module, symbol, offset);
            }

            if (written > 0) {
                pos += written;
                remaining -= written;
            }

            frameNum++;
        }

        if (frameNum == 0) {
            snprintf(buffer, bufferSize, "  No stack frames available\n");
        }
        else {
            snprintf(pos, remaining, "============================================================\n");
            snprintf(pos + strlen(pos), remaining - strlen(pos), "Total frames: %d\n", frameNum);
            snprintf(pos + strlen(pos), remaining - strlen(pos), "Want to use DIAGLIB for yourself ?, find it at -> https://github.com/imp1338/diaglib");
        }
    }
};

thread_local bool DiagnosticsLibrary::isHandlingException = false;

DIAGLIB_API int DiagLib_Initialize(void) 
{
    return DiagLib_InitializeEx(nullptr, DIAG_LOG_CONSOLE, TRUE, 10000);
}

DIAGLIB_API int DiagLib_InitializeEx(const char* logFilePath, int logTargets, BOOL enablePerformanceMonitor, DWORD performanceIntervalMs) 
{
    std::lock_guard<std::mutex> lock(g_instanceMutex);

    if (!g_diagnosticsInstance)
        g_diagnosticsInstance = new DiagnosticsLibrary();

    return g_diagnosticsInstance->Initialize(logFilePath, logTargets, enablePerformanceMonitor, performanceIntervalMs);
}

DIAGLIB_API void DiagLib_Shutdown(void) 
{
    std::lock_guard<std::mutex> lock(g_instanceMutex);

    if (g_diagnosticsInstance) {
        g_diagnosticsInstance->Shutdown();
        delete g_diagnosticsInstance;
        g_diagnosticsInstance = nullptr;
    }
}

DIAGLIB_API void DiagLib_RegisterExceptionCallback(DIAG_EXCEPTION_CALLBACK callback, int types, void* userData) 
{
    if (g_diagnosticsInstance)
        g_diagnosticsInstance->RegisterExceptionCallback(callback, types, userData);
}

DIAGLIB_API void DiagLib_RegisterPerformanceCallback(DIAG_PERFORMANCE_CALLBACK callback, void* userData) 
{
    if (g_diagnosticsInstance)
        g_diagnosticsInstance->RegisterPerformanceCallback(callback, userData);
}

DIAGLIB_API void DiagLib_LogMessage(int severity, const char* format, ...) 
{
    if (!g_diagnosticsInstance) 
        return;

    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    g_diagnosticsInstance->LogMessage(severity, buffer);
}

DIAGLIB_API DIAG_PERFORMANCE_INFO DiagLib_GetPerformanceMetrics(void) 
{
    if (g_diagnosticsInstance)
        return g_diagnosticsInstance->GetPerformanceMetrics();

    DIAG_PERFORMANCE_INFO metrics = { 0 };
    return metrics;
}

DIAGLIB_API const char* DiagLib_GetLastError(void) 
{
    if (g_diagnosticsInstance)
        return g_diagnosticsInstance->GetLastError();

    return "Library not initialized";
}

DIAGLIB_API void DiagLib_SetMinSeverity(int severity) 
{
    if (g_diagnosticsInstance)
        g_diagnosticsInstance->SetMinSeverity(severity);
}

DIAGLIB_API BOOL DiagLib_GenerateCrashDump(const char* dumpPath) 
{
    HANDLE hFile = CreateFileA(dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) 
        return FALSE;

    MINIDUMP_EXCEPTION_INFORMATION mei = { 0 };
    mei.ThreadId = GetCurrentThreadId();
    mei.ClientPointers = FALSE;

    BOOL result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mei, NULL, NULL);

    CloseHandle(hFile);
    return result;
}

DIAGLIB_API void DiagLib_EnableVerboseLogging(BOOL enable) 
{
    if (g_diagnosticsInstance)
        g_diagnosticsInstance->EnableVerboseLogging(enable);
}