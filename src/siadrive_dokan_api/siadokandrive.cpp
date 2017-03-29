#include <siadokandrive.h>
#include <filesystem>
#include <uploadmanager.h>
#include <dokan.h>
#include <filepath.h>

using namespace Sia::Api;
using namespace Sia::Api::Dokan;

// TODO Handle paths greater than MAX_PATH!!

// The general idea is that normal file I/O occurs in a local cache folder and once the file is closed, it is scheduled for upload into Sia.
//	Files requested to be openned that are not cached will be downloaded first. If the file is not found in Sia, it will be treated as new.
//	Keeping cache and Sia in synch will be a bit of a hastle, so it's strongly suggested to treat the cache folder as if it doesn't exist;
//	however, simply deleting files in the cache folder should not be an issue as long as the drive is not mounted.
class SIADRIVE_DOKAN_EXPORTABLE DokanImpl
{
private:
	typedef struct
	{
		FilePath SiaPath;
    FilePath CacheFilePath;
    bool Dummy;
		bool Changed;
	} OpenFileInfo;

private:
	static std::mutex _dokanMutex;
	static CSiaApi* _siaApi;
	static CSiaDriveConfig* _siaDriveConfig;
	static std::unique_ptr<CUploadManager> _uploadManager;
	static DOKAN_OPERATIONS _dokanOps;
	static DOKAN_OPTIONS _dokanOptions;
	static FilePath _cacheLocation;
	static std::unordered_map<ULONG64, OpenFileInfo> _openFileMap;
	static std::unique_ptr<std::thread> _fileListThread;
	static HANDLE _fileListStopEvent;
	static CSiaFileTreePtr _siaFileTree;
	static std::mutex _fileTreeMutex;
	static std::unique_ptr<std::thread> _mountThread;
	static NTSTATUS _mountStatus;
	static SString _mountPoint;

private:
	inline static const FilePath& GetCacheLocation()
	{
		return _cacheLocation;
	}

  inline static CSiaFileTreePtr GetFileTree()
	{
    std::lock_guard<std::mutex> l(_fileTreeMutex);
    auto ret = _siaFileTree;
    return ret;
	}

  static bool AddFileToCache(const SString& siaPath)
  {
    FilePath tempFilePath = FilePath::GetTempDirectory();
    tempFilePath.Append(GenerateSha256(siaPath) + ".siatmp");

    // TODO Check cache size is large enough to hold new file
    bool ret = ApiSuccess(_siaApi->GetRenter()->DownloadFile(siaPath, tempFilePath));
    if (ret)
    {
      FilePath src(tempFilePath);
      FilePath dest(GetCacheLocation(), siaPath);
      ret = src.MoveFile(dest);
      if (!ret)
      {
        src.DeleteFile();
      }
    }

    return ret;
  }

  inline static bool AddDummyFileToCache(const SString& siaPath)
  {
      FilePath dest(GetCacheLocation(), siaPath);
      return dest.CreateEmptyFile();
  }

  static void HandleSiaFileClose(const FilePath& filePath, const ULONG64 id, const std::uint64_t& fileSize, const bool& deleteOnClose)
  {
    std::lock_guard<std::mutex> l(_dokanMutex);
    auto ofi = _openFileMap.find(id);
    if (ofi != _openFileMap.end())
    {
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanCloseFile(ofi->second.CacheFilePath)));
      if (deleteOnClose)
      {
        // TODO Handle failure
        _uploadManager->Remove(ofi->second.SiaPath);
      }
      else
      {
        QueueUploadIfChanged(id, fileSize);
      }
      _openFileMap.erase(id);
    }
  }

	static void QueueUploadIfChanged(const ULONG64& id, const std::uint64_t& size)
	{
		if (_openFileMap[id].Changed)
		{
			if (size > 0)
			{
				// TODO Handle error return
				_uploadManager->AddOrUpdate(_openFileMap[id].SiaPath, _openFileMap[id].CacheFilePath);
			}
			else
			{
				// Treat 0 length files as deleted in Sia - cache retains 0-length
				_uploadManager->Remove(_openFileMap[id].SiaPath);
			}
		}
	}

	static void StartFileListThread()
	{
		if (!_fileListThread)
		{
      _fileListStopEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
			_fileListThread.reset(new std::thread([]()
			{
				do
				{
					CSiaFileTreePtr siaFileTree;
					_siaApi->GetRenter()->GetFileTree(siaFileTree);
					{
						std::lock_guard<std::mutex> l(_fileTreeMutex);
						_siaFileTree = siaFileTree;
					}
        } while (::WaitForSingleObject(_fileListStopEvent, 1000) == WAIT_TIMEOUT);
			}));
		}
	}

	static void StopFileListThread()
	{
		if (_fileListThread)
		{
      ::SetEvent(_fileListStopEvent);
			_fileListThread->join();
			_fileListThread.reset(nullptr);
      ::CloseHandle(_fileListStopEvent);
		}
	}

	// Dokan callbacks
