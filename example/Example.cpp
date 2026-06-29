#include "diaglib\diaglib.h"
#include <float.h>
#include <stdio.h>

void TestAccessViolation() 
{
    volatile int* p = nullptr;
    *p = 1337;
}

void TestDoubleFree()
 {
    int* ptr = (int*)malloc(sizeof(int) * 10);
    if (ptr) {
        free(ptr);
        free(ptr);
    }
}

void TestHeapCorruption() 
{
    int* ptr = (int*)malloc(sizeof(int) * 10);
    if (ptr) {
        for (int i = 0; i < 100; i++) {
            ptr[i] = i;  // Write past allocated memory
        }
        free(ptr);
    }
}

void TestStackBufferOverflow()
 {
    char buffer[10];
    for (int i = 0; i < 100; i++) {
        buffer[i] = 'A';
    }
}

void TestUseAfterFree() 
{
    struct TestStruct {
        int value;
        void DoSomething() { value = 42; }
    };

    TestStruct* obj = new TestStruct();
    delete obj;
    obj->DoSomething();
}

void TestDivByZero() 
{
    volatile int a = 5;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

void TestFloatDivByZero()
 {
    _controlfp_s(NULL, 0, _EM_ZERODIVIDE);

    float a = 1.0f;
    float b = 0.0f;
    volatile float c = a / b;
    (void)c;
}

void TestFloatOverflow() 
{
    _controlfp_s(NULL, 0, _EM_OVERFLOW);

    float a = 1e38f;
    float b = 1e38f;
    volatile float c = a * b;  // Overflow
    (void)c;
}

void TestIntegerOverflow() 
{
    RaiseException(STATUS_INTEGER_OVERFLOW, 0, 0, NULL);
}

void TestStackOverflow(int depth)
 {
    volatile char buffer[4096];
    buffer[0] = (char)depth;
    printf("Depth: %d\n", depth);
    TestStackOverflow(depth + 1);
}

class PureVirtualBase {
public:
    virtual void PureFunc() = 0;
};

class PureVirtualDerived : public PureVirtualBase {
public:
    virtual void PureFunc() override {}
};

void TestPureVirtualCall() 
{
    PureVirtualBase* obj = new PureVirtualDerived();
    delete obj;
    obj->PureFunc();
}

void TestInvalidHandle() 
{
    HANDLE hFile = (HANDLE)0xDEADBEEF;
    DWORD bytesRead;
    char buffer[100];
    ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL);
}

void InvalidParamHandler(const wchar_t* expr, const wchar_t* func, const wchar_t* file, unsigned int line, uintptr_t reserved) 
{
    RaiseException(STATUS_INVALID_PARAMETER, 0, 0, NULL);
}

void TestInvalidParameter() 
{
    _set_invalid_parameter_handler(InvalidParamHandler);
    printf("%s", NULL);
}

void TestIllegalInstruction() 
{
    RaiseException(STATUS_ILLEGAL_INSTRUCTION, 0, 0, NULL);
}

void TestArrayBounds() 
{
    RaiseException(STATUS_ARRAY_BOUNDS_EXCEEDED, 0, 0, NULL);
}

void TestInPageError()
 {
    RaiseException(STATUS_IN_PAGE_ERROR, 0, 0, NULL);
}

