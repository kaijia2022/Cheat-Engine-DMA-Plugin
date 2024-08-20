#pragma once
#include <vmmdll.h>
#include <stdio.h>

typedef struct tagMODULEENTRY32
{
    DWORD   dwSize;
    DWORD   th32ModuleID;       // This module
    DWORD   th32ProcessID;      // owning process
    DWORD   GlblcntUsage;       // Global usage count on the module
    DWORD   ProccntUsage;       // Module usage count in th32ProcessID's context
    BYTE* modBaseAddr;        // Base address of module in th32ProcessID's context
    DWORD   modBaseSize;        // Size in bytes of module starting at modBaseAddr
    HMODULE hModule;            // The hModule of this module in th32ProcessID's context
    char    szModule[256];
    char    szExePath[260];
} MODULEENTRY32;
typedef MODULEENTRY32* PMODULEENTRY32;
typedef MODULEENTRY32* LPMODULEENTRY32;

typedef struct tagPROCESSENTRY32
{
    DWORD   dwSize;
    DWORD   cntUsage;
    DWORD   th32ProcessID;          // this process
    ULONG_PTR th32DefaultHeapID;
    DWORD   th32ModuleID;           // associated exe
    DWORD   cntThreads;
    DWORD   th32ParentProcessID;    // this process's parent process
    LONG    pcPriClassBase;         // Base priority of process's threads
    DWORD   dwFlags;
    CHAR    szExeFile[260];    // Path
} PROCESSENTRY32;
typedef PROCESSENTRY32* PPROCESSENTRY32;
typedef PROCESSENTRY32* LPPROCESSENTRY32;


enum dwFlags {
    TH32CS_SNAPHEAPLIST = 0x00000001,
    TH32CS_SNAPPROCESS = 0x00000002,
    TH32CS_SNAPTHREAD = 0x00000004,
    TH32CS_SNAPMODULE = 0x00000008,
    TH32CS_SNAPMODULE32 = 0x00000010,
    TH32CS_SNAPALL = TH32CS_SNAPHEAPLIST | TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD | TH32CS_SNAPMODULE,
    TH32CS_INHERIT = 0x80000000,
};

extern VMM_HANDLE hVMM;
extern PVMMDLL_PROCESS_INFORMATION pProcInfoAll;
extern DWORD cProcInfo;
extern PVMMDLL_MAP_MODULE pmModule;
extern PVMMDLL_MAP_VAD pVadMap;


extern HANDLE __stdcall hOpenProcess(DWORD dwDesirteedAccess,  BOOL bInheritHandle, DWORD dwProcessId);
extern BOOL __stdcall hIsWow64Process(HANDLE hProcess);
extern BOOL __stdcall hReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
extern BOOL _stdcall hWriteProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);
extern DWORD __stdcall hVirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, DWORD dwLength);

extern HANDLE __stdcall hCreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID);
extern BOOL __stdcall hProcess32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
extern BOOL __stdcall hProcess32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
extern BOOL __stdcall hModule32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
extern BOOL __stdcall hModule32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme);