private:
	static NTSTATUS DOKAN_CALLBACK Sia_ZwCreateFile(
		LPCWSTR fileName,
		PDOKAN_IO_SECURITY_CONTEXT securityContext,
		ACCESS_MASK desiredAccess,
		ULONG fileAttributes,
		ULONG shareAccess,
		ULONG createDisposition,
		ULONG createOptions,
		PDOKAN_FILE_INFO dokanFileInfo)
	{
		SECURITY_ATTRIBUTES securityAttrib;
		securityAttrib.nLength = sizeof(securityAttrib);
		securityAttrib.lpSecurityDescriptor = securityContext->AccessState.SecurityDescriptor;
		securityAttrib.bInheritHandle = FALSE;

		DWORD fileAttributesAndFlags;
		DWORD creationDisposition;
		DokanMapKernelToUserCreateFileFlags(fileAttributes, createOptions, createDisposition, &fileAttributesAndFlags, &creationDisposition);
		
		ACCESS_MASK genericDesiredAccess = DokanMapStandardToGenericAccess(desiredAccess);

		NTSTATUS ret = STATUS_SUCCESS;
		// Probably not going to happen, but just in case
		if (FilePath(fileName).IsUNC())
		{
			ret = STATUS_ILLEGAL_ELEMENT_ADDRESS;
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanCreateFile(fileName, fileAttributesAndFlags, creationDisposition, genericDesiredAccess, ret)));
		}
		else
		{
			// When filePath is a directory, needs to change the flag so that the file can
			// be opened.
			FilePath cacheFilePath(GetCacheLocation(), &fileName[1]);
			DWORD fileAttr = ::GetFileAttributes(&cacheFilePath[0]);

			if ((fileAttr != INVALID_FILE_ATTRIBUTES) &&
				(fileAttr & FILE_ATTRIBUTE_DIRECTORY) &&
				!(createOptions & FILE_NON_DIRECTORY_FILE))
			{
				dokanFileInfo->IsDirectory = TRUE;
				if (desiredAccess & DELETE)
				{
					// Needed by FindFirstFile to see if directory is empty or not
					shareAccess |= FILE_SHARE_READ;
				}
			}

			// Folder (cache operation only)
			if (dokanFileInfo->IsDirectory) 
			{
				if (creationDisposition == CREATE_NEW) 
				{
					if (!cacheFilePath.CreateDirectory())
					{
						DWORD error = GetLastError();
						ret = DokanNtStatusFromWin32(error);
					}
				}
				else if (creationDisposition == OPEN_ALWAYS) 
				{
					if (cacheFilePath.CreateDirectory()) 
					{
						DWORD error = GetLastError();
						if (error != ERROR_ALREADY_EXISTS) 
						{
							ret = DokanNtStatusFromWin32(error);
						}
					}
				}

				if (ret == STATUS_SUCCESS) 
				{
					//Check first if we're trying to open a file as a directory.
					if (fileAttr != INVALID_FILE_ATTRIBUTES &&
						!(fileAttr & FILE_ATTRIBUTE_DIRECTORY) &&
						(createOptions & FILE_DIRECTORY_FILE)) 
					{
						return STATUS_NOT_A_DIRECTORY;
					}

					// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
					HANDLE handle = ::CreateFile(&cacheFilePath[0], genericDesiredAccess, shareAccess, &securityAttrib, OPEN_EXISTING, fileAttributesAndFlags | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
					if (handle == INVALID_HANDLE_VALUE) 
					{
						DWORD error = GetLastError();
						ret = DokanNtStatusFromWin32(error);
					}
					else 
					{
						dokanFileInfo->Context = reinterpret_cast<ULONG64>(handle); // save the file handle in Context
					}
				}
			}
			else // File (cache and/or Sia operation)
			{
				// Formulate Sia path
				SString siaPath = CSiaApi::FormatToSiaPath(FilePath(fileName).SkipRoot()); // Strip drive letter to get Sia path
				if (siaPath.Length())
				{
					// If cache file already exists and is a directory, requested file operation isn't valid
					DWORD attribs = ::GetFileAttributes(&cacheFilePath[0]);
					if ((attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY))
					{
						ret = STATUS_OBJECT_NAME_COLLISION;
					}
					else
					{
						bool exists;
						if (ApiSuccess(_siaApi->GetRenter()->FileExists(siaPath, exists)))
						{
              exists = exists || cacheFilePath.IsFile();
						  // Operations on existing files that are requested to be truncated, overwritten or re-created
							//	will first be deleted and then replaced if, after the file operation is done, the resulting file
							//	size is > 0. Sia doesn't support random access to files (upload/download/rename/delete).
							bool isCreateOp = false;
							bool isReplaceOp = false;
							switch (creationDisposition)
							{
							case CREATE_ALWAYS:
							{
								isCreateOp = true;
								isReplaceOp = exists;
							}
							break;

							case CREATE_NEW:
							{
								if (exists)
								{
									ret = STATUS_OBJECT_NAME_EXISTS;
								}
								else
								{
									isCreateOp = true;
								}
							}
							break;

							case OPEN_ALWAYS:
							{
								if (!exists)
								{
									isCreateOp = true;
								}
							}
							break;

							case OPEN_EXISTING:
							{
								if (!exists)
								{
                  ret = STATUS_OBJECT_NAME_NOT_FOUND;
								}
							}
							break;

							case TRUNCATE_EXISTING:
							{
								if (exists)
								{
									isCreateOp = isReplaceOp = true;
								}
								else
								{
									ret = STATUS_OBJECT_NAME_NOT_FOUND;
								}
							}
							break;

							default:
								// Nothing to do
								break;
							}

							if (ret == STATUS_SUCCESS)
							{
								if (isReplaceOp)
								{
									// Since this is a request to replace an existing file, make sure cache is deleted first.
									//	If file isn't cached, delete from Sia only
									if (!cacheFilePath.IsFile() || cacheFilePath.DeleteFile())
									{
										if (!ApiSuccess(_uploadManager->Remove(siaPath)))
										{
											ret = STATUS_INVALID_SERVER_STATE;
										}
									}
									else
									{
										ret = DokanNtStatusFromWin32(GetLastError());
									}
								}

                bool isDummy = false;
								if (ret == STATUS_SUCCESS)
								{
									// If file must exist, then check for it in cache location. If not found,
									//	it must be downloaded first and placed in cache
									if (!isCreateOp && !cacheFilePath.IsFile())
									{
                    if (exists)
                    {
                      isDummy = AddDummyFileToCache(siaPath);
                      if (!isDummy)
                      {
                        ret = STATUS_ACCESS_DENIED;
                      }
                    }
                    else
                    {
                      ret = STATUS_NOT_FOUND;
                    }
									}

									if (ret == STATUS_SUCCESS)
									{
										// Create file as specified
										HANDLE handle = ::CreateFile(
											&cacheFilePath[0],
											genericDesiredAccess,
											shareAccess,
											&securityAttrib,
											creationDisposition,
											fileAttributesAndFlags,
											nullptr);
										if (handle == INVALID_HANDLE_VALUE)
										{
											ret = DokanNtStatusFromWin32(GetLastError());
										}
										else
										{
											dokanFileInfo->Context = reinterpret_cast<ULONG64>(handle); // save the file handle in Context
                      if ((creationDisposition == OPEN_ALWAYS) || (creationDisposition == CREATE_ALWAYS))
                      {
                        DWORD error = GetLastError();
                        if (error == ERROR_ALREADY_EXISTS) 
                        {
                          ret = STATUS_OBJECT_NAME_COLLISION;
                        }
                      }

											OpenFileInfo ofi;
											ofi.SiaPath = siaPath;
											ofi.CacheFilePath = cacheFilePath;
											// TODO Detect if file is read-only
                      ofi.Dummy = isDummy;
                      ofi.Changed = false;
                      std::lock_guard<std::mutex> l(_dokanMutex);
											_openFileMap.insert({ dokanFileInfo->Context, ofi });
										}
									}
								}
							}
						}
						else
						{
							ret = STATUS_INVALID_SERVER_STATE;
						}
					}
				}
				else
				{
					ret = STATUS_OBJECT_NAME_INVALID;
				}
			}
		  
		  CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanCreateFile(cacheFilePath, fileAttributesAndFlags, creationDisposition, genericDesiredAccess, ret)));
		}

		return ret;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_FindFiles(LPCWSTR fileName, PFillFindData fillFindData, PDOKAN_FILE_INFO dokanFileInfo)
	{
    NTSTATUS ret = STATUS_SUCCESS;
		auto siaFileTree = GetFileTree();
		if (siaFileTree)
		{
			SString siaFileQuery = CSiaApi::FormatToSiaPath(FilePath(fileName).SkipRoot());
			FilePath siaDirQuery = siaFileQuery;
      FilePath findFile = GetCacheLocation();
			FilePath cachePath = GetCacheLocation();
			if (FilePath::DirSep == fileName)
			{
				siaFileQuery += L"/*.*";
        findFile.Append("*.*");
			}
			else
			{
				cachePath.Append(fileName);
        findFile.Append(fileName);
				if (cachePath.IsDirectory())
				{
					siaFileQuery += L"/*.*";
          findFile.Append("*.*");
				}
				else
				{
					siaDirQuery = cachePath;
					siaDirQuery = CSiaApi::FormatToSiaPath(siaDirQuery.RemoveFileName());
				}
			}

      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanFindFiles(cachePath, siaDirQuery, siaFileQuery, findFile)));
      
      WIN32_FIND_DATA findData = { 0 };
      HANDLE findHandle = ::FindFirstFile(&findFile[0], &findData);
      if (findHandle == INVALID_HANDLE_VALUE)
      {
        DWORD error = GetLastError();
        ret = DokanNtStatusFromWin32(error);
      }
      else
      {
        // Find local files
        std::unordered_map<SString, std::uint8_t> dirs;
        std::unordered_map<SString, std::uint8_t> files;
        do
        {
          if ((wcscmp(fileName, L"\\") != 0) || (wcscmp(findData.cFileName, L".") != 0) && (wcscmp(findData.cFileName, L"..") != 0))
          {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
              dirs.insert({findData.cFileName, 0});
            }
            else
            {
              files.insert({ findData.cFileName, 1 });
            }
            fillFindData(&findData, dokanFileInfo);
          }
        } while (::FindNextFile(findHandle, &findData) != 0);
        ::FindClose(findHandle);

        // Find Sia directories
        auto dirList = siaFileTree->QueryDirectories(siaDirQuery);
        for (auto& dir : dirList)
        {
          if (dirs.find(dir) == dirs.end())
          {
            WIN32_FIND_DATA fd = { 0 };
            wcscpy_s(fd.cFileName, dir.str().c_str());
            fd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            fillFindData(&fd, dokanFileInfo);

            // Create cache sub-folder
            FilePath subCachePath(cachePath, dir);
            if (!subCachePath.IsDirectory())
            {
              subCachePath.CreateDirectory();
            }
          }
        }

        // Find Sia files
        auto fileList = siaFileTree->Query(siaFileQuery);
        for (auto& file : fileList)
        {
          FilePath fp = file->GetSiaPath();
          fp.StripToFileName();
          if (files.find(fp) == files.end())
          {
            WIN32_FIND_DATA fd = { 0 };
            wcscpy_s(fd.cFileName, &fp[0]);

            LARGE_INTEGER li = { 0 };
            li.QuadPart = file->GetFileSize();
            fd.nFileSizeHigh = li.HighPart;
            fd.nFileSizeLow = li.LowPart;
            fd.dwFileAttributes = FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NORMAL;
            fillFindData(&fd, dokanFileInfo);
          }
        }
      }
		}

		return ret;
	}

	static void DOKAN_CALLBACK Sia_CloseFile(LPCWSTR fileName, PDOKAN_FILE_INFO dokanFileInfo)
	{
    FilePath filePath(GetCacheLocation(), fileName);
    if (dokanFileInfo->Context) 
    {
      auto id = dokanFileInfo->Context;
      HANDLE handle = reinterpret_cast<HANDLE>(id);
      if (!dokanFileInfo->IsDirectory)
      {
        LARGE_INTEGER li = { 0 };
        ::GetFileSizeEx(handle, &li);
        HandleSiaFileClose(filePath, id, li.QuadPart, dokanFileInfo->DeleteOnClose ? true : false);
      }
      ::CloseHandle(reinterpret_cast<HANDLE>(dokanFileInfo->Context));
      dokanFileInfo->Context = 0;
    }
	}

	static NTSTATUS DOKAN_CALLBACK Sia_GetFileInformation(LPCWSTR fileName, LPBY_HANDLE_FILE_INFORMATION handleFileInfo, PDOKAN_FILE_INFO dokanFileInfo) 
	{
		HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
		BOOL opened = FALSE;
		NTSTATUS ret = STATUS_SUCCESS;

		FilePath cachePath(GetCacheLocation(), fileName);

    SString siaPath = CSiaApi::FormatToSiaPath(FilePath(fileName).SkipRoot());
    auto siaFileTree = GetFileTree();
    auto siaFile = siaFileTree ? siaFileTree->GetFile(siaPath) : nullptr;
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanGetFileInformation(cachePath)));

	  if (!siaFile && (!handle || (handle == INVALID_HANDLE_VALUE)))
		{
			handle = ::CreateFile(&cachePath[0], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
			if (handle == INVALID_HANDLE_VALUE) 
			{
				ret = DokanNtStatusFromWin32(::GetLastError());
			}
			else
			{
				opened = TRUE;
			}
		}

		if (ret == STATUS_SUCCESS)
		{
      bool isDummy = false;
      if (siaFile)
      {
        std::lock_guard<std::mutex> l(_fileTreeMutex);
        isDummy = _openFileMap.find(dokanFileInfo->Context) != _openFileMap.end();
      }

      if (isDummy)
      {
        LARGE_INTEGER li = { 0 };
        li.QuadPart = siaFile->GetFileSize();
        handleFileInfo->dwFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE;
        handleFileInfo->nFileSizeHigh = li.HighPart;
        handleFileInfo->nFileSizeLow = li.LowPart;
        ret = STATUS_SUCCESS;
      } 
		  else if (!::GetFileInformationByHandle(handle, handleFileInfo))
			{
				// fileName is a root directory
				// in this case, FindFirstFile can't get directory information
				if (wcscmp(fileName, L"\\") == 0)
				{
					handleFileInfo->dwFileAttributes = ::GetFileAttributes(&cachePath[0]);
				}
				else
				{
					WIN32_FIND_DATAW find = { 0 };
					HANDLE findHandle = ::FindFirstFile(&cachePath[0], &find);
					if (findHandle == INVALID_HANDLE_VALUE)
					{
            DWORD error = ::GetLastError();
            ret = DokanNtStatusFromWin32(error);
					}
					else
					{
						handleFileInfo->dwFileAttributes = find.dwFileAttributes;
						handleFileInfo->ftCreationTime = find.ftCreationTime;
						handleFileInfo->ftLastAccessTime = find.ftLastAccessTime;
						handleFileInfo->ftLastWriteTime = find.ftLastWriteTime;
						handleFileInfo->nFileSizeHigh = find.nFileSizeHigh;
						handleFileInfo->nFileSizeLow = find.nFileSizeLow;

						::FindClose(findHandle);
					}
				}
			}
		}

		if (opened)
		{
			::CloseHandle(handle);
		}

		return ret;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_Mounted(PDOKAN_FILE_INFO dokanFileInfo)
	{
    // May spend a little wait time here while files are cleaned-up and re-added to queue
    _uploadManager.reset(new CUploadManager(CSiaCurl(_siaApi->GetHostConfig()), _siaDriveConfig));
		StartFileListThread();

    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DriveMounted(_mountPoint)));
		return STATUS_SUCCESS;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_Unmounted(PDOKAN_FILE_INFO dokanFileInfo)
	{
    _uploadManager.reset(nullptr);
		StopFileListThread();
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DriveUnMounted(_mountPoint)));
		return STATUS_SUCCESS;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_GetDiskFreeSpaceW(
		PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes,
		PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO dokanFileInfo)
	{
		UNREFERENCED_PARAMETER(dokanFileInfo);

		// TODO Implement this correctly
		*FreeBytesAvailable = static_cast<ULONGLONG>(512 * 1024 * 1024);
		*TotalNumberOfBytes = 9223372036854775807;
		*TotalNumberOfFreeBytes = 9223372036854775807;

		return STATUS_SUCCESS;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_GetVolumeInformationW(
		LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber,
		LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags,
		LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize,
		PDOKAN_FILE_INFO dokanFileInfo) {
		UNREFERENCED_PARAMETER(dokanFileInfo);
		
		wcscpy_s(VolumeNameBuffer, VolumeNameSize, L"SiaDrive");
		*VolumeSerialNumber = 0x19831116;
		*MaximumComponentLength = 256;
		*FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES |
			FILE_SUPPORTS_REMOTE_STORAGE | FILE_UNICODE_ON_DISK |
			FILE_PERSISTENT_ACLS;

		// File system name could be anything up to 10 characters.
		// But Windows check few feature availability based on file system name.
		// For this, it is recommended to set NTFS or FAT here.
		wcscpy_s(FileSystemNameBuffer, FileSystemNameSize, L"NTFS");

		return STATUS_SUCCESS;
	}


	static NTSTATUS DOKAN_CALLBACK Sia_ReadFile(LPCWSTR fileName, LPVOID buffer,
		DWORD bufferLen,
		LPDWORD readLength,
		LONGLONG offset,
		PDOKAN_FILE_INFO dokanFileInfo) 
	{
    // TODO Check dummy and add to cache if not found
		FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanReadFile(filePath)));

		HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
		BOOL opened = FALSE;
		if (!handle || (handle == INVALID_HANDLE_VALUE))
		{
      // TODO Need to add to cache if missing
			handle = ::CreateFile(&filePath[0], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
			if (handle == INVALID_HANDLE_VALUE) 
			{
				DWORD error = GetLastError();
				return DokanNtStatusFromWin32(error);
			}
			opened = TRUE;
		}

		LARGE_INTEGER distanceToMove;
		distanceToMove.QuadPart = offset;
		if (!::SetFilePointerEx(handle, distanceToMove, nullptr, FILE_BEGIN)) 
		{
			DWORD error = GetLastError();
			if (opened)
				CloseHandle(handle);
			return DokanNtStatusFromWin32(error);
		}

		if (!::ReadFile(handle, buffer, bufferLen, readLength, nullptr)) 
		{
			DWORD error = GetLastError();
			if (opened)
				CloseHandle(handle);
			return DokanNtStatusFromWin32(error);
		}

		if (opened)
			CloseHandle(handle);

		return STATUS_SUCCESS;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_WriteFile(LPCWSTR fileName, LPCVOID buffer,
		DWORD bytesToWrite,
		LPDWORD bytesWritten,
		LONGLONG offset,
		PDOKAN_FILE_INFO dokanFileInfo) 
	{
    // TODO Check dummy and add to cache if not found
		FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanWriteFile(filePath)));

		HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
		BOOL opened = FALSE;

		// reopen the file
		if (!handle || (handle == INVALID_HANDLE_VALUE))
		{
			handle = ::CreateFile(&filePath[0], GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			if (handle == INVALID_HANDLE_VALUE) 
			{
				DWORD error = GetLastError();
				return DokanNtStatusFromWin32(error);
			}
			opened = TRUE;
		}

    LARGE_INTEGER li = { 0 };
		if (!::GetFileSizeEx(handle, &li)) 
		{
			DWORD error = GetLastError();
			if (opened)
				CloseHandle(handle);
			return DokanNtStatusFromWin32(error);
		}

		LARGE_INTEGER distanceToMove;
		if (dokanFileInfo->WriteToEndOfFile) 
		{
			LARGE_INTEGER z = {0};
			if (!::SetFilePointerEx(handle, z, nullptr, FILE_END)) 
			{
				DWORD error = GetLastError();
				if (opened)
					::CloseHandle(handle);
				return DokanNtStatusFromWin32(error);
			}
		}
		else 
		{
			// Paging IO cannot write after allocate file size.
			if (dokanFileInfo->PagingIo) 
			{
				if (static_cast<UINT64>(offset) >= li.QuadPart) 
				{
					*bytesWritten = 0;
					if (opened)
						CloseHandle(handle);
					return STATUS_SUCCESS;
				}

				if (static_cast<UINT64>(offset + bytesToWrite) > li.QuadPart) 
				{
					UINT64 bytes = li.QuadPart - offset;
					if (bytes >> 32) 
					{
						bytesToWrite = static_cast<DWORD>(bytes & 0xFFFFFFFFUL);
					}
					else 
					{
						bytesToWrite = static_cast<DWORD>(bytes);
					}
				}
			}

			if (static_cast<UINT64>(offset) > li.QuadPart) 
			{
				// In the mirror sample helperZeroFileData is not necessary. NTFS will
				// zero a hole.
				// But if user's file system is different from NTFS( or other Windows's
				// file systems ) then  users will have to zero the hole themselves.
			}

			distanceToMove.QuadPart = offset;
			if (!::SetFilePointerEx(handle, distanceToMove, nullptr, FILE_BEGIN)) 
			{
				DWORD error = GetLastError();
				if (opened)
					CloseHandle(handle);
				return DokanNtStatusFromWin32(error);
			}
		}

		if (::WriteFile(handle, buffer, bytesToWrite, bytesWritten, nullptr)) 
		{
      std::lock_guard<std::mutex> l(GetMutex());
      _openFileMap.at(reinterpret_cast<ULONG64>(handle)).Changed = true;
		}
		else 
    {
			DWORD error = GetLastError();
			if (opened)
				CloseHandle(handle);
			return DokanNtStatusFromWin32(error);
		}

		// close the file when it is reopene bxkjuoqoa'qq d
		if (opened)
			CloseHandle(handle);

		return STATUS_SUCCESS;
	}

	static NTSTATUS DOKAN_CALLBACK Sia_SetEndOfFile(LPCWSTR fileName, LONGLONG byteOffset, PDOKAN_FILE_INFO dokanFileInfo) 
	{
    NTSTATUS ret = STATUS_SUCCESS;
    // TODO Check dummy and add to cache if not found

    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanSetEndOfFile(filePath)));

		HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
		if (!handle || (handle == INVALID_HANDLE_VALUE))
		{
			ret = STATUS_INVALID_HANDLE;
		}
    else
    {
      LARGE_INTEGER sz = { 0 };
      ::GetFileSizeEx(handle, &sz);

      LARGE_INTEGER offset;
      offset.QuadPart = byteOffset;
      if (!::SetFilePointerEx(handle, offset, nullptr, FILE_BEGIN))
      {
        DWORD error = GetLastError();
        ret = DokanNtStatusFromWin32(error);
      }
      else if (!::SetEndOfFile(handle))
      {
        DWORD error = GetLastError();
        ret = DokanNtStatusFromWin32(error);
      }

      if (ret == STATUS_SUCCESS)
      {
        std::lock_guard<std::mutex> l(GetMutex());
        _openFileMap.at(reinterpret_cast<ULONG64>(handle)).Changed = (offset.QuadPart != (sz.QuadPart - 1)) ;
      }
    }

		return ret;
	}

	static void DOKAN_CALLBACK Sia_Cleanup(LPCWSTR fileName, PDOKAN_FILE_INFO dokanFileInfo) 
	{
		FilePath filePath(GetCacheLocation(), fileName);
    ULONG64 id = dokanFileInfo->DeleteOnClose;
		if (dokanFileInfo->Context) 
		{
      HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
      if (!dokanFileInfo->IsDirectory)
      {
        LARGE_INTEGER li = { 0 };
        ::GetFileSizeEx(handle, &li);
        HandleSiaFileClose(filePath, dokanFileInfo->Context, li.QuadPart, dokanFileInfo->DeleteOnClose ? true : false);
      }

			::CloseHandle(handle);
			dokanFileInfo->Context = 0;
		}

		if (dokanFileInfo->DeleteOnClose) 
		{
			// Should already be deleted by CloseHandle
			// if open with FILE_FLAG_DELETE_ON_CLOSE
			if (dokanFileInfo->IsDirectory) 
			{
				if (filePath.RemoveDirectory()) 
				{
				}
				else 
				{
				}
			}
			else 
			{
				if (filePath.DeleteFile()) 
				{
				}
				else 
				{
				}
			}
		}
	}

	static NTSTATUS DOKAN_CALLBACK Sia_FlushFileBuffers(LPCWSTR fileName, PDOKAN_FILE_INFO dokanFileInfo) 
	{
    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanFlushFileBuffers(filePath)));

		HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
		if (!handle || (handle == INVALID_HANDLE_VALUE))
		{
			return STATUS_SUCCESS;
		}

		if (::FlushFileBuffers(handle)) 
		{
			return STATUS_SUCCESS;
		}
		else 
		{
			DWORD error = GetLastError();
			return DokanNtStatusFromWin32(error);
		}
	}

  static NTSTATUS DOKAN_CALLBACK Sia_DeleteDirectory(LPCWSTR fileName, PDOKAN_FILE_INFO dokanFileInfo) 
  {
    FilePath filePath = FilePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanDeleteDirectory(filePath)));

    NTSTATUS ret = STATUS_SUCCESS;
    if (dokanFileInfo->DeleteOnClose)
    {
      filePath.Append("*");

      WIN32_FIND_DATAW findData;
      HANDLE findHandle = ::FindFirstFile(&filePath[0], &findData);
      if (findHandle == INVALID_HANDLE_VALUE)
      {
        DWORD error = GetLastError();
        ret = DokanNtStatusFromWin32(error);
      }
      else
      {
        do
        {
          if ((wcscmp(findData.cFileName, L"..") != 0) && (wcscmp(findData.cFileName, L".") != 0))
          {
            ret = STATUS_DIRECTORY_NOT_EMPTY;
          }
        } 
        while ((ret == STATUS_SUCCESS) && (::FindNextFile(findHandle, &findData) != 0));
        
        DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_FILES)
        {
          ret = DokanNtStatusFromWin32(error);
        }
        
        ::FindClose(findHandle);
      }
    }

    return ret;
  }

  static NTSTATUS DOKAN_CALLBACK Sia_DeleteFileW(LPCWSTR fileName, PDOKAN_FILE_INFO dokanFileInfo)
  {
    // TODO Check dummy and add to cache if not found
    NTSTATUS ret = STATUS_SUCCESS;

    HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanDeleteFileW(filePath)));

    DWORD dwAttrib = ::GetFileAttributes(&filePath[0]);
    if ((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
    {
      ret = STATUS_ACCESS_DENIED;
    }
    else if (handle && (handle != INVALID_HANDLE_VALUE)) 
    {
      FILE_DISPOSITION_INFO fdi;
      fdi.DeleteFile = dokanFileInfo->DeleteOnClose;
      if (!::SetFileInformationByHandle(handle, FileDispositionInfo, &fdi, sizeof(FILE_DISPOSITION_INFO)))
      {
        ret = DokanNtStatusFromWin32(GetLastError());
      }
    }

    return ret;
  }

  static NTSTATUS DOKAN_CALLBACK Sia_MoveFileW(LPCWSTR fileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO dokanFileInfo)
  {
    // TODO Check dummy and add to cache if not found
    NTSTATUS ret = STATUS_SUCCESS;

    FilePath filePath(GetCacheLocation(), fileName);
    FilePath newFilePath(GetCacheLocation(), NewFileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanMoveFileW(filePath, newFilePath)));
    
	  HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
    if (!handle || (handle == INVALID_HANDLE_VALUE))
    {
      ret = STATUS_INVALID_HANDLE;
    }
    else
    {
      size_t len = wcslen(&newFilePath[0]);
      DWORD bufferSize = static_cast<DWORD>(sizeof(FILE_RENAME_INFO) + (len * sizeof(newFilePath[0])));

      PFILE_RENAME_INFO renameInfo = static_cast<PFILE_RENAME_INFO>(malloc(bufferSize));
      if (renameInfo)
      {
        ::ZeroMemory(renameInfo, bufferSize);

        renameInfo->ReplaceIfExists = ReplaceIfExisting ? TRUE : FALSE; // some warning about converting BOOL to BOOLEAN
        renameInfo->RootDirectory = nullptr; // hope it is never needed, shouldn't be
        renameInfo->FileNameLength = static_cast<DWORD>(len) * sizeof(newFilePath[0]); // they want length in bytes

        wcscpy_s(renameInfo->FileName, len + 1, &newFilePath[0]);

        BOOL result = ::SetFileInformationByHandle(handle, FileRenameInfo, renameInfo, bufferSize);
        free(renameInfo);

        if (!result)
        {
          DWORD error = GetLastError();
          ret = DokanNtStatusFromWin32(error);
        }
      }
      else
      {
        ret = STATUS_BUFFER_OVERFLOW;
      }
    }

    return ret;
  }

  static NTSTATUS DOKAN_CALLBACK Sia_SetFileAttributesW(LPCWSTR fileName, DWORD fileAttributes, PDOKAN_FILE_INFO dokanFileInfo)
  {
    UNREFERENCED_PARAMETER(dokanFileInfo);
    NTSTATUS ret = STATUS_SUCCESS;

    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanSetFileAttributesW(filePath)));

    if (!::SetFileAttributes(&filePath[0], fileAttributes)) 
    {
      DWORD error = GetLastError();
      ret = DokanNtStatusFromWin32(error);
    }

    return ret;
  }

  static NTSTATUS DOKAN_CALLBACK Sia_GetFileSecurityW(
    LPCWSTR fileName, PSECURITY_INFORMATION securityInfo,
    PSECURITY_DESCRIPTOR securityDescriptor, ULONG bufferLen,
    PULONG lengthNeeded, PDOKAN_FILE_INFO dokanFileInfo) 
  {
    UNREFERENCED_PARAMETER(dokanFileInfo);
    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanGetFileAttributesW(filePath)));
    SECURITY_INFORMATION requestingSaclInfo = ((*securityInfo & SACL_SECURITY_INFORMATION) || (*securityInfo & BACKUP_SECURITY_INFORMATION));

    //if (!g_HasSeSecurityPrivilege) {
    if (true)
    {
      *securityInfo &= ~SACL_SECURITY_INFORMATION;
      *securityInfo &= ~BACKUP_SECURITY_INFORMATION;
    }

    HANDLE handle = ::CreateFile(&filePath[0], READ_CONTROL | ((requestingSaclInfo && false) ? ACCESS_SYSTEM_SECURITY : 0),
      FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
      nullptr, // security attribute
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS, // |FILE_FLAG_NO_BUFFERING,
      nullptr);

    if (!handle || (handle == INVALID_HANDLE_VALUE)) 
    {
      int error = ::GetLastError();
      return DokanNtStatusFromWin32(error);
    }

    if (!::GetUserObjectSecurity(handle, securityInfo, securityDescriptor, bufferLen, lengthNeeded)) 
    {
      int error = ::GetLastError();
      if (error == ERROR_INSUFFICIENT_BUFFER) 
      {
        ::CloseHandle(handle);
        return STATUS_BUFFER_OVERFLOW;
      }
      else 
      {
        ::CloseHandle(handle);
        return DokanNtStatusFromWin32(error);
      }
    }
    ::CloseHandle(handle);

    return STATUS_SUCCESS;
  }


  static NTSTATUS DOKAN_CALLBACK Sia_SetFileSecurityW(
    LPCWSTR fileName, PSECURITY_INFORMATION securityInfo,
    PSECURITY_DESCRIPTOR securityDescriptor, ULONG securityDescriptorLength,
    PDOKAN_FILE_INFO dokanFileInfo) 
  {
    UNREFERENCED_PARAMETER(securityDescriptorLength);

    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanSetFileSecurityW(filePath)));

    HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
    if (!handle || (handle == INVALID_HANDLE_VALUE))
    {
      return STATUS_INVALID_HANDLE;
    }

    if (!::SetUserObjectSecurity(handle, securityInfo, securityDescriptor))
    {
      int error = ::GetLastError();
      return DokanNtStatusFromWin32(error);
    }

    return STATUS_SUCCESS;
  }

  static NTSTATUS DOKAN_CALLBACK Sia_SetFileTime(LPCWSTR fileName, CONST FILETIME *creationTime,
      CONST FILETIME *lastAccessTime, CONST FILETIME *lastWriteTime,
      PDOKAN_FILE_INFO dokanFileInfo) 
  {
    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanSetFileTime(filePath)));

    HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
    if (!handle || (handle == INVALID_HANDLE_VALUE))
    {
      return STATUS_INVALID_HANDLE;
    }

    if (!::SetFileTime(handle, creationTime, lastAccessTime, lastWriteTime)) 
    {
      DWORD error = GetLastError();
      return DokanNtStatusFromWin32(error);
    }

    return STATUS_SUCCESS;
  }

  static NTSTATUS DOKAN_CALLBACK Sia_SetAllocationSize(LPCWSTR fileName, LONGLONG allocSize, PDOKAN_FILE_INFO dokanFileInfo) 
  {
    // TODO Check dummy and add to cache if not found
    NTSTATUS ret = STATUS_SUCCESS;
    FilePath filePath(GetCacheLocation(), fileName);
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DokanSetAllocationSize(filePath)));

    HANDLE handle = reinterpret_cast<HANDLE>(dokanFileInfo->Context);
    if (!handle || (handle == INVALID_HANDLE_VALUE))
    {
      ret = STATUS_INVALID_HANDLE;
    }
    else
    {
      LARGE_INTEGER fileSize = { 0 };
      if (::GetFileSizeEx(handle, &fileSize))
      {
        if (allocSize < fileSize.QuadPart)
        {
          fileSize.QuadPart = allocSize;
          if (!::SetFilePointerEx(handle, fileSize, nullptr, FILE_BEGIN))
          {
            DWORD error = GetLastError();
            ret = DokanNtStatusFromWin32(error);
          }
          else if (!::SetEndOfFile(handle))
          {
            DWORD error = GetLastError();
            ret = DokanNtStatusFromWin32(error);
          }
        }
      }
      else
      {
        DWORD error = GetLastError();
        ret = DokanNtStatusFromWin32(error);
      }
    }

    return ret;
  }

