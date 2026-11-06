#include "PapyrusManager.h"
#include "SCOURGE.h"

namespace Utils
{
	RE::TESNPC* GetBaseNPC(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}

		if (a_actor->extraList) {
			if (auto* extraLvlCreature = a_actor->extraList->GetByType(RE::EXTRA_DATA_TYPE::kLeveledCreature)) {
				auto* leveledCreature = static_cast<RE::ExtraLeveledCreature*>(extraLvlCreature);
				if (leveledCreature->originalBase) {
					if (auto* originalNPC = leveledCreature->originalBase->As<RE::TESNPC>()) {
						return originalNPC;
					}
				}
			}
		}

		return a_actor->GetNPC();
	}

	bool ActorMatchesKeyword(RE::Actor* a_actor, RE::TESNPC* a_baseNPC, RE::BGSKeyword* a_keyword);

	bool CanRecalculateActor(RE::Actor* a_actor, bool a_skipAlreadyTracked)
	{
		if (!a_actor) {
			return false;
		}

		if (a_actor->IsPlayer() || a_actor->IsDead(true)) {
			return false;
		}

		auto* baseNPC = GetBaseNPC(a_actor);
		if (!baseNPC) {
			return false;
		}

		if (ActorMatchesKeyword(a_actor, baseNPC, pluginData.kw_All)) {
			return false;
		}

		if (a_skipAlreadyTracked && pluginData.av_SCOURGE && a_actor->GetActorValue(*pluginData.av_SCOURGE) != 0.0f) {
			return false;
		}

		return true;
	}

	// The actor's own race is the only authority, as in the original mod. A leveled actor's base record reads
	// SynthGen2RaceValentine at runtime: testing it put every such actor through that race's filter as well, and the
	// last filter wins.
	bool ActorMatchesRace(RE::Actor* a_actor, const std::unordered_set<RE::TESRace*>& a_races)
	{
		if (a_races.empty()) {
			return true;
		}

		return a_actor && a_actor->race && a_races.find(a_actor->race) != a_races.end();
	}

	bool ActorMatchesFaction(RE::Actor* a_actor, RE::TESNPC* a_baseNPC, RE::TESFaction* a_faction)
	{
		if (!a_faction) {
			return false;
		}

		return (a_actor && a_actor->IsInFaction(a_faction)) || (a_baseNPC && a_baseNPC->IsInFaction(a_faction));
	}

	bool ActorMatchesKeyword(RE::Actor* a_actor, RE::TESNPC* a_baseNPC, RE::BGSKeyword* a_keyword)
	{
		if (!a_keyword) {
			return false;
		}

		if (a_actor && a_actor->HasKeyword(a_keyword)) {
			return true;
		}

		if (a_baseNPC && a_baseNPC->HasKeyword(a_keyword)) {
			return true;
		}

		if (a_actor && a_actor->race && a_actor->race->HasKeyword(a_keyword)) {
			return true;
		}

		return false;
	}
}

