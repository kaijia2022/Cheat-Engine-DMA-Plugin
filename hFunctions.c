#pragma warning(suppress : 6011)
#include "hFunctions.h"


VMM_HANDLE hVMM = NULL;
PVMMDLL_PROCESS_INFORMATION pProcInfoAll = NULL;
DWORD cProcInfo = 0;
PVMMDLL_MAP_MODULE pmModule = NULL;
PVMMDLL_MAP_VAD pVadMap = NULL;
DWORD currentPID = 0;
QWORD nextVaStart = 0;
QWORD previousAoQ = 0; //Address of Query
unsigned int procIdx = 0;
unsigned int moduleIdx = 0;
unsigned int vadIdx = 0;
unsigned int lastCommittedIdx = 0;
BOOL vadMapInitialized = FALSE;
BOOL vadMapUpdated = FALSE;

const char* commonProcesses[] = {
    "[system process]", "system", "svchost.exe", "services.exe", "wininit.exe",
    "smss.exe", "csrss.exe", "lsass.exe", "winlogon.exe", "wininit.exe", "dwm.exe",
    "rundll32.exe", "net1.exe", "net.exe", "Code.exe"
};
/**
 * @brief   Determine the memory state of a vad entry
 * @param   pVad - pointer to the vad entry.
 * @return  memory state of the vad entry.
 */
DWORD VadMap_State(_In_ PVMMDLL_MAP_VADENTRY pVad)
{
    if (pVad->CommitCharge > 0) {
        return MEM_COMMIT;
    }
    else {
        return MEM_FREE;
    }
}

/**
 * @brief   Determine the protection flags of a vad entry
 * @param   pVad - pointer to the vad entry.
 * @return  protection flags of the vad entry.
 */
DWORD VadMap_Protection(_In_ PVMMDLL_MAP_VADENTRY pVad)
{
    CHAR sz[7] = { 0 };
    DWORD szVadProtection = 0;
    BYTE vh = (BYTE)pVad->Protection >> 3;
    BYTE vl = (BYTE)pVad->Protection & 7;
    sz[0] = pVad->fPrivateMemory ? 'p' : '-';                                    // PRIVATE MEMORY
    sz[1] = (vh & 2) ? ((vh & 1) ? 'm' : 'g') : ((vh & 1) ? 'n' : '-');         // -/NO_CACHE/GUARD/WRITECOMBINE
    sz[2] = ((vl == 1) || (vl == 3) || (vl == 4) || (vl == 6)) ? 'r' : '-';     // READ
    sz[3] = (vl & 4) ? 'w' : '-';                                               // WRITE
    sz[4] = (vl & 2) ? 'x' : '-';                                               // EXECUTE
    sz[5] = ((vl == 5) || (vl == 7)) ? 'c' : '-';                               // COPY ON WRITE
    if (sz[1] != '-' && sz[2] == '-' && sz[3] == '-' && sz[4] == '-' && sz[5] == '-') { sz[1] = '-'; }


    if (sz[2] == 'r' && sz[4] == 'x') { szVadProtection |= PAGE_EXECUTE_READ; }
    if (sz[4] == 'x' && sz[5] == 'c') { szVadProtection |= PAGE_EXECUTE_WRITECOPY; }
    if (sz[2] == 'r' && sz[3] == 'w') { szVadProtection |= PAGE_READWRITE; }
    if (sz[2] == 'r' && sz[3] == 'w' && sz[4] == 'x') { szVadProtection |= PAGE_EXECUTE_READWRITE; }
    if (sz[4] == 'x') { szVadProtection |= PAGE_EXECUTE; }
    if (sz[5] == 'c') { szVadProtection |= PAGE_WRITECOPY; }
    if (sz[1] == 'g') { szVadProtection |= PAGE_GUARD; }
    if (sz[1] == 'n') { szVadProtection |= PAGE_NOCACHE; }
    if (sz[1] == 'm') { szVadProtection |= PAGE_WRITECOMBINE; }
    if (sz[2] == 'r' && sz[0] == '-' && sz[1] == '-' && sz[3] == '-' && sz[4] == '-' && sz[5] == '-') { szVadProtection |= PAGE_READONLY; }

    return szVadProtection;
}

/**
 * @brief   Determine the memory type of a vad entry
 * @param   pVad  - pointer to the vad entry.
 * @return  memory type of the vad entry.
 */
