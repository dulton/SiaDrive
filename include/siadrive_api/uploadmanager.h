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
		Modified,
		Uploading,
		Complete,
		Error
	};

	enum class _UploadError
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
	SQLite::Database _uploadDatabase;
	std::mutex _uploadMutex;
	SString _activeSiaPath;

private:
	void HandleFileRemove(const CSiaCurl& siaCurl, const SString& siaPath, const SString& siaDriveFilePath);
	bool CreateSiaDriveFile(const SString& siaPath, const SString& filePath, const SString& tempSourcePath, const SString& siaDriveFilePath);
	void DeleteFilesRemovedFromSia(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, const bool& isStartup = false);
  void RemoveFileFromSia(const CSiaCurl& siaCurl, const SString& siaPath, FilePath removeFilePath);

protected:
	virtual void AutoThreadCallback(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) override;

public:
	static SString UploadStatusToString(const _UploadStatus& uploadStatus);

public:
	_UploadStatus GetUploadStatus(const SString& siaPath);
	_UploadError AddOrUpdate(const SString& siaPath, SString filePath);
	_UploadError Remove(const SString& siaPath);
};

typedef Sia::Api::CUploadManager::_UploadStatus UploadStatus;
typedef Sia::Api::CUploadManager::_UploadError UploadError;

// Event Notifications
class CreatingTemporarySiaDriveFile :
	public CEvent
{
public:
	CreatingTemporarySiaDriveFile(const SString& siaPath, const SString& filePath, const SString& tempSourcePath) :
		_siaPath(siaPath),
		_filePath(filePath),
		_tempSourcePath(tempSourcePath)
	{

	}

public:
	virtual ~CreatingTemporarySiaDriveFile()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _tempSourcePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"CreatingTemporarySiaDriveFile|SP|" + _siaPath + L"|FP|" + _filePath + L"|TSP|" + _tempSourcePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new CreatingTemporarySiaDriveFile(_siaPath, _filePath, _tempSourcePath));
	}
};

class FileAddedToQueue :
	public CEvent
{
public:
  FileAddedToQueue(const SString& siaPath, const SString& filePath, const SString& siaDriveFilePath) :
		_siaPath(siaPath),
		_filePath(filePath),
    _siaDriveFilePath(siaDriveFilePath)
	{

	}

public:
	virtual ~FileAddedToQueue()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
  const SString _siaDriveFilePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"FileAddedToQueue|SP|" + _siaPath + L"|FP|" + _filePath + "|SFP|" + _siaDriveFilePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new FileAddedToQueue(_siaPath, _filePath, _siaDriveFilePath));
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

class ModifiedUploadQueued :
	public CEvent
{
public:
	ModifiedUploadQueued(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~ModifiedUploadQueued()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"ModifiedUploadQueued|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new ModifiedUploadQueued(_siaPath, _filePath));
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

class UploadComplete :
	public CEvent
{
public:
	UploadComplete(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~UploadComplete()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"UploadComplete|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new UploadComplete(_siaPath, _filePath));
	}
};

class FileRemoved :
	public CEvent
{
public:
	FileRemoved(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~FileRemoved()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"FileRemoved|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new FileRemoved(_siaPath, _filePath));
	}
};

class UploadStatusSetToModified :
	public CEvent
{
public:
	UploadStatusSetToModified(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~UploadStatusSetToModified()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"UploadStatusSetToModified|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new UploadStatusSetToModified(_siaPath, _filePath));
	}
};

class UploadStatusSetToRemoved :
	public CEvent
{
public:
	UploadStatusSetToRemoved(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~UploadStatusSetToRemoved()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"UploadStatusSetToRemoved|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new UploadStatusSetToRemoved(_siaPath, _filePath));
	}
};

class FailedToDeleteFromSia :
	public CEvent
{
public:
	FailedToDeleteFromSia(const SString& siaPath, const SString& filePath, const SiaCurlError& curlError) :
		_siaPath(siaPath),
		_filePath(filePath),
		_curlError(curlError)
	{

	}

public:
	virtual ~FailedToDeleteFromSia()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SiaCurlError _curlError = SiaCurlError::Success;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"FailedToDeleteFromSia|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new FailedToDeleteFromSia(_siaPath, _filePath, _curlError));
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

class CreatingTemporarySiaDriveFileFailed :
	public CEvent
{
public:
	CreatingTemporarySiaDriveFileFailed(const SString& siaPath, const SString& filePath, const SString& tempSourcePath) :
		_siaPath(siaPath),
		_filePath(filePath),
		_tempSourcePath(tempSourcePath)
	{

	}

public:
	virtual ~CreatingTemporarySiaDriveFileFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _tempSourcePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"CreatingTemporarySiaDriveFileFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|TSP|" + _tempSourcePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new CreatingTemporarySiaDriveFileFailed(_siaPath, _filePath, _tempSourcePath));
	}
};

class DeleteSiaDriveFileFailed :
	public CEvent
{
public:
	DeleteSiaDriveFileFailed(const SString& siaPath, const SString& filePath, const SString& siaDriveFilePath) :
		_siaPath(siaPath),
		_filePath(filePath),
		_siaDriveFilePath(siaDriveFilePath)
	{

	}

public:
	virtual ~DeleteSiaDriveFileFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _siaDriveFilePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"DeleteSiaDriveFileFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|SDP|" + _siaDriveFilePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new DeleteSiaDriveFileFailed(_siaPath, _filePath, _siaDriveFilePath));
	}
};