public:
	static void Initialize(CSiaApi* siaApi, CSiaDriveConfig* siaDriveConfig)
	{
		_siaApi = siaApi;
		_siaDriveConfig = siaDriveConfig;
		_dokanOps.Cleanup = Sia_Cleanup;
		_dokanOps.CloseFile = Sia_CloseFile;
		_dokanOps.DeleteDirectory = Sia_DeleteDirectory;
		_dokanOps.DeleteFileW = Sia_DeleteFileW;
		_dokanOps.FindFiles = Sia_FindFiles;
		_dokanOps.FindFilesWithPattern = nullptr;
		_dokanOps.FindStreams = nullptr;
		_dokanOps.FlushFileBuffers = Sia_FlushFileBuffers;
		_dokanOps.GetDiskFreeSpaceW = Sia_GetDiskFreeSpaceW;
		_dokanOps.GetFileInformation = Sia_GetFileInformation;
		_dokanOps.GetFileSecurityW = Sia_GetFileSecurityW;
		_dokanOps.GetVolumeInformationW = Sia_GetVolumeInformationW;
		_dokanOps.LockFile = nullptr;
		_dokanOps.Mounted = Sia_Mounted;
		_dokanOps.MoveFileW = Sia_MoveFileW;
		_dokanOps.ReadFile = Sia_ReadFile;
		_dokanOps.SetAllocationSize = Sia_SetAllocationSize;
		_dokanOps.SetEndOfFile = Sia_SetEndOfFile;
		_dokanOps.SetFileAttributesW = Sia_SetFileAttributesW;
		_dokanOps.SetFileSecurityW = Sia_SetFileSecurityW;
		_dokanOps.SetFileTime = Sia_SetFileTime;
		_dokanOps.UnlockFile = nullptr;
		_dokanOps.Unmounted = Sia_Unmounted;
		_dokanOps.WriteFile = Sia_WriteFile;
		_dokanOps.ZwCreateFile = Sia_ZwCreateFile;

		ZeroMemory(&_dokanOptions, sizeof(DOKAN_OPTIONS));
		_dokanOptions.Version = DOKAN_VERSION;
		_dokanOptions.ThreadCount = 0; // use default
#ifdef _DEBUG
		_dokanOptions.Options = DOKAN_OPTION_DEBUG | DOKAN_OPTION_DEBUG_LOG_FILE;
#endif
	}

	static void Mount(const wchar_t& driveLetter, const SString& cacheLocation)
	{
		if (_siaApi && !_mountThread)
		{
			_cacheLocation = cacheLocation;
			wchar_t tmp[] = { driveLetter, ':', '\\', 0 };
			_mountPoint = tmp;
			_mountThread.reset(new std::thread([&]()
			{
				_dokanOptions.MountPoint = _mountPoint.ToUpper().str().c_str();
				_mountStatus = DokanMain(&_dokanOptions, &_dokanOps);
        CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DriveMountEnded(_mountPoint, _mountStatus)));
			}));
		}
	}

	static void Unmount()
	{
		if (_mountThread)
		{
      while (!DokanRemoveMountPoint(&_mountPoint[0]))
        ::Sleep(1000);

      _mountThread->join();
      _mountThread.reset(nullptr);
			_mountPoint = "";
		}
	}

	static void Shutdown()
	{
		StopFileListThread();
		Unmount();
		_siaApi = nullptr;
		_siaDriveConfig = nullptr;
		ZeroMemory(&_dokanOps, sizeof(_dokanOps));
		ZeroMemory(&_dokanOptions, sizeof(_dokanOptions));
	}

	static std::mutex& GetMutex()
	{
		return _dokanMutex;
	}

  static bool IsMounted()
	{
    return (_mountThread != nullptr);
	}

	static bool IsInitialized()
	{
		return (_siaApi != nullptr);
	}
};
// Static member variables
std::mutex DokanImpl::_dokanMutex;
CSiaApi* DokanImpl::_siaApi = nullptr;
CSiaDriveConfig* DokanImpl::_siaDriveConfig = nullptr;
std::unique_ptr<CUploadManager> DokanImpl::_uploadManager;
DOKAN_OPERATIONS DokanImpl::_dokanOps;
DOKAN_OPTIONS DokanImpl::_dokanOptions;
FilePath DokanImpl::_cacheLocation;
HANDLE DokanImpl::_fileListStopEvent;
CSiaFileTreePtr DokanImpl::_siaFileTree;
std::mutex DokanImpl::_fileTreeMutex;
std::unique_ptr<std::thread> DokanImpl::_fileListThread;
std::unordered_map<ULONG64, DokanImpl::OpenFileInfo> DokanImpl::_openFileMap;
std::unique_ptr<std::thread> DokanImpl::_mountThread;
NTSTATUS DokanImpl::_mountStatus = STATUS_SUCCESS;
SString DokanImpl::_mountPoint;

