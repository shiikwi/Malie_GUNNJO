#include <iostream>
#include <Windows.h>
#include <Psapi.h>
#include <thread>
#define BaseImage 0x400000

STARTUPINFOW si;
PROCESS_INFORMATION pi;

HMODULE WaitModule()
{
	DWORD start = GetTickCount();
	while (GetTickCount() - start < 5000) {
		HMODULE hMods[1024];
		DWORD cbNeeded;
		if (EnumProcessModules(pi.hProcess, hMods, sizeof(hMods), &cbNeeded)) {
			for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
				char szModName[MAX_PATH];
				if (GetModuleBaseNameA(pi.hProcess, hMods[i], szModName, sizeof(szModName)) &&
					_stricmp(szModName, "tools.dll") == 0) {
					return hMods[i];
				}
			}
		}
		Sleep(50);
	}
	return NULL;
}

void InitilizePatch() {

	HMODULE hMoule = WaitModule();
	if (hMoule == NULL) {
		MessageBoxW(NULL, L"Not Found tools.dll", L"Error", NULL);
		return;
	}

	//00F37260 | 8078 34 80        | cmp byte ptr ds:[eax+34],80                 |
	DWORD oldPortect;
	const DWORD cmp_RVA = 0x7260;
	byte pbytes = 0x86;
	DWORD Patch_cmp_Address = (DWORD)hMoule + cmp_RVA;

	VirtualProtectEx(pi.hProcess, (LPVOID)(Patch_cmp_Address + 3), 1, PAGE_READWRITE, &oldPortect);
	WriteProcessMemory(pi.hProcess, (void*)(Patch_cmp_Address + 3), &pbytes, 1, nullptr);
	VirtualProtectEx(pi.hProcess, (LPVOID)(Patch_cmp_Address + 3), 1, oldPortect, &oldPortect);

	//0041CD9A | 68 6C304900       | push malie.49306C                           |
	byte sjisfont[] = { 0x82,0x6C,0x82,0x72,0x20,0x83,0x53,0x83,0x56,0x83,0x62,0x83,0x4E };
	byte gbkfont[] = { 0xBA,0xDA,0xCC,0xE5,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
	const DWORD font_RVA = 0x9306C;
	DWORD Font_Address = (DWORD)BaseImage + font_RVA;
	VirtualProtectEx(pi.hProcess, (LPVOID)(Font_Address), sizeof(sjisfont), PAGE_READWRITE, &oldPortect);
	WriteProcessMemory(pi.hProcess, (void*)Font_Address, &gbkfont, sizeof(gbkfont), nullptr);
	VirtualProtectEx(pi.hProcess, (LPVOID(Font_Address)), sizeof(sjisfont), oldPortect, &oldPortect);
}



void Font_Pacth()
{
	//0042AE06 | C64424 23 80 | mov byte ptr ss : [esp + 23] , 80 |
	DWORD oldPortect;
	const DWORD cmp_RVA = 0X2AE06;
	byte pbytes = 0x86;
	DWORD Patch_cmp_Address = (DWORD)BaseImage + cmp_RVA;

	VirtualProtectEx(pi.hProcess, (LPVOID)(Patch_cmp_Address + 4), 1, PAGE_READWRITE, &oldPortect);
	WriteProcessMemory(pi.hProcess, (void*)(Patch_cmp_Address + 4), &pbytes, 1, nullptr);
	VirtualProtectEx(pi.hProcess, (LPVOID)(Patch_cmp_Address + 4), 1, oldPortect, &oldPortect);

	//0043E737 | C64424 2B 80      | mov byte ptr ss:[esp+2B],80                 |
	const DWORD font_RVA = 0x3E737;
	DWORD Patch_font_Address = (DWORD)BaseImage + font_RVA;

	VirtualProtectEx(pi.hProcess, (LPVOID)(Patch_font_Address + 4), 1, PAGE_READWRITE, &oldPortect);
	WriteProcessMemory(pi.hProcess, (void*)(Patch_font_Address + 4), &pbytes, 1, nullptr);
	VirtualProtectEx(pi.hProcess, (LPVOID)(Patch_font_Address + 4), 1, oldPortect, &oldPortect);
}

void PatchThread()
{
	InitilizePatch();
	Font_Pacth();
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcess(L"malie.exe", NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
		MessageBox(NULL, L"PATCH ERROR", L"ERROR", NULL);
		return 1;
	}

	std::thread patcher(PatchThread);
	ResumeThread(pi.hThread);
	patcher.join();

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 0;
}