namespace Papyrus
{
	// populate a_array with pointers to Globals from filters
	int BuildValueList(RE::Actor* a_actor, std::array<std::pair<ValueData, float>, AV_COUNT>& a_arrayToPopulate) {
		int lastFilter{ 0 };
		auto* baseNPC = Utils::GetBaseNPC(a_actor);

		if (!baseNPC) {
			return lastFilter;
		}

		// see if the NPC matches the filter conditions
		for (const auto& filter : configData.filters) {
			bool skipFilter{ false };

			// NPC forms
			if (filter.npcList.empty() == false) {
				//logger::warn("npc form id: {}", Utils::GetBaseNPC(a_actor)->formID);
				if (filter.npcList.find(baseNPC->formID) != filter.npcList.end()) {
					goto RECALCULATION;
				}

				// if the other lists are empty we do an early continue to prevent all NPCs from being
				// included in the recalculation as there are no conditions for them to meet
				if (filter.raceList.empty() && filter.factionList.empty() && filter.keywordList.empty()) {
					continue;
				}
			}

			if (filter.raceList.empty() == false) {
				// if the NPC's Race isn't listed we skip this filter
				if (!Utils::ActorMatchesRace(a_actor, filter.raceList)) {
					continue;
				}
			}

			if (filter.factionList.empty() == false) {
				for (RE::TESFaction* faction : filter.factionList) {
					if (!Utils::ActorMatchesFaction(a_actor, baseNPC, faction)) {
						skipFilter = true;
						break;
					}
				}
				if (skipFilter) {
					continue;
				}
			}

			if (filter.keywordList.empty() == false) {
				for (RE::BGSKeyword* keyword : filter.keywordList) {
					if (!Utils::ActorMatchesKeyword(a_actor, baseNPC, keyword)) {
						skipFilter = true;
						break;
					}
				}
				if (skipFilter) {
					continue;
				}
			}

			if (baseNPC->IsFemale()) {
				if (filter.gender.female == false) {
					continue;
				}
			} else {
				if (filter.gender.male == false) {
					continue;
				}
			}

		RECALCULATION:

			// an NPC will have a single recalculation applied to them based on the combined values
			// of all filters that it passed
			const auto iterator = configData.valueMap.find(filter.globalString);
			if (iterator == configData.valueMap.end()) {
				logger::warn("Skipping filter {} because global string \"{}\" was not loaded", filter.filterName, filter.globalString);
				continue;
			}

			// populate a_array's Globals with the ones from this filter. If any are nullptr then they
			// are most likely purposefully left out to create a partial lists of Globals that only
			// affect a single stat
			auto valueCount = pluginData.pluginRecords.size();
			valueCount = (std::min)(valueCount, iterator->second.size());
			valueCount = (std::min)(valueCount, a_arrayToPopulate.size());
			for (std::size_t i = 0; i < valueCount; i++) {
				if (iterator->second[i].enableGlobal.first) {
					a_arrayToPopulate[i].first.enableGlobal.first = iterator->second[i].enableGlobal.first;
				}

				if (iterator->second[i].minGlobal.first) {
					a_arrayToPopulate[i].first.minGlobal.first = iterator->second[i].minGlobal.first;
				}

				if (iterator->second[i].maxGlobal.first) {
					a_arrayToPopulate[i].first.maxGlobal.first = iterator->second[i].maxGlobal.first;
				}

				if (iterator->second[i].biasGlobal.first) {
					a_arrayToPopulate[i].first.biasGlobal.first = iterator->second[i].biasGlobal.first;
				}

				if (iterator->second[i].influenceGlobal.first) {
					a_arrayToPopulate[i].first.influenceGlobal.first = iterator->second[i].influenceGlobal.first;
				}

				if (const auto mult = filter.mults.find(pluginData.pluginRecords[i].actorValueID); mult != filter.mults.end()) {
					a_arrayToPopulate[i].second = mult->second;
				} else {
					a_arrayToPopulate[i].second = 1.0f;
				}
			}

			lastFilter = filter.filterID;
		}

		return lastFilter;
	}