CSiaDokanDrive::CSiaDokanDrive(CSiaApi& siaApi, CSiaDriveConfig* siaDriveConfig) :
	_siaApi(siaApi),
	_siaDriveConfig(siaDriveConfig)
{
	std::lock_guard<std::mutex> l(DokanImpl::GetMutex());
	if (DokanImpl::IsInitialized())
		throw SiaDokanDriveException("Sia drive has already been activated");
	
	DokanImpl::Initialize(&_siaApi, _siaDriveConfig);
}

CSiaDokanDrive::~CSiaDokanDrive()
{
	std::lock_guard<std::mutex> l(DokanImpl::GetMutex());
	DokanImpl::Shutdown();
}

bool CSiaDokanDrive::IsMounted() const
{
  return DokanImpl::IsMounted();
}

void CSiaDokanDrive::Mount(const wchar_t& driveLetter, const SString& cacheLocation, const std::uint64_t& maxCacheSizeBytes)
{
	std::lock_guard<std::mutex> l(DokanImpl::GetMutex());
	DokanImpl::Mount(driveLetter, cacheLocation);
}

void CSiaDokanDrive::Unmount(const bool& clearCache)
{
	std::lock_guard<std::mutex> l(DokanImpl::GetMutex());
	DokanImpl::Unmount();
}

void CSiaDokanDrive::ClearCache()
{
	std::lock_guard<std::mutex> l(DokanImpl::GetMutex());
}