#include <SQLiteCpp/Exception.h>
#include <uploadmanager.h>
#include <siaapi.h>
#include <eventsystem.h>
#include <siadriveconfig.h>
#include <filepath.h>

using namespace Sia::Api;

#define TABLE_CREATE L"create table if not exists %s (%s);"
#define UPLOAD_TABLE L"upload_table"
#define UPLOAD_TABLE_COLUMNS L"id integer primary key autoincrement, sia_path text unique not null, file_path text unique not null, status integer not null"
#define QUERY_STATUS "select * from upload_table where sia_path=@sia_path order by id desc limit 1;"
#define QUERY_UPLOADS_BY_STATUS "select * from upload_table where status=@status order by id desc limit 1;"
#define QUERY_UPLOADS_BY_SIA_PATH "select * from upload_table where sia_path=@sia_path order by id desc limit 1;"
#define QUERY_UPLOADS_BY_SIA_PATH_AND_STATUS "select * from upload_table where sia_path=@sia_path and status=@status order by id desc limit 1;"
#define UPDATE_STATUS "update upload_table set status=@status where sia_path=@sia_path;"
#define INSERT_UPLOAD "insert into upload_table (sia_path, status, file_path) values (@sia_path, @status, @file_path);"
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
  _siaDriveConfig(siaDriveConfig),
	_uploadDatabase(siaDriveConfig->GetRenter_UploadDbFilePath(), SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE)
{
	CreateTableIfNotFound(&_uploadDatabase, UPLOAD_TABLE, UPLOAD_TABLE_COLUMNS);

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

bool CUploadManager::HandleFileRemove(const CSiaCurl& siaCurl, const SString& siaPath)
{
  bool ret = false;
  FilePath removeFilePath(GetSiaDriveConfig()->GetCacheFolder(), siaPath);
	
  json response;
  SiaCurlError cerror = siaCurl.Post(SString(L"/renter/delete/") + siaPath, {}, response);
  if (ApiSuccess(cerror))
  {
    SQLite::Statement del(_uploadDatabase, DELETE_UPLOAD);
    del.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
    if (del.exec() >= 0)
    {
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FileRemovedFromSia(siaPath, removeFilePath)));
      ret = true;
    }
    else
    {
      CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseDeleteFailed(siaPath, removeFilePath, del.getErrorMsg())));
    }
  }
  else
  {
    CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FailedToRemoveFileFromSia(siaPath, removeFilePath, cerror)));
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
			SQLite::Statement query(_uploadDatabase, QUERY_UPLOADS_BY_STATUS);
			query.bind("@status", static_cast<unsigned>(UploadStatus::Uploading));

			fileTree->BuildTree(result);
			if (query.executeStep())
			{
				SString siaPath = static_cast<const char*>(query.getColumn(query.getColumnIndex("sia_path")));
				SString filePath = static_cast<const char*>(query.getColumn(query.getColumnIndex("file_path")));
				UploadStatus uploadStatus = static_cast<UploadStatus>(query.getColumn(query.getColumnIndex("status")).getUInt());

				auto fileList = fileTree->GetFileList();
				auto it = std::find_if(fileList.begin(), fileList.end(), [&](const CSiaFilePtr& ptr)
				{
					return ptr->GetSiaPath() == siaPath;
				});
				
				// Removed by another client
        if (it == fileList.end())
        {
          HandleFileRemove(siaCurl, siaPath);
        }
				// Upload is complete
				else if ((*it)->GetAvailable())
				{
					SET_STATUS(UploadStatus::Complete, UploadToSiaComplete, ModifyUploadStatusFailed)
				}
				// Upload still active, don't process another file
				else
				{
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
        
        json response;
        SiaCurlError cerror = siaCurl.Post(SString(L"/renter/upload/") + siaPath, { {L"source", filePath} }, response);
        if (ApiSuccess(cerror))
        {
          SET_STATUS(UploadStatus::Uploading, UploadToSiaStarted, ModifyUploadStatusFailed)
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
			SQLite::Statement query(_uploadDatabase, QUERY_UPLOADS_BY_SIA_PATH_AND_STATUS);
			query.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
			query.bind("@status", static_cast<unsigned>(UploadStatus::Uploading));

			// Check uploading
      bool addToDatabase = true;
			if (query.executeStep())
			{
				UploadStatus uploadStatus = static_cast<UploadStatus>(static_cast<unsigned>(query.getColumn(query.getColumnIndex("status"))));
				if (uploadStatus == UploadStatus::Uploading)
				{
          addToDatabase = HandleFileRemove(CSiaCurl(GetHostConfig()), siaPath);
				}
        else if (uploadStatus == UploadStatus::Queued)
        {
          addToDatabase = false;
        }
			}
			
      if (addToDatabase)
			{
				// Add to db
				try
				{
					SQLite::Statement insert(_uploadDatabase, INSERT_UPLOAD);
					insert.bind("@sia_path", SString::ToUtf8(siaPath).c_str());
					insert.bind("@file_path", SString::ToUtf8(filePath).c_str());
					insert.bind("@status", static_cast<unsigned>(UploadStatus::Queued));
					if (insert.exec() == 1)
					{
						CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(FileAddedToQueue(siaPath, filePath)));
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
	try
	{
	  std::lock_guard<std::mutex> l(_uploadMutex);
		if (!HandleFileRemove(CSiaCurl(GetHostConfig()), siaPath))
		{
      ret = UploadErrorCode::SourceFileNotFound;
		}
	}
  catch (SQLite::Exception e)
	{
		CEventSystem::EventSystem.NotifyEvent(CreateSystemEvent(DatabaseExceptionOccurred("Remove", e)));
    ret = { UploadErrorCode::DatabaseError, e.getErrorStr() };
	}

	return ret;
}