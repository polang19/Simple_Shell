#include "pch.h"
#include <iostream>
#include <windows.h>
#include <winternl.h>

#include "E://Vs_Work_space//EnCode//Encode//Share.h"
//ע��tls

#ifdef _WIN64
#pragma comment(linker,"/INCLUDE:_tls_used")
#else
#pragma comment(linker,"/INCLUDE:__tls_used")
#endif // _WIN64

//�����ݶ���������
#pragma comment(linker,"/merge:.data=.text")
//��ֻ�����ݶ���������
#pragma comment(linker,"/merge:.rdata=.text")
//���ô����Ϊ�ɶ���д��ִ��
#pragma comment(linker,"/section:.text,RWE")

extern "C" __declspec(dllexport) StubConf g_Sc = { 0 };

typedef FARPROC(WINAPI*FuGetProcAddress)(
	_In_ HMODULE hModule,
	_In_ LPCSTR lpProcName
);
typedef HMODULE(WINAPI*FuLoadLibraryExA)(
	_In_ LPCSTR lpLibFileName,
	 HANDLE hFile,
	_In_ DWORD dwFlags
);
typedef int(WINAPI*FuMessageBoxW)(
	_In_opt_ HWND hWnd,
	_In_opt_ LPCWSTR lpText,
	_In_opt_ LPCWSTR lpCaption,
	_In_ UINT uType);
typedef HMODULE (WINAPI* FuGetCurrentProcess)();
typedef BOOL(WINAPI *FuVirtualProtect)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef HMODULE(WINAPI *FuGetModuleHandleW)(_In_opt_ LPCTSTR lpModuleName);
typedef BOOL (WINAPI *FuCheckRemoteDebuggerPresent)(
      HANDLE hProcess,
   PBOOL  pbDebuggerPresent
);
typedef void (* FuExitProcess)(
   UINT uExitCode
);

HMODULE g_hKernel32 = 0;
HMODULE g_hUser32 = 0;
HMODULE g_hModule = 0;

FuGetProcAddress MyGetProcAddress = 0;
FuLoadLibraryExA MyLoadLibraryExA = 0;
FuMessageBoxW MyMessageBoxW = 0;
FuVirtualProtect   MyVirtualProtect = 0;
FuGetModuleHandleW MyGetModuleHandleW = 0;
FuGetCurrentProcess MyGetCurrentProcess = 0;
FuCheckRemoteDebuggerPresent MyCheckRemoteDebuggerPresent = 0;
FuExitProcess MyExitProcess = 0;



int a = 1;
int b = 1;

//��ȡ�ں�ģ���ַ
void GetKernel()
{
	__asm 
	{
		push esi;
		mov esi, fs:[0x30];   //�õ�PEB��ַ
		mov esi, [esi + 0xc]; //ָ��PEB_LDR_DATA�ṹ���׵�ַ
		mov esi, [esi + 0x1c];//һ��˫������ĵ�ַ
		mov esi, [esi];       //�õ���2����ĿkernelBase������
		mov esi, [esi];       //�õ���3����Ŀkernel32������(win10ϵͳ)
		mov esi, [esi + 0x8]; //kernel32.dll��ַ
		mov g_hKernel32, esi;
		pop esi;
	}
}

//��ȡGetProcAddress������ַ
void MyGetFunAddress()
{	
	__asm 
	{
		pushad;		
		mov ebp, esp;
		sub esp, 0xc;
		mov edx, g_hKernel32;
		mov esi, [edx + 0x3c];     //NTͷ��RVA
		lea esi, [esi + edx];      //NTͷ��VA
		mov esi, [esi + 0x78];     //Export��Rva		
		lea edi, [esi + edx];      //Export��Va
							       
		mov esi, [edi + 0x1c];     //Eat��Rva
		lea esi, [esi + edx];      //Eat��Va
		mov[ebp - 0x4], esi;       //����Eat
							       
		mov esi, [edi + 0x20];     //Ent��Rva
		lea esi, [esi + edx];      //Ent��Va
		mov[ebp - 0x8], esi;       //����Ent
							       
		mov esi, [edi + 0x24];     //Eot��Rva
		lea esi, [esi + edx];      //Eot��Va
		mov[ebp - 0xc], esi;       //����Eot

		xor ecx, ecx;
		jmp _First;
	_Zero:
		inc ecx;
	_First:
		mov esi, [ebp - 0x8];     //Ent��Va
		mov esi, [esi + ecx * 4]; //FunName��Rva

		lea esi, [esi + edx];     //FunName��Va
		cmp dword ptr[esi], 050746547h;// 47657450 726F6341 64647265 7373;
		jne _Zero;                     // �����16������GetProcAddress��
		cmp dword ptr[esi + 4], 041636f72h;
		jne _Zero;
		cmp dword ptr[esi + 8], 065726464h;
		jne _Zero;
		cmp word  ptr[esi + 0ch], 07373h;
		jne _Zero;

		xor ebx,ebx
		mov esi, [ebp - 0xc];     //Eot��Va
		mov bx, [esi + ecx * 2];  //�õ����

		mov esi, [ebp - 0x4];     //Eat��Va
		mov esi, [esi + ebx * 4]; //FunAddr��Rva
		lea eax, [esi + edx];     //FunAddr
		mov MyGetProcAddress, eax;	
		add esp, 0xc;
		popad;
	}
}

