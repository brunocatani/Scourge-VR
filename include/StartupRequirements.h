#pragma once

#include <filesystem>
#include <string_view>

namespace StartupRequirements
{
	enum class Failure
	{
		none,
		scalingFlagRemoverMissing,
		robcoFileMissing,
		robcoFileUnreadable,
		robcoSettingMissing,
		robcoSettingInvalid,
		robcoRecalculationEnabled
	};

	std::filesystem::path RobcoPathFromGameDirectory(const std::filesystem::path& gameDirectory);
	std::filesystem::path ResolveRobcoPath();
	Failure Check(bool scalingFlagRemoverLoaded, bool robcoLoaded, const std::filesystem::path& robcoPath);
	std::string_view Describe(Failure failure);
}
