#include <filepath.h>
#include <regex>
#ifdef _WIN32
#include <Shlobj.h>
#endif
using namespace Sia::Api;

SString FilePath::FinalizePath(const SString& path)
{
#ifdef _WIN32
  std::wregex r(L"/+");
  SString str = std::regex_replace(path.str(), r, L"\\");
  std::wregex r2(L"\\\\+");
  return std::regex_replace(str.str(), r2, L"\\");
#else
  a
#endif
}

SString FilePath::GetTempDirectory()
{
#ifdef _WIN32
  SString tempPath;
  tempPath.Resize(MAX_PATH + 1);
  GetTempPath(MAX_PATH, &tempPath[0]);
  return std::move(tempPath.Fit());
#else
  a
#endif
}

SString FilePath::GetAppDataDirectory()
{
#ifdef _WIN32
  PWSTR localAppData = nullptr;
  ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData);
  SString ret = localAppData;
  ::CoTaskMemFree(localAppData);
  return ret;
#else
  a
#endif
}

FilePath::FilePath()
{
}

FilePath::FilePath(const FilePath& filePath) :
  _path(filePath)
{
}

FilePath::FilePath(const SString& path)
{
  _path = FinalizePath(path);
}

FilePath::FilePath(const FilePath& filePath1, const FilePath& filePath2)
{
  _path = filePath1;
  Append(filePath2);  
}

FilePath::FilePath(const FilePath& filePath1, const SString& path2)
{
  _path = filePath1;
  Append(FinalizePath(path2));
}

FilePath::FilePath(const SString& path1, const FilePath& filePath2)
{
  _path = FinalizePath(path1);
  Append(filePath2);
}

FilePath::FilePath(const SString& path1, const SString& path2)
{
  _path = FinalizePath(path1);
  Append(FinalizePath(path2));
}

FilePath::FilePath(FilePath&& filePath) :
  _path(std::move(filePath._path))
{
}

#ifdef _WIN32
const SString FilePath::DirSep = "\\";
#else
const SString FilePath::DirSep = "/";
#endif

FilePath& FilePath::Append(const FilePath& filePath)
{
  _path = FinalizePath(_path + DirSep + filePath);
  return *this;
}

FilePath& FilePath::Append(const SString& path)
{
  _path = FinalizePath(_path + DirSep + path);
  return *this;
}

bool FilePath::IsDirectory() const
{
#ifdef _WIN32
  return ::PathIsDirectory(&_path[0]) ? true : false;
#else
  a
#endif
}

bool FilePath::IsFile() const
{
#ifdef _WIN32
  return ::PathFileExists(&_path[0]) ? true : false;
#else
  a
#endif
}

bool FilePath::IsUNC() const
{
#ifdef _WIN32
  return ::PathIsUNC(&_path[0]) ? true : false;
#else
  a
#endif
}

bool FilePath::CreateDirectory() const
{
#ifdef _WIN32
  FilePath fp(_path);
  fp.MakeAbsolute();
  return (::SHCreateDirectory(nullptr, &fp[0]) == ERROR_SUCCESS);
#else
  a
#endif
}

bool FilePath::RemoveDirectory() const
{
#ifdef _WIN32
  return ::RemoveDirectory(&_path[0]) ? true : false;
#else
  a
#endif
}

bool FilePath::DeleteFile() const
{
#ifdef _WIN32
  return ::DeleteFile(&_path[0]) ? true : false;
#else
  a
#endif
}

bool FilePath::MoveFile(const FilePath& filePath)
{
#ifdef _WIN32
  FilePath folder(filePath);
  folder.RemoveFileName().CreateDirectory();
  return ::MoveFile(&_path[0], &filePath[0]) ? true : false;
#else
  a
#endif
}

FilePath& FilePath::RemoveFileName()
{
#ifdef _WIN32
  ::PathRemoveFileSpec(&_path[0]);
  _path = _path.str().c_str();
  return *this;
#else
  a
#endif
}

FilePath& FilePath::SkipRoot()
{
#ifdef _WIN32
  ::PathSkipRoot(&_path[0]);
  _path = _path.str().c_str();
  return *this;
#else
  a
#endif
}

FilePath& FilePath::operator=(const FilePath& filePath)
{
  if (this != &filePath)
  {
    _path = filePath._path;
  }

  return *this;
}

FilePath& FilePath::operator=(FilePath&& filePath)
{
  if (this != &filePath)
  {
    _path = std::move(filePath._path);
  }

  return *this;
}

SString::SChar& FilePath::operator[](const size_t& idx)
{
  return _path[idx];
}

const SString::SChar& FilePath::operator[](const size_t& idx) const
{
  return _path[idx];
}

FilePath::operator SString() const
{
  return _path;
}

FilePath& FilePath::MakeAbsolute()
{
#ifdef _WIN32
  if (::PathIsRelative(&_path[0]))
  {
    SString temp;
    temp.Resize(MAX_PATH + 1);
    _path = _wfullpath(&temp[0], &_path[0], MAX_PATH);
  }
#else
  a
#endif

  return *this;
}

FilePath& FilePath::StripToFileName()
{
#ifdef _WIN32
  _path = ::PathFindFileName(&_path[0]);
#else
  a
#endif

  return *this;
}

bool FilePath::CreateEmptyFile()
{
  FILE* fp;
  if (_wfopen_s(&fp, &_path[0], L"w+") == 0)
  {
    fclose(fp);
  }

  return false;
}