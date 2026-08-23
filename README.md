# Android Emulator Hypervisor Memory Hook (`simple_hook`)

![Repository Banner](assets/banner.png)

[![Website](https://img.shields.io/badge/Website-falconx64.com-blueviolet.svg?style=for-the-badge&logo=google-chrome&logoColor=white)](https://falconx64.com)
[![Discord](https://img.shields.io/badge/Discord-Join%20Community-5865F2.svg?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/HnBRncZScM)
[![Author](https://img.shields.io/badge/Author-Falcon--x64-blue.svg?style=for-the-badge)](#)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg?style=for-the-badge)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-0078D6.svg?style=for-the-badge)](https://microsoft.com)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)

An enterprise-grade, high-performance C++ hypervisor memory access engine and inline hooking framework developed by **Falcon-x64** for x64 Android Emulators (BlueStacks / MSI App Player). 

This library hooks low-level physical memory primitives (`PGMPhysRead` inside `BstkVMM.dll`) and queries guest Linux kernel mapping tables via ADB to read guest virtual memory seamlessly from Windows host processes.

---

## 🌟 Key Features

- **⚡ Hypervisor Level Hooking**: Intercepts physical guest memory read calls (`PGMPhysRead`) by hooking `BstkVMM.dll` using MinHook.
- **🔍 ADB Auto-Base Resolution**: Interrogates `/proc/<pid>/maps` dynamically over background ADB pipes (`HD-Adb.exe`) to resolve dynamic target module base addresses (`libil2cpp.so`) without manual offset searching.
- **🧠 Hardware Address Translation**: Translates guest virtual addresses (GCPtr) to physical host addresses (GCPhys) via hypervisor CPU paging tables (`PGMPhysGCPtr2GCPhys`).
- **🚀 High-Speed Thread-Safe TLB Cache**: Caches address translations in memory with automatic periodic TLB resets to maximize memory throughput and minimize lag.
- **🛡️ SEH Exception Protection**: Wraps memory reading logic inside Structured Exception Handling (`__try / __except`) to prevent host process or emulator crashes during transient memory invalidations.

---

## 🏗 Architecture & Traversal Flow

```text
+-----------------------------------------------------------------------+
|                         Windows Host Process                          |
|                                                                       |
|  [DllMain / AllocConsole]  -- By Falcon-x64                           |
|        |                                                              |
|        v                                                              |
|  [MinHook] ----> Hooks PGMPhysRead (BstkVMM.dll)                      |
|        |                                                              |
|        +-------> Captures VM Physical Pointer (vmmPtr)                |
|        |                                                              |
|        v                                                              |
|  [ADB Shell Bridge]                                                   |
|        |                                                              |
|        +-------> Executes: cat /proc/$(pidof <package>)/maps | grep   |
|        |         Extracts Base Address (e.g. 0x9d082000)               |
|        v                                                              |
|  [Hypervisor Paging Engine]                                           |
|        |                                                              |
|        +-------> PGMPhysGCPtr2GCPhys (Virtual -> Physical)            |
|        |                                                              |
|        v                                                              |
|  [Memory Reader Thread]                                               |
|        +-------> Reads GameFacade -> CurrentMatch -> LocalPlayer      |
+-----------------------------------------------------------------------+
```

---

## 📂 Project Structure

```text
simple_hook/
├── assets/
│   └── banner.png             # Repository header graphic
├── hook/
│   ├── include/
│   │   └── MinHook.h          # MinHook API headers
│   └── src/                   # MinHook source & disassembly engine (HDE)
├── memory/
│   └── InternalMemory.h       # Hypervisor memory translation & hook engine
├── build.bat                  # One-click MSVC compilation script
├── main.cpp                   # DLL Entry Point, ADB pipe shell & worker thread
├── LICENSE                    # MIT License (Falcon-x64)
└── README.md                  # Project documentation
```

---

## 🛠 Building from Source

### Prerequisites
- **Visual Studio 2022** (with C++ Desktop Development Workload)
- Windows 10/11 x64

### Compilation Steps
1. Open PowerShell or Command Prompt.
2. Navigate to the project folder:
   ```cmd
   cd Android-Emulator-Hypervisor-Memory-Hook
   ```
3. Run the automated build script:
   ```cmd
   build.bat
   ```
4. The output DLL `SimpleHook.dll` will be generated in the root directory.

---

## 🖥 Output Console Example

Upon DLL injection into the emulator host process (`HD-Player.exe`), standard output displays:

```text
====================================================
   SimpleHook DLL Loaded into Process (AllocConsole)
                    By Falcon-x64
====================================================
[+] MinHook installed on PGMPhysRead successfully!
[+] Worker Thread Started.
[*] Fetching libil2cpp.so base address via ADB...
[+] ADB Extracted libil2cpp.so Base: 0x9d082000

libil2cpp_base   = 0x9d082000
baseGameFacade   = 0xA986E9C
gameFacade       = 0x7e412000
staticGameFacade = 0x7e41205c
currentGame      = 0x8fa10000
currentMatch     = 0x8fd62000
matchStatus      = 1
localPlayer      = 0x877ff000
---------------------------------------------------
```

---

## ⚠️ Disclaimer

This project is created strictly for **educational, reverse-engineering research, and security analysis purposes**. Use of hypervisor memory hooking tools on commercial software should comply with applicable end-user license agreements and local laws.

---

## 🌐 Contact & Community

- **Website**: [falconx64.com](https://falconx64.com)
- **Discord Community**: [Join Discord](https://discord.gg/HnBRncZScM)
- **Developer**: Falcon-x64 / f64fa

---

## 📄 License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for more details.