	void RecalculateStats(RE::Actor* a_actor, std::array<std::pair<ValueData, float>, AV_COUNT>& a_statArray, int& a_trackerValue)
	{
		auto* baseNPC = Utils::GetBaseNPC(a_actor);
		if (!baseNPC) {
			return;
		}

		if (!pluginData.pluginRecords.empty() && a_statArray[0].first.enableGlobal.first && pluginData.globalStatMults.size() > 0 && pluginData.globalStatMults[0]) {
			if (a_statArray[0].first.enableGlobal.first->GetValue() == 1.0f) {
				if (!Utils::ActorMatchesKeyword(a_actor, baseNPC, pluginData.pluginRecords[0].exclusionKeyword)) {
					if (a_statArray[0].first.minGlobal.first && a_statArray[0].first.maxGlobal.first) {
						float bias = 0.5f;

						if (a_statArray[0].first.biasGlobal.first != nullptr) {
							// if the user is stupid and forced a value outside of the MCM range then that's their problem
							bias = a_statArray[0].first.biasGlobal.first->GetValue();
						}

						float randomValue = mathUtils.DoGaussian(a_statArray[0].first.minGlobal.first->GetValue(), a_statArray[0].first.maxGlobal.first->GetValue(), bias);
						float newScale = a_statArray[0].second * randomValue * pluginData.globalStatMults[0]->GetValue();
						a_actor->SetScale(newScale);
					}
				}
			}
		}

		for (int i = 1; i < a_statArray.size(); i++) {
			if (i >= pluginData.pluginRecords.size() || i >= pluginData.globalStatMults.size()) {
				continue;
			}
			// check if the _Enable Global exists and isn't disabled
			if (a_statArray[i].first.enableGlobal.first == nullptr || a_statArray[i].first.enableGlobal.first->GetValue() == 0.0f) {
				continue;
			}
			// check if the NPC has the exclusion keyword
			if (Utils::ActorMatchesKeyword(a_actor, baseNPC, pluginData.pluginRecords[i].exclusionKeyword)) {
				continue;
			}
			// make sure that both the _Min and _Max Globals exist
			if (a_statArray[i].first.minGlobal.first == nullptr || a_statArray[i].first.maxGlobal.first == nullptr || !pluginData.pluginRecords[i].actorValue || !pluginData.globalStatMults[i]) {
				continue;
			}

			float bias = 0.5f;

			if (a_statArray[i].first.biasGlobal.first != nullptr) {
				bias = a_statArray[i].first.biasGlobal.first->GetValue();
			}

			// calculate the new randomised value of the AV using the Globals
			float randomisedValue = a_statArray[i].second * mathUtils.DoGaussian(a_statArray[i].first.minGlobal.first->GetValue(), a_statArray[i].first.maxGlobal.first->GetValue(), bias) * pluginData.globalStatMults[i]->GetValue();

			if (i < 5) {
				a_actor->SetActorValue(*pluginData.pluginRecords[i].actorValue, randomisedValue);
			} else {
				// armor resistances are baked into BaseValue so we need to take away the value that's inside the NPC record
				// also can't use SetValue :todd:
				// The record is the actor's own, as in the original mod, not the leveled base that GetBaseNPC returns.
				auto* actorNPC = a_actor->GetNPC();
				if (!actorNPC) {
					continue;
				}
				float armorValue = a_actor->GetBaseActorValue(*pluginData.pluginRecords[i].actorValue) - actorNPC->GetActorValue(*pluginData.pluginRecords[i].actorValue);

				float trashValue = a_actor->GetActorValue(*pluginData.pluginRecords[i].actorValue) - armorValue;

				a_actor->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, *pluginData.pluginRecords[i].actorValue, randomisedValue - trashValue);
			}

		}