DWORD VadMap_Type(_In_ PVMMDLL_MAP_VADENTRY pVad)
{
    if (pVad->fImage) {
        return MEM_IMAGE;
    }
    else if (pVad->fFile) {
        return MEM_MAPPED;
    }
    else if (pVad->fHeap || pVad->fStack || pVad->fTeb || pVad->fPageFile) {
        return MEM_PRIVATE;
    }
    else {
        return MEM_PRIVATE;
    }
}

BOOL updateVadMap() {
    BOOL result;
    VMMDLL_MemFree(pVadMap); 
    pVadMap = NULL;
    result = VMMDLL_Map_GetVadU(hVMM, currentPID, TRUE, &pVadMap);
    if (!result) {
        return 0;
    }
    if (pVadMap->dwVersion != VMMDLL_MAP_VAD_VERSION) {
        VMMDLL_MemFree(pVadMap); pVadMap = NULL;
        return 0;
    }
    return 1;
}

/**
 * @brief   Get the process handle(in our case the PID)
 * @param   dwDesirteedAccess - access rights desired for the process handle (neglected).
 * @param   bInheritHandle  - if 'TRUE' the handle can be inherited by child processes(neglected).
 * @param   dwProcessId  - the pid of the process.
 * @return  handle(dwProcessId) to the process.
 */
HANDLE __stdcall hOpenProcess(DWORD dwDesirteedAccess, BOOL bInheritHandle, DWORD dwProcessId)
{
    currentPID = dwProcessId;
    vadMapInitialized = FALSE;

    return  (HANDLE)dwProcessId;
}

/**
 * @brief   Read virtual memory from a process
 * @param   hProcess - process handle(dwProcessId).
 * @param   lpBaseAddress  - A pointer to the base address in the specified process from which to read.
 * @param   lpBuffer  - A pointer to a buffer that receives the contents from the address space of the specified process.
 * @param   nSize  - The number of bytes to be read from the specified process.
 * @param   lpNumberOfBytesRead  - A pointer to a variable that receives the number of bytes transferred into the specified buffer. This parameter is optional and can be NULL.
 * @return  TRUE if successful, FALSE otherwise.
 */
BOOL __stdcall hReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead)
{
    BOOL result;
    result = VMMDLL_MemReadEx(hVMM, (DWORD)hProcess, (ULONG64)lpBaseAddress, (PBYTE)lpBuffer, (DWORD)nSize, (PDWORD)lpNumberOfBytesRead, VMMDLL_FLAG_NOCACHE);

    if (*lpNumberOfBytesRead == nSize) {
        return result;
    }
    else
        return FALSE;
}

/**
 * @brief   Write to virtual memory of a process
 * @param   hProcess - process handle(dwProcessId).
 * @param   lpBaseAddress  - A pointer to the base address in the specified process to which data is to be written.
 * @param   lpBuffer  - A pointer to the buffer that contains the data to be written to the specified process.
 * @param   nSize  - The number of bytes to be written to the specified process.
 * @param   lpNumberOfBytesRead  - A pointer to a variable that receives the number of bytes transferred into the specified process. This parameter is optional and can be NULL.
 * @return  TRUE if successful, FALSE otherwise.
 */
BOOL _stdcall hWriteProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten)
{
    BOOL result;
    result = VMMDLL_MemWrite(hVMM, (DWORD)hProcess, (ULONG64)lpBaseAddress, (PBYTE)lpBuffer, (DWORD)nSize);
    if (result) {
        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = nSize;
        }

    }
    else {
        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = 0;
        }
    }
    return result;
}


BOOL GetNextVaStartAddr() {
    if (!pVadMap) {
        return 0;
    }

    PVMMDLL_MAP_VADENTRY pVadMapEntry;
    for (unsigned int i = vadIdx; i < pVadMap->cMap; i++) {
        pVadMapEntry = &pVadMap->pMap[i];
        if (pVadMapEntry->CommitCharge > 0) {
            nextVaStart = pVadMapEntry->vaStart;
            vadIdx = i;
            return 1;
        }
    }
    nextVaStart = 0;
    vadIdx = 0;
    return 0;
}

/**
 * @brief   Retrieve information about a section of memory starting at lpAddress
 * @param   hProcess - process handle(dwProcessId).
 * @param   lpAddress  - A pointer to the base address of the region of pages to be queried. This can be any address within the region of interest.
 * @param   lpBuffer  - A pointer to a MEMORY_BASIC_INFORMATION structure that receives information about the specified page range.
 * @param   dwLength  - The size of the buffer pointed to by lpBuffer, in bytes.
 * @return  1 if successful, 0 otherwise.
 */
