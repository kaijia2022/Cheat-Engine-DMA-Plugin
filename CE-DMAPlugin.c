// example-c.cpp : Defines the entry point for the DLL application.
//
#define _CRT_SECURE_NO_WARNINGS
//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:


#include <windows.h>
#include <stdio.h>
#include "cepluginsdk.h"
#include "memory.h"

int selfid;
int memorybrowserpluginid=-1; //initialize it to -1 to indicate failure (used by the DisablePlugin routine)
int addresslistPluginID=-1;
int debugpluginID=-1;
int ProcesswatchpluginID=-1;
int PointerReassignmentPluginID=-1;
int MainMenuPluginID=-1;

ExportedFunctions Exported;



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


BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef , int pluginid)
{
	MAINMENUPLUGIN_INIT init0;
	
	hVMM = VMMDLL_Initialize(3, (LPSTR[]) { "", "-device", "fpga" });

	selfid=pluginid;

	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);

	//copy the EF list to Exported
	Exported=*ef; //Exported is defined in the .h
	if (Exported.sizeofExportedFunctions!=sizeof(Exported))
		return FALSE;

	//auto pOpenedProcessID = ef->OpenedProcessID;
	//auto pOpenedProcessHandle = ef->OpenedProcessHandle;

	PVOID pOpenProcess= ef->OpenProcess;
	CEP_READPROCESSMEMORY pReadProcessMemory = ef->ReadProcessMemory;
	PVOID pWriteProcessMemory = ef->WriteProcessMemory;
	PVOID pVirtualQueryEx = ef->VirtualQueryEx;

	PVOID pCreateToolhelp32Snapshot = ef->CreateToolhelp32Snapshot;
	PVOID pProcess32First = ef->Process32First;
	PVOID pProcess32Next = ef->Process32Next;
	PVOID pModule32First = ef->Module32First;
	PVOID pModule32Next = ef->Module32Next;


	//*(PULONG)pOpenedProcessHandle = (ULONG)*pOpenedProcessID;
	
	*(uintptr_t*)pOpenProcess = (uintptr_t)&hOpenProcess;
	*(uintptr_t*)pReadProcessMemory = (uintptr_t)&hReadProcessMemory;
	*(uintptr_t*)pWriteProcessMemory = (uintptr_t)&hWriteProcessMemory;
	*(uintptr_t*)pVirtualQueryEx = (uintptr_t) &hVirtualQueryEx;

	*(uintptr_t*)pCreateToolhelp32Snapshot = (uintptr_t)&hCreateToolhelp32Snapshot;
	*(uintptr_t*)pProcess32First = (uintptr_t)&hProcess32First;
	*(uintptr_t*)pProcess32Next = (uintptr_t)&hProcess32Next;
	*(uintptr_t*)pModule32First = (uintptr_t)&hModule32First;
	*(uintptr_t*)pModule32Next = (uintptr_t)&hModule32Next;
	


	init0.name="DMAPlugin -- kaijia2024";
	init0.callbackroutine=mainmenuplugin;
	//init0.shortcut="Ctrl+R";
	MainMenuPluginID=Exported.RegisterFunction(pluginid, ptMainMenu, &init0); //adds a plugin menu item to the memory view
	if ( MainMenuPluginID == -1 )
	{
		Exported.ShowMessage("Failure to register DMAPlugin");
		return FALSE;
	}	

	//lua_State *lua_state=ef->GetLuaState();

	//lua_register(lua_state, "DMAPlugin -- kaijia2024", lua_pluginExample);

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
	
	//clean up memory you might have allocated
	MessageBoxA(0, "disabled plugin", "DMAPlugin", MB_OK);

	return TRUE;
}

