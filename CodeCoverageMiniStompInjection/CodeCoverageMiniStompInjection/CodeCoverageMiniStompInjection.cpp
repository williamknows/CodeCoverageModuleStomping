#include <algorithm>
#include <iostream>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <psapi.h> 
#include <tlhelp32.h>
#include <chrono>
#include <thread>

#pragma comment(lib, "mincore.lib")

#include "CodeCoverageMiniStompInjection.h"

DWORD FindProcessByPID(const std::wstring& processName)
{
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processesSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	Process32First(processesSnapshot, &processInfo);
	if (!processName.compare(processInfo.szExeFile))
	{
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
		{
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}

void KillProcessByName(const std::wstring& processName)
{
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processesSnapshot == INVALID_HANDLE_VALUE)
		return;

	Process32First(processesSnapshot, &processInfo);
	if (!processName.compare(processInfo.szExeFile))
	{
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0,
			processInfo.th32ProcessID);
		if (hProcess != NULL)
		{
			TerminateProcess(hProcess, 9);
			CloseHandle(hProcess);
		}
	}

	while (Process32Next(processesSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
		{
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0,
				processInfo.th32ProcessID);
			if (hProcess != NULL)
			{
				TerminateProcess(hProcess, 9);
				CloseHandle(hProcess);
			}
		}
	}

	CloseHandle(processesSnapshot);
}

DWORD SetUpTargetProcess(std::wstring targetProcessName)
{
	// Kill existing notepad processes
	KillProcessByName(targetProcessName);

	// Start a new notepad process
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	// Start the child process. 
	if (!CreateProcess(NULL,   // No module name (use command line)
		(LPWSTR)targetProcessName.c_str(),        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return 0;
	}
	std::this_thread::sleep_for(std::chrono::seconds(2)); // sleep to give process time to actually start

	// Find PID of new notepad process
	DWORD targetPID = FindProcessByPID(targetProcessName);
	return targetPID;
}

int InjectIntoModule(DWORD processID, std::wstring moduleTarget, DWORD offsetInMemory)
{
	std::cout << std::endl;
	//std::cout << "Process launched. Attach debugger now if required. Proceed with injection?" << std::endl;
	//system("pause");

	HMODULE hMods[1024];
	HANDLE hProcess;
	DWORD cbNeeded;
	unsigned int i;

	// Print the process identifier.

	std::cout << "Process ID: " << processID << std::endl;

	// Get a handle to the process.

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
	if (NULL == hProcess)
	{
		std::cout << "Could not open handle to process." << std::endl;
		return 1;
	}
	std::cout << "Handle to external process obtained." << std::endl;

	// Get a list of all the modules in this process.
	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			TCHAR szModName[MAX_PATH];

			// Get the full path to the module's file.

			if (GetModuleFileNameEx(hProcess, hMods[i], szModName,
				sizeof(szModName) / sizeof(TCHAR)))
			{
				std::wstring moduleCurrent = std::wstring(szModName);
				std::transform(moduleCurrent.begin(), moduleCurrent.end(), moduleCurrent.begin(), std::tolower);

				if (wcsstr(moduleCurrent.c_str(), moduleTarget.c_str()) != 0)
				{
					std::wcout << "Target module found: " << szModName << " is at " << hMods[i] << std::endl;

					PVOID remoteBuffer;// = hMods[i];
					remoteBuffer = (int*)((char*)hMods[i] + offsetInMemory); //offsetInMemory
					//remoteBuffer = VirtualAllocEx(hProcess, NULL, sizeof shellcode, (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);

					// make memory section writable
					std::cout << "Modifying module memory to be writable." << std::endl;
					DWORD oldProtect;
					DWORD virtualProtectRWE = VirtualProtectEx(hProcess, remoteBuffer, sizeof shellcode, PAGE_EXECUTE_WRITECOPY, &oldProtect);
					if (virtualProtectRWE == 0)
					{
						std::cout << "Error when making memory RWE: " << GetLastError() << std::endl;
					}

					//system("pause");

					MODULEINFO currentModule;
					ZeroMemory(&currentModule, sizeof(currentModule));
					GetModuleInformation(hProcess, hMods[i], &currentModule, sizeof currentModule);

					std::cout << "Setting CFG call targets." << std::endl;
					for (unsigned int n = 0; n < currentModule.SizeOfImage; n += 16)
					{
						CFG_CALL_TARGET_INFO offsetInfo;
						offsetInfo.Flags = CFG_CALL_TARGET_VALID;
						offsetInfo.Offset = n;
						if (!SetProcessValidCallTargets(hProcess, (void*)currentModule.lpBaseOfDll, currentModule.SizeOfImage, 1, &offsetInfo))
						{
							std::cout << "Error when calling SetProcessValidCallTargets: " << GetLastError() << std::endl;
						}
					}

					// write data
					std::cout << "Writing shellcode to external process. Total bytes: " << (sizeof shellcode) << std::endl;
					DWORD writeProcessMemory = WriteProcessMemory(hProcess, remoteBuffer, shellcode, sizeof shellcode, NULL);
					if (writeProcessMemory == 0)
					{
						std::cout << "Error when writing to external process' memory: " << GetLastError() << std::endl;
					}

					//system("pause");

					std::cout << "Creating thread in external process." << std::endl;
					HANDLE rThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)remoteBuffer, NULL, 0, NULL);
					if (rThread == NULL)
					{
						std::cout << "Error when creating remote thread: " << GetLastError() << std::endl;
					}

					//system("pause");

					std::cout << "Modifying module memory to be non-writable." << std::endl;
					VirtualProtectEx(hProcess, hMods[i], sizeof shellcode, PAGE_EXECUTE_READ, &oldProtect);
				}
			}
		}
	}

	// Release the handle to the process.

	CloseHandle(hProcess);

	return 0;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc != 4)
	{
		std::cout << "DLLCoverage.exe <program.exe> <module.dll> <offsetBytes>" << std::endl;
		return 1;
	}

	// Set up target process
	DWORD targetPID = SetUpTargetProcess(argv[1]);

	// 1==moduleName, 2=offset, 3==freeSpace
	InjectIntoModule(targetPID, argv[2], _wtoi(argv[3]));

	return 0;
}