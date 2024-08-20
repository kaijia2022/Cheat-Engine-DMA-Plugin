// example-c.cpp : Defines the entry point for the DLL application.
//
#define _CRT_SECURE_NO_WARNINGS
//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:


#include <windows.h>
#include <stdio.h>
#include "cepluginsdk.h"
#include "hFunctions.h"

int selfid;
int memorybrowserpluginid=-1; //initialize it to -1 to indicate failure (used by the DisablePlugin routine)
int addresslistPluginID=-1;
int debugpluginID=-1;
int ProcesswatchpluginID=-1;
int PointerReassignmentPluginID=-1;
int MainMenuPluginID=-1;

ExportedFunctions Exported;

PVOID pOpenProcess;
PVOID pIsWow64Process;
CEP_READPROCESSMEMORY pReadProcessMemory;
PVOID pWriteProcessMemory;
PVOID pVirtualQueryEx;

PVOID pCreateToolhelp32Snapshot;
PVOID pProcess32First;
PVOID pProcess32Next;
PVOID pModule32First;
PVOID pModule32Next;



void __stdcall mainmenuplugin(void)
{
	Exported.ShowMessage("Main menu plugin");
	return;
}


BOOL __stdcall CEPlugin_GetVersion(PPluginVersion pv, int sizeofpluginversion)
{
	pv->version = CESDK_VERSION;
	pv->pluginname = "DMAPlugin by Kaijia2024 v0.9 (SDK version 6: 5.0+)"; //exact strings like this are pointers to the string in the dll, so workable
	return TRUE;
}

/*int lua_pluginExample(lua_State* L) //make sure this is cdecl
{
	Exported.ShowMessage("Called from lua");
	lua_pushinteger(L, 123);
	return 1;
*/

void hookFunctions() {

	pOpenProcess = *(PVOID*)Exported.OpenProcess;
	pIsWow64Process = Exported.IsWow64Process;
	pReadProcessMemory = *(CEP_READPROCESSMEMORY*)Exported.ReadProcessMemory;
	pWriteProcessMemory = *(PVOID*)Exported.WriteProcessMemory;
	pVirtualQueryEx = *(PVOID*)Exported.VirtualQueryEx;

	pCreateToolhelp32Snapshot = *(PVOID*)Exported.CreateToolhelp32Snapshot;
	pProcess32First = *(PVOID*)Exported.Process32First;
	pProcess32Next = *(PVOID*)Exported.Process32Next;
	pModule32First = *(PVOID*)Exported.Module32First;
	pModule32Next = *(PVOID*)Exported.Module32Next;

	*(PVOID*)Exported.OpenProcess = (PVOID)&hOpenProcess;
	Exported.IsWow64Process = &hIsWow64Process;
	*(CEP_READPROCESSMEMORY*)Exported.ReadProcessMemory = (CEP_READPROCESSMEMORY)&hReadProcessMemory;
	*(PVOID*)Exported.WriteProcessMemory = (PVOID)&hWriteProcessMemory;
	*(PVOID*)Exported.VirtualQueryEx = (PVOID)&hVirtualQueryEx;

	*(PVOID*)Exported.CreateToolhelp32Snapshot = (PVOID)&hCreateToolhelp32Snapshot;
	*(PVOID*)Exported.Process32First = (PVOID)&hProcess32First;
	*(PVOID*)Exported.Process32Next = (PVOID)&hProcess32Next;
	*(PVOID*)Exported.Module32First = (PVOID)&hModule32First;
	*(PVOID*)Exported.Module32Next = (PVOID)&hModule32Next;
}

void unhookFunctions() {
	*(PVOID*)Exported.OpenProcess = pOpenProcess;
	Exported.IsWow64Process = pIsWow64Process;
	*(CEP_READPROCESSMEMORY*)Exported.ReadProcessMemory = pReadProcessMemory;
	*(PVOID*)Exported.WriteProcessMemory = pWriteProcessMemory;
	*(PVOID*)Exported.VirtualQueryEx = pVirtualQueryEx;

	*(PVOID*)Exported.CreateToolhelp32Snapshot = pCreateToolhelp32Snapshot;
	*(PVOID*)Exported.Process32First = pProcess32First;
	*(PVOID*)Exported.Process32Next = pProcess32Next;
	*(PVOID*)Exported.Module32First = pModule32First;
	*(PVOID*)Exported.Module32Next = pModule32Next;
}
BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef , int pluginid)
{
	MAINMENUPLUGIN_INIT init0;
	
	hVMM = VMMDLL_Initialize(3, (LPSTR[]) { "", "-device", "fpga" });

	selfid=pluginid;

	//copy the EF list to Exported
	Exported = *ef; //Exported is defined in the .h
	if (Exported.sizeofExportedFunctions != sizeof(Exported))
		return FALSE;

	hookFunctions();


	init0.name="DMAPlugin -- kaijia2024";
	init0.callbackroutine=mainmenuplugin;

	MainMenuPluginID=Exported.RegisterFunction(pluginid, ptMainMenu, &init0); //adds a plugin menu item to the memory view
	if ( MainMenuPluginID == -1 )
	{
		Exported.ShowMessage("Failure to register DMAPlugin");
		return FALSE;
	}	

	Exported.ShowMessage("The \"DMA\" plugin got enabled");
	
	return TRUE;
}


BOOL __stdcall CEPlugin_DisablePlugin(void)
{

	VMMDLL_MemFree(pProcInfoAll);
	VMMDLL_MemFree(pmModule);
	VMMDLL_MemFree(pVadMap);
	pProcInfoAll = NULL;
	pmModule = NULL;
	pVadMap = NULL;

	VMMDLL_Close(hVMM);
	hVMM = NULL;

	unhookFunctions();

	//clean up memory you might have allocated
	MessageBoxA(0, "disabled plugin", "DMAPlugin", MB_OK);

	return TRUE;
}
