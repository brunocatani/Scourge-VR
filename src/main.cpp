#include "LoadEditorIDs.h"
#include "FormLookup.h"
#include "LogPath.h"
#include "PapyrusManager.h"
#include "RNJesus.h"
#include "SCOURGE.h"
#include "StartupRequirements.h"
#include "YamlParser.h"

#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"
#include "REX/W32/KERNEL32.h"
#include "REX/W32/USER32.h"

ModConfigs configData;
PluginData pluginData;
RNJesus mathUtils;

namespace
{
	bool startupVerified{ false };

	[[noreturn]] void AbortStartup(std::string_view reason, const std::filesystem::path& configPath = {}) noexcept
	{
		logger::critical("{}", reason);
		std::wstring message(reason.begin(), reason.end());
		if (!configPath.empty()) {
			const auto utf8 = configPath.u8string();
			logger::critical("RobCo Patcher INI: {}", std::string(utf8.begin(), utf8.end()));
			message += L"\n\n" + configPath.native();
			message += L"\n\nSet [Features] iEnableReCalculateStatsWithSaveLoad=0 in this file.";
		}
		message += L"\n\nThe game will terminate when this message is closed.";
		spdlog::default_logger()->flush();
		REX::W32::MessageBoxW(nullptr, message.c_str(), L"SCOURGE requirements", 0x00001010);
		std::abort();
	}

	void IgnoreDependencyMessages(F4SE::MessagingInterface::Message*) {}

	bool IsExcludedNPC(const RE::TESNPC* a_npc, std::uint32_t a_workbenchBot, std::uint32_t a_levelledWorkbenchBot)
	{
		// the player, MQ101 Nate and Nora so that they don't become midgets, and the automatron
		// workbench robots
		return a_npc->IsPlayer() || a_npc->formID == 687412 || a_npc->formID == 687413 ||
			a_npc->formID == a_levelledWorkbenchBot || a_npc->formID == a_workbenchBot;
	}

	// TESSpellList::AddSpell (Fallout4VR.exe 0x14016E480 -> 0x14016DEE0) skips a spell already in the
	// list and returns false then, so this counts only the records that lacked it.
	void DistributeSpell(RE::TESDataHandler* a_dataHandler, std::string_view a_when)
	{
		std::uint32_t workbenchBot{ 0 };
		std::uint32_t levelledWorkbenchBot{ 0 };
		if (a_dataHandler->LookupModByName("DLCRobot.esm")) {
			workbenchBot = FormLookup::LookupFormID(a_dataHandler, 69753, "DLCRobot.esm");
			levelledWorkbenchBot = FormLookup::LookupFormID(a_dataHandler, 7917, "DLCRobot.esm");
		}

		std::uint32_t added{ 0 };
		for (auto currentNPC : a_dataHandler->GetFormArray<RE::TESNPC>()) {
			if (currentNPC && !IsExcludedNPC(currentNPC, workbenchBot, levelledWorkbenchBot) && currentNPC->AddSpell(pluginData.spellForm)) {
				++added;
			}
		}
		logger::warn("Distributed SCOURGE_SPEL to {} NPC records ({})", added, a_when);
	}

	bool IsPluginLoaded(const char* registeredName, const wchar_t* dllName)
	{
		const auto* messaging = F4SE::GetMessagingInterface();
		if (!messaging) AbortStartup("F4SEVR messaging is unavailable; dependencies cannot be verified.");
		// F4SEVR 0.6.21 has no GetPluginInfo accessor. After PostLoad,
		// named listener registration succeeds only for an accepted plugin.
		// The inert listener has the same process lifetime as this DLL.
		return REX::W32::GetModuleHandleW(dllName) &&
			messaging->RegisterListener(IgnoreDependencyMessages, registeredName);
	}

	void VerifyRequirements()
	{
		const bool scalingLoaded = IsPluginLoaded("GLXRM_ScalingFlagRemover", L"GLXRM_ScalingFlagRemover.dll");
		const bool robcoLoaded = IsPluginLoaded("RobCo_Patcher", L"RobCo_Patcher.dll");
		const auto configPath = robcoLoaded ? StartupRequirements::ResolveRobcoPath() : std::filesystem::path{};
		const auto failure = StartupRequirements::Check(scalingLoaded, robcoLoaded, configPath);
		if (failure != StartupRequirements::Failure::none) {
			AbortStartup(StartupRequirements::Describe(failure),
				failure == StartupRequirements::Failure::scalingFlagRemoverMissing ? std::filesystem::path{} : configPath);
		}
		if (robcoLoaded) {
			const auto utf8 = configPath.u8string();
			logger::warn("Verified RobCo INI: {}; iEnableReCalculateStatsWithSaveLoad=0", std::string(utf8.begin(), utf8.end()));
		}
		logger::warn("Startup requirements passed: Scaling Flag Remover loaded; RobCo Patcher {}",
			robcoLoaded ? "save-load stat recalculation explicitly disabled" : "not loaded");
	}

