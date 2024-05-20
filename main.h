#include"pch.h"

#include<iostream>
#include<cstring>
#include"Share.h"



struct StubInfo{
	HMODULE hModule;//句柄
	DWORD dwTextRva;//代码段RVA基址
	DWORD dwTextSize;//代码段大小
	DWORD dwFunAddr;//入口函数
	StubConf* sc;//程序入口
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
	//1.获取代码段首地址
	char* pTarText = GetSecHeaderByName(szBuff, ".text")->PointerToRawData + szBuff;
	//2.获取代码段实际大小
	int size = GetSecHeaderByName(szBuff, ".text")->Misc.VirtualSize;
	for (int i = 0; i < size; ++i)
	{
		pTarText[i] ^= 0x15;
	}
//	*/

	//植入DLL
	Implant(szBuff,nSize);
	//去掉随机基址
	GetOptHead(szBuff)->DllCharacteristics &= (~0x40);

	//保存文件
	SavePeFile(szBuff,nSize,szPath);	
	MessageBoxW(0, L"加壳成功", L"提示", 0);
	return 0;
}


//1.加载DLL
StubInfo LoadStub()
{
	StubInfo si = {0};
	//加载DLL
	HMODULE hStubDll = LoadLibraryExA("Stub.dll",0,DONT_RESOLVE_DLL_REFERENCES);
	//结构体赋值
	si.hModule = hStubDll;
	//获取text段起始地址和大小
	si.dwTextRva = (DWORD)GetSecHeaderByName((char*)hStubDll,".text")->VirtualAddress;
	si.dwTextSize = (DWORD)GetSecHeaderByName((char*)hStubDll,".text")->SizeOfRawData;
	//获取Start的地址
	si.dwFunAddr = (DWORD)GetProcAddress(hStubDll,"Start");
	//OEP
	si.sc = (StubConf*)GetProcAddress(hStubDll, "g_Sc");
	return si;
}
//2.添加一个区段
//pTarBuff:源程序的文件指针  nSize:源文件大小
//pSecName:新区段名称       nSecSize:新区段大小
void AddSection(char*& pTarBuff,int& nSize,
				const char* pSecName,int nSecSize)
{
	//区段数目加1
	int n = GetFileHeader(pTarBuff)->NumberOfSections++;
	//获取最后一个区段的信息
	PIMAGE_SECTION_HEADER pSec = (PIMAGE_SECTION_HEADER)GetLastSecHeader(pTarBuff);
	DWORD dwFileAlig = GetOptHead(pTarBuff)->FileAlignment;
	DWORD dwMemAlig = GetOptHead(pTarBuff)->SectionAlignment;
	//设置新区段信息
	memcpy(pSec->Name,pSecName,8);
	pSec->Misc.VirtualSize = nSecSize;
	pSec->SizeOfRawData = Aligment(nSecSize,dwFileAlig);
	//内存的RVA，内存对齐
	pSec->VirtualAddress = (pSec - 1)->VirtualAddress + 
		Aligment((pSec - 1)->SizeOfRawData,dwMemAlig);
	//文件偏移，对齐
	pSec->PointerToRawData = Aligment(nSize,dwFileAlig);
	pSec->Characteristics = 0xE00000E0;
	//映像大小,进行内存对齐
	GetOptHead(pTarBuff)->SizeOfImage = 
		Aligment(pSec->VirtualAddress + pSec->SizeOfRawData,dwMemAlig);

	//增加文件大小
	int nNewSize = pSec->SizeOfRawData + pSec->PointerToRawData;
	char* pNewFile = new char[nNewSize];
	memset(pNewFile,0,nNewSize);
	//复制源程序内容
	memcpy(pNewFile,pTarBuff,nSize);
	delete[] pTarBuff;
	pTarBuff = pNewFile;
	nSize = nNewSize;
}

void FixDllReloc(char* hModule,DWORD dwNewBase,DWORD dwNewSecRva)
{
	//重定位表基址
	auto pReloc = (PIMAGE_BASE_RELOCATION)
		(GetOptHead(hModule)->DataDirectory[5].VirtualAddress + hModule);
	//获取DLL text段的rva
	DWORD dwTextRva = (DWORD)GetSecHeaderByName(hModule,".text")->VirtualAddress;
	while(pReloc->SizeOfBlock)
	{
		struct TypeOffset{
			WORD offset : 12;
			WORD type : 4;
		};
		TypeOffset* pTyOf = (TypeOffset*)(pReloc + 1);
		DWORD dwCount = (pReloc->SizeOfBlock - 8) / 2;
		//修复重定位
		for(int i = 0;i < dwCount;i++)
		{
			if (pTyOf[i].type != 3)
				continue;
			//要修复Rva
			DWORD dwFixRva = pTyOf[i].offset + pReloc->VirtualAddress;
			//要修复的地址
			DWORD* pFixAddr = (DWORD*)(dwFixRva + (DWORD)hModule);

			DWORD dwOld;
			VirtualProtect(pFixAddr,4,PAGE_READWRITE,&dwOld);
			*pFixAddr -= (DWORD)hModule;
			*pFixAddr -= dwTextRva;
			*pFixAddr += dwNewBase;
			*pFixAddr += dwNewSecRva;
			VirtualProtect(pFixAddr,4,dwOld,&dwOld);
		}
		//指向下一个重定位块
		pReloc = (PIMAGE_BASE_RELOCATION)
			((DWORD)pReloc + pReloc->SizeOfBlock);
	}
}


void Implant(char*& szBuff,int& dwSize)
{
	//加载DLL
	StubInfo st = LoadStub();
	//新区段名称
	char NewSecName[] = ".first";
	//新增段
	AddSection(szBuff,dwSize,NewSecName,st.dwTextSize);

	//修复重定位
	DWORD dwBase = GetOptHead(szBuff)->ImageBase;
	DWORD dwNewSecRva = GetSecHeaderByName(szBuff,NewSecName)->VirtualAddress;
	FixDllReloc((char*)st.hModule,dwBase,dwNewSecRva);
	//保存原始OEP	
	st.sc->dwOep = GetOptHead(szBuff)->AddressOfEntryPoint;
	st.sc->g_module = (HMODULE)szBuff;
	//移植dll中的代码到新区段中你
	char* pNewSecAddr = GetSecHeaderByName(szBuff,NewSecName)->PointerToRawData
			        + szBuff;
	char* pDllTextAddr = st.dwTextRva + (char*)st.hModule;
	memcpy(pNewSecAddr,pDllTextAddr,st.dwTextSize);

	//设置新的OEP
	GetOptHead(szBuff)->AddressOfEntryPoint = st.dwFunAddr - (DWORD)st.hModule - 
		st.dwTextRva + GetSecHeaderByName(szBuff,NewSecName)->VirtualAddress;
}