DWORD __stdcall hVirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, DWORD dwLength)
{

    if (!vadMapInitialized) {
        if (pVadMap) {
            VMMDLL_MemFree(pVadMap); pVadMap = NULL;
        }
        BOOL validVadMap = updateVadMap();
        vadIdx = 0;
        vadMapInitialized = TRUE;
        if (!validVadMap) { return 0; }
    }
    if ((QWORD)lpAddress < previousAoQ) {
        VMMDLL_MemFree(pVadMap); pVadMap = NULL;
        BOOL validVadMap = updateVadMap();
        vadIdx = 0;
        if (!validVadMap) { return 0; }
    }
    previousAoQ = (QWORD)lpAddress;

    //printf("Address Queried: %llu\n", (ULONG64)lpAddress);
    PVMMDLL_MAP_VADENTRY pVadMapEntry;

    BOOL result = GetNextVaStartAddr();
    if (!result) {
        return 0;
    }
    if ((ULONG64)lpAddress < nextVaStart) {
        lpBuffer->RegionSize = nextVaStart - (ULONG64)lpAddress;
        lpBuffer->BaseAddress = (PVOID)(ULONG64)lpAddress;
        lpBuffer->Protect = PAGE_NOACCESS;
        lpBuffer->State = MEM_FREE;
        lpBuffer->Type = MEM_PRIVATE;
        //printf("No Section located, increment query address: BaseAddress = %llu, RegionSize = %llu\n", (ULONG64)lpAddress, lpBuffer->RegionSize);
        return sizeof(lpBuffer);
    }


    for (unsigned int i = vadIdx; i < pVadMap->cMap; i++) {
        pVadMapEntry = &pVadMap->pMap[i];
        if (!pVadMapEntry->CommitCharge) {
            continue;
        }
        if ((ULONG64)lpAddress >= pVadMapEntry->vaStart && (ULONG64)lpAddress <= pVadMapEntry->vaEnd) {
            lpBuffer->BaseAddress = (PVOID)pVadMapEntry->vaStart;
            lpBuffer->RegionSize = pVadMapEntry->vaEnd + 1 - pVadMapEntry->vaStart;
            lpBuffer->Type = VadMap_Type(pVadMapEntry);
            lpBuffer->Protect = VadMap_Protection(pVadMapEntry);
            lpBuffer->State = VadMap_State(pVadMapEntry);
            //printf("Section Located with vaStarts = %llu, vaEnd = %llu, sectionSize = %llu\n", pVadMapEntry->vaStart, pVadMapEntry->vaEnd, lpBuffer->RegionSize);

            vadIdx = i + 1;

            return sizeof(lpBuffer);
        }
    }
    
    return 0;


}

/**
 * @brief   Retrieve the processlist/modulelist based on input flag, Returns pointer to idx as handle for processlist/module entries traversal.
 * @param   dwFlags - The portions of the system to be included in the snapshot.
 * @param   th32ProcessID  - the pid of the process, 0 if all processes.
 * @return  A pointer to the idx of the requested array if successful, 0 otherwise (not INVALID_HANLDE_VALUE because the sourcecode of CE explicitly checks for 0).
 */
HANDLE __stdcall hCreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID)
{
    BOOL result;
    
    if (dwFlags == TH32CS_SNAPPROCESS) {
        VMMDLL_MemFree(pProcInfoAll);
        pProcInfoAll = NULL;
        cProcInfo = 0;
        result = VMMDLL_ProcessGetInformationAll(hVMM, &pProcInfoAll, &cProcInfo);
        if (result) {
            procIdx = 0;
            return (HANDLE)&procIdx;
        }
    }
    else if (dwFlags == (TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32)) {
        VMMDLL_MemFree(pmModule);
        pmModule = NULL;
        result = VMMDLL_Map_GetModuleU(hVMM, th32ProcessID, &pmModule, 0);
        if (result) {
            moduleIdx = 0;
            return (HANDLE)&moduleIdx;
        }
    }

    return (HANDLE)0;
}

