#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <unordered_map>

typedef int(__cdecl *PGMPhysReadFunc)(void *pVM, uintptr_t GCPhys, void *pvBuf, size_t cbRead);
typedef int(__cdecl *PGMPhysSimpleWriteGCPhysFunc)(void *pVM, uintptr_t GCPhys, void *pvBuf, size_t cbWrite);
typedef int(__cdecl *PGMPhysGCPtr2GCPhysFunc)(void *pVCpu, uintptr_t address, uintptr_t *physAddress);
typedef void *(__cdecl *VMMGetCpuByIdFunc)(void *pVM, int cpuId);

inline void *vmPtr = nullptr;
inline void *pVMAddr = nullptr;
inline void *cpuAddr = nullptr;

inline PGMPhysReadFunc ogPhysRead = nullptr;
inline VMMGetCpuByIdFunc ogCPU = nullptr;
inline PGMPhysGCPtr2GCPhysFunc ogCast = nullptr;
inline PGMPhysSimpleWriteGCPhysFunc ogWrite = nullptr;

inline std::mutex g_CacheMutex;
inline std::unordered_map<uintptr_t, uintptr_t> g_TranslationCache;
inline auto g_LastResetTime = std::chrono::steady_clock::now();

inline void InitMemoryEngine(void *pVMParam) {
    pVMAddr = pVMParam;
    if (ogCPU) cpuAddr = ogCPU(pVMParam, 0);
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    g_TranslationCache.clear();
}

inline int __cdecl HookedPGMPhysRead(void *pVMParam, uintptr_t GCPhys, void *pvBuf, size_t cbRead) {
    if (vmPtr == nullptr) {
        vmPtr = pVMParam;
        InitMemoryEngine(pVMParam);
    }
    return ogPhysRead ? ogPhysRead(pVMParam, GCPhys, pvBuf, cbRead) : -1;
}

inline bool VirtToPhys(uintptr_t virtAddr, uintptr_t &physAddr) {
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        if (now - g_LastResetTime > std::chrono::seconds(10)) {
            g_TranslationCache.clear();
            g_LastResetTime = now;
        }
        auto it = g_TranslationCache.find(virtAddr);
        if (it != g_TranslationCache.end()) {
            physAddr = it->second;
            return true;
        }
    }

    if (!cpuAddr && ogCPU) cpuAddr = ogCPU(pVMAddr, 0);
    if (!cpuAddr || !ogCast) return false;

    if (ogCast(cpuAddr, virtAddr, &physAddr) != 0) return false;

    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_TranslationCache[virtAddr] = physAddr;
    }
    return true;
}

template <typename T>
bool ReadMem(uintptr_t address, T &data) {
    uintptr_t phys = 0;
    if (!VirtToPhys(address, phys) || !ogPhysRead) return false;
    return (ogPhysRead(pVMAddr, phys, &data, sizeof(T)) == 0);
}

template <typename T>
bool WriteMem(uintptr_t address, const T &value) {
    uintptr_t phys = 0;
    if (!VirtToPhys(address, phys) || !ogWrite) return false;
    return (ogWrite(pVMAddr, phys, const_cast<T *>(&value), sizeof(T)) == 0);
}
