#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "StartupRequirements.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <stdexcept>
#include <system_error>

namespace StartupRequirements
{
	std::filesystem::path RobcoPathFromGameDirectory(const std::filesystem::path& gameDirectory)
	{
		return gameDirectory / "Data" / "F4SE" / "Plugins" / "RobCo_Patcher.ini";
	}

	std::filesystem::path ResolveRobcoPath()
	{
		std::array<wchar_t, 32768> executable{};
		const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
		if (length == 0 || length == executable.size()) {
			throw std::runtime_error("Cannot resolve the game directory for RobCo Patcher configuration");
		}
		return RobcoPathFromGameDirectory(std::filesystem::path(executable.data()).parent_path());
	}

	Failure Check(bool scalingFlagRemoverLoaded, bool robcoLoaded, const std::filesystem::path& robcoPath)
	{
		if (!scalingFlagRemoverLoaded) return Failure::scalingFlagRemoverMissing;
		if (!robcoLoaded) return Failure::none;

		std::error_code error;
		const bool exists = std::filesystem::exists(robcoPath, error);
		if (error) return Failure::robcoFileUnreadable;
		if (!exists) return Failure::robcoFileMissing;
		if (!std::filesystem::is_regular_file(robcoPath, error) || error) return Failure::robcoFileUnreadable;
		FILE* rawFile{ nullptr };
		if (_wfopen_s(&rawFile, robcoPath.c_str(), L"rb") != 0) return Failure::robcoFileUnreadable;
		const std::unique_ptr<FILE, decltype(&fclose)> file(rawFile, fclose);

		// Match RobCo VR's Windows profile API, including its first-key-wins
		// behavior. SimpleIni's duplicate-key handling can disagree with it.
		std::array<wchar_t, 64> value{};
		const auto length = GetPrivateProfileStringW(L"Features", L"iEnableReCalculateStatsWithSaveLoad",
			L"", value.data(), static_cast<DWORD>(value.size()), robcoPath.c_str());
		if (length == 0) return Failure::robcoSettingMissing;
		if (length == value.size() - 1) return Failure::robcoSettingInvalid;
		wchar_t* end{ nullptr };
		errno = 0;
		const auto parsed = std::wcstoll(value.data(), &end, 0);
		if (errno == ERANGE || end == value.data()) return Failure::robcoSettingInvalid;
		while (std::iswspace(*end)) ++end;
		if (*end != L'\0') return Failure::robcoSettingInvalid;
		if (parsed != 0 || GetPrivateProfileIntW(L"Features", L"iEnableReCalculateStatsWithSaveLoad", 1, robcoPath.c_str()) != 0) {
			return Failure::robcoRecalculationEnabled;
		}
		return Failure::none;
	}

	std::string_view Describe(Failure failure)
	{
		switch (failure) {
		case Failure::none: return "Startup requirements passed";
		case Failure::scalingFlagRemoverMissing: return "Scaling Flag Remover VR must be loaded by F4SEVR. Install the original mod, overwrite its GLXRM_ScalingFlagRemover.dll with the VR port, and enable it in MO2.";
		case Failure::robcoFileMissing: return "The active RobCo Patcher INI is missing. SCOURGE cannot verify that save-load stat recalculation is disabled.";
		case Failure::robcoFileUnreadable: return "The active RobCo Patcher INI cannot be read. SCOURGE cannot verify that save-load stat recalculation is disabled.";
		case Failure::robcoSettingMissing: return "The active RobCo Patcher INI has no explicit iEnableReCalculateStatsWithSaveLoad setting.";
		case Failure::robcoSettingInvalid: return "The active RobCo Patcher INI has an invalid iEnableReCalculateStatsWithSaveLoad value.";
		case Failure::robcoRecalculationEnabled: return "RobCo Patcher save-load stat recalculation is enabled and conflicts with SCOURGE.";
		}
		return "Unknown startup requirement failure";
	}
}
