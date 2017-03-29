#include <siaapi.h>
#include <regex>

using namespace Sia::Api;

CSiaApi::_CSiaFileTree::_CSiaFileTree(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
	CSiaBase(siaCurl, siaDriveConfig)
{
	
}

CSiaApi::_CSiaFileTree::~_CSiaFileTree()
{
	
}

void CSiaApi::_CSiaFileTree::BuildTree(const json& result)
{
	_fileList.clear();
	for (const auto& file : result["files"])
	{
		_fileList.push_back(CSiaFilePtr(new CSiaFile(GetSiaCurl(), &GetSiaDriveConfig(), file)));
	}
}

bool CSiaApi::_CSiaFileTree::FileExists(const SString& siaPath) const
{
	auto result = std::find_if(_fileList.begin(), _fileList.end(), [&](const CSiaFilePtr& item)->bool
	{
		return (item->GetSiaPath() == siaPath);
	});

	return (result != _fileList.end());
}

CSiaFileCollection CSiaApi::_CSiaFileTree::GetFileList() const
{
	return _fileList;
}

CSiaFilePtr CSiaApi::_CSiaFileTree::GetFile(const SString& siaPath) const
{
	auto result = std::find_if(_fileList.begin(), _fileList.end(), [&](const CSiaFilePtr& item)->bool
	{
		return (item->GetSiaPath() == siaPath);
	});
	
	return ((result != _fileList.end()) ? *result : nullptr);
}

CSiaFileCollection CSiaApi::_CSiaFileTree::Query(SString query) const
{
	query = CSiaApi::FormatToSiaPath(query);
	query.Replace(".", "\\.").Replace("*", "[^/]+").Replace("?", "[^/]?");
	std::wregex r(query.str());

	CSiaFileCollection ret;
	std::copy_if(_fileList.begin(), _fileList.end(), std::back_inserter(ret), [&](const CSiaFilePtr& v) -> bool
	{
		return std::regex_match(v->GetSiaPath().str(), r);
	});

	return std::move(ret);
}

std::vector<SString> CSiaApi::_CSiaFileTree::QueryDirectories(SString rootFolder) const
{
	CSiaFileCollection col;
	rootFolder.Replace("/", "\\");
	if (rootFolder[0] == '\\')
	{
		rootFolder = rootFolder.SubString(1);
	}

	std::vector<SString> ret;
	std::for_each(_fileList.begin(), _fileList.end(), [&](const CSiaFilePtr& v)
	{
		SString dir = v->GetSiaPath();
		dir.Replace("/", "\\");
		::PathRemoveFileSpec(&dir[0]);
		::PathRemoveBackslash(&dir[0]);
		::PathRemoveBackslash(&rootFolder[0]);
		dir = dir.str().c_str();
		rootFolder = rootFolder.str().c_str();
		if ((dir.Length() > rootFolder.Length()) && (dir.SubString(0, rootFolder.Length()) == rootFolder))
		{
			SString subFolder = dir.SubString(rootFolder.Length());
			int idx = subFolder.str().find('\\');
			if (idx == 0)
			{
				subFolder = subFolder.SubString(1);
				idx = subFolder.str().find('\\');
			}

			if (idx > 0)
			{
				subFolder = subFolder.SubString(0, idx);
			}

			auto it = std::find(ret.begin(), ret.end(), subFolder);
			if (it == ret.end())
			{
				ret.push_back(subFolder);
			}
		}
	});

	return std::move(ret);
}