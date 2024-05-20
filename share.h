#pragma once

#include<Windows.h>

struct  StubConf
{
	DWORD dwOep;//程序入口
	HMODULE g_module;
};
HANDLE OpenPeFile(const char* pPath)
{
	return CreateFileA(pPath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
}

void SavePeFile(const char* pFile,int nSize,const char* pPath)
{
	//打开要保存的文件
	HANDLE hFile = CreateFileA(pPath, GENERIC_WRITE,
		FILE_SHARE_READ,NULL,CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ;
	//实际保存的字节数
	DWORD dwWrite = 0;
	WriteFile(hFile,pFile,nSize,&dwWrite,NULL);
	CloseHandle(hFile);
}

char* GetFileData(const char* path,int* FileSize)
{
	//获取文件句柄
	HANDLE hFile = OpenPeFile(path);
	if (INVALID_HANDLE_VALUE == hFile)
		return NULL;
	//获取文件大小
	DWORD dwSize = GetFileSize(hFile,NULL);
	*FileSize = dwSize;

	//读取文件
	char* pFileBuff = new char[dwSize];
	memset(pFileBuff,0,dwSize);
	DWORD	dwRead;
	ReadFile(hFile,pFileBuff,dwSize,&dwRead,NULL);
	CloseHandle(hFile);
	//返回堆内存
	return pFileBuff;
}
//获取DOS头
PIMAGE_DOS_HEADER GetDosHeader(char* pBase)
{
	return (PIMAGE_DOS_HEADER)pBase;
}
//获取NT头
PIMAGE_NT_HEADERS GetNtHeader(char* pBase)
{
	return (PIMAGE_NT_HEADERS)(GetDosHeader(pBase)->e_lfanew + (DWORD)pBase);
}
//获取FileHeader
PIMAGE_FILE_HEADER GetFileHeader(char* pBase)
{
	return &GetNtHeader(pBase)->FileHeader;
}
//获取OptionHeader
PIMAGE_OPTIONAL_HEADER GetOptHead(char* pBase)
{
	return &GetNtHeader(pBase)->OptionalHeader;
}
//获取区段头
PIMAGE_SECTION_HEADER GetSectionHeader(char* pBase)
{
	return IMAGE_FIRST_SECTION(GetNtHeader(pBase));
}
//获取指定名字区段头
PIMAGE_SECTION_HEADER GetSecHeaderByName(char* pFile,const char* Name)
{
	//区段数目
	DWORD dwSecNum = GetFileHeader(pFile)->NumberOfSections;
	//区段头
	PIMAGE_SECTION_HEADER pSec = GetSectionHeader(pFile);
	char szBuf[10] = {0};
	for(DWORD i = 0;i < dwSecNum;i++)
	{
		memcpy(szBuf,(char*)pSec[i].Name, 8);
		if (!strcmp(szBuf,Name))
		{
			return pSec+i;
		}
	}
	return NULL;
}

//
PIMAGE_SECTION_HEADER GetLastSecHeader(char* pFile)
{
	//区段数目
	DWORD dwSecNum = GetFileHeader(pFile)->NumberOfSections;
	//区段头
	PIMAGE_SECTION_HEADER pSec = GetSectionHeader(pFile);
	
	return pSec + (dwSecNum - 1);
}
//对齐粒度
DWORD Aligment(DWORD dwSize,DWORD dwAlig)
{
	return (dwSize % dwAlig == 0) ? dwSize : (dwSize / dwAlig + 1) * dwAlig; 
}