#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <windows.h>
#include "yuzu/mini_dump.h"
#include "yuzu/startup_checks.h"

// dbghelp.h must be included after windows.h
#include <dbghelp.h>

void CreateMiniDump(HANDLE process_handle, DWORD process_id, MINIDUMP_EXCEPTION_INFORMATION* info,
                    EXCEPTION_POINTERS* pep) {
    char file_name[255];
    const std::time_t the_time = std::time(nullptr);
    std::strftime(file_name, 255, "yuzu-crash-%Y%m%d%H%M%S.dmp", std::localtime(&the_time));

    // Open the file
    HANDLE file_handle = CreateFile(file_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if ((file_handle != nullptr) && (file_handle != INVALID_HANDLE_VALUE)) {
        // Create the minidump
        const MINIDUMP_TYPE dump_type = MiniDumpNormal;

        const bool write_dump_status = MiniDumpWriteDump(process_handle, process_id, file_handle,
                                                         dump_type, (pep != 0) ? info : 0, 0, 0);

        if (!write_dump_status) {
            std::fprintf(stderr, "MiniDumpWriteDump failed. Error: %d\n", GetLastError());
        } else {
            std::fprintf(stderr, "MiniDump created: %s\n", file_name);
        }

        // Close the file
        CloseHandle(file_handle);

    } else {
        std::fprintf(stderr, "CreateFile failed. Error: %d\n", GetLastError());
    }
}

void DumpFromDebugEvent(DEBUG_EVENT& deb_ev, PROCESS_INFORMATION& pi) {
    EXCEPTION_RECORD& record = deb_ev.u.Exception.ExceptionRecord;

    HANDLE thread_handle = OpenThread(THREAD_GET_CONTEXT, false, deb_ev.dwThreadId);
    if (thread_handle == nullptr) {
        std::fprintf(stderr, "OpenThread failed (%d)\n", GetLastError());
    }

    // Get child process context
    CONTEXT context;
    std::memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(thread_handle, &context)) {
        std::fprintf(stderr, "GetThreadContext failed (%d)\n", GetLastError());
        return;
    }

    // Create exception pointers for minidump
    EXCEPTION_POINTERS ep;
    ep.ExceptionRecord = &record;
    ep.ContextRecord = &context;

    MINIDUMP_EXCEPTION_INFORMATION info;
    info.ThreadId = deb_ev.dwThreadId;
    info.ExceptionPointers = &ep;
    info.ClientPointers = false;

    CreateMiniDump(pi.hProcess, pi.dwProcessId, &info, &ep);

    if (CloseHandle(thread_handle) == 0) {
        std::fprintf(stderr, "error: CloseHandle(thread_handle) failed (%d)\n", GetLastError());
    }
}

bool SpawnDebuggee(const char* arg0, PROCESS_INFORMATION& pi) {
    std::memset(&pi, 0, sizeof(pi));

    if (!SpawnChild(arg0, &pi, 0)) {
        std::fprintf(stderr, "warning: continuing without crash dumps\n");
        return false;
    }

    // Don't debug if we are already being debugged
    if (IsDebuggerPresent()) {
        return false;
    }

    const bool can_debug = DebugActiveProcess(pi.dwProcessId);
    if (!can_debug) {
        std::fprintf(stderr,
                     "warning: DebugActiveProcess failed (%d), continuing without crash dumps\n",
                     GetLastError());
        return false;
    }

    return true;
}

