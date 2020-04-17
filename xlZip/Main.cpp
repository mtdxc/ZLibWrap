//------------------------------------------------------------------------------
//
//    Copyright (C) Streamlet. All rights reserved.
//
//    File Name:   Main.cpp
//    Author:      Streamlet
//    Create Time: 2010-09-14
//    Description: 
//
//    Version history:
//
//
//
//------------------------------------------------------------------------------


#include <stdio.h>
#include <tchar.h>
#include <locale.h>
#include "../ZLibWrapLib/ZLibWrapLib.h"

void ShowBanner()
{
    _tprintf(_T("xlZip By Streamlet\n"));
    _tprintf(_T("\n"));
}

void ShowHelp()
{
    _tprintf(_T("Usage:\n"));
    _tprintf(_T("    xlZip /z <SourceFiles> <ZipFile> [/utf8]\n"));
    _tprintf(_T("    xlZip /u <ZipFile> <DestFolder>\n"));
}

int _tmain(int argc, TCHAR *argv[])
{
    setlocale(LC_ALL, "");

    ShowBanner();

    if (argc < 4)
    {
        ShowHelp();
        return 0;
    }

    enum
    {
        ZIP,
        UNZIP

    } TODO;

    bool bUtf8 = false;

    if (_tcsicmp(argv[1], _T("/z")) == 0)
    {
        TODO = ZIP;

        if (argc >= 5 && _tcsicmp(argv[4], _T("/utf8")) == 0)
        {
            bUtf8 = true;
        }
    }
    else if (_tcsicmp(argv[1], _T("/u")) == 0)
    {
        TODO = UNZIP;
    }
    else
    {
        ShowHelp();
        return 0;
    }

    switch (TODO)
    {
    case ZIP:
        if (ZipCompress(argv[2], argv[3], bUtf8))
        {
            _tprintf(_T("Compressed %s to %s successfully.\n"), argv[2], argv[3]);
        }
        else
        {
            _tprintf(_T("Failed to compress %s to %s.\n"), argv[2], argv[3]);
        }
        break;
    case UNZIP:
        if (ZipExtract(argv[2], argv[3]))
        {
            _tprintf(_T("Extracted %s to %s successfully.\n"), argv[2], argv[3]);
        }
        else
        {
            _tprintf(_T("Failed to Extract %s to %s.\n"), argv[2], argv[3]);
        }
        break;
    default:
        break;
    }

    return 0;
}


#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include "tps/dirent.h"
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include "minizip/zip.h"

void EnumDirFiles(const std::string& dirPrefix, const std::string& dirName, std::vector<std::string>& vFiles)
{
  if (dirPrefix.empty() || dirName.empty())
    return;
  std::string dirNameTmp = dirName;
  std::string dirPre = dirPrefix;

  if (dirNameTmp.find_last_of("/") != dirNameTmp.length() - 1)
    dirNameTmp += "/";
  if (dirNameTmp[0] == '/')
    dirNameTmp = dirNameTmp.substr(1);
  if (dirPre.find_last_of("/") != dirPre.length() - 1)
    dirPre += "/";

  std::string path;

  path = dirPre + dirNameTmp;


  struct stat fileStat;
  DIR* pDir = opendir(path.c_str());
  if (!pDir) return;

  struct dirent* pDirEnt = NULL;
  while ((pDirEnt = readdir(pDir)) != NULL)
  {
    if (strcmp(pDirEnt->d_name, ".") == 0 || strcmp(pDirEnt->d_name, "..") == 0)
      continue;

    std::string tmpDir = dirPre + dirNameTmp + pDirEnt->d_name;
    if (stat(tmpDir.c_str(), &fileStat) != 0)
      continue;

    std::string innerDir = dirNameTmp + pDirEnt->d_name;
    if (fileStat.st_mode & S_IFDIR == S_IFDIR)
    {
      EnumDirFiles(dirPrefix, innerDir, vFiles);
      continue;
    }

    vFiles.push_back(innerDir);
  }

  if (pDir)
    closedir(pDir);
}

int writeInZipFile(zipFile zFile, const std::string& file)
{
  std::fstream f(file.c_str(), std::ios::binary | std::ios::in);
  f.seekg(0, std::ios::end);
  long size = f.tellg();
  f.seekg(0, std::ios::beg);
  if (size <= 0)
  {
    return zipWriteInFileInZip(zFile, NULL, 0);
  }
  char* buf = new char[size];
  f.read(buf, size);
  int ret = zipWriteInFileInZip(zFile, buf, size);
  delete[] buf;
  return ret;
}

int ZipDir(std::string src, std::string dest) {
  if (src.find_last_of("/") == src.length() - 1)
    src = src.substr(0, src.length() - 1);

  struct stat fileInfo;
  stat(src.c_str(), &fileInfo);
  if (fileInfo.st_mode & S_IFREG == S_IFREG)
  {
    zipFile zFile = zipOpen(dest.c_str(), APPEND_STATUS_CREATE);
    if (zFile == NULL)
    {
      std::cout << "openfile failed" << std::endl;
      return -1;
    }
    zip_fileinfo zFileInfo = { 0 };
    zFileInfo.dosDate = fileInfo.st_mtime;
    int ret = zipOpenNewFileInZip(zFile, src.c_str(), &zFileInfo, NULL, 0, NULL, 0, NULL, 0, Z_DEFAULT_COMPRESSION);
    if (ret != ZIP_OK)
    {
      std::cout << "openfile in zip failed" << std::endl;
      zipClose(zFile, NULL);
      return -1;
    }
    ret = writeInZipFile(zFile, src);
    if (ret != ZIP_OK)
    {
      std::cout << "write in zip failed" << std::endl;
      zipClose(zFile, NULL);
      return -1;
    }
    zipClose(zFile, NULL);
    std::cout << "zip ok" << std::endl;
  }
  else if (fileInfo.st_mode & S_IFDIR == S_IFDIR)
  {
    size_t pos = src.find_last_of("/");
    std::string dirName = src.substr(pos + 1);
    std::string dirPrefix = src.substr(0, pos);

    zipFile zFile = zipOpen(dest.c_str(), APPEND_STATUS_CREATE);
    if (zFile == NULL)
    {
      std::cout << "openfile failed" << std::endl;
      return -1;
    }

    std::vector<std::string> vFiles;
    EnumDirFiles(dirPrefix, dirName, vFiles);
    auto itF = vFiles.begin();
    for (; itF != vFiles.end(); ++itF)
    {
      zip_fileinfo zFileInfo = { 0 };
      stat(itF->c_str(), &fileInfo);
      zFileInfo.dosDate = fileInfo.st_mtime;
      int ret = zipOpenNewFileInZip(zFile, itF->c_str(), &zFileInfo, NULL, 0, NULL, 0, NULL, 0, Z_DEFAULT_COMPRESSION);
      if (ret != ZIP_OK)
      {
        std::cout << "openfile in zip failed" << std::endl;
        zipClose(zFile, NULL);
        return -1;
      }
      ret = writeInZipFile(zFile, *itF);
      if (ret != ZIP_OK)
      {
        std::cout << "write in zip failed" << std::endl;
        zipClose(zFile, NULL);
        return -1;
      }
    }

    zipClose(zFile, NULL);
    std::cout << "zip ok" << std::endl;
  }
  return 0;
}



