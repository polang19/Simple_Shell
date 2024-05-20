#include"pch.h"

#include<iostream>
#include<cstring>
#include"Share.h"



struct StubInfo{
	HMODULE hModule;//���
	DWORD dwTextRva;//�����RVA��ַ
	DWORD dwTextSize;//����δ�С
	DWORD dwFunAddr;//��ں���
	StubConf* sc;//�������
};
void FixDllReloc(char* hModule,DWORD dwNewBase,DWORD dwNewSecRva);

void Implant(char*& szBuff,int& dwSize);

int fun(char* szPath)
{
	//char szPath[MAX_PATH] = "E://Vs_Work_space//MyHello//Debug//MyHello.exe";
	int len = strlen(szPath);	
	char* szBuff = NULL;
	int nSize = 0;
	szBuff = GetFileData(szPath,&nSize);
	if (szBuff == NULL)
	{
		MessageBox(0,(LPCWSTR)"Error",0,0);
	}
	///*
	//1.��ȡ������׵�ַ
	char* pTarText = GetSecHeaderByName(szBuff, ".text")->PointerToRawData + szBuff;
	//2.��ȡ�����ʵ�ʴ�С
	int size = GetSecHeaderByName(szBuff, ".text")->Misc.VirtualSize;
	for (int i = 0; i < size; ++i)
	{
		pTarText[i] ^= 0x15;
	}
//	*/

	//ֲ��DLL
	Implant(szBuff,nSize);
	//ȥ�������ַ
	GetOptHead(szBuff)->DllCharacteristics &= (~0x40);

	//�����ļ�
	SavePeFile(szBuff,nSize,szPath);	
	MessageBoxW(0, L"�ӿǳɹ�", L"��ʾ", 0);
	return 0;
}


//1.����DLL
StubInfo LoadStub()
{
	StubInfo si = {0};
	//����DLL
	HMODULE hStubDll = LoadLibraryExA("Stub.dll",0,DONT_RESOLVE_DLL_REFERENCES);
	//�ṹ�帳ֵ
	si.hModule = hStubDll;
	//��ȡtext����ʼ��ַ�ʹ�С
	si.dwTextRva = (DWORD)GetSecHeaderByName((char*)hStubDll,".text")->VirtualAddress;
	si.dwTextSize = (DWORD)GetSecHeaderByName((char*)hStubDll,".text")->SizeOfRawData;
	//��ȡStart�ĵ�ַ
	si.dwFunAddr = (DWORD)GetProcAddress(hStubDll,"Start");
	//OEP
	si.sc = (StubConf*)GetProcAddress(hStubDll, "g_Sc");
	return si;
}
//2.���һ������
//pTarBuff:Դ������ļ�ָ��  nSize:Դ�ļ���С
//pSecName:����������       nSecSize:�����δ�С
void AddSection(char*& pTarBuff,int& nSize,
				const char* pSecName,int nSecSize)
{
	//������Ŀ��1
	int n = GetFileHeader(pTarBuff)->NumberOfSections++;
	//��ȡ���һ�����ε���Ϣ
	PIMAGE_SECTION_HEADER pSec = (PIMAGE_SECTION_HEADER)GetLastSecHeader(pTarBuff);
	DWORD dwFileAlig = GetOptHead(pTarBuff)->FileAlignment;
	DWORD dwMemAlig = GetOptHead(pTarBuff)->SectionAlignment;
	//������������Ϣ
	memcpy(pSec->Name,pSecName,8);
	pSec->Misc.VirtualSize = nSecSize;
	pSec->SizeOfRawData = Aligment(nSecSize,dwFileAlig);
	//�ڴ��RVA���ڴ����
	pSec->VirtualAddress = (pSec - 1)->VirtualAddress + 
		Aligment((pSec - 1)->SizeOfRawData,dwMemAlig);
	//�ļ�ƫ�ƣ�����
	pSec->PointerToRawData = Aligment(nSize,dwFileAlig);
	pSec->Characteristics = 0xE00000E0;
	//ӳ���С,�����ڴ����
	GetOptHead(pTarBuff)->SizeOfImage = 
		Aligment(pSec->VirtualAddress + pSec->SizeOfRawData,dwMemAlig);

	//�����ļ���С
	int nNewSize = pSec->SizeOfRawData + pSec->PointerToRawData;
	char* pNewFile = new char[nNewSize];
	memset(pNewFile,0,nNewSize);
	//����Դ��������
	memcpy(pNewFile,pTarBuff,nSize);
	delete[] pTarBuff;
	pTarBuff = pNewFile;
	nSize = nNewSize;
}

void FixDllReloc(char* hModule,DWORD dwNewBase,DWORD dwNewSecRva)
{
	//�ض�λ���ַ
	auto pReloc = (PIMAGE_BASE_RELOCATION)
		(GetOptHead(hModule)->DataDirectory[5].VirtualAddress + hModule);
	//��ȡDLL text�ε�rva
	DWORD dwTextRva = (DWORD)GetSecHeaderByName(hModule,".text")->VirtualAddress;
	while(pReloc->SizeOfBlock)
	{
		struct TypeOffset{
			WORD offset : 12;
			WORD type : 4;
		};
		TypeOffset* pTyOf = (TypeOffset*)(pReloc + 1);
		DWORD dwCount = (pReloc->SizeOfBlock - 8) / 2;
		//�޸��ض�λ
		for(int i = 0;i < dwCount;i++)
		{
			if (pTyOf[i].type != 3)
				continue;
			//Ҫ�޸�Rva
			DWORD dwFixRva = pTyOf[i].offset + pReloc->VirtualAddress;
			//Ҫ�޸��ĵ�ַ
			DWORD* pFixAddr = (DWORD*)(dwFixRva + (DWORD)hModule);

			DWORD dwOld;
			VirtualProtect(pFixAddr,4,PAGE_READWRITE,&dwOld);
			*pFixAddr -= (DWORD)hModule;
			*pFixAddr -= dwTextRva;
			*pFixAddr += dwNewBase;
			*pFixAddr += dwNewSecRva;
			VirtualProtect(pFixAddr,4,dwOld,&dwOld);
		}
		//ָ����һ���ض�λ��
		pReloc = (PIMAGE_BASE_RELOCATION)
			((DWORD)pReloc + pReloc->SizeOfBlock);
	}
}


void Implant(char*& szBuff,int& dwSize)
{
	//����DLL
	StubInfo st = LoadStub();
	//����������
	char NewSecName[] = ".first";
	//������
	AddSection(szBuff,dwSize,NewSecName,st.dwTextSize);

	//�޸��ض�λ
	DWORD dwBase = GetOptHead(szBuff)->ImageBase;
	DWORD dwNewSecRva = GetSecHeaderByName(szBuff,NewSecName)->VirtualAddress;
	FixDllReloc((char*)st.hModule,dwBase,dwNewSecRva);
	//����ԭʼOEP	
	st.sc->dwOep = GetOptHead(szBuff)->AddressOfEntryPoint;
	st.sc->g_module = (HMODULE)szBuff;
	//��ֲdll�еĴ��뵽����������
	char* pNewSecAddr = GetSecHeaderByName(szBuff,NewSecName)->PointerToRawData
			        + szBuff;
	char* pDllTextAddr = st.dwTextRva + (char*)st.hModule;
	memcpy(pNewSecAddr,pDllTextAddr,st.dwTextSize);

	//�����µ�OEP
	GetOptHead(szBuff)->AddressOfEntryPoint = st.dwFunAddr - (DWORD)st.hModule - 
		st.dwTextRva + GetSecHeaderByName(szBuff,NewSecName)->VirtualAddress;
}

