#include <siacommon.h>
#include <ttmath/ttmath.h>
#include <SQLiteCpp/Exception.h>
#include <sqlite3.h>
#include <bitset>

#ifdef _WIN32
#include <Wincrypt.h>

// HACK Fix exports
namespace SQLite
{
// Return the result code (if any, otherwise -1).
int Exception::getErrorCode() const noexcept // nothrow
{
	return mErrcode;
}

// Return the extended numeric result code (if any, otherwise -1).
int Exception::getExtendedErrorCode() const noexcept // nothrow
{
	return mExtendedErrcode;
}

// Return a string, solely based on the error code    
const char* Exception::getErrorStr() const noexcept // nothrow
{
	return sqlite3_errstr(mErrcode);
}
}
#endif

NS_BEGIN(Sia)
NS_BEGIN(Api)

SString GenerateSha256(const SString& str)
{
#ifdef _WIN32
	SString ret;
	HCRYPTPROV hCryptProv = 0;
	HCRYPTHASH hHash = 0;
	BOOL ok = ::CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_AES, 0);
	ok = ok && ::CryptCreateHash(hCryptProv, CALG_SHA_256, 0, 0, &hHash);
	ok = ok && ::CryptHashData(hHash, reinterpret_cast<const BYTE*>(&str[0]), static_cast<DWORD>(str.ByteLength()), 0);
	if (ok)
	{
		DWORD dwHashLen;
		DWORD dwCount = sizeof(DWORD);
		if (::CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE *>(&dwHashLen), &dwCount, 0))
		{
			std::vector<unsigned char> hash(dwHashLen);
			if (::CryptGetHashParam(hHash, HP_HASHVAL, reinterpret_cast<BYTE *>(&hash[0]), &dwHashLen, 0))
			{
				std::ostringstream ss;
				ss << std::hex << std::uppercase << std::setfill('0');
				for (int c : hash)
				{
					ss << std::setw(2) << c;
				}
				ret = ss.str();
			}
		}
	}
	
	if (hHash) ::CryptDestroyHash(hHash);
	if (hCryptProv) ::CryptReleaseContext(hCryptProv, 0);

	return ret;
#else
  a
#endif
}

BOOL RecurDeleteFilesByExtentsion(const SString& folder, const SString& extensionWithDot)
{
#ifdef _WIN32
	BOOL ret = FALSE;
	WIN32_FIND_DATA fd = { 0 };	
	
	SString search;
	search.Resize(MAX_PATH + 1);
	::PathCombine(&search[0], folder.str().c_str(), L"*.*");

	HANDLE find = ::FindFirstFile(search.str().c_str(), &fd);
	if (find != INVALID_HANDLE_VALUE)
	{
		ret = TRUE;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if ((SString(fd.cFileName) != ".") && (SString(fd.cFileName) != ".."))
				{
					SString nextFolder;
					nextFolder.Resize(MAX_PATH + 1);
					::PathCombine(&nextFolder[0], folder.str().c_str(), fd.cFileName);
					ret = RecurDeleteFilesByExtentsion(nextFolder, extensionWithDot);
				}
			}
			else
			{
				SString ext = ::PathFindExtension(fd.cFileName);
				if (ext == extensionWithDot)
				{
					SString filePath;
					filePath.Resize(MAX_PATH + 1);
					::PathCombine(&filePath[0], folder.str().c_str(), fd.cFileName);

					ret = RetryDeleteFileIfExists(filePath.str().c_str());
				}
			}
		} while (ret && (::FindNextFile(find, &fd) != 0));
	}

	return ret;
#else
  a
#endif
}

BOOL RetryDeleteFileIfExists(const SString& filePath)
{
#ifdef _WIN32
	BOOL ret = TRUE;
	if (::PathFileExists(filePath.str().c_str()))
	{
		ret = RetryableAction(::DeleteFile(filePath.str().c_str()), DEFAULT_RETRY_COUNT, DEFAULT_RETRY_DELAY_MS);
	}

	return ret;
#else
  a
#endif
}

BOOL RetryAction(std::function<BOOL()> func, std::uint16_t retryCount, const DWORD& retryDelay)
{
#ifdef _WIN32
	BOOL ret = FALSE;
	while (retryCount-- && !((ret = func())))
	{
		::Sleep(retryDelay);
	}

	return ret;
#else
  a
#endif
}

#ifdef _WIN32
std::vector<SString> GetAvailableDrives()
{
  static const std::vector<char> alpha = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z' };
  std::bitset<26> drives(~GetLogicalDrives() & 0xFFFFFFFF);

  std::vector<SString> avail;
  for (size_t i = 0; i < alpha.size(); i++)
  {
    if (drives[i] && (alpha[i] != 'A') && (alpha[i] != 'B'))
    {
      avail.push_back(alpha[i]);
    }
  }

  return std::move(avail);
}
#endif

NS_END(2)