void DebugDebuggee(PROCESS_INFORMATION& pi) {
    DEBUG_EVENT deb_ev;
    const std::time_t start_time = std::time(nullptr);
    //~ bool seen_nonzero_thread_exit = false;

    while (deb_ev.dwDebugEventCode != EXIT_PROCESS_DEBUG_EVENT) {
        const bool wait_success = WaitForDebugEvent(&deb_ev, INFINITE);
        if (!wait_success) {
            std::fprintf(stderr, "error: WaitForDebugEvent failed (%d)\n", GetLastError());
            return;
        }

        switch (deb_ev.dwDebugEventCode) {
        case OUTPUT_DEBUG_STRING_EVENT:
        case CREATE_PROCESS_DEBUG_EVENT:
        case CREATE_THREAD_DEBUG_EVENT:
        case EXIT_PROCESS_DEBUG_EVENT:
        case EXIT_THREAD_DEBUG_EVENT:
        case LOAD_DLL_DEBUG_EVENT:
        case RIP_EVENT:
        case UNLOAD_DLL_DEBUG_EVENT:
            ContinueDebugEvent(deb_ev.dwProcessId, deb_ev.dwThreadId, DBG_CONTINUE);
            break;
            //~ case EXIT_THREAD_DEBUG_EVENT: {
            //~ const DWORD& exit_code = deb_ev.u.ExitThread.dwExitCode;

            //~ // Generate a crash dump on the first abnormal thread exit.
            //~ // We don't want to generate on every abnormal thread exit since ALL the other
            // threads ~ // in the application will follow by exiting with the same code. ~ if
            //(!seen_nonzero_thread_exit && exit_code != 0) { ~ seen_nonzero_thread_exit = true; ~
            // std::fprintf(stderr, ~ "Creating MiniDump on first non-zero thread exit: code
            // 0x%08x\n", ~ exit_code);
            //~ DumpFromDebugEvent(deb_ev, pi);
            //~ }
            //~ ContinueDebugEvent(deb_ev.dwProcessId, deb_ev.dwThreadId, DBG_CONTINUE);
            //~ break;
        //~ }
        case EXCEPTION_DEBUG_EVENT: {
            EXCEPTION_RECORD& record = deb_ev.u.Exception.ExceptionRecord;
            const std::time_t now = std::time(nullptr);
            const std::time_t delta = now - start_time;

            if (ExceptionName(record.ExceptionCode) == nullptr) {
                int record_count = 0;
                EXCEPTION_RECORD* next_record = &deb_ev.u.Exception.ExceptionRecord;
                while (next_record != nullptr) {
                    std::fprintf(stderr,
                                 "[%d] code(%d): 0x%08x\n\tflags: %08x %s\n\taddress: "
                                 "0x%08x\n\tparameters: %d\n",
                                 delta, record_count, next_record->ExceptionCode,
                                 next_record->ExceptionFlags,
                                 next_record->ExceptionFlags == EXCEPTION_NONCONTINUABLE
                                     ? "noncontinuable"
                                     : "",
                                 next_record->ExceptionAddress, next_record->NumberParameters);
                    for (int i = 0; i < static_cast<int>(next_record->NumberParameters); i++) {
                        std::fprintf(stderr, "\t\t%0d: 0x%08x\n", i,
                                     next_record->ExceptionInformation[i]);
                    }

                    record_count++;
                    next_record = next_record->ExceptionRecord;
                }
            }
            // We want to generate a crash dump if we are seeing the same exception again.
            if (!deb_ev.u.Exception.dwFirstChance) {
                std::fprintf(stderr, "Creating MiniDump on ExceptionCode: 0x%08x %s\n",
                             record.ExceptionCode, ExceptionName(record.ExceptionCode));
                DumpFromDebugEvent(deb_ev, pi);
            }
            ContinueDebugEvent(deb_ev.dwProcessId, deb_ev.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
            break;
        }
        }
    }
}

const char* ExceptionName(DWORD exception) {
    switch (exception) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_BREAKPOINT:
        return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_SINGLE_STEP:
        return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
        return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
        return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
        return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:
        return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_GUARD_PAGE:
        return "EXCEPTION_GUARD_PAGE";
    case EXCEPTION_INVALID_HANDLE:
        return "EXCEPTION_INVALID_HANDLE";
    default:
        return nullptr;
    }
}
