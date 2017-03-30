#ifndef _SIACURL_H
#define _SIACURL_H

#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class SIADRIVE_EXPORTABLE CSiaCurl
{
public:
	enum class _SiaCurlErrorCode
	{
		Success,
		ServerVersionMismatch,
		InvalidRequiredVersion,
		NoResponse,
		HttpError,
		UnknownFailure
	};

	typedef std::unordered_map<SString, SString> _HttpParameters;

public:
	CSiaCurl();

	CSiaCurl(const SiaHostConfig& hostConfig);

public:
	~CSiaCurl();

private:
	Property(SiaHostConfig, HostConfig, public, public)

public:
	static SString UrlEncode(const SString& data, const bool& allowSlash = false);

private:
  static SString CSiaCurl::GetApiErrorMessage(const json& result);

private:
	std::string ConstructPath(const SString& relativePath) const;
  CSiaError<_SiaCurlErrorCode> _Get(const SString& path, const _HttpParameters& parameters, json& response) const;
	bool CheckVersion(CSiaError<_SiaCurlErrorCode>& error) const;
  CSiaError<_SiaCurlErrorCode> ProcessResponse(const int& res, const int& httpCode, const std::string& result, json& response) const;

public:
	SString GetServerVersion() const;
  CSiaError<_SiaCurlErrorCode> Get(const SString& path, json& result) const;
  CSiaError<_SiaCurlErrorCode> Get(const SString& path, const _HttpParameters& parameters, json& result) const;
  CSiaError<_SiaCurlErrorCode> Post(const SString& path, const _HttpParameters& parameters, json& response) const;
};

typedef CSiaCurl::_SiaCurlErrorCode SiaCurlErrorCode;
typedef CSiaError<SiaCurlErrorCode> SiaCurlError;
typedef CSiaCurl::_HttpParameters HttpParameters;

NS_END(2)

#endif //_SIACURL_H