		if (pluginData.av_SCOURGE) {
			a_actor->SetActorValue(*pluginData.av_SCOURGE, static_cast<float>(a_trackerValue));
		}
	}

	namespace
	{
		bool RunActorRecalculation(RE::Actor* a_actor, bool a_skipAlreadyTracked)
		{
			if (!Utils::CanRecalculateActor(a_actor, a_skipAlreadyTracked)) {
				return false;
			}

			std::array<std::pair<ValueData, float>, AV_COUNT> globalsToUse{};

			// the return value of BuildValueList will be used for the tracker AV
			int lastAppliedFilter = BuildValueList(a_actor, globalsToUse);

			if (lastAppliedFilter != 0) {
				RecalculateStats(a_actor, globalsToUse, lastAppliedFilter);
				return true;
			}

			return false;
		}

		void QueueActorRecalculation(RE::Actor* a_actor, bool a_skipAlreadyTracked)
		{
			if (!a_actor) {
				return;
			}

			RE::ActorHandle actorHandle{ a_actor };
			if (!actorHandle) {
				logger::warn("Unable to queue SCOURGE recalculation for actor {:08X}: no actor handle", a_actor->formID);
				return;
			}

			if (!F4SE::GetTaskInterface()) {
				logger::error("Unable to queue SCOURGE recalculation for actor {:08X}: task interface unavailable", a_actor->formID);
				return;
			}

			F4SE::GetTaskInterface()->AddTask([actorHandle, a_skipAlreadyTracked]() {
				const auto actorPtr = actorHandle.get();
				RunActorRecalculation(actorPtr.get(), a_skipAlreadyTracked);
			});
		}

		void RunGlobalRecalculation(bool a_skipAlreadyTracked)
		{
			const auto processLists = RE::ProcessLists::GetSingleton();

			if (!processLists) {
				return;
			}

			std::uint32_t seenCount{ 0 };
			std::uint32_t recalculatedCount{ 0 };
			std::unordered_set<std::uint32_t> seenActors;

			std::array<RE::BSTArray<RE::ActorHandle>*, 4> actorBuckets{
				&processLists->highActorHandles,
				&processLists->lowActorHandles,
				&processLists->middleHighActorHandles,
				&processLists->middleLowActorHandles
			};

			for (auto* actorHandles : actorBuckets) {
				for (const auto& actorHandle : *actorHandles) {
					const auto actorPtr = actorHandle.get();
					const auto currentActor = actorPtr.get();

					if (!currentActor || !seenActors.insert(currentActor->formID).second) {
						continue;
					}

					++seenCount;
					if (RunActorRecalculation(currentActor, a_skipAlreadyTracked)) {
						++recalculatedCount;
					}
				}
			}

			logger::warn("SCOURGE global recalculation scanned {} actors and recalculated {}", seenCount, recalculatedCount);		}
	}

	void QueueGlobalRecalculation(bool a_skipAlreadyTracked)
	{
		if (!F4SE::GetTaskInterface()) {
			logger::error("Unable to queue SCOURGE global recalculation: task interface unavailable");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_skipAlreadyTracked]() {
			RunGlobalRecalculation(a_skipAlreadyTracked);
		});
	}

	void SCOURGE(std::monostate, RE::Actor* a_actor)
	{
		QueueActorRecalculation(a_actor, true);
	}

	void SCOURGE_Recalculate_Actor(std::monostate, RE::Actor* a_actor)
	{
		QueueActorRecalculation(a_actor, false);
	}

	void SCOURGE_ResetGlobals(std::monostate)
	{
		for (auto valueStructs : configData.valueMap) {
			for (auto globals : valueStructs.second) {
				if (globals.enableGlobal.first) {
					globals.enableGlobal.first->value = globals.enableGlobal.second;
				}

				if (globals.minGlobal.first) {
					globals.minGlobal.first->value = globals.minGlobal.second;
				}

				if (globals.maxGlobal.first) {
					globals.maxGlobal.first->value = globals.maxGlobal.second;
				}

				if (globals.biasGlobal.first) {
					globals.biasGlobal.first->value = globals.biasGlobal.second;
				}

				if (globals.influenceGlobal.first) {
					globals.influenceGlobal.first->value = globals.influenceGlobal.second;
				}
			}
		}
	}

	void SCOURGE_ResetGlobalsByName(std::monostate, std::string a_string) {
		auto iterator = configData.valueMap.find(a_string);
		if (iterator != configData.valueMap.end()) {
			for (auto globals : iterator->second) {
				if (globals.enableGlobal.first) {
					globals.enableGlobal.first->value = globals.enableGlobal.second;
				}

				if (globals.minGlobal.first) {
					globals.minGlobal.first->value = globals.minGlobal.second;
				}

				if (globals.maxGlobal.first) {
					globals.maxGlobal.first->value = globals.maxGlobal.second;
				}

				if (globals.biasGlobal.first) {
					globals.biasGlobal.first->value = globals.biasGlobal.second;
				}

				if (globals.influenceGlobal.first) {
					globals.influenceGlobal.first->value = globals.influenceGlobal.second;
				}
			}
		}
	}

	void SCOURGE_Recalculate_Global(std::monostate)
	{
		QueueGlobalRecalculation(false);
	}

	bool BindFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->BindNativeMethod("SCOURGE_Native", "SCOURGE", SCOURGE, true);
		a_vm->BindNativeMethod("SCOURGE_Native", "SCOURGE_Recalculate_Actor", SCOURGE_Recalculate_Actor, true);
		a_vm->BindNativeMethod("SCOURGE_Native", "SCOURGE_Recalculate_Global", SCOURGE_Recalculate_Global, true);
		a_vm->BindNativeMethod("SCOURGE_Native", "SCOURGE_ResetGlobals", SCOURGE_ResetGlobals, true);
		a_vm->BindNativeMethod("SCOURGE_Native", "SCOURGE_ResetGlobalsByName", SCOURGE_ResetGlobalsByName, true);

		return true;
	}
}