const char* COLOR_RESET = "\033[0m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_BLUE = "\033[34m";
const char* COLOR_MAGENTA = "\033[35m";
const char* COLOR_CYAN = "\033[36m";
const char* COLOR_WHITE = "\033[37m";
const char* COLOR_BRIGHT_RED = "\033[91m";
const char* COLOR_BRIGHT_GREEN = "\033[92m";
const char* COLOR_BRIGHT_YELLOW = "\033[93m";
const char* COLOR_BRIGHT_BLUE = "\033[94m";
const char* COLOR_BRIGHT_MAGENTA = "\033[95m";
const char* COLOR_BRIGHT_CYAN = "\033[96m";
const char* COLOR_BRIGHT_WHITE = "\033[97m";
const char* COLOR_BOLD = "\033[1m";
const char* COLOR_DIM = "\033[2m";
const char* COLOR_UNDERLINE = "\033[4m";
const char* COLOR_BLINK = "\033[5m";
const char* COLOR_REVERSE = "\033[7m";

const char* BG_RED = "\033[41m";
const char* BG_GREEN = "\033[42m";
const char* BG_YELLOW = "\033[43m";
const char* BG_BLUE = "\033[44m";
const char* BG_MAGENTA = "\033[45m";
const char* BG_CYAN = "\033[46m";
const char* BG_BRIGHT_RED = "\033[101m";
const char* BG_BRIGHT_GREEN = "\033[102m";
const char* BG_BRIGHT_YELLOW = "\033[103m";

void ShowTestMenu() 
{
    printf("\n%s%s", COLOR_BRIGHT_CYAN, COLOR_BOLD);
    printf("========================================\n");
    printf("     (DiagLib Test Menu)\n");
    printf("========================================\n");
    printf("%s%s", COLOR_RESET, COLOR_RESET);

    printf("\n%s--- MEMORY CORRUPTION ---%s\n", COLOR_BRIGHT_YELLOW, COLOR_RESET);
    printf("%s 1.%s Access Violation (null pointer)\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 2.%s Double Free (heap corruption)\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 3.%s Heap Corruption (buffer overrun)\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 4.%s Stack Buffer Overflow (/GS violation)\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 5.%s Use After Free\n", COLOR_BRIGHT_GREEN, COLOR_RESET);

    printf("\n%s--- ARITHMETIC ERRORS ---%s\n", COLOR_BRIGHT_YELLOW, COLOR_RESET);
    printf("%s 6.%s Integer Division by Zero\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 7.%s Float Division by Zero\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 8.%s Float Overflow\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s 9.%s Integer Overflow\n", COLOR_BRIGHT_GREEN, COLOR_RESET);

    printf("\n%s--- STACK CORRUPTION ---%s\n", COLOR_BRIGHT_YELLOW, COLOR_RESET);
    printf("%s10.%s Stack Overflow (recursion)\n", COLOR_BRIGHT_GREEN, COLOR_RESET);

    printf("\n%s--- C++ SPECIFIC ---%s\n", COLOR_BRIGHT_YELLOW, COLOR_RESET);
    printf("%s11.%s Pure Virtual Function Call\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s12.%s Invalid Handle Operation\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s13.%s Invalid Parameter\n", COLOR_BRIGHT_GREEN, COLOR_RESET);

    printf("\n%s--- RAISED EXCEPTIONS ---%s\n", COLOR_BRIGHT_YELLOW, COLOR_RESET);
    printf("%s14.%s Illegal Instruction\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s15.%s Array Bounds Exception\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("%s16.%s In-Page I/O Error\n", COLOR_BRIGHT_GREEN, COLOR_RESET);

    printf("\n%s 0.%s Exit\n", COLOR_BRIGHT_RED, COLOR_RESET);

    printf("\n%sChoice: %s", COLOR_BRIGHT_GREEN, COLOR_RESET);
}

int main() 
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }

    _controlfp_s(NULL, _EM_ZERODIVIDE | _EM_OVERFLOW, _MCW_EM);

    printf("%s%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD);
    printf("========================================\n");
    printf("  DiagLib Example Program\n");
    printf("========================================\n");
    printf("%s%s\n", COLOR_RESET, COLOR_RESET);

    if (DiagLib_Initialize() != 0) {
        printf("Failed to initialize diagnostics!\n");
        return 1;
    }

    int choice = -1;
    while (choice != 0) 
    {
        ShowTestMenu();
        scanf_s("%d", &choice);

        if (choice >= 1 && choice <= 16) {
            printf("\n%s>>> TEST %d - CRASHING...%s\n", COLOR_BRIGHT_RED, choice, COLOR_RESET);
            Sleep(500);

            switch (choice) {
            case 1:  TestAccessViolation(); break;
            case 2:  TestDoubleFree(); break;
            case 3:  TestHeapCorruption(); break;
            case 4:  TestStackBufferOverflow(); break;
            case 5:  TestUseAfterFree(); break;
            case 6:  TestDivByZero(); break;
            case 7:  TestFloatDivByZero(); break;
            case 8:  TestFloatOverflow(); break;
            case 9:  TestIntegerOverflow(); break;
            case 10: TestStackOverflow(0); break;
            case 11: TestPureVirtualCall(); break;
            case 12: TestInvalidHandle(); break;
            case 13: TestInvalidParameter(); break;
            case 14: TestIllegalInstruction(); break;
            case 15: TestArrayBounds(); break;
            case 16: TestInPageError(); break;
            }

            printf("Test completed without crash?\n");
            Sleep(2000);
        }
    }

    DiagLib_Shutdown();
    return 0;
}