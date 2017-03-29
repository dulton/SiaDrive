#ifndef _SIACURL_H
#define _SIACURL_H

#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class SIADRIVE_EXPORTABLE CSiaCurl
{
public:
	enum class _SiaCurlError
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
	static _SiaCurlError CheckApiError(const json& result);
	static _SiaCurlError CheckHttpError(const std::string& result);

private:
	std::string ConstructPath(const SString& relativePath) const;
	_SiaCurlError _Get(const SString& path, const _HttpParameters& parameters, json& response) const;
	bool CheckVersion(_SiaCurlError& error) const;
	_SiaCurlError ProcessResponse(const int& res, const int& httpCode, const std::string& result, json& response) const;

public:
	SString GetServerVersion() const;
	_SiaCurlError Get(const SString& path, json& result) const;
	_SiaCurlError Get(const SString& path, const _HttpParameters& parameters, json& result) const;
	_SiaCurlError Post(const SString& path, const _HttpParameters& parameters, json& response) const;
};

typedef CSiaCurl::_SiaCurlError SiaCurlError;
typedef CSiaCurl::_HttpParameters HttpParameters;

NS_END(2)

#endif //_SIACURL_H