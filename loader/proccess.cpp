#include "proccess.h"
#include <TlHelp32.h>
#include <exception>
#include <fstream>

DWORD proc::FindProccess(const std::string& procName)
{
	PROCESSENTRY32 pe = { 0 };
	pe.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	BOOL haveMore = Process32First(snapshot, &pe);
	while (haveMore)
	{
		if (pe.szExeFile == procName)
			return pe.th32ProcessID;

		haveMore = Process32Next(snapshot, &pe);
	}

	return NULL;
}

DWORD proc::InjectDLL(const std::string& fileName, DWORD pid)
{
	HANDLE proccess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (proccess == INVALID_HANDLE_VALUE || proccess == nullptr)
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't OpenProcess").c_str());

	DWORD bufferSize = fileName.size() + 1;
	LPVOID buffer = VirtualAllocEx(proccess, NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
	if (buffer == nullptr)
	{
		CloseHandle(proccess);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't VirtualAllocEx").c_str());
	}

	if (!WriteProcessMemory(proccess, buffer, fileName.c_str(), bufferSize, nullptr))
	{
		VirtualFreeEx(proccess, buffer, bufferSize, MEM_FREE);
		CloseHandle(proccess);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't WriteProcessMemory").c_str());
	}

	HMODULE module = GetModuleHandleA(u8"kernel32.dll");
	if (module == nullptr)
	{
		VirtualFreeEx(proccess, buffer, bufferSize, MEM_FREE);
		CloseHandle(proccess);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't GetModuleHandleA").c_str());
	}

	LPTHREAD_START_ROUTINE fnLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
		GetProcAddress(module, u8"LoadLibraryA"));

	if (fnLoadLibrary == nullptr)
	{
		VirtualFreeEx(proccess, buffer, bufferSize, MEM_FREE);
		CloseHandle(proccess);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't GetProcAddress").c_str());
	}

	HANDLE thread = CreateRemoteThread(proccess, NULL, 0, fnLoadLibrary, buffer, 0, NULL);
	if (thread == nullptr)
	{
		VirtualFreeEx(proccess, buffer, bufferSize, MEM_FREE);
		CloseHandle(proccess);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't CreateRemoteThread").c_str());
	}

	WaitForSingleObject(thread, INFINITE);

	DWORD result = NULL;
	GetExitCodeThread(thread, &result);

	CloseHandle(thread);
	VirtualFreeEx(proccess, buffer, bufferSize, MEM_FREE);
	CloseHandle(proccess);

	return result;
}

std::string proc::GetErrorMessage(DWORD code, const std::string& defaults)
{
	char* message = nullptr;
	DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&message), 0, nullptr);

	if (size == 0)
		return defaults + ":" + std::to_string(code);

	std::string result = message;
	LocalFree(message);
	return result + ":" + std::to_string(code);
}

std::string proc::GetCurrentPath(const std::string& selfPath)
{
	return selfPath.substr(0, selfPath.rfind("\\"));
}

DWORD proc::GetModuleBase(const std::string& moduleName, DWORD pid)
{
	MODULEENTRY32 lpModuleEntry = { 0 };
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if (!hSnapShot)
		return NULL;

	lpModuleEntry.dwSize = sizeof(lpModuleEntry);
	BOOL __RunModule = Module32First(hSnapShot, &lpModuleEntry);
	while (__RunModule)
	{
		if (lpModuleEntry.szModule == moduleName)
		{
			CloseHandle(hSnapShot);
			return (DWORD)lpModuleEntry.modBaseAddr;
		}
		__RunModule = Module32Next(hSnapShot, &lpModuleEntry);
	}
	CloseHandle(hSnapShot);
	return NULL;
}

