#include <siadriveconfig.h>
#include <fstream>
#include <filepath.h>

using namespace Sia::Api;

CSiaDriveConfig::CSiaDriveConfig() :
	CSiaDriveConfig(DEFAULT_CONFIG_FILE_PATH)
{
}

CSiaDriveConfig::CSiaDriveConfig(const SString& filePath) :
	_FilePath(filePath)
{
#ifdef _DEBUG
  FilePath(filePath).DeleteFile();
#endif
	Load();
}

CSiaDriveConfig::~CSiaDriveConfig()
{
	Save();
}

void CSiaDriveConfig::LoadDefaults()
{
	SetRenter_UploadDbFilePath(static_cast<SString>(FilePath(DEFAULT_RENTER_DB_FILE_PATH)));

	FilePath tempFolder = FilePath::GetTempDirectory();
	SetTempFolder(static_cast<SString>(tempFolder));

	FilePath cacheFolder(FilePath::GetAppDataDirectory(), "SiaDrive");
  cacheFolder.Append("Cache");

	SetCacheFolder(static_cast<SString>(cacheFolder));

  SetHostNameOrIp("localhost");
  SetHostPort(9980);
}

void CSiaDriveConfig::Load( )
{
  FilePath filePath = GetFilePath();
	std::ifstream myfile(static_cast<SString>(filePath).str().c_str());
	if (myfile.is_open())
	{
		std::stringstream ss;
		ss << myfile.rdbuf();
		std::string jsonTxt = ss.str();
		_configDocument = json::parse(jsonTxt.begin(), jsonTxt.end());
		myfile.close();
	}
	else
	{
		LoadDefaults();
		Save();
	}
}

void CSiaDriveConfig::Save() const
{
	FilePath filePath = GetFilePath();
  FilePath folder = filePath;
  folder.RemoveFileName();

	if (!folder.IsDirectory())
	{
    folder.CreateDirectory();
	}
	std::ofstream(SString::ToUtf8(static_cast<SString>(filePath)).c_str()) << std::setw(2) << _configDocument << std::endl;
}