#ifndef _FILEPATH_H
#define _FILEPATH_H

#include <siacommon.h>
NS_BEGIN(Sia)
NS_BEGIN(Api)

class SIADRIVE_EXPORTABLE FilePath
{
public:
  FilePath();
  FilePath(const FilePath& filePath);
  FilePath(const SString& path);
  FilePath(const FilePath& filePath1, const FilePath& filePath2);
  FilePath(const FilePath& filePath1, const SString& path2);
  FilePath(const SString& path1, const FilePath& filePath2);
  FilePath(const SString& path1, const SString& path2);
  FilePath(FilePath&& filePath);

public:
  static const SString DirSep;

public:
  static SString FinalizePath(const SString& path);
  static SString GetTempDirectory();
  static SString GetAppDataDirectory();

private:
  SString _path;

public:
  FilePath& Append(const FilePath& filePath);
  FilePath& Append(const SString& path);
  bool IsDirectory() const;
  bool IsFile() const;
  bool IsUNC() const;
  bool CreateDirectory() const;
  bool RemoveDirectory() const;
  bool DeleteFile() const;
  bool MoveFile(const FilePath& filePath);
  FilePath& RemoveFileName();
  FilePath& MakeAbsolute();
  FilePath& SkipRoot();
  FilePath& StripToFileName();
  bool CreateEmptyFile();

public:
  FilePath& operator=(const FilePath& filePath);
  FilePath& operator=(FilePath&& filePath);
  SString::SChar& operator[](const size_t& idx);
  const SString::SChar& operator[](const size_t& idx) const;
  operator SString() const;
};

NS_END(2)
#endif //_FILEPATH_H