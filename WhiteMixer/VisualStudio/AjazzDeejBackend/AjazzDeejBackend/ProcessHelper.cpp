#include <windows.h>
#include <psapi.h>
#include <string>
#include <iostream>

// Linkiamo la libreria necessaria per PSAPI
#pragma comment(lib, "psapi.lib")

std::wstring GetProcessNameFromID(DWORD processID) {
    std::wstring processName = L"<unknown>";

    // Apriamo il processo con permessi di lettura
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

    if (NULL != hProcess) {
        HMODULE hMod;
        DWORD cbNeeded;

        // EnumProcessModules ci serve per ottenere l'handle del modulo principale (l'exe)
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            WCHAR szProcessName[MAX_PATH];

            // Otteniamo il nome base (es. "discord.exe")
            if (GetModuleBaseNameW(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(WCHAR))) {
                processName = szProcessName;
            }
        }
    }

    // Chiudiamo sempre l'handle per evitare memory leaks
    if (hProcess) CloseHandle(hProcess);

    return processName;
}