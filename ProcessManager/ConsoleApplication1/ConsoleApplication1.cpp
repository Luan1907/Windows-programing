#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <map>

#pragma comment(lib, "psapi.lib")

std::wstring GetProcessNameByPID(DWORD pid, const std::wstring& fallbackName) {
    if (!fallbackName.empty() && fallbackName != L"<unknown>")
        return fallbackName;

    std::wstring processName = L"<unknown>";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        WCHAR name[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, name, &size)) {
            processName = wcsrchr(name, L'\\') ? wcsrchr(name, L'\\') + 1 : name;
        }
        CloseHandle(hProcess);
    }
    return processName;
}

int main() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Không thể tạo snapshot.\n";
        return 1;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    struct ProcInfo {
        DWORD ppid;
        std::wstring name;
    };

    std::map<DWORD, ProcInfo> processMap;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            processMap[pe32.th32ProcessID] = { pe32.th32ParentProcessID, pe32.szExeFile };
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    std::wcout << L"PID\tProcess Name\t\tPPID\tParent Process Name\n";
    std::wcout << L"-------------------------------------------------------------\n";

    for (auto& p : processMap) {
        DWORD pid = p.first;
        DWORD ppid = p.second.ppid;
        std::wstring pname = GetProcessNameByPID(pid, p.second.name);

        if (pname == L"<unknown>")
            continue; // bỏ process nếu không lấy được tên

        std::wstring parentName = L"<unknown>";
        if (processMap.count(ppid)) {
            parentName = GetProcessNameByPID(ppid, processMap[ppid].name);
        }

        if (parentName == L"<unknown>")
            continue; // bỏ nếu process cha không xác định được tên

        std::wcout << pid << L"\t" << pname << L"\t\t" << ppid << L"\t" << parentName << L"\n";
    }

    return 0;
}
