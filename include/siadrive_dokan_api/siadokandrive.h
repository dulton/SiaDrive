#ifndef _SIADOKANDRIVE_H
#define _SIADOKANDRIVE_H

#ifdef _WIN32 
  #define WIN32_NO_STATUS

	// Import or export functions and/or classes
	#ifdef SIADRIVE_DOKAN_EXPORT_SYMBOLS
		#define SIADRIVE_DOKAN_EXPORTABLE __declspec(dllexport)
	#else
		#define SIADRIVE_DOKAN_EXPORTABLE __declspec(dllimport)
	#endif //SIADRIVE_DOKAN_EXPORT_SYMBOLS
#else
	#define SIADRIVE_DOKAN_EXPORTABLE
#endif

#include <siaapi.h>
#include <mutex>
#include <eventsystem.h>
NS_BEGIN(Sia)
NS_BEGIN(Api)
NS_BEGIN(Dokan)

class SIADRIVE_DOKAN_EXPORTABLE SiaDokanDriveException : 
	std::exception
{
public:
	SiaDokanDriveException(char* message) : 
		std::exception(message)
	{

	}
};

class SIADRIVE_DOKAN_EXPORTABLE CSiaDokanDrive
{
public:
	// throws SiaDokenDriveException
	CSiaDokanDrive(CSiaApi& siaApi, CSiaDriveConfig* siaDriveConfig); 

public:
	~CSiaDokanDrive();

private:
	CSiaApi& _siaApi;

	CSiaDriveConfig* _siaDriveConfig;

public:
  bool IsMounted() const;

	void Mount(const wchar_t& driveLetter, const SString& cacheLocation, const std::uint64_t& maxCacheSizeBytes);

	void Unmount(const bool& clearCache = false);

	void ClearCache();
};


class SIADRIVE_DOKAN_EXPORTABLE DriveMountEnded :
  public CEvent
{
public:
  DriveMountEnded(const SString mountLocation, const NTSTATUS& mountStatus) :
    _mountLocation(mountLocation),
    _mountStatus(mountStatus)
  {
  }

public:
  virtual ~DriveMountEnded()
  {
  }

private:
  const SString _mountLocation;
  const NTSTATUS _mountStatus;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DriveMountEnded(_mountLocation, _mountStatus));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DriveMountEnded|LOC|" + _mountLocation + L"|STATUS|" + SString::FromInt32(_mountStatus);
  }
};

class SIADRIVE_DOKAN_EXPORTABLE DriveUnMounted :
  public CEvent
{
public:
  DriveUnMounted(const SString& mountLocation) :
    _mountLocation(mountLocation)
  {

  }

public:
  virtual ~DriveUnMounted()
  {
  }

private:
  const SString _mountLocation;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DriveUnMounted(_mountLocation));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DriveUnMounted|LOC|" + _mountLocation;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DriveMounted :
  public CEvent
{
public:
  DriveMounted(const SString& mountLocation) :
    _mountLocation(mountLocation)
  {

  }

public:
  virtual ~DriveMounted()
  {
  }

private:
  const SString _mountLocation;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DriveMounted(_mountLocation));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DriveMounted|LOC|" + _mountLocation;
  }
};

