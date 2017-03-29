#ifndef _SIACOMMON_H
#define _SIACOMMON_H
#include <cstdint>

#define SIDRIVE_VERSION_STRING "0.0.1"
#define COMPAT_SIAD_VERSION "1.1.2"

const std::uint8_t SIA_BLOCKS_PER_MIN = 10;
const std::uint32_t MINUTES_PER_MONTH = (730 * 60);
const std::uint32_t SIA_BLOCKS_PER_MONTH = MINUTES_PER_MONTH / SIA_BLOCKS_PER_MIN;

#ifdef _WIN32 
  // Disable DLL-interface warnings
  #pragma warning(disable: 4251)
  #pragma warning(disable: 4275)

	// Unicode for Win32
	#define UNICODE
	#define _UNICODE

	// _WIN32_WINNT version constants  
	#define _WIN32_WINNT_NT4                    0x0400 // Windows NT 4.0  
	#define _WIN32_WINNT_WIN2K                  0x0500 // Windows 2000  
	#define _WIN32_WINNT_WINXP                  0x0501 // Windows XP  
	#define _WIN32_WINNT_WS03                   0x0502 // Windows Server 2003  
	#define _WIN32_WINNT_WIN6                   0x0600 // Windows Vista  
	#define _WIN32_WINNT_VISTA                  0x0600 // Windows Vista  
	#define _WIN32_WINNT_WS08                   0x0600 // Windows Server 2008  
	#define _WIN32_WINNT_LONGHORN               0x0600 // Windows Vista  
	#define _WIN32_WINNT_WIN7                   0x0601 // Windows 7  
	#define _WIN32_WINNT_WIN8                   0x0602 // Windows 8  
	#define _WIN32_WINNT_WINBLUE                0x0603 // Windows 8.1  
	#define _WIN32_WINNT_WINTHRESHOLD           0x0A00 // Windows 10  
	#define _WIN32_WINNT_WIN10                  0x0A00 // Windows 10  

	// Windows 8.1 or above supported
	#define WINVER _WIN32_WINNT_WINBLUE  
	#define _WIN32_WINNT _WIN32_WINNT_WINBLUE  
	#define _WINSOCKAPI_
	#include <Windows.h>
	#include <Shlwapi.h>

	// Import or export functions and/or classes
	#ifdef SIADRIVE_EXPORT_SYMBOLS
		#define SIADRIVE_EXPORTABLE __declspec(dllexport)
	#else
		#define SIADRIVE_EXPORTABLE __declspec(dllimport)
	#endif //SIADRIVE_EXPORT_SYMBOLS
#else
	#define SIADRIVE_EXPORT_SYMBOLS
#endif

#include <cstdint>
#include <memory>
#include <json.hpp>
#include <ttmath/ttmath.h>
#include <sstring.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <deque>

using json = nlohmann::json;

#define NS_BEGIN(n) namespace n {

#define NS_END1() }
#define NS_END2() NS_END1() }
#define NS_END3() NS_END2() }
#define NS_END4() NS_END3() }
#define NS_END5() NS_END4() }

#define NS_END(c) NS_END##c()

NS_BEGIN(Sia)
NS_BEGIN(Api)

class StartupException :
	public std::exception
{
public:
	StartupException(const SString& reason) :
		std::exception(SString::ToUtf8(reason).c_str())
	{
	}
};

#define DEFAULT_CONFIG_FILE_PATH L"./config/siadriveconfig.json"
#define DEFAULT_RENTER_DB_FILE_PATH L"./config/renter_upload.db3"

#define Property(type, name, get_access, set_access) \
private:\
	type _##name;\
get_access:\
	const type& Get##name() const { return _##name;}\
set_access:\
	const type& Set##name(const type& value) { _##name = value; return _##name; }

#define JProperty(type, name, get_access, set_access, json_doc) \
get_access:\
	type Get##name() const { return json_doc[#name].get<type>();}\
set_access:\
	type Set##name(const type& value) { json_doc[#name] = value; return value; }

