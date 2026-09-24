#pragma once

#include "RE/Fallout.h"
#include "REX/W32/KERNEL32.h"

#include <string_view>
#include <unordered_set>

namespace FormLookup
{
	constexpr std::uint8_t kDaytripperSmallFileSlot = 0xFE;
	constexpr RE::TESFormID kFullFormMask = 0x00FFFFFF;
	constexpr RE::TESFormID kSmallFormMask = 0x00000FFF;
	constexpr RE::TESFormID kSmallFormPrefix = 0xFE000000;

	struct LoadedFile
	{
		const RE::TESFile* file{ nullptr };
		bool isFullPlugin{ false };
		bool usedDaytripperSmallFileStorage{ false };
	};

	inline bool PluginNameEquals(const RE::TESFile* a_file, std::string_view a_pluginName)
	{
		if (!a_file) {
			return false;
		}

		const auto filename = a_file->GetFilename();
		return filename.size() == a_pluginName.size() &&
			_strnicmp(filename.data(), a_pluginName.data(), a_pluginName.size()) == 0;
	}

	inline bool IsDaytripperLoaded()
	{
		return REX::W32::GetModuleHandleW(L"falloutvresl") ||
			REX::W32::GetModuleHandleW(L"falloutvresl.dll") ||
			REX::W32::GetModuleHandleW(L"Daytripper4") ||
			REX::W32::GetModuleHandleW(L"Daytripper4.dll");
	}

	inline RE::BSTArray<RE::TESFile*>* GetDaytripperSmallFiles(RE::TESDataHandler* a_dataHandler)
	{
		if (!REL::Module::IsVR() || !IsDaytripperLoaded() || !a_dataHandler) {
			return nullptr;
		}

		const auto vrModData = a_dataHandler->GetVRModData();
		if (!vrModData) {
			return nullptr;
		}

		auto* rawSmallFiles = vrModData->loadedMods[kDaytripperSmallFileSlot];
		if (!rawSmallFiles) {
			return nullptr;
		}

		return reinterpret_cast<RE::BSTArray<RE::TESFile*>*>(rawSmallFiles);
	}

	inline const RE::TESFile* LookupDaytripperSmallFileByName(RE::TESDataHandler* a_dataHandler, std::string_view a_pluginName)
	{
		auto* smallFiles = GetDaytripperSmallFiles(a_dataHandler);
		if (!smallFiles) {
			return nullptr;
		}

		for (std::uint32_t i = 0; i < smallFiles->size(); ++i) {
			const auto* file = smallFiles->at(i);
			if (PluginNameEquals(file, a_pluginName)) {
				return file;
			}
		}

		return nullptr;
	}

	inline void LogDaytripperSmallFileResolution(const RE::TESFile* a_file)
	{
		if (!a_file) {
			return;
		}

		static std::unordered_set<std::string> loggedPlugins;
		const auto filename = std::string{ a_file->GetFilename() };
		if (loggedPlugins.insert(filename).second) {
			logger::info("Resolved light plugin {} through Daytripper small-file storage (FE{:03X})",
				filename,
				a_file->smallFileCompileIndex);
		}
	}

	inline LoadedFile ResolveLoadedFile(RE::TESDataHandler* a_dataHandler, std::string_view a_pluginName)
	{
		if (!a_dataHandler || a_pluginName.empty()) {
			return {};
		}

		if (const auto* fullPlugin = a_dataHandler->LookupLoadedModByName(a_pluginName)) {
			return { fullPlugin, true, false };
		}

		if (const auto* lightPlugin = a_dataHandler->LookupLoadedLightModByName(a_pluginName)) {
			return { lightPlugin, false, false };
		}

		if (const auto* daytripperSmallFile = LookupDaytripperSmallFileByName(a_dataHandler, a_pluginName)) {
			LogDaytripperSmallFileResolution(daytripperSmallFile);
			return { daytripperSmallFile, false, true };
		}

		return {};
	}

	inline RE::TESFormID BuildRuntimeFormID(const LoadedFile& a_loadedFile, RE::TESFormID a_rawFormID)
	{
		if (!a_loadedFile.file) {
			return 0;
		}

		if (a_loadedFile.isFullPlugin) {
			return (static_cast<RE::TESFormID>(a_loadedFile.file->compileIndex) << 24) | (a_rawFormID & kFullFormMask);
		}

		return kSmallFormPrefix |
			((static_cast<RE::TESFormID>(a_loadedFile.file->smallFileCompileIndex) & 0x0FFF) << 12) |
			(a_rawFormID & kSmallFormMask);
	}

	inline RE::TESFormID LookupFormID(RE::TESDataHandler* a_dataHandler, RE::TESFormID a_rawFormID, std::string_view a_pluginName)
	{
		return BuildRuntimeFormID(ResolveLoadedFile(a_dataHandler, a_pluginName), a_rawFormID);
	}

	inline RE::TESForm* LookupForm(RE::TESDataHandler* a_dataHandler, RE::TESFormID a_rawFormID, std::string_view a_pluginName)
	{
		const auto formID = LookupFormID(a_dataHandler, a_rawFormID, a_pluginName);
		return formID != 0 ? RE::TESForm::GetFormByID(formID) : nullptr;
	}

	template <class T>
	T* LookupForm(RE::TESDataHandler* a_dataHandler, RE::TESFormID a_rawFormID, std::string_view a_pluginName)
	{
		const auto form = LookupForm(a_dataHandler, a_rawFormID, a_pluginName);
		return form && form->Is(T::FORM_ID) ? form->As<T>() : nullptr;
	}
}