class SIADRIVE_DOKAN_EXPORTABLE DokanCreateFile :
  public CEvent
{
public:
  DokanCreateFile(const SString& filePath, const DWORD& fileAttributesAndFlags, const DWORD& creationDisposition, const ACCESS_MASK& genericDesiredAccess, const NTSTATUS& returnStatus) :
    _filePath(filePath),
    _fileAttributesAndFlags(fileAttributesAndFlags),
    _creationDisposition(creationDisposition),
    _genericDesiredAccess(genericDesiredAccess),
    _returnStatus(returnStatus)
  {

  }

public:
  virtual ~DokanCreateFile()
  {
  }

private:
  const SString _filePath;
  const DWORD _fileAttributesAndFlags;
  const DWORD _creationDisposition;
  const ACCESS_MASK _genericDesiredAccess;
  const NTSTATUS _returnStatus;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanCreateFile(_filePath, _fileAttributesAndFlags, _creationDisposition, _genericDesiredAccess, _returnStatus));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanCreateFile|FILE|" + _filePath + 
      "|ATTR|" + SString::FromUInt32(_fileAttributesAndFlags) +
      "|DISP|" + SString::FromUInt32(_creationDisposition) +
      "|MASK|" + SString::FromUInt32(_genericDesiredAccess) +
      "|RET|" + SString::FromUInt32(_returnStatus);
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanFindFiles :
  public CEvent
{
public:
  DokanFindFiles(const SString& cachePath, const SString& rootPath, const SString& siaQuery, const SString& findFile) :
    _cachePath(cachePath),
    _rootPath(rootPath),
    _siaQuery(siaQuery),
    _findFile(findFile)
  {
  }

public:
  virtual ~DokanFindFiles()
  {
  }

private:
  const SString _cachePath;
  const SString _rootPath;
  const SString _siaQuery;
  const SString _findFile;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanFindFiles(_cachePath, _rootPath, _siaQuery, _findFile));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanFindFiles|PATH|" + _cachePath +
      "|ROOT|" + _rootPath +
      "|QUERY|" + _siaQuery +
      "|FIND|" + _findFile;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanCloseFile :
  public CEvent
{
public:
  DokanCloseFile(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanCloseFile()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanCloseFile(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanCloseFile|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanGetFileInformation :
  public CEvent
{
public:
  DokanGetFileInformation(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanGetFileInformation()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanGetFileInformation(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanGetFileInformation|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanReadFile :
  public CEvent
{
public:
  DokanReadFile(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanReadFile()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanReadFile(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanReadFile|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanWriteFile :
  public CEvent
{
public:
  DokanWriteFile(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanWriteFile()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanWriteFile(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanWriteFile|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanSetEndOfFile :
  public CEvent
{
public:
  DokanSetEndOfFile(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanSetEndOfFile()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanSetEndOfFile(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanSetEndOfFile|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanFlushFileBuffers :
  public CEvent
{
public:
  DokanFlushFileBuffers(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanFlushFileBuffers()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanFlushFileBuffers(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanFlushFileBuffers|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanDeleteDirectory :
  public CEvent
{
public:
  DokanDeleteDirectory(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanDeleteDirectory()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanDeleteDirectory(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanDeleteDirectory|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanDeleteFileW :
  public CEvent
{
public:
  DokanDeleteFileW(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanDeleteFileW()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanDeleteFileW(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanDeleteFileW|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanSetFileAttributesW :
  public CEvent
{
public:
  DokanSetFileAttributesW(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanSetFileAttributesW()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanSetFileAttributesW(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanSetFileAttributesW|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanGetFileAttributesW :
  public CEvent
{
public:
  DokanGetFileAttributesW(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanGetFileAttributesW()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanGetFileAttributesW(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanGetFileAttributesW|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanSetFileSecurityW :
  public CEvent
{
public:
  DokanSetFileSecurityW(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanSetFileSecurityW()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanSetFileSecurityW(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanSetFileSecurityW|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanSetFileTime :
  public CEvent
{
public:
  DokanSetFileTime(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanSetFileTime()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanSetFileTime(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanSetFileTime|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanSetAllocationSize :
  public CEvent
{
public:
  DokanSetAllocationSize(const SString& cachePath) :
    _cachePath(cachePath)
  {
  }

public:
  virtual ~DokanSetAllocationSize()
  {
  }

private:
  const SString _cachePath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanSetAllocationSize(_cachePath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanSetAllocationSize|PATH|" + _cachePath;
  }
};


class SIADRIVE_DOKAN_EXPORTABLE DokanMoveFileW :
  public CEvent
{
public:
  DokanMoveFileW(const SString& srcPath, const SString& destPath) :
    _srcPath(srcPath),
    _destPath(destPath)
  {
  }

public:
  virtual ~DokanMoveFileW()
  {
  }

private:
  const SString _srcPath;
  const SString _destPath;

public:
  virtual std::shared_ptr<CEvent> Clone() const override
  {
    return std::shared_ptr<CEvent>(new DokanMoveFileW(_srcPath, _destPath));
  }

  virtual SString GetSingleLineMessage() const override
  {
    return L"DokanMoveFileW|SRC|" + _srcPath + "|DEST|" + _destPath;
  }
};
NS_END(3)
#endif //_SIADOKANDRIVE_H