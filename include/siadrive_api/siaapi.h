#ifndef _SIAAPI_H
#define _SIAAPI_H

#include <siacommon.h>
#include <siacurl.h>
#include <autothread.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class CSiaDriveConfig;
class SIADRIVE_EXPORTABLE CSiaBase
{
public:
	explicit CSiaBase(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
		_siaCurl(siaCurl),
		_siaDriveConfig(siaDriveConfig)
	{
	}

public:
	virtual ~CSiaBase() = 0
	{
	}

private:
	const CSiaCurl& _siaCurl;
	CSiaDriveConfig* _siaDriveConfig;

protected:
	inline const CSiaCurl& GetSiaCurl() const
	{
		return _siaCurl;
	}

	inline CSiaDriveConfig& GetSiaDriveConfig() const
	{
		return *_siaDriveConfig;
	}
};


class SIADRIVE_EXPORTABLE CSiaApi
{
public:
	enum class _SiaApiError 
	{
		Success,
		NotImplemented,
		RequestError,
		WalletExists,
		WalletLocked,
		WalletUnlocked,
		WalletNotCreated
	};

	enum class _SiaSeedLanguage
	{
		English,
		German,
		Japanese
	};

	class _CSiaFileTree;
	class SIADRIVE_EXPORTABLE _CSiaFile :
		public virtual CSiaBase
	{
		friend CSiaApi;
		friend _CSiaFileTree;

	private:
		explicit _CSiaFile(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, const json& fileJson);

	public:
		virtual ~_CSiaFile();

	// Properties
		Property(SString, SiaPath, public, private)
		Property(std::uint64_t, FileSize, public, private)
		Property(bool, Available, public, private)
		Property(bool, Renewing, public, private)
		Property(std::uint32_t, Redundancy, public, private)
		Property(std::uint32_t, UploadProgress, public, private)
		Property(std::uint32_t, Expiration, public, private)
	};

	class SIADRIVE_EXPORTABLE _CSiaFileTree :
		public virtual CSiaBase
	{
		friend CSiaApi;
	public:
		explicit _CSiaFileTree(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

	public:
		virtual ~_CSiaFileTree();

	private:
		std::vector<std::shared_ptr<_CSiaFile>> _fileList;

	public:
		void BuildTree(const json& result);

		std::vector<std::shared_ptr<_CSiaFile>> GetFileList() const;

		std::vector<std::shared_ptr<_CSiaFile>> Query(SString query) const;

		std::shared_ptr<_CSiaFile> GetFile(const SString& siaPath) const;

		std::vector<SString> QueryDirectories(SString query) const;

		bool FileExists(const SString& siaPath) const;
	};

	class SIADRIVE_EXPORTABLE _CSiaWallet :
		public virtual CSiaBase
	{
		friend CSiaApi;
	private:
		explicit _CSiaWallet(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

	public:
		virtual ~_CSiaWallet();

	// Properties
		Property(bool, Created, public, private)
		Property(bool, Locked, public, private)
    Property(bool, Connected, public, private)
    Property(SiaCurrency, ConfirmedBalance, public, private)
    Property(SiaCurrency, UnconfirmedBalance, public, private)
    Property(SString, ReceiveAddress, public, private)

	private:
    virtual void Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

	public:
		_SiaApiError Create(const _SiaSeedLanguage& seedLanguage, SString& seed);
		_SiaApiError Restore(const SString& seed);
		_SiaApiError Lock();
		_SiaApiError Unlock(const SString& password);
	};

	class SIADRIVE_EXPORTABLE _CSiaRenter :
		public virtual CSiaBase
	{
		friend CSiaApi;

	public:
    typedef struct
    {
      SiaCurrency Funds;
      std::uint64_t Hosts;
      std::uint64_t Period;
      std::uint64_t RenewWindowInBlocks;
    } _SiaRenterAllowance;

	private:
		explicit _CSiaRenter(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

	public:
		virtual ~_CSiaRenter();
		
		Property(SiaCurrency, Funds, public, private)
		Property(std::uint64_t, Hosts, public, private)
		Property(SiaCurrency, Unspent, public, private)
		Property(std::uint64_t, TotalUsedBytes, public, private)
		Property(std::uint32_t, TotalUploadProgress, public, private)
    Property(std::uint64_t, Period, public, private)
    Property(std::uint64_t, RenewWindow, public, private)

	private:
    _SiaRenterAllowance _currentAllowance;
    std::shared_ptr<_CSiaFileTree> _fileTree;
    std::mutex _fileTreeMutex;

	private:
		void Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

	public:
		_SiaApiError FileExists(const SString& siaPath, bool& exists) const;
		_SiaApiError DownloadFile(const SString& siaPath, const SString& location) const;
		_SiaApiError GetFileTree(std::shared_ptr<_CSiaFileTree>& siaFileTree) const;
    _SiaRenterAllowance GetAllowance() const;
    _SiaApiError SetAllowance(const _SiaRenterAllowance& renterAllowance);
    _SiaApiError RefreshFileTree( );
	};

	class SIADRIVE_EXPORTABLE _CSiaConsensus : 
		public virtual CSiaBase
	{
		friend CSiaApi;

	private:
		explicit _CSiaConsensus(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

	public:
		virtual ~_CSiaConsensus();

	// Properties
		Property(std::uint64_t, Height, public, private)
		Property(bool, Synced, public, private)
		Property(SString, CurrentBlock, public, private)

	private:
		void Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);
	};

public:
	explicit CSiaApi(const SiaHostConfig& hostConfig, CSiaDriveConfig* siaDriveConfig);

public:
	~CSiaApi();

private:
	SiaHostConfig _hostConfig;
	CSiaCurl _siaCurl;
	CSiaDriveConfig* _siaDriveConfig;
	std::shared_ptr<_CSiaWallet> _wallet;
	std::shared_ptr<_CSiaRenter> _renter;
	std::shared_ptr<_CSiaConsensus> _consensus;
  std::unique_ptr<CAutoThread> _refreshThread;
  
private:
  void Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

public:
	static SString FormatToSiaPath(SString path);

public:
	std::shared_ptr<_CSiaWallet> GetWallet() const;
	std::shared_ptr<_CSiaRenter> GetRenter() const;
	std::shared_ptr<_CSiaConsensus> GetConsensus() const;
	SString GetServerVersion() const;
	SiaHostConfig GetHostConfig() const;
};

typedef CSiaApi::_SiaApiError SiaApiError;
typedef CSiaApi::_SiaSeedLanguage SiaSeedLanguage;
typedef CSiaApi::_CSiaWallet CSiaWallet;
typedef CSiaApi::_CSiaRenter CSiaRenter;
typedef CSiaRenter::_SiaRenterAllowance SiaRenterAllowance;
typedef CSiaApi::_CSiaConsensus CSiaConsensus;
typedef std::shared_ptr<CSiaWallet> CSiaWalletPtr;
typedef std::shared_ptr<CSiaRenter> CSiaRenterPtr;
typedef std::shared_ptr<CSiaConsensus> CSiaConsensusPtr;
typedef CSiaApi::_CSiaFile CSiaFile;
typedef std::shared_ptr<CSiaFile> CSiaFilePtr;
typedef std::vector<CSiaFilePtr> CSiaFileCollection;
typedef CSiaApi::_CSiaFileTree CSiaFileTree;
typedef std::shared_ptr<CSiaFileTree> CSiaFileTreePtr;
NS_END(2)
#endif //_SIAAPI_H