#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

static std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

static std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

static std::wstring PathFromHandle(HANDLE file) {
    if (file == nullptr || file == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::wstring path(MAX_PATH, L'\0');
    DWORD size = GetFinalPathNameByHandleW(file, path.data(), static_cast<DWORD>(path.size()),
                                           FILE_NAME_NORMALIZED);
    if (size == 0) {
        return {};
    }
    if (size >= path.size()) {
        path.resize(size + 1);
        size = GetFinalPathNameByHandleW(file, path.data(), static_cast<DWORD>(path.size()),
                                         FILE_NAME_NORMALIZED);
    }
    path.resize(size);
    constexpr std::wstring_view prefix = L"\\\\?\\";
    if (path.rfind(prefix, 0) == 0) {
        path.erase(0, prefix.size());
    }
    return path;
}

static void LoadModule(HANDLE process, HANDLE file, void* base, DWORD size) {
    const std::wstring wide_path = PathFromHandle(file);
    const std::string path = WideToUtf8(wide_path);
    SymLoadModuleEx(process, file, path.empty() ? nullptr : path.c_str(), nullptr,
                    reinterpret_cast<DWORD64>(base), size, nullptr, 0);
}

static void PrintStack(HANDLE process, HANDLE thread) {
    CONTEXT context{};
    context.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(thread, &context)) {
        std::cout << "GetThreadContext failed: " << GetLastError() << '\n';
        return;
    }

    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    std::vector<char> symbol_buffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);

    std::cout << "STACK\n";
    for (int index = 0; index < 80; ++index) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }

        DWORD64 displacement = 0;
        std::cout << "#" << index << " 0x" << std::hex << frame.AddrPC.Offset << std::dec;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
            std::cout << " " << symbol->Name << " + 0x" << std::hex << displacement << std::dec;
        }

        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line)) {
            std::cout << " (" << line.FileName << ":" << line.LineNumber << ")";
        }
        std::cout << '\n';
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: stack_debugger.exe <exe> [args...]\n"
                     "       stack_debugger.exe --attach <pid>\n";
        return 2;
    }

    HANDLE process = nullptr;
    std::unordered_map<DWORD, HANDLE> threads;
    bool attach_mode = false;

    if (std::string_view(argv[1]) == "--attach") {
        if (argc < 3) {
            std::cerr << "--attach requires a pid\n";
            return 2;
        }

        attach_mode = true;
        const DWORD pid = static_cast<DWORD>(std::stoul(argv[2]));
        process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (process == nullptr) {
            std::cerr << "OpenProcess failed: " << GetLastError() << '\n';
            return 1;
        }

        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process,
                      "C:\\Users\\charl\\Documents\\SuyuEclipse\\build-ninja\\bin;"
                      "C:\\Users\\charl\\Documents\\SuyuEclipse\\build-ninja;"
                      "C:\\Users\\charl\\Documents\\SuyuEclipse",
                      TRUE);

        if (!DebugActiveProcess(pid)) {
            std::cerr << "DebugActiveProcess failed: " << GetLastError() << '\n';
            SymCleanup(process);
            CloseHandle(process);
            return 1;
        }
        DebugSetProcessKillOnExit(FALSE);
        std::cout << "ATTACHED pid=" << pid << '\n';
    } else {
        std::wostringstream command;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) {
                command << L' ';
            }
            std::wstring arg = Utf8ToWide(argv[i]);
            command << L'"';
            for (wchar_t ch : arg) {
                if (ch == L'"') {
                    command << L'\\';
                }
                command << ch;
            }
            command << L'"';
        }
        std::wstring command_line = command.str();

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process_info{};
        if (!CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE,
                            DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE, nullptr, nullptr,
                            &startup, &process_info)) {
            std::cerr << "CreateProcessW failed: " << GetLastError() << '\n';
            return 1;
        }

        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
    }
    bool printed = false;

    DEBUG_EVENT event{};
    while (WaitForDebugEvent(&event, INFINITE)) {
        DWORD continue_status = DBG_CONTINUE;
        switch (event.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            if (!attach_mode || process == nullptr) {
                process = event.u.CreateProcessInfo.hProcess;
            }
            threads[event.dwThreadId] = event.u.CreateProcessInfo.hThread;
            if (!attach_mode) {
                SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
                SymInitialize(process,
                              "C:\\Users\\charl\\Documents\\SuyuEclipse\\build-ninja\\bin;"
                              "C:\\Users\\charl\\Documents\\SuyuEclipse\\build-ninja;"
                              "C:\\Users\\charl\\Documents\\SuyuEclipse",
                              FALSE);
            }
            LoadModule(process, event.u.CreateProcessInfo.hFile,
                       event.u.CreateProcessInfo.lpBaseOfImage, 0);
            if (event.u.CreateProcessInfo.hFile) {
                CloseHandle(event.u.CreateProcessInfo.hFile);
            }
            break;
        case CREATE_THREAD_DEBUG_EVENT:
            threads[event.dwThreadId] = event.u.CreateThread.hThread;
            break;
        case EXIT_THREAD_DEBUG_EVENT:
            if (auto it = threads.find(event.dwThreadId); it != threads.end()) {
                CloseHandle(it->second);
                threads.erase(it);
            }
            break;
        case LOAD_DLL_DEBUG_EVENT:
            if (process != nullptr) {
                LoadModule(process, event.u.LoadDll.hFile, event.u.LoadDll.lpBaseOfDll, 0);
            }
            if (event.u.LoadDll.hFile) {
                CloseHandle(event.u.LoadDll.hFile);
            }
            break;
        case EXCEPTION_DEBUG_EVENT: {
            const auto& info = event.u.Exception;
            const DWORD code = info.ExceptionRecord.ExceptionCode;
            if ((code == EXCEPTION_ACCESS_VIOLATION || !info.dwFirstChance) && !printed) {
                printed = true;
                std::cout << "EXCEPTION code=0x" << std::hex << code << std::dec
                          << " firstChance=" << info.dwFirstChance << " address="
                          << info.ExceptionRecord.ExceptionAddress << '\n';
                if (auto it = threads.find(event.dwThreadId); it != threads.end()) {
                    PrintStack(process, it->second);
                } else {
                    HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, event.dwThreadId);
                    if (thread != nullptr) {
                        PrintStack(process, thread);
                        CloseHandle(thread);
                    } else {
                        std::cout << "thread handle not found for " << event.dwThreadId << '\n';
                    }
                }
            }
            continue_status = info.dwFirstChance ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE;
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            std::cout << "EXIT code=" << event.u.ExitProcess.dwExitCode << '\n';
            for (auto& [id, handle] : threads) {
                CloseHandle(handle);
            }
            if (process != nullptr) {
                SymCleanup(process);
            }
            ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
            return 0;
        default:
            break;
        }

        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status);
    }

    return 0;
}
