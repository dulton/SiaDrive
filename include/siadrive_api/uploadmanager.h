#ifndef _UPLOADMANAGER_H
#define _UPLOADMANAGER_H

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Exception.h>
#include <autothread.h>
#include <deque>
#include <siacurl.h>
#include <eventsystem.h>
#include <filepath.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class SIADRIVE_EXPORTABLE CUploadManager :
	public CAutoThread
{
public:
	enum class _UploadStatus : unsigned
	{
		NotFound,
		Queued,
		Uploading,
		Complete,
		Error
	};

	enum class _UploadErrorCode
	{
		Success,
		SourceFileNotFound,
		DatabaseError
	};

private:
	typedef struct
	{
		std::uint64_t Id;
		SString SiaPath;
		SString FilePath;
		SString TempPath;
		_UploadStatus Status;
	} UploadData;

public:
	CUploadManager(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

public:
	virtual ~CUploadManager();

private:
  CSiaDriveConfig* _siaDriveConfig;
	SQLite::Database _uploadDatabase;
	std::mutex _uploadMutex;

private:
  CSiaDriveConfig* GetSiaDriveConfig() const { return _siaDriveConfig; }

	bool HandleFileRemove(const CSiaCurl& siaCurl, const SString& siaPath);
	void DeleteFilesRemovedFromSia(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, const bool& isStartup = false);

protected:
	virtual void AutoThreadCallback(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) override;

public:
	static SString UploadStatusToString(const _UploadStatus& uploadStatus);

public:
	_UploadStatus GetUploadStatus(const SString& siaPath);
  CSiaError<_UploadErrorCode> AddOrUpdate(const SString& siaPath, SString filePath);
  CSiaError<_UploadErrorCode> Remove(const SString& siaPath);
};

typedef CUploadManager::_UploadStatus UploadStatus;
typedef CUploadManager::_UploadErrorCode UploadErrorCode;
typedef CSiaError<CUploadManager::_UploadErrorCode> UploadError;

// Event Notifications
class FileAddedToQueue :
	public CEvent
{
public:
  FileAddedToQueue(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~FileAddedToQueue()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
    return L"FileAddedToQueue|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new FileAddedToQueue(_siaPath, _filePath));
	}
};

class ExternallyRemovedFileDetected :
	public CEvent
{
public:
	ExternallyRemovedFileDetected(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~ExternallyRemovedFileDetected()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"ExternallyRemovedFileDetected|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new ExternallyRemovedFileDetected(_siaPath, _filePath));
	}
};

class UploadToSiaStarted :
	public CEvent
{
public:
	UploadToSiaStarted(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~UploadToSiaStarted()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"UploadToSiaStarted|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new UploadToSiaStarted(_siaPath, _filePath));
	}
};

class UploadToSiaComplete :
	public CEvent
{
public:
  UploadToSiaComplete(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~UploadToSiaComplete()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"UploadToSiaComplete|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new UploadToSiaComplete(_siaPath, _filePath));
	}
};

class FileRemovedFromSia :
	public CEvent
{
public:
  FileRemovedFromSia(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~FileRemovedFromSia()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"FileRemovedFromSia|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new FileRemovedFromSia(_siaPath, _filePath));
	}
};

class FailedToRemoveFileFromSia :
	public CEvent
{
public:
  FailedToRemoveFileFromSia(const SString& siaPath, const SString& filePath, const SiaCurlError& curlError) :
		_siaPath(siaPath),
		_filePath(filePath),
		_curlError(curlError)
	{

	}

public:
	virtual ~FailedToRemoveFileFromSia()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
  const SiaCurlError _curlError;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"FailedToRemoveFileFromSia|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new FailedToRemoveFileFromSia(_siaPath, _filePath, _curlError));
	}
};

class ModifyUploadStatusFailed :
	public CEvent
{
public:
	ModifyUploadStatusFailed(const SString& siaPath, const SString& filePath, const UploadStatus& uploadStatus, const SString& errorMsg) :
		_siaPath(siaPath),
		_filePath(filePath),
		_uploadStatus(uploadStatus),
		_errorMsg(errorMsg)
	{

	}

public:
	virtual ~ModifyUploadStatusFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const UploadStatus _uploadStatus;
	const SString _errorMsg;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"ModifyUploadStatusFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|ST|" + CUploadManager::UploadStatusToString(_uploadStatus) + L"|MSG|" + _errorMsg;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new ModifyUploadStatusFailed(_siaPath, _filePath, _uploadStatus, _errorMsg));
	}
};

class DatabaseInsertFailed :
	public CEvent
{
public:
	DatabaseInsertFailed(const SString& siaPath, const SString& filePath, const SString& errorMessage) :
		_siaPath(siaPath),
		_filePath(filePath),
		_errorMessage(errorMessage)
	{

	}

public:
	virtual ~DatabaseInsertFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _errorMessage;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"DatabaseInsertFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|MSG|" + _errorMessage;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new DatabaseInsertFailed(_siaPath, _filePath, _errorMessage));
	}
};

class DatabaseDeleteFailed :
	public CEvent
{
public:
	DatabaseDeleteFailed(const SString& siaPath, const SString& filePath, const SString& errorMessage) :
		_siaPath(siaPath),
		_filePath(filePath),
		_errorMessage(errorMessage)
	{

	}

public:
	virtual ~DatabaseDeleteFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _errorMessage;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"DatabaseDeleteFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|MSG|" + _errorMessage;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new DatabaseDeleteFailed(_siaPath, _filePath, _errorMessage));
	}
};

class SourceFileNotFound :
	public CEvent
{
public:
	SourceFileNotFound(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~SourceFileNotFound()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new SourceFileNotFound(_siaPath, _filePath));
	}

	virtual SString GetSingleLineMessage() const override
	{
		return L"SourceFileNotFound|SP|" + _siaPath + L"|FP|" + _filePath;
	}
};

class SIADRIVE_EXPORTABLE DatabaseExceptionOccurred :
	public CEvent
{
public:
	DatabaseExceptionOccurred(const SString& duringOperation, const SQLite::Exception& exception) :
    _duringOperation(duringOperation),
		_exception(exception)
	{

	}

public:
	virtual ~DatabaseExceptionOccurred()
	{
	}

private:
  const SString _duringOperation;
	const SQLite::Exception _exception;

public:
	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new DatabaseExceptionOccurred(_duringOperation, _exception));
	}

	virtual SString GetSingleLineMessage() const override
	{
		return L"DatabaseExceptionOccurred|MSG|" + SString(_exception.getErrorStr()) + "|OPR|" + _duringOperation;
	}
};

NS_END(2)

#endif //_UPLOADMANAGER_H