#pragma once

#include <windows.h>

#include <dbghelp.h>

void CreateMiniDump(HANDLE process_handle, DWORD process_id, MINIDUMP_EXCEPTION_INFORMATION* info,
                    EXCEPTION_POINTERS* pep);

bool SpawnDebuggee(const char* arg0, PROCESS_INFORMATION& pi);
void DebugDebuggee(PROCESS_INFORMATION& pi);
const char* ExceptionName(DWORD exception);