	std::optional<std::filesystem::path> GetFallout4VRLogDirectory()
	{
		wchar_t* buffer{ nullptr };
		const auto result = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(buffer));
		std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> documentsPath(buffer, REX::W32::CoTaskMemFree);
		if (!documentsPath || result != 0) {
			return std::nullopt;
		}

		auto path = LogPath::Fallout4VRLogDirectoryFromDocuments(documentsPath.get());
		std::error_code ec;
		std::filesystem::create_directories(path, ec);
		if (ec) {
			return std::nullopt;
		}

		return path;
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) noexcept
{
	try {
		if (!a_f4se || !a_info) return false;
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = GetFallout4VRLogDirectory();
		if (!path) {
			return false;
		}

		*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
		log->set_level(spdlog::level::trace);
#else
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::warn);
#endif

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("--> %v"s);

		logger::info("SCOURGE v{}.{}.{} log:", Version::MAJOR, Version::MINOR, Version::PATCH);
		logger::info("---------------------------------------------------------------------");

		a_info->infoVersion = F4SE::PluginInfo::kVersion;
		a_info->name = "GLXRM_SCOURGE";
		a_info->version = Version::MAJOR;

		if (a_f4se->IsEditor()) {
			logger::critical("loaded in editor");
			return false;
		}

		if (!REL::Module::IsVR() || REL::Module::get().version() != F4SE::RUNTIME_VR_1_2_72) {
			logger::critical("Fallout4VR.exe 1.2.72.0 is required");
			return false;
		}
		logger::warn("Query passed: executable {}; F4SE compatibility runtime {}",
			REL::Module::get().version().string(), a_f4se->RuntimeVersion().string());

		return true;
	} catch (const std::exception& error) {
		logger::critical("SCOURGE Query failed: {}", error.what());
		return false;
	} catch (...) {
		logger::critical("SCOURGE Query failed with an unknown error");
		return false;
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) noexcept
{
	try {
		if (!a_f4se) return false;
		F4SE::Init(a_f4se, false);  // false = don't reinitialize logger, keep the one from Query

		const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
		if (!papyrus || !papyrus->Register(Papyrus::BindFunctions)) {
			logger::critical("Unable to register SCOURGE Papyrus functions");
			return false;
		}

		const F4SE::MessagingInterface* messageInterface = F4SE::GetMessagingInterface();
		if (!messageInterface || !messageInterface->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
			try {
				if (!msg) return;
				if (msg->type == F4SE::MessagingInterface::kPostLoad) {
					VerifyRequirements();
					if (IsPluginLoaded("BakaFramework", L"BakaFramework.dll")) {
						logger::warn("Baka Framework is loaded");
					} else {
						logger::warn("Baka Framework is not loaded. Installing EditorID hooks");
						Patches::LoadEditorIDs::Install();
						logger::warn("EditorID hooks installed successfully");
					}
					startupVerified = true;
					return;
				}
				if (msg->type == F4SE::MessagingInterface::kPreLoadGame) {
					// RobCo rereads this option on each save load; check before it runs.
					VerifyRequirements();
					// Returning to the main menu (a death reload, or quitting to the menu) reverts NPC
					// records to their plugin data before the next save is read: measured on the bench,
					// 274 records, placed NPCs such as Piper among them, had lost the spell here. Game
					// data is ready only once per process, so the spell is distributed before each load.
					if (pluginData.loadedCorrectly && pluginData.spellForm) {
						if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
							DistributeSpell(dataHandler, "before save load");
						}
					}
					return;
				}
				if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
					if (!msg->data) return;
					if (!startupVerified) AbortStartup("SCOURGE startup dependencies were not verified before game data became ready.");
					logger::warn("kGameDataReady received");

					pluginData.LoadPluginData();

					logger::warn("---------------------------------------------------------------------");

					if (pluginData.loadedCorrectly == false) {
						logger::warn("Plugin has not been able to load correctly. Aborting further operations...");
					} else {
						configData.LoadConfigs();

						if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
							DistributeSpell(dataHandler, "game data ready");
						} else {
							logger::warn("Unable to distribute SCOURGE_SPEL! The plugin will not work...");
						}
					}
				}
			} catch (const std::exception& error) {
				logger::critical("SCOURGE message processing failed: {}", error.what());
				AbortStartup("SCOURGE could not safely initialize or process game data. See GLXRM_SCOURGE.log for details.");
			} catch (...) {
				AbortStartup("SCOURGE encountered an unknown initialization error. See GLXRM_SCOURGE.log for details.");
			}
		})) {
			logger::critical("Unable to register SCOURGE F4SE listener");
			return false;
		}

		logger::warn("Load complete; awaiting dependency validation after all plugins load");
		return true;
	} catch (const std::exception& error) {
		logger::critical("SCOURGE Load failed: {}", error.what());
		AbortStartup("SCOURGE initialization failed. See GLXRM_SCOURGE.log for details.");
	} catch (...) {
		AbortStartup("SCOURGE initialization failed with an unknown error.");
	}
}
