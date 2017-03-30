#include <siacurl.h>
#include <curl/curl.h>

using namespace Sia::Api;

CSiaCurl::CSiaCurl()
{
	SetHostConfig({ L"localhost", 9980, L""});
}

CSiaCurl::CSiaCurl(const SiaHostConfig& hostConfig)
{
	SetHostConfig(hostConfig);
}

CSiaCurl::~CSiaCurl()
{
}

SString CSiaCurl::UrlEncode(const SString& data, const bool& allowSlash)
{
	CURL* curlHandle = curl_easy_init();
	curl_easy_reset(curlHandle);

	char* value = curl_easy_escape(curlHandle, SString::ToUtf8(data).c_str(), 0);
	SString ret = value;
	curl_free(value);
	if (allowSlash)
	{
		ret.Replace("%2F", "/");
	}

	curl_easy_cleanup(curlHandle);

	return ret;
}

SString CSiaCurl::GetApiErrorMessage(const json& result)
{

  SString ret;
	if (result.find("message") != result.end())
	{
		ret = result["message"].get<std::string>();
	}

	return ret;
}

std::string CSiaCurl::ConstructPath(const SString& relativePath) const
{
	const std::string ret = "http://" + GetHostConfig().HostName + ":" + std::to_string(GetHostConfig().HostPort) + UrlEncode(relativePath, true);
	return ret;
}

SiaCurlError CSiaCurl::ProcessResponse(const int& res, const int& httpCode, const std::string& result, json& response) const
{
  SiaCurlError ret;
	if ((res == CURLE_OK) && ((httpCode >= 200) && (httpCode <300)))
	{
    if (result.length())
    {
      response = json::parse(result.c_str());
    }
	}
	else
	{
		if ((res == CURLE_COULDNT_RESOLVE_HOST) || (res == CURLE_COULDNT_CONNECT))
		{
			ret = SiaCurlErrorCode::NoResponse;
		}
		else if (httpCode)
		{
      ret = { SiaCurlErrorCode::HttpError, GetApiErrorMessage(result) };
		}
		else
		{
      ret = { SiaCurlErrorCode::UnknownFailure, "Unknown curl error" };
		}
	}

	return ret;
}

SiaCurlError CSiaCurl::_Get(const SString& path, const HttpParameters& parameters, json& response) const
{
	CURL* curlHandle = curl_easy_init();
	curl_easy_reset(curlHandle);
	SString url = ConstructPath(path);
	if (parameters.size())
	{
		url += "?";
		for (const auto& param : parameters)
		{
			if (url[url.Length() - 1] != '?')
			{
				url += "&";
			}
			url += (param.first + "=" + UrlEncode(param.second));
		}
	}
	curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "Sia-Agent");
	curl_easy_setopt(curlHandle, CURLOPT_URL, SString::ToUtf8(url).c_str());
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, static_cast<size_t(*)(char*, size_t, size_t, void *)>([](char *buffer, size_t size, size_t nitems, void *outstream) -> size_t
	{
		(*reinterpret_cast<SString*>(outstream)) += std::string(buffer, size * nitems);
		return size * nitems;
	}));

	SString result;
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &result);
	const CURLcode res = curl_easy_perform(curlHandle);

	long httpCode = 0;
	curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);

	SiaCurlError ret = ProcessResponse(res, httpCode, result, response);
	curl_easy_cleanup(curlHandle);
	return ret;
}

bool CSiaCurl::CheckVersion(SiaCurlError& error) const
{
	error = SiaCurlErrorCode::InvalidRequiredVersion;
	if (GetHostConfig().RequiredVersion.Length())
	{
		error = SiaCurlErrorCode::NoResponse;
		const SString serverVersion = GetServerVersion();
		if (serverVersion.Length())
		{
			error = (serverVersion == GetHostConfig().RequiredVersion) ? SiaCurlErrorCode::Success : SiaCurlErrorCode::ServerVersionMismatch;
		}
	}

	return ApiSuccess(error);
}

SString CSiaCurl::GetServerVersion() const
{
	json response;
	if (ApiSuccess(_Get(L"/daemon/version", {}, response)))
	{
		return response["version"].get<std::string>();
	}

	return L"";
}

SiaCurlError CSiaCurl::Get(const SString& path, json& response) const
{
	SiaCurlError ret;
	if (CheckVersion(ret))
	{
		ret = _Get(path, {}, response);
	}

	return ret;
}

SiaCurlError CSiaCurl::Get(const SString& path, const HttpParameters& parameters, json& response) const
{
	SiaCurlError ret;
	if (CheckVersion(ret))
	{
		ret = _Get(path, parameters, response);
	}

	return ret;
}

SiaCurlError CSiaCurl::Post(const SString& path, const HttpParameters& parameters, json& response) const
{
	SiaCurlError ret;
	if (CheckVersion(ret))
	{
		CURL* curlHandle = curl_easy_init();
		curl_easy_reset(curlHandle);

		curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "Sia-Agent");
		curl_easy_setopt(curlHandle, CURLOPT_URL, ConstructPath(path).c_str());
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, static_cast<size_t(*)(char*, size_t, size_t, void *)>([](char *buffer, size_t size, size_t nitems, void *outstream) -> size_t
		{
			(*reinterpret_cast<SString*>(outstream)) += std::string(buffer, size * nitems);
			return size * nitems;
		}));

		SString fields;
		for (const auto& param : parameters)
		{
			if (fields.Length())
			{
				fields += "&";
			}

			fields += (param.first + "=" + param.second);
		}

    std::string utf8Fields = SString::ToUtf8(fields);
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, &utf8Fields[0]);

		SString result;
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &result);
		const CURLcode res = curl_easy_perform(curlHandle);

		long httpCode = 0;
		curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);

		ret = ProcessResponse(res, httpCode, result, response);
		curl_easy_cleanup(curlHandle);
	}

	return ret;
}