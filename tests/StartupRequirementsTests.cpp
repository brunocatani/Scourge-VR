#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "StartupRequirements.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void Write(const std::filesystem::path& path, const std::string& contents)
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output << contents;
		output.close();
		Require(output.good(), "Unable to write test fixture");
	}
}

int main()
{
	using StartupRequirements::Failure;
	using StartupRequirements::Check;
	const auto root = std::filesystem::temp_directory_path() /
		("ScourgeRequirementsTests_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64()));
	try {
		Require(std::filesystem::create_directory(root), "Test directory already exists");
		const auto path = StartupRequirements::RobcoPathFromGameDirectory(root / L"Game_\u00e7\u6e2c");
		Require(path == root / L"Game_\u00e7\u6e2c" / "Data" / "F4SE" / "Plugins" / "RobCo_Patcher.ini", "Wrong authoritative INI location");
		Require(Check(false, false, path) == Failure::scalingFlagRemoverMissing, "Missing Scaling Flag Remover was accepted");
		Require(Check(false, true, path) == Failure::scalingFlagRemoverMissing, "RobCo presence bypassed Scaling Flag Remover");
		Require(Check(true, false, path) == Failure::none, "Optional absent RobCo was treated as required");
		Require(Check(true, true, path) == Failure::robcoFileMissing, "Missing RobCo INI was accepted");
		std::filesystem::create_directories(path.parent_path());
		const std::string valid = "; Preserve all user data\r\n[Features]\r\niEnableReCalculateStatsWithSaveLoad=0\r\nOther=1\r\n";
		Write(path, valid);
		const auto stamp = std::filesystem::last_write_time(path);
		Require(Check(true, true, path) == Failure::none, "Explicit disabled setting was rejected");
		std::ifstream input(path, std::ios::binary);
		const std::string after{ std::istreambuf_iterator<char>(input), {} };
		input.close();
		Require(after == valid && std::filesystem::last_write_time(path) == stamp, "Validation changed the user's INI");

		struct Case { const char* name; const char* contents; Failure expected; };
		const Case cases[]{
			{"enabled", "[Features]\niEnableReCalculateStatsWithSaveLoad=1\n", Failure::robcoRecalculationEnabled},
			{"negative", "[Features]\niEnableReCalculateStatsWithSaveLoad=-1\n", Failure::robcoRecalculationEnabled},
			{"missing", "[Features]\nOther=0\n", Failure::robcoSettingMissing},
			{"section", "[Other]\niEnableReCalculateStatsWithSaveLoad=0\n", Failure::robcoSettingMissing},
			{"empty", "[Features]\niEnableReCalculateStatsWithSaveLoad=\n", Failure::robcoSettingMissing},
			{"invalid", "[Features]\niEnableReCalculateStatsWithSaveLoad=false\n", Failure::robcoSettingInvalid},
			{"suffix", "[Features]\niEnableReCalculateStatsWithSaveLoad=0garbage\n", Failure::robcoSettingInvalid},
			{"overflow", "[Features]\niEnableReCalculateStatsWithSaveLoad=999999999999999999999999999999\n", Failure::robcoSettingInvalid},
			{"duplicate-enabled", "[Features]\niEnableReCalculateStatsWithSaveLoad=1\niEnableReCalculateStatsWithSaveLoad=0\n", Failure::robcoRecalculationEnabled},
			{"duplicate-disabled", "[Features]\niEnableReCalculateStatsWithSaveLoad=0\niEnableReCalculateStatsWithSaveLoad=1\n", Failure::none},
			{"case-whitespace", "[features]\nIENABLERECALCULATESTATSWITHSAVELOAD = 0  \n", Failure::none},
			{"quoted", "[Features]\niEnableReCalculateStatsWithSaveLoad=\"0\"\n", Failure::none},
			{"hex", "[Features]\niEnableReCalculateStatsWithSaveLoad=0x0\n", Failure::none}
		};
		for (const auto& test : cases) {
			const auto fixture = root / (std::string(test.name) + ".ini");
			Write(fixture, test.contents);
			Require(Check(true, true, fixture) == test.expected, test.name);
		}
		Require(Check(true, true, root) == Failure::robcoFileUnreadable, "A directory was accepted as an INI");
		const auto lockedPath = root / "locked.ini";
		Write(lockedPath, valid);
		const HANDLE locked = CreateFileW(lockedPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Require(locked != INVALID_HANDLE_VALUE, "Cannot lock fixture");
		const auto lockedResult = Check(true, true, lockedPath);
		CloseHandle(locked);
		Require(lockedResult == Failure::robcoFileUnreadable, "Unreadable INI was treated as disabled");
		std::filesystem::remove_all(root);
		std::cout << "Startup dependency and RobCo configuration checks passed.\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << "\nFixtures: " << root << '\n';
		return 1;
	}
}