proc::DWORDX __stdcall proc::MemLoadLibrary2(PARAMX* X)
{
	LPCVOID lpFileData = X->lpFileData;
	DWORDX DataLength = X->DataLength;

	/****************��ʼ�����ú���********************/
	LdrGetProcedureAddressT LdrGetProcedureAddress = (X->LdrGetProcedureAddress);

	NtAllocateVirtualMemoryT pNtAllocateVirtualMemory = (X->dwNtAllocateVirtualMemory);
	LdrLoadDllT pLdrLoadDll = (X->pLdrLoadDll);
	RtlInitAnsiStringT RtlInitAnsiString = X->RtlInitAnsiString;
	RtlAnsiStringToUnicodeStringT RtlAnsiStringToUnicodeString = X->RtlAnsiStringToUnicodeString;
	RtlFreeUnicodeStringT RtlFreeUnicodeString = X->RtlFreeUnicodeString;

	ProcDllMain pDllMain = NULL;
	void* pMemoryAddress = NULL;



	ANSI_STRING ansiStr;
	UNICODE_STRING UnicodeString;
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS pNTHeader;
	PIMAGE_SECTION_HEADER pSectionHeader;
	int ImageSize = 0;

	int nAlign = 0;
	int i = 0;


	//���������Ч�ԣ�����ʼ��
	/*********************CheckDataValide**************************************/
	//	PIMAGE_DOS_HEADER pDosHeader;
	//��鳤��
	if (DataLength > sizeof(IMAGE_DOS_HEADER))
	{
		pDosHeader = (PIMAGE_DOS_HEADER)lpFileData; // DOSͷ
		//���dosͷ�ı��
		if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) goto CODEEXIT; //0��5A4D : MZ

		//��鳤��
		if ((DWORDX)DataLength < (pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS))) goto CODEEXIT;
		//ȡ��peͷ
		pNTHeader = (PIMAGE_NT_HEADERS)((DWORDX)lpFileData + pDosHeader->e_lfanew); // PEͷ
		//���peͷ�ĺϷ���
		if (pNTHeader->Signature != IMAGE_NT_SIGNATURE) goto CODEEXIT; //0��00004550: PE00
		if ((pNTHeader->FileHeader.Characteristics & IMAGE_FILE_DLL) == 0) //0��2000: File is a DLL
			goto CODEEXIT;
		if ((pNTHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) //0��0002: ָ���ļ���������
			goto CODEEXIT;
		if (pNTHeader->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER))
			goto CODEEXIT;


		//ȡ�ýڱ��α�
		pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORDX)pNTHeader + sizeof(IMAGE_NT_HEADERS));
		//��֤ÿ���ڱ�Ŀռ�
		for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
		{
			if ((pSectionHeader[i].PointerToRawData + pSectionHeader[i].SizeOfRawData) > (DWORD)DataLength) goto CODEEXIT;
		}

		/**********************************************************************/
		nAlign = pNTHeader->OptionalHeader.SectionAlignment; //�ζ����ֽ���

		//ImageSize = pNTHeader->OptionalHeader.SizeOfImage;
		//// ��������ͷ�ĳߴ硣����dos, coff, peͷ �� �α�Ĵ�С
		ImageSize = (pNTHeader->OptionalHeader.SizeOfHeaders + nAlign - 1) / nAlign * nAlign;
		// �������нڵĴ�С
		for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i)
		{
			//�õ��ýڵĴ�С
			int CodeSize = pSectionHeader[i].Misc.VirtualSize;
			int LoadSize = pSectionHeader[i].SizeOfRawData;
			int MaxSize = (LoadSize > CodeSize) ? (LoadSize) : (CodeSize);

			int SectionSize = (pSectionHeader[i].VirtualAddress + MaxSize + nAlign - 1) / nAlign * nAlign;
			if (ImageSize < SectionSize)
				ImageSize = SectionSize; //Use the Max;
		}
		if (ImageSize == 0) goto CODEEXIT;

		// ���������ڴ�
		SIZE_T uSize = ImageSize;
		pNtAllocateVirtualMemory((HANDLE)-1, &pMemoryAddress, 0, &uSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

		if (pMemoryAddress != NULL)
		{

			// ������Ҫ���Ƶ�PEͷ+�α��ֽ���
			int HeaderSize = pNTHeader->OptionalHeader.SizeOfHeaders;
			int SectionSize = pNTHeader->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
			int MoveSize = HeaderSize + SectionSize;
			//����ͷ�Ͷ���Ϣ
			for (i = 0; i < MoveSize; i++)
			{
				*((PCHAR)pMemoryAddress + i) = *((PCHAR)lpFileData + i);
			}
			//memmove(pMemoryAddress, lpFileData, MoveSize);//Ϊ������һ��API,ֱ��������ĵ��ֽڸ������ݾ�����

			//����ÿ����
			for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i)
			{
				if (pSectionHeader[i].VirtualAddress == 0 || pSectionHeader[i].SizeOfRawData == 0)continue;
				// ��λ�ý����ڴ��е�λ��
				void* pSectionAddress = (void*)((DWORDX)pMemoryAddress + pSectionHeader[i].VirtualAddress);
				// ���ƶ����ݵ������ڴ�
			//	memmove((void *)pSectionAddress,(void *)((DWORDX)lpFileData + pSectionHeader[i].PointerToRawData),	pSectionHeader[i].SizeOfRawData);
				//Ϊ������һ��API,ֱ��������ĵ��ֽڸ������ݾ�����
				for (size_t k = 0; k < pSectionHeader[i].SizeOfRawData; k++)
				{
					*((PCHAR)pSectionAddress + k) = *((PCHAR)lpFileData + pSectionHeader[i].PointerToRawData + k);
				}
			}
			/*******************�ض�λ��Ϣ****************************************************/

			if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress > 0
				&& pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0)
			{

				DWORDX Delta = (DWORDX)pMemoryAddress - pNTHeader->OptionalHeader.ImageBase;
				DWORDX* pAddress;
				//ע���ض�λ���λ�ÿ��ܺ�Ӳ���ļ��е�ƫ�Ƶ�ַ��ͬ��Ӧ��ʹ�ü��غ�ĵ�ַ
				PIMAGE_BASE_RELOCATION pLoc = (PIMAGE_BASE_RELOCATION)((DWORDX)pMemoryAddress
					+ pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
				while ((pLoc->VirtualAddress + pLoc->SizeOfBlock) != 0) //��ʼɨ���ض�λ��
				{
					WORD* pLocData = (WORD*)((DWORDX)pLoc + sizeof(IMAGE_BASE_RELOCATION));
					//���㱾����Ҫ�������ض�λ���ַ������Ŀ
					int NumberOfReloc = (pLoc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
					for (i = 0; i < NumberOfReloc; i++)
					{
						if ((DWORDX)(pLocData[i] & 0xF000) == 0x00003000 || (DWORDX)(pLocData[i] & 0xF000) == 0x0000A000) //����һ����Ҫ�����ĵ�ַ
						{
							// ������
							// pLoc->VirtualAddress = 0��1000;
							// pLocData[i] = 0��313E; ��ʾ����ƫ�Ƶ�ַ0��13E����Ҫ����
							// ��� pAddress = ����ַ + 0��113E
							// ����������� A1 ( 0c d4 02 10) �������ǣ� mov eax , [1002d40c]
							// ��Ҫ����1002d40c�����ַ
							pAddress = (DWORDX*)((DWORDX)pMemoryAddress + pLoc->VirtualAddress + (pLocData[i] & 0x0FFF));
							*pAddress += Delta;
						}
					}
					//ת�Ƶ���һ���ڽ��д���
					pLoc = (PIMAGE_BASE_RELOCATION)((DWORDX)pLoc + pLoc->SizeOfBlock);
				}
				/***********************************************************************/
			}

			/******************* ���������ַ��**************/
			DWORDX Offset = pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
			if (Offset == 0)
				goto CODEEXIT; //No Import Table

			PIMAGE_IMPORT_DESCRIPTOR pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORDX)pMemoryAddress + Offset);

			PIMAGE_IMPORT_BY_NAME pByName = NULL;
			while (pID->Characteristics != 0)
			{
				PIMAGE_THUNK_DATA pRealIAT = (PIMAGE_THUNK_DATA)((DWORDX)pMemoryAddress + pID->FirstThunk);
				PIMAGE_THUNK_DATA pOriginalIAT = (PIMAGE_THUNK_DATA)((DWORDX)pMemoryAddress + pID->OriginalFirstThunk);
				//��ȡdll������
				char* pName = (char*)((DWORDX)pMemoryAddress + pID->Name);
				HANDLE hDll = 0;

				RtlInitAnsiString(&ansiStr, pName);

				RtlAnsiStringToUnicodeString(&UnicodeString, &ansiStr, true);
				pLdrLoadDll(NULL, NULL, &UnicodeString, &hDll);
				RtlFreeUnicodeString(&UnicodeString);

				if (hDll == NULL) {

					goto CODEEXIT; //NOT FOUND DLL
				}

				//��ȡDLL��ÿ�����������ĵ�ַ������IAT
				//ÿ��IAT�ṹ�� ��
				// union { PBYTE ForwarderString;
				// PDWORDX Function;
				// DWORDX Ordinal;
				// PIMAGE_IMPORT_BY_NAME AddressOfData;
				// } u1;
				// ������һ��DWORDX ����������һ����ַ��
				for (i = 0; ; i++)
				{
					if (pOriginalIAT[i].u1.Function == 0)break;
					FARPROC lpFunction = NULL;
					if (IMAGE_SNAP_BY_ORDINAL(pOriginalIAT[i].u1.Ordinal)) //�����ֵ�������ǵ������
					{
						if (IMAGE_ORDINAL(pOriginalIAT[i].u1.Ordinal))
						{

							LdrGetProcedureAddress(hDll, NULL, IMAGE_ORDINAL(pOriginalIAT[i].u1.Ordinal), &lpFunction);
						}
					}
					else//�������ֵ���
					{
						//��ȡ��IAT���������ĺ�������
						pByName = (PIMAGE_IMPORT_BY_NAME)((DWORDX)pMemoryAddress + (DWORDX)(pOriginalIAT[i].u1.AddressOfData));
						if ((char*)pByName->Name)
						{
							RtlInitAnsiString(&ansiStr, (char*)pByName->Name);
							LdrGetProcedureAddress(hDll, &ansiStr, 0, &lpFunction);

						}

					}

					//���***********

					if (lpFunction != NULL) //�ҵ��ˣ�
						pRealIAT[i].u1.Function = (DWORDX)lpFunction;
					else
						goto CODEEXIT;
				}

				//move to next
				pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORDX)pID + sizeof(IMAGE_IMPORT_DESCRIPTOR));
			}

			/***********************************************************/
			//��������ַ
			pNTHeader->OptionalHeader.ImageBase = (DWORDX)pMemoryAddress;

			//NtProtectVirtualMemory((HANDLE)-1, &pMemoryAddress, (PSIZE_T)&ImageSize, PAGE_EXECUTE_READ, &oldProtect);
			pDllMain = (ProcDllMain)(pNTHeader->OptionalHeader.AddressOfEntryPoint + (DWORDX)pMemoryAddress);

			pDllMain(0, DLL_PROCESS_ATTACH, pMemoryAddress);//����Ĳ���1����Ӧ�ô�����(HMODULE)pMemoryAddress,����û��Ҫ,��Ϊ�޷�ʹ����Դ,����û��Ҫ,Ҫʹ����Դ,��̳��������˵�����ʹ��

		}
	}

