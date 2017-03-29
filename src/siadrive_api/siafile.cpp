#include <siaapi.h>

using namespace Sia::Api;

CSiaApi::_CSiaFile::_CSiaFile(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, const json& fileJson) :
	CSiaBase(siaCurl, siaDriveConfig),
	_SiaPath(fileJson["siapath"].get<std::string>()),
	_FileSize(fileJson["filesize"].get<std::uint64_t>()),
	_Available(fileJson["available"].get<bool>()),
	_Renewing(fileJson["renewing"].get<bool>()),
	_Redundancy(fileJson["redundancy"].get<std::uint32_t>()),
	_UploadProgress(fileJson["uploadprogress"].get<std::uint32_t>()),
	_Expiration(fileJson["expiration"].get<std::uint32_t>())
{
	
}

CSiaApi::_CSiaFile::~_CSiaFile()
{
	
}