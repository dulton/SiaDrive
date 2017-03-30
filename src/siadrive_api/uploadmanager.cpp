#include <SQLiteCpp/Exception.h>
#include <uploadmanager.h>
#include <siaapi.h>
#include <eventsystem.h>
#include <siadriveconfig.h>
#include <filepath.h>
using namespace Sia::Api;

#define TABLE_CREATE L"create table if not exists %s (%s);"
#define UPLOAD_TABLE L"upload_table"
#define UPLOAD_TABLE_COLUMNS L"id integer primary key autoincrement, sia_path text unique not null, file_path text unique not null, sd_file_path text unique not null, status integer not null"
#define QUERY_STATUS "select * from upload_table where sia_path=@sia_path order by id desc limit 1;"
#define QUERY_UPLOADS_BY_STATUS "select * from upload_table where status=@status order by id desc limit 1;"
#define QUERY_UPLOADS_BY_2_STATUS "select * from upload_table where (status=@status1 or status=@status2) order by id desc limit 1;"
#define QUERY_UPLOADS_BY_SIA_PATH "select * from upload_table where sia_path=@sia_path order by id desc limit 1;"
#define QUERY_UPLOADS_BY_SIA_PATH_AND_2_STATUS "select * from upload_table where sia_path=@sia_path and (status=@status1 or status=@status2) order by id desc limit 1;"
#define UPDATE_STATUS "update upload_table set status=@status where sia_path=@sia_path;"
#define INSERT_UPLOAD "insert into upload_table (sia_path, status, file_path, sd_file_path) values (@sia_path, @status, @file_path, @sd_file_path);"
#define DELETE_UPLOAD "delete from upload_table where sia_path=@sia_path;"

#define SET_STATUS(status, success_event, fail_event)\
bool statusUpdated = false;\
try\
{\
	SQLite::Statement update(_uploadDatabase, UPDATE_STATUS);\
	update.bind("@sia_path", SString::ToUtf8(siaPath).c_str());\
	update.bind("@status", static_cast<unsigned>(status));\
	if (update.exec() != 1)\
	{\
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(fail_event(siaPath, filePath, status, update.getErrorMsg())));\
	}\
	else\
	{\
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(success_event(siaPath, filePath)));\
		statusUpdated = true;\
	}\
}\
catch (SQLite::Exception e)\
{\
	CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("SetStatus", e)));\
}

template <typename... Ts>
SString fmt(const SString &fmt, Ts... vs)
{
	size_t required = _snwprintf(nullptr, 0, fmt.str().c_str(), vs...);

	SString ret;
	ret.Resize(required);
	_snwprintf(&ret[0], required, fmt.str().c_str(), vs...);

	return ret;
}

static void CreateTableIfNotFound(SQLite::Database* database, const SString& tableName, const SString& columns)
{
	SString sqlCreate = fmt(TABLE_CREATE, &tableName[0], &columns[0]);
	database->exec(SString::ToUtf8(sqlCreate).c_str());
}

SString CUploadManager::UploadStatusToString(const UploadStatus& uploadStatus)
{
	switch (uploadStatus)
	{
	case UploadStatus::Complete:
		return L"Complete";

	case UploadStatus::Error:
		return L"Error";

	case UploadStatus::Modified:
		return L"Modified";

	case UploadStatus::NotFound:
		return L"Not Found";

	case UploadStatus::Queued:
		return L"Queued";

	case UploadStatus::Uploading:
		return L"Uploading";

	default:
		return L"!!Not Defined!!";
	}
}

