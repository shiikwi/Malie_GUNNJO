#include <iostream>
#include <Windows.h>
#define TARGET_ADDRESS 0x7782A0

/*
0042AEC0 | A1 A0827700       | mov eax,dword ptr ds:[7782A0]
0042AEC5 | C3                | ret
*/
const DWORD Patch_Address_RVA = 0x2AEC0;
STARTUPINFOW si;
PROCESS_INFORMATION pi;

_declspec(naked) void Patchasm() {
	_asm {
		mov eax, 1
		mov dword ptr ds : [TARGET_ADDRESS] , eax
		ret
	}
}
_declspec(naked) void Patchasm_End() {}

void Patch() {
	DWORD oldPortect;
	DWORD Patch_Address = 0x400000 + Patch_Address_RVA;

	auto pbytes = (void*)Patchasm;
	auto pszie = (size_t)Patchasm_End - (size_t)Patchasm;

	VirtualProtectEx(pi.hProcess, (LPVOID)Patch_Address, pszie, PAGE_READWRITE, &oldPortect);
	
	WriteProcessMemory(pi.hProcess, (LPVOID)Patch_Address, pbytes, pszie, NULL);

	VirtualProtectEx(pi.hProcess, (LPVOID)Patch_Address, pszie, oldPortect, &oldPortect);
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

	Patch();

	ResumeThread(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 0;
}
