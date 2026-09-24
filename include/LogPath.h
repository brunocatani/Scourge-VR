#pragma once

#include <filesystem>

namespace LogPath
{
	inline std::filesystem::path Fallout4VRLogDirectoryFromDocuments(const std::filesystem::path& a_documentsDirectory)
	{
		return a_documentsDirectory / "My Games" / "Fallout4VR" / "F4SE";
	}
}