class DeleteTemporarySiaDriveFileFailed :
	public CEvent
{
public:
	DeleteTemporarySiaDriveFileFailed(const SString& siaPath, const SString& filePath, const SString& tempSourcePath) :
		_siaPath(siaPath),
		_filePath(filePath),
		_tempSourcePath(tempSourcePath)
	{

	}

public:
	virtual ~DeleteTemporarySiaDriveFileFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _tempSourcePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"DeleteTemporarySiaDriveFileFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|TSP|" + _tempSourcePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new DeleteTemporarySiaDriveFileFailed(_siaPath, _filePath, _tempSourcePath));
	}
};

class RenamingTemporarySiaDriveFile :
	public CEvent
{
public:
	RenamingTemporarySiaDriveFile(const SString& siaPath, const SString& filePath, const SString& tempSourcePath, const SString& siaDriveFilePath) :
		_siaPath(siaPath),
		_filePath(filePath),
		_tempSourcePath(tempSourcePath),
		_siaDriveFilePath(siaDriveFilePath)
	{

	}

public:
	virtual ~RenamingTemporarySiaDriveFile()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _tempSourcePath;
	const SString _siaDriveFilePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"RenamingTemporarySiaDriveFile|SP|" + _siaPath + L"|FP|" + _filePath + L"|TSP|" + _tempSourcePath + L"|SDP|" + _siaDriveFilePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new RenamingTemporarySiaDriveFile(_siaPath, _filePath, _tempSourcePath, _siaDriveFilePath));
	}
};

class RenamingTemporarySiaDriveFileFailed :
	public CEvent
{
public:
	RenamingTemporarySiaDriveFileFailed(const SString& siaPath, const SString& filePath, const SString& tempSourcePath, const SString& siaDriveFilePath) :
		_siaPath(siaPath),
		_filePath(filePath),
		_tempSourcePath(tempSourcePath),
		_siaDriveFilePath(siaDriveFilePath)
	{

	}

public:
	virtual ~RenamingTemporarySiaDriveFileFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const SString _tempSourcePath;
	const SString _siaDriveFilePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"RenamingTemporarySiaDriveFileFailed|SP|" + _siaPath + L"|FP|" + _filePath + L"|TSP|" + _tempSourcePath + L"|SDP|" + _siaDriveFilePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new RenamingTemporarySiaDriveFileFailed(_siaPath, _filePath, _tempSourcePath, _siaDriveFilePath));
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

class RemoveFileFailed :
	public CEvent
{
public:
	RemoveFileFailed(const SString& siaPath, const SString& filePath) :
		_siaPath(siaPath),
		_filePath(filePath)
	{

	}

public:
	virtual ~RemoveFileFailed()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"RemoveFileFailed|SP|" + _siaPath + L"|FP|" + _filePath;
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new RemoveFileFailed(_siaPath, _filePath));
	}
};

class ExistingUploadFound :
	public CEvent
{
public:
	ExistingUploadFound(const SString& siaPath, const SString& filePath, const UploadStatus& uploadStatus) :
		_siaPath(siaPath),
		_filePath(filePath),
		_uploadStatus(uploadStatus)
	{

	}

public:
	virtual ~ExistingUploadFound()
	{
	}

private:
	const SString _siaPath;
	const SString _filePath;
	const UploadStatus _uploadStatus;

public:
	virtual SString GetSingleLineMessage() const override
	{
		return L"ExistingUploadFound|SP|" + _siaPath + L"|FP|" + _filePath + L"|ST|" + CUploadManager::UploadStatusToString(_uploadStatus);
	}

	virtual std::shared_ptr<CEvent> Clone() const override
	{
		return std::shared_ptr<CEvent>(new ExistingUploadFound(_siaPath, _filePath, _uploadStatus));
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