CODEEXIT:

	return (DWORDX)pMemoryAddress;
}

DWORD proc::InjectDLLEx(const std::string& fileName, DWORD pid)
{
	PARAMX params;
	ZeroMemory(&params, sizeof(params));

	HMODULE module = GetModuleHandleA("ntdll");
	params.LdrGetProcedureAddress = reinterpret_cast<LdrGetProcedureAddressT>(GetProcAddress(module, "LdrGetProcedureAddress"));
	params.dwNtAllocateVirtualMemory = reinterpret_cast<NtAllocateVirtualMemoryT>(GetProcAddress(module, "NtAllocateVirtualMemory"));
	params.pLdrLoadDll = reinterpret_cast<LdrLoadDllT>(GetProcAddress(module, "LdrLoadDll"));
	params.RtlInitAnsiString = reinterpret_cast<RtlInitAnsiStringT>(GetProcAddress(module, "RtlInitAnsiString"));
	params.RtlAnsiStringToUnicodeString = reinterpret_cast<RtlAnsiStringToUnicodeStringT>(GetProcAddress(module, "RtlAnsiStringToUnicodeString"));
	params.RtlFreeUnicodeString = reinterpret_cast<RtlFreeUnicodeStringT>(GetProcAddress(module, "RtlFreeUnicodeString"));

	HANDLE proccess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (proccess == INVALID_HANDLE_VALUE)
	{
		delete[] reinterpret_cast<PBYTE>(params.lpFileData);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't OpenProcess").c_str());
	}

	auto [shellCode, shellCodeSize] = GetMemLoadLibrary2ShellCode();
	LPVOID bufShellCode = VirtualAllocEx(proccess, NULL, shellCodeSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (bufShellCode == nullptr)
	{
		CloseHandle(proccess);
		delete[] reinterpret_cast<PBYTE>(params.lpFileData);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't VirtualAllocEx").c_str());
	}

	LPVOID bufShellParam = VirtualAllocEx(proccess, NULL, sizeof(params), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (bufShellParam == nullptr)
	{
		VirtualFreeEx(proccess, bufShellCode, shellCodeSize, MEM_DECOMMIT);
		CloseHandle(proccess);
		delete[] reinterpret_cast<PBYTE>(params.lpFileData);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't VirtualAllocEx").c_str());
	}

	{
		HANDLE file = CreateFileA(fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
		if (file == nullptr)
			throw std::exception(GetErrorMessage(GetLastError(), u8"can't CreateFileA").c_str());

		params.DataLength = GetFileSize(file, NULL);
		params.lpFileData = new BYTE[params.DataLength];
		ReadFile(file, params.lpFileData, params.DataLength, nullptr, NULL);
		CloseHandle(file);
	}

	LPVOID bufDLLCode = VirtualAllocEx(proccess, NULL, params.DataLength, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (bufDLLCode == nullptr)
	{
		VirtualFreeEx(proccess, bufShellCode, shellCodeSize, MEM_FREE);
		VirtualFreeEx(proccess, bufShellParam, sizeof(params), MEM_FREE);
		CloseHandle(proccess);
		delete[] reinterpret_cast<PBYTE>(params.lpFileData);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't VirtualAllocEx").c_str());
	}

	if (!WriteProcessMemory(proccess, bufShellCode, shellCode, shellCodeSize, nullptr) ||
		!WriteProcessMemory(proccess, bufShellParam, &params, sizeof(params), nullptr) ||
		!WriteProcessMemory(proccess, bufDLLCode, params.lpFileData, params.DataLength, nullptr))
	{
		VirtualFreeEx(proccess, bufShellCode, shellCodeSize, MEM_FREE);
		VirtualFreeEx(proccess, bufShellParam, sizeof(params), MEM_FREE);
		VirtualFreeEx(proccess, bufDLLCode, params.DataLength, MEM_FREE);
		CloseHandle(proccess);
		delete[] reinterpret_cast<PBYTE>(params.lpFileData);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't VirtualAllocEx").c_str());
	}

	delete[] reinterpret_cast<PBYTE>(params.lpFileData);
	params.lpFileData = bufDLLCode;

	HANDLE thread = CreateRemoteThread(proccess, nullptr, 0,
		reinterpret_cast<LPTHREAD_START_ROUTINE>(bufShellCode), bufShellParam,
		0, NULL);
	if (thread == nullptr)
	{
		VirtualFreeEx(proccess, bufShellCode, shellCodeSize, MEM_FREE);
		VirtualFreeEx(proccess, bufShellParam, sizeof(params), MEM_FREE);
		VirtualFreeEx(proccess, bufDLLCode, params.DataLength, MEM_FREE);
		CloseHandle(proccess);
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't CreateRemoteThread").c_str());
	}

	WaitForSingleObject(thread, INFINITE);

	DWORD result = EXIT_SUCCESS;
	GetExitCodeThread(thread, &result);

	CloseHandle(thread);
	VirtualFreeEx(proccess, bufShellCode, shellCodeSize, MEM_FREE);
	VirtualFreeEx(proccess, bufShellParam, sizeof(params), MEM_FREE);
	VirtualFreeEx(proccess, bufDLLCode, params.DataLength, MEM_FREE);
	CloseHandle(proccess);

	return result;
}

std::pair<LPVOID, DWORD> proc::GetMemLoadLibrary2ShellCode()
{
	DWORD size = 0;
	WORD* Memx = (WORD*)MemLoadLibrary2;
	while (*Memx != 0xCCCC)
	{
		Memx++;
		size += 2;
	}

	return { Memx, size };
}

bool proc::EnableDebugPrivilege()
{
	HANDLE token = nullptr;
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token) || token == INVALID_HANDLE_VALUE || token == nullptr)
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't OpenProcessToken").c_str());
	
	TOKEN_PRIVILEGES tp = { 0 };
	tp.PrivilegeCount = 1;

	if(!LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid))
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't LookupPrivilegeValue").c_str());

	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if(!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), nullptr, nullptr))
		throw std::exception(GetErrorMessage(GetLastError(), u8"can't AdjustTokenPrivileges").c_str());

	CloseHandle(token);
	return true;
}