CUploadManager::CUploadManager(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
	CAutoThread(siaCurl, siaDriveConfig),
	_uploadDatabase(siaDriveConfig->GetRenter_UploadDbFilePath(), SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE)
{
	CreateTableIfNotFound(&_uploadDatabase, UPLOAD_TABLE, UPLOAD_TABLE_COLUMNS);

	// Clean-up cache folder
	if (!RecurDeleteFilesByExtentsion(siaDriveConfig->GetCacheFolder(), L".siadrive"))
	{
		throw StartupException(L"Failed to remove '.siadrive' files");
	}
	
	if (!RecurDeleteFilesByExtentsion(siaDriveConfig->GetCacheFolder(), L".siadrive.temp"))
	{
		throw StartupException(L"Failed to remove '.siadrive.temp' files");
	}

	// Detect files that have been removed since last startup
	DeleteFilesRemovedFromSia(siaCurl, siaDriveConfig, true);

	// Begin normal processing
	StartAutoThread();
}

CUploadManager::~CUploadManager()
{
	// Stop all processing
	StopAutoThread();
}

void CUploadManager::DeleteFilesRemovedFromSia(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, const bool& isStartup)
{
	CSiaFileTreePtr fileTree(new CSiaFileTree(siaCurl, siaDriveConfig));
	json result;
	SiaCurlError cerror = siaCurl.Get(L"/renter/files", result);
	if (ApiSuccess(cerror))
	{
		fileTree->BuildTree(result);
		auto fileList = fileTree->GetFileList();
    // TODO Implement this
	}
	else
	{
		if (isStartup)
		{
			throw StartupException(L"Failed to get Sia files");
		}
	}
}

void CUploadManager::RemoveFileFromSia(const CSiaCurl& siaCurl, const SString& siaPath, FilePath removeFilePath = SString("*"))
{
  json response;
  SiaCurlError cerror = siaCurl.Post(SString(L"/renter/delete/") + siaPath, {}, response);
  if (ApiSuccess(cerror))
  {
    SQLite::Statement del(_uploadDatabase, DELETE_UPLOAD);
    del.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
    auto ret = del.exec();
    if (ret >= 0)
    {
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FileRemoved(siaPath, removeFilePath)));
    }
    else
    {
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseDeleteFailed(siaPath, removeFilePath, del.getErrorMsg())));
    }
  }
  else
  {
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FailedToDeleteFromSia(siaPath, removeFilePath, cerror)));
  }
}

void CUploadManager::HandleFileRemove(const CSiaCurl& siaCurl, const SString& siaPath, const SString& siaDriveFilePath)
{
	try
	{
		std::lock_guard<std::mutex> l(_uploadMutex);
		SQLite::Statement query(_uploadDatabase, QUERY_STATUS);
		query.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
		if (query.executeStep())
		{
			FilePath removeFilePath = SString(static_cast<const char*>(query.getColumn(query.getColumnIndex("file_path"))));
			UploadStatus uploadStatus = static_cast<UploadStatus>(static_cast<unsigned>(query.getColumn(query.getColumnIndex("status"))));
			// Make sure status is still remove
			
			bool deleteFromSia = true;
			if (removeFilePath.IsFile())
			{
				if (RetryDeleteFileIfExists(removeFilePath))
				{
					if (!RetryDeleteFileIfExists(siaDriveFilePath))
					{
						CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteSiaDriveFileFailed(siaPath, removeFilePath, siaDriveFilePath)));
					}
				}
				else
				{
					CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(RemoveFileFailed(siaPath, removeFilePath)));
					deleteFromSia = false;
				}
			}
					
			if (deleteFromSia)
			{
				RemoveFileFromSia(siaCurl, siaPath, removeFilePath);
			}
		}
	}
	catch (SQLite::Exception e)
	{
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("HandleFileRemove", e)));
	}
}

bool CUploadManager::CreateSiaDriveFile(const SString& siaPath, const SString& filePath, const SString& tempSourcePath, const SString& siaDriveFilePath)
{
  bool ret = false;
	CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(CreatingTemporarySiaDriveFile(siaPath, filePath, tempSourcePath)));
	if (RetryableAction(::CopyFile(filePath.str().c_str(), tempSourcePath.str().c_str(), FALSE), DEFAULT_RETRY_COUNT, DEFAULT_RETRY_DELAY_MS))
	{
		// Delete existing '.siadrive' file, if found
		//	!!Should never come here. If so, there was a problem with startup clean-up
		if (!RetryDeleteFileIfExists(siaDriveFilePath))
		{
			CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteSiaDriveFileFailed(siaPath, filePath, siaDriveFilePath)));
		}

		// Rename '.siadrive.temp' to '.siadrive'
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(RenamingTemporarySiaDriveFile(siaPath, filePath, tempSourcePath, siaDriveFilePath)));
		if (RetryableAction(::MoveFile(tempSourcePath.str().c_str(), siaDriveFilePath.str().c_str()), DEFAULT_RETRY_COUNT, DEFAULT_RETRY_DELAY_MS))
		{
			if (RetryDeleteFileIfExists(tempSourcePath))
			{
        ret = true;
			}
      else
			{
				CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteTemporarySiaDriveFileFailed(siaPath, filePath, tempSourcePath)));
			}
		}
		else
		{
			CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(RenamingTemporarySiaDriveFileFailed(siaPath, filePath, tempSourcePath, siaDriveFilePath)));
			if (!RetryDeleteFileIfExists(tempSourcePath))
			{
				CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteTemporarySiaDriveFileFailed(siaPath, filePath, tempSourcePath)));
			}

			if (!RetryDeleteFileIfExists(siaDriveFilePath))
			{
				CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteSiaDriveFileFailed(siaPath, filePath, siaDriveFilePath)));
			}
		}
	}
  else
  {
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(CreatingTemporarySiaDriveFileFailed(siaPath, filePath, tempSourcePath)));

    // If temp copy fails, try to delete
    //	If partial copy and file is unable to be deleted, log warning
    if (!RetryDeleteFileIfExists(tempSourcePath))
    {
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteTemporarySiaDriveFileFailed(siaPath, filePath, tempSourcePath)));
    }
  }
  return ret;
}