BOOL isCommonProcess(PVMMDLL_PROCESS_INFORMATION proc)
{
    for (int i = 0; i < sizeof(commonProcesses) / sizeof(commonProcesses[0]); i++) {
        if (strcmp(proc->szNameLong, commonProcesses[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief   Get process information for the first non-common process in the procInfo list, increment the index
 * @param   hSnapshot - handle(pointer to index).
 * @param   lppe  - A pointer to a PROCESSENTRY32 structure. 
 * @return  1 if successful, 0 otherwise.
 */
BOOL __stdcall hProcess32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
    if (!pProcInfoAll || (ULONG64)hSnapshot == 0) {
        return 0;
    }
    PVMMDLL_PROCESS_INFORMATION pProcInfoEntry;

    pProcInfoEntry = &pProcInfoAll[*(unsigned int*)hSnapshot];

    while (isCommonProcess(pProcInfoEntry)) {
        *(unsigned int*)hSnapshot += 1;
        pProcInfoEntry = &pProcInfoAll[*(unsigned int*)hSnapshot];
    }

    lppe->th32ProcessID = pProcInfoEntry->dwPID;
    lppe->th32ParentProcessID = pProcInfoEntry->dwPPID;
    lppe->dwSize = pProcInfoEntry->wSize;
    strncpy_s(lppe->szExeFile, sizeof(lppe->szExeFile),pProcInfoEntry->szNameLong, 64);
    *(unsigned int*)hSnapshot += 1;
    return 1;
}

/**
 * @brief  Get process information for the next non-common process in the procInfo list, increment the index
 * @param   hSnapshot - handle(pointer to index).
 * @param   lppe  - A pointer to a PROCESSENTRY32 structure.
 * @return  1 if successful, 0 otherwise.
 */
BOOL __stdcall hProcess32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
    if ((ULONG64)hSnapshot == 0 || *(unsigned int*)hSnapshot >= cProcInfo || *(unsigned int*)hSnapshot <= 0) {
        return 0;
    }
    PVMMDLL_PROCESS_INFORMATION pProcInfoEntry;

    pProcInfoEntry = &pProcInfoAll[*(unsigned int*)hSnapshot];

    while (isCommonProcess(pProcInfoEntry)) {
        *(unsigned int*)hSnapshot += 1;
        pProcInfoEntry = &pProcInfoAll[*(unsigned int*)hSnapshot];
    }

    lppe->th32ProcessID = pProcInfoEntry->dwPID;
    lppe->th32ParentProcessID = pProcInfoEntry->dwPPID;
    lppe->dwSize = pProcInfoEntry->wSize;
    strncpy_s(lppe->szExeFile, sizeof(lppe->szExeFile), pProcInfoEntry->szNameLong, 64);
    *(unsigned int*)hSnapshot += 1;
    return 1;
}

/**
 * @brief  Get module information for the first module in the module list, increment the index
 * @param   hSnapshot - handle(pointer to index).
 * @param   lpme  - A pointer to a MODULEENTRY32 structure.
 * @return  1 if successful, 0 otherwise.
 */
BOOL __stdcall hModule32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
    if (!pmModule || (ULONG64)hSnapshot == 0) {
        return 0;
    }

    PVMMDLL_MAP_MODULEENTRY pmModuleEntry;

    pmModuleEntry = &pmModule->pMap[*(unsigned int*)hSnapshot];

    lpme->modBaseAddr = (BYTE*)pmModuleEntry->vaBase;
    lpme->modBaseSize = pmModuleEntry->cbImageSize;
    strncpy_s(lpme->szExePath, sizeof(lpme->szExePath),pmModuleEntry->uszFullName, 260);
    strncpy_s(lpme->szModule, sizeof(lpme->szModule), pmModuleEntry->uszText, 256);
    *(unsigned int*)hSnapshot += 1;
    return 1;

}

/**
 * @brief  Get module information for the next module in the module list, increment the index
 * @param   hSnapshot - handle(pointer to index).
 * @param   lpme  - A pointer to a MODULEENTRY32 structure.
 * @return  1 if successful, 0 otherwise.
 */
BOOL __stdcall hModule32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
    if (!pmModule || (ULONG64)hSnapshot == 0 || *(unsigned int*)hSnapshot >= pmModule->cMap || *(unsigned int*)hSnapshot <= 0) {
        return 0;
    }

    PVMMDLL_MAP_MODULEENTRY pmModuleEntry;

    pmModuleEntry = &pmModule->pMap[*(unsigned int*)hSnapshot];

    lpme->modBaseAddr = (BYTE*)pmModuleEntry->vaBase;
    lpme->modBaseSize = pmModuleEntry->cbImageSize;
    strncpy_s(lpme->szExePath, sizeof(lpme->szExePath), pmModuleEntry->uszFullName, 260);
    strncpy_s(lpme->szModule, sizeof(lpme->szModule), pmModuleEntry->uszText, 256);
    *(unsigned int*)hSnapshot += 1;
    return 1;
}