//��дstrcmp
int StrCmpText(const char* pStr, char* pBuff)
{
	int nFlag = 1;
	__asm
	{
		mov esi, pStr;
		mov edi, pBuff;
		mov ecx, 0x6;
		cld;
		repe cmpsb;
		je _end;
		mov nFlag, 0;
	_end:
	}
	return nFlag;
}

//����
void Decryption()
{
	//��ȡ.text������ͷ
	auto pNt = GetNtHeader((char*)g_hModule);
	DWORD dwSecNum = pNt->FileHeader.NumberOfSections;
	auto pSec = IMAGE_FIRST_SECTION(pNt);

	//�ҵ���������
	for (size_t i = 0; i < dwSecNum; i++)
	{
		if (StrCmpText(".text", (char*)pSec[i].Name))
		{
			pSec += i;
			break;
		}
	}

	//��ȡ������׵�ַ
	char* pTarText = pSec->VirtualAddress + (char*)g_hModule;
	int nSize = pSec->Misc.VirtualSize;
	DWORD old = 0;
	//���ܴ����
	MyVirtualProtect(pTarText, nSize, PAGE_READWRITE, &old);
	for (int i = 0; i < nSize; ++i) {
		pTarText[i] ^= 0x15;
	}
	MyVirtualProtect(pTarText, nSize, old, &old);
}
 
//���к���
void RunFun()
{
	MyLoadLibraryExA = (FuLoadLibraryExA)MyGetProcAddress(g_hKernel32, "LoadLibraryExA");
	g_hUser32 = MyLoadLibraryExA("user32.dll", 0, 0);

	MyMessageBoxW = (FuMessageBoxW)MyGetProcAddress(g_hUser32, "MessageBoxW");
	MyMessageBoxW(0, L"��Һ�����һ����", L"��ʾ", 0);
	MyGetModuleHandleW = (FuGetModuleHandleW)MyGetProcAddress(g_hKernel32,"GetModuleHandleW");
	g_hModule = (HMODULE)(FuGetModuleHandleW)MyGetModuleHandleW(0);
	MyVirtualProtect = (FuVirtualProtect)MyGetProcAddress(g_hKernel32,"VirtualProtect");
	b = 0;
}

int ExceptFilter()
{	
	b = 1; // �޸�b��ֵΪ1���Է�ֹ����ѭ�����쳣����
	Decryption();
	g_Sc.dwOep += 0x400000;
	__asm jmp g_Sc.dwOep;
	return EXCEPTION_CONTINUE_EXECUTION; // �ڴ������쳣�󣬳������ִ���쳣����λ�õĴ���
}

extern "C" __declspec(dllexport) __declspec(naked)
void Start()
{
	//��ȡkernel32.dll��ַ
	GetKernel();
	__asm {
         call LABEL9;
         _emit 0x83;
     LABEL9:
         add dword ptr ss : [esp] , 8;
         ret;
         __emit 0xF3;
     }

	//��ȡGetProcAddress������ַ
	MyGetFunAddress();
	 __asm
    {
        lea eax, lab1
        jmp eax
            _emit 0x90
    };
lab1:

	RunFun();

	 __asm{
        jz label;
        jnz label;
        _emit 0xe8;   
label:
    }
	_try  {  
        int c = a / b;
    }	 
    _except (ExceptFilter()) {
        
    }
}