typedef struct 
{
	SString HostName;
	std::uint16_t HostPort;
	SString RequiredVersion;
} SiaHostConfig;

template<typename T>
inline bool ApiSuccess(const T& t) {
	return t == T::Success;
}

typedef ttmath::UInt<256> Hastings;
typedef ttmath::Big<1, 30> SiaCurrency;

/*
 BigNumber.config({ EXPONENTIAL_AT: 1e+9 })
 BigNumber.config({ DECIMAL_PLACES: 30 })

const hastingsPerSiacoin = new BigNumber('10').toPower(24)
const siacoinsToHastings = (siacoins) => new BigNumber(siacoins).times(hastingsPerSiacoin)
const hastingsToSiacoins = (hastings) => new BigNumber(hastings).dividedBy(hastingsPerSiacoin)
 */
inline static SiaCurrency HastingsStringToSiaCurrency(const SString& value)
{
	ttmath::Parser<SiaCurrency> parser;
	parser.Parse((value + " / (10 ^ 24)").str());
	return parser.stack[0].value;
}

inline static SString SiaCurrencyToString(const SiaCurrency& value)
{
	ttmath::Conv conv;
	conv.base = 10;
	conv.round = 8;

	return value.ToWString(conv);
}

inline static SString SiaCurrencyToHastingsString(const SiaCurrency& value)
{
  ttmath::Parser<SiaCurrency> parser;
  parser.Parse(value.ToString() + " * (10 ^ 24)");

  ttmath::Conv conv;
  conv.base = 10;
  conv.round = 0;
  return parser.stack[0].value.ToWString(conv);
}

inline static SString SiaCurrencyToGB(const SiaCurrency& value)
{
	ttmath::Conv conv;
	conv.base = 10;
	conv.round = 3;

	return value.ToWString(conv);
}

class IHost
{
public:
	IHost() {}
	virtual ~IHost() {}

public:
	virtual Hastings GetStoragePrice() const = 0;
	virtual Hastings GetDownloadPrice() const = 0;
	virtual Hastings GetUploadPrice() const = 0;
};

template<typename T, typename R>
inline static R CalculateAveragePrice(const std::vector<T>& v, std::function<R(const T& t)> PriceGetter)
{
	R result = v.size() ? std::accumulate(std::next(v.begin()), v.end(), PriceGetter(v[0]), [&](const R& a, const T& b) {
		return a + PriceGetter(b);
	}).Div(v.size()) : 0;

	return result;
}

inline static Hastings CalculateAverageHostPrice(const std::vector<IHost>& hosts)
{
	return CalculateAveragePrice<IHost, Hastings>(hosts, [](const IHost& host)->Hastings { return host.GetStoragePrice(); });
}

inline static Hastings CalculateAverageDownloadPrice(const std::vector<IHost>& hosts)
{
	return CalculateAveragePrice<IHost, Hastings>(hosts, [](const IHost& host)->Hastings { return host.GetDownloadPrice(); });
}

inline static Hastings CalculateAverageUploadPrice(const std::vector<IHost>& hosts)
{
	return CalculateAveragePrice<IHost, Hastings>(hosts, [](const IHost& host)->Hastings { return host.GetUploadPrice(); });
}

BOOL SIADRIVE_EXPORTABLE RetryAction(std::function<BOOL()> func, std::uint16_t retryCount, const DWORD& retryDelay);

BOOL SIADRIVE_EXPORTABLE RetryDeleteFileIfExists(const SString& filePath);

SString SIADRIVE_EXPORTABLE GenerateSha256(const SString& str);

BOOL SIADRIVE_EXPORTABLE RecurDeleteFilesByExtentsion(const SString& folder, const SString& extensionWithDot);

#ifdef _WIN32
std::vector<SString> SIADRIVE_EXPORTABLE GetAvailableDrives();
#endif

#define RetryableAction(exec, count, delayMs) RetryAction([&]()->BOOL{return exec;}, count, delayMs)
#define DEFAULT_RETRY_COUNT 10
#define DEFAULT_RETRY_DELAY_MS 1000

NS_END(2)
#endif //_SIACOMMON_H