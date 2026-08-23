// ============================================================================
// simple_hook/main.cpp - Clean Consolidated SimpleHook DLL
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "InternalMemory.h"
#include "MinHook.h"

// Game Offsets
namespace Offsets {
    inline uintptr_t Il2Cpp       = 0x0;
    inline uintptr_t InitBase     = 0xA986E9C;
    inline uintptr_t StaticClass  = 0x5C;
    inline uintptr_t CurrentMatch = 0x50;
    inline uintptr_t MatchStatus  = 0x8C;
    inline uintptr_t LocalPlayer  = 0x94;
}

// Compact System & ADB Utilities
namespace SystemUtils {

inline DWORD GetProcessId(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{ sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

inline void TerminateProcessByName(const wchar_t* name) {
    DWORD pid = GetProcessId(name);
    if (pid != 0) {
        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hp) { ::TerminateProcess(hp, 0); CloseHandle(hp); }
    }
}

inline void CleanAdb() {
    TerminateProcessByName(L"adb.exe");
    TerminateProcessByName(L"HD-Adb.exe");
}

inline std::string ExecAdbShell(const std::string& command) {
    HANDLE hOutRead, hOutWrite, hInRead, hInWrite;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0) || !CreatePipe(&hInRead, &hInWrite, &sa, 0)) return "";
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{ sizeof(si) };
    si.hStdError = hOutWrite; si.hStdOutput = hOutWrite; si.hStdInput = hInRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::string cmd = ".\\HD-Adb shell \"getprop ro.secure ; /boot/android/android/system/xbin/bstk/su\"";
    if (!CreateProcessA(NULL, const_cast<LPSTR>(cmd.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hOutRead); CloseHandle(hOutWrite); CloseHandle(hInRead); CloseHandle(hInWrite);
        return "";
    }
    CloseHandle(hOutWrite); CloseHandle(hInRead);

    DWORD written = 0;
    std::string input = command + "\n";
    WriteFile(hInWrite, input.c_str(), (DWORD)input.length(), &written, NULL);
    CloseHandle(hInWrite);

    char buf[256]; DWORD read = 0; std::string output;
    while (ReadFile(hOutRead, buf, sizeof(buf) - 1, &read, NULL) && read > 0) {
        buf[read] = '\0'; output += buf;
    }
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(hOutRead);
    return output;
}

inline uintptr_t FetchIl2CppBaseADB() {
    std::cout << "[*] Fetching libil2cpp.so base address via ADB..." << std::endl;
    CleanAdb();
    std::string out = ExecAdbShell("cat /proc/$(pidof com.dts.freefireth)/maps | grep libil2cpp.so");
    CleanAdb();

    size_t dash = out.find('-');
    if (dash != std::string::npos && dash >= 8) {
        std::string hexStr = out.substr(dash - 8, 8);
        uintptr_t addr = strtoull(hexStr.c_str(), nullptr, 16);
        std::cout << "[+] ADB Extracted libil2cpp.so Base: 0x" << std::hex << addr << std::dec << std::endl;
        return addr;
    }
    return 0;
}

} // namespace SystemUtils

// Hypervisor Hooking Setup
bool InitHooks() {
    HMODULE hVMM = GetModuleHandleA("BstkVMM.dll");
    if (!hVMM) hVMM = LoadLibraryA("BstkVMM.dll");
    if (!hVMM) return false;

    auto targetRead = (PGMPhysReadFunc)GetProcAddress(hVMM, "PGMPhysRead");
    ogWrite = (PGMPhysSimpleWriteGCPhysFunc)GetProcAddress(hVMM, "PGMPhysSimpleWriteGCPhys");
    ogCast = (PGMPhysGCPtr2GCPhysFunc)GetProcAddress(hVMM, "PGMPhysGCPtr2GCPhys");
    ogCPU = (VMMGetCpuByIdFunc)GetProcAddress(hVMM, "VMMGetCpuById");

    if (!targetRead || !ogCast || !ogCPU) return false;

    if (MH_Initialize() != MH_OK) return false;
    if (MH_CreateHook((LPVOID)targetRead, (LPVOID)&HookedPGMPhysRead, (LPVOID*)&ogPhysRead) != MH_OK) return false;
    if (MH_EnableHook((LPVOID)targetRead) != MH_OK) return false;

    std::cout << "[+] MinHook installed on PGMPhysRead successfully!" << std::endl;
    return true;
}

// Memory Traversal Data Struct & SEH Wrapper
struct TraversalResult {
    uint32_t baseGameFacade = 0, gameFacade = 0, staticGameFacade = 0;
    uint32_t currentGame = 0, currentMatch = 0, matchStatus = 0, localPlayer = 0;
};

inline bool ReadGameTraversal(TraversalResult& res) {
    __try {
        if (!ReadMem(Offsets::Il2Cpp + Offsets::InitBase, res.baseGameFacade) || !res.baseGameFacade) return false;
        if (!ReadMem(res.baseGameFacade, res.gameFacade) || !res.gameFacade) return false;
        if (!ReadMem(res.gameFacade + Offsets::StaticClass, res.staticGameFacade) || !res.staticGameFacade) return false;
        if (!ReadMem(res.staticGameFacade, res.currentGame) || !res.currentGame) return false;
        if (!ReadMem(res.currentGame + Offsets::CurrentMatch, res.currentMatch) || !res.currentMatch) return false;

        ReadMem(res.currentMatch + Offsets::MatchStatus, res.matchStatus);
        ReadMem(res.currentMatch + Offsets::LocalPlayer, res.localPlayer);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void PrintTraversal() {
    TraversalResult res;
    if (!ReadGameTraversal(res)) return;

    std::cout << "libil2cpp_base   = 0x" << std::hex << Offsets::Il2Cpp << "\n";
    std::cout << "baseGameFacade   = 0x" << std::hex << res.baseGameFacade << "\n";
    std::cout << "gameFacade       = 0x" << std::hex << res.gameFacade << "\n";
    std::cout << "staticGameFacade = 0x" << std::hex << res.staticGameFacade << "\n";
    std::cout << "currentGame      = 0x" << std::hex << res.currentGame << "\n";
    std::cout << "currentMatch     = 0x" << std::hex << res.currentMatch << "\n";
    std::cout << "matchStatus      = " << std::dec << res.matchStatus << "\n";
    std::cout << "localPlayer      = 0x" << std::hex << res.localPlayer << "\n";
    std::cout << "---------------------------------------------------\n";
}

void MainLoop() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "[+] Worker Thread Started." << std::endl;

    Offsets::Il2Cpp = SystemUtils::FetchIl2CppBaseADB();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (pVMAddr == nullptr || cpuAddr == nullptr || Offsets::Il2Cpp == 0) continue;
        PrintTraversal();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        AllocConsole();
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);

        std::cout << "====================================================" << std::endl;
        std::cout << "   SimpleHook DLL Loaded into Process (AllocConsole)  " << std::endl;
        std::cout << "                    By Falcon-x64                   " << std::endl;
        std::cout << "====================================================" << std::endl;

        if (InitHooks()) {
            std::thread(MainLoop).detach();
        }
    } else if (dwReason == DLL_PROCESS_DETACH) {
        FreeConsole();
    }
    return TRUE;
}
