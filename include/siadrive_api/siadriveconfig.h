#pragma once
#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class SIADRIVE_EXPORTABLE CSiaDriveConfig
{
public:
	CSiaDriveConfig();

	CSiaDriveConfig(const SString& filePath);

public:
	~CSiaDriveConfig();
	
	Property(SString, FilePath, public, private)
	JProperty(std::string, Renter_UploadDbFilePath, public, private, _configDocument)
	JProperty(std::string, TempFolder, public, private, _configDocument)
	JProperty(std::string, CacheFolder, public, public, _configDocument)
	JProperty(std::uint16_t, HostPort, public, public, _configDocument)
	JProperty(std::string, HostNameOrIp, public, public, _configDocument)

private:
	json _configDocument;

private:
	void LoadDefaults();

	void Load( );

	void Save() const;
};

NS_END(2)