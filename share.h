#pragma once

#include<Windows.h>

struct  StubConf
{
	DWORD dwOep;//�������
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
	//��Ҫ������ļ�
	HANDLE hFile = CreateFileA(pPath, GENERIC_WRITE,
		FILE_SHARE_READ,NULL,CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ;
	//ʵ�ʱ�����ֽ���
	DWORD dwWrite = 0;
	WriteFile(hFile,pFile,nSize,&dwWrite,NULL);
	CloseHandle(hFile);
}

char* GetFileData(const char* path,int* FileSize)
{
	//��ȡ�ļ����
	HANDLE hFile = OpenPeFile(path);
	if (INVALID_HANDLE_VALUE == hFile)
		return NULL;
	//��ȡ�ļ���С
	DWORD dwSize = GetFileSize(hFile,NULL);
	*FileSize = dwSize;

	//��ȡ�ļ�
	char* pFileBuff = new char[dwSize];
	memset(pFileBuff,0,dwSize);
	DWORD	dwRead;
	ReadFile(hFile,pFileBuff,dwSize,&dwRead,NULL);
	CloseHandle(hFile);
	//���ض��ڴ�
	return pFileBuff;
}
//��ȡDOSͷ
PIMAGE_DOS_HEADER GetDosHeader(char* pBase)
{
	return (PIMAGE_DOS_HEADER)pBase;
}
//��ȡNTͷ
PIMAGE_NT_HEADERS GetNtHeader(char* pBase)
{
	return (PIMAGE_NT_HEADERS)(GetDosHeader(pBase)->e_lfanew + (DWORD)pBase);
}
//��ȡFileHeader
PIMAGE_FILE_HEADER GetFileHeader(char* pBase)
{
	return &GetNtHeader(pBase)->FileHeader;
}
//��ȡOptionHeader
PIMAGE_OPTIONAL_HEADER GetOptHead(char* pBase)
{
	return &GetNtHeader(pBase)->OptionalHeader;
}
//��ȡ����ͷ
PIMAGE_SECTION_HEADER GetSectionHeader(char* pBase)
{
	return IMAGE_FIRST_SECTION(GetNtHeader(pBase));
}
//��ȡָ����������ͷ
PIMAGE_SECTION_HEADER GetSecHeaderByName(char* pFile,const char* Name)
{
	//������Ŀ
	DWORD dwSecNum = GetFileHeader(pFile)->NumberOfSections;
	//����ͷ
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
	//������Ŀ
	DWORD dwSecNum = GetFileHeader(pFile)->NumberOfSections;
	//����ͷ
	PIMAGE_SECTION_HEADER pSec = GetSectionHeader(pFile);
	
	return pSec + (dwSecNum - 1);
}
//��������
DWORD Aligment(DWORD dwSize,DWORD dwAlig)
{
	return (dwSize % dwAlig == 0) ? dwSize : (dwSize / dwAlig + 1) * dwAlig; 
}