void CUploadManager::AutoThreadCallback(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
	bool processNext = true;

	try
	{
		CSiaFileTreePtr fileTree(new CSiaFileTree(siaCurl, siaDriveConfig));
		json result;
		if (ApiSuccess(siaCurl.Get(L"/renter/files", result)))
		{
			// Lock here - if file is modified again before previously queued upload is complete, delete it and 
			//	start again later
			std::lock_guard<std::mutex> l(_uploadMutex);
			SQLite::Statement query(_uploadDatabase, QUERY_UPLOADS_BY_2_STATUS);
			query.bind("@status1", static_cast<unsigned>(UploadStatus::Uploading));
      query.bind("@status2", static_cast<unsigned>(UploadStatus::Modified));

			fileTree->BuildTree(result);
			if (query.executeStep())
			{
				SString siaPath = static_cast<const char*>(query.getColumn(query.getColumnIndex("sia_path")));
				SString filePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("file_path")));
				SString siaDriveFilePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("sd_file_path")));
				UploadStatus uploadStatus = static_cast<UploadStatus>(query.getColumn(query.getColumnIndex("status")).getUInt());

				auto fileList = fileTree->GetFileList();
				auto it = std::find_if(fileList.begin(), fileList.end(), [&](const CSiaFilePtr& ptr)
				{
					return ptr->GetSiaPath() == siaPath;
				});
				
				// Removed by another client
        if (it == fileList.end())
        {
          // TODO SET_STATUS(UploadStatus::Remove, ExternallyRemovedFileDetected, ModifyUploadStatusFailed)
          HandleFileRemove(siaCurl, siaPath, siaDriveFilePath);
        }
				// Changed file detected
				else if (uploadStatus == UploadStatus::Modified)
				{
					json response;
					SiaCurlError cerror = siaCurl.Post(SString(L"/renter/delete/") + siaPath, {}, response);
					if (ApiSuccess(cerror))
					{
						// TODO validate response
						SET_STATUS(UploadStatus::Queued, ModifiedUploadQueued, ModifyUploadStatusFailed)
					}
					else
					{
						CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FailedToDeleteFromSia(siaPath, filePath, cerror)));
					}
				}
				// Upload is complete
				else if ((*it)->GetUploadProgress() >= 100)
				{
					SET_STATUS(UploadStatus::Complete, UploadComplete, ModifyUploadStatusFailed)
					if (!RetryDeleteFileIfExists(siaDriveFilePath))
					{
						CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DeleteSiaDriveFileFailed(siaPath, filePath, siaDriveFilePath)));
					}
				}
				// Upload still active, don't process another file
				else
				{
          // TODO Check upload count
					processNext = false;
				}
			}
		}
		else
		{
			// error condition - host down?
			processNext = false;
		}
	}
	catch (const SQLite::Exception& e)
	{
		// error condition - database not initialized (i.e. no table)?
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("AutoThreadCallback(/renter/files)", e)));
		processNext = false;
	}

	if (processNext)
	{
		try
		{
			std::lock_guard<std::mutex> l(_uploadMutex);
			SQLite::Statement query(_uploadDatabase, QUERY_UPLOADS_BY_STATUS);
			query.bind("@status", static_cast<unsigned>(UploadStatus::Queued));

			if (query.executeStep())
			{
				SString siaPath = static_cast<const char*>(query.getColumn(query.getColumnIndex("sia_path")));
        SString filePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("file_path")));
        SString sdFilePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("sd_file_path")));
        // TODO Rethink this - could hang here for quite a while creating temporary files
        if (CreateSiaDriveFile(siaPath, filePath, sdFilePath + ".temp", sdFilePath))
        {
          // TODO Validate response
          json response;
          SiaCurlError cerror = siaCurl.Post(SString(L"/renter/upload/") + siaPath, { {L"source", filePath} }, response);
          if (ApiSuccess(cerror))
          {
            SET_STATUS(UploadStatus::Uploading, UploadToSiaStarted, ModifyUploadStatusFailed)
          }
        }
			}
		}
		catch (const SQLite::Exception& e)
		{
			// error condition
			CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("AutoThreadCallback(processNext)", e)));
		}
	}
}

UploadStatus CUploadManager::GetUploadStatus(const SString& siaPath)
{
	UploadStatus uploadStatus = UploadStatus::NotFound;

	SQLite::Statement query(_uploadDatabase, QUERY_UPLOADS_BY_SIA_PATH);
	query.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
	if (query.executeStep())
	{
		uploadStatus = static_cast<UploadStatus>(static_cast<unsigned>(query.getColumn(query.getColumnIndex("status"))));
	}

	return uploadStatus;
}

UploadError CUploadManager::AddOrUpdate(const SString& siaPath, SString filePath)
{
  UploadError ret;

	// Relative to absolute and grab parent folder of source
	FilePath rootPath = filePath;
  rootPath.MakeAbsolute().RemoveFileName();

	if (FilePath(filePath).IsFile())
	{
		// Lock here - if file is modified again before a prior upload is complete, delete it and 
		//	start again later
		std::lock_guard<std::mutex> l(_uploadMutex);

		try
		{
			SQLite::Statement query(_uploadDatabase, QUERY_UPLOADS_BY_SIA_PATH_AND_2_STATUS);
			query.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
			query.bind("@status1", static_cast<unsigned>(UploadStatus::Uploading));
			query.bind("@status2", static_cast<unsigned>(UploadStatus::Modified));

			// Check copying
			if (query.executeStep())
			{
				UploadStatus uploadStatus = static_cast<UploadStatus>(static_cast<unsigned>(query.getColumn(query.getColumnIndex("status"))));
				CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(ExistingUploadFound(siaPath, filePath, uploadStatus)));
				if (uploadStatus == UploadStatus::Uploading)
				{
					SET_STATUS(UploadStatus::Modified, UploadStatusSetToModified, ModifyUploadStatusFailed)
				}
			}
			else
			{
				// Strip drive specification (i.e. C:\)
				// TODO If mount to folder is ever enabled, this will need to change
				// TODO Case sensative file names? Going to be a bit of an issue.
				SString siaDriveFileName = GenerateSha256(&filePath[3]) + L".siadrive";
				FilePath siaDriveFilePath(rootPath, siaDriveFileName);

				// Add to db
				try
				{
					SQLite::Statement insert(_uploadDatabase, INSERT_UPLOAD);
					insert.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
					insert.bind("@file_path", SString::ToUtf8(filePath).c_str());
					insert.bind("@sd_file_path", SString::ToUtf8(static_cast<SString>(siaDriveFilePath)).c_str());
					insert.bind("@status", static_cast<unsigned>(UploadStatus::Queued));
					if (insert.exec() == 1)
					{
						CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FileAddedToQueue(siaPath, filePath, siaDriveFilePath)));
					}
					else
					{
						CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseInsertFailed(siaPath, filePath, insert.getErrorMsg())));
						ret = UploadErrorCode::DatabaseError;
					}
				}
				catch (SQLite::Exception e)
				{
					CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("AddOrUpdate(insert)", e)));
          ret = { UploadErrorCode::DatabaseError, e.getErrorStr() };
				}
			}
		}
		catch (SQLite::Exception e)
		{
			CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("AddOrUpdate(query)", e)));
      ret = { UploadErrorCode::DatabaseError, e.getErrorStr() };
		}
	}
	else
	{
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(SourceFileNotFound(siaPath, filePath)));
		ret = UploadErrorCode::SourceFileNotFound;
	}

	return ret;
}

UploadError CUploadManager::Remove(const SString& siaPath)
{
  UploadError ret;
	bool remove = false;
  SString siaDriveFilePath;
	try
	{
	  std::lock_guard<std::mutex> l(_uploadMutex);

		SQLite::Statement query(_uploadDatabase, QUERY_STATUS);
		query.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
		if (query.executeStep())
		{
			SString filePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("file_path")));
			siaDriveFilePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("sd_file_path")));
			UploadStatus uploadStatus = static_cast<UploadStatus>(static_cast<unsigned>(query.getColumn(query.getColumnIndex("status"))));
			switch (uploadStatus)
			{
			case UploadStatus::Complete:
			case UploadStatus::Queued:
			case UploadStatus::Modified:
			case UploadStatus::Uploading:
				{
					remove = true;
				}
				break;

			default:
				{
					remove = false;
				}
				break;
			}
		}
    else
    {
      RemoveFileFromSia(CSiaCurl(GetHostConfig()), siaPath);
    }
	}
	catch (SQLite::Exception e)
	{
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("Remove", e)));
    ret = { UploadErrorCode::DatabaseError, e.getErrorStr() };
	}

	if (remove)
	{
    HandleFileRemove(CSiaCurl(GetHostConfig()), siaPath, siaDriveFilePath);
	}

	return ret;
}