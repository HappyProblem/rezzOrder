#include "ReviveSkills.h"

#include <string_view>

#include "Arc.h"

bool CountsAsUsed(const ReviveSkill& aSkill, uint8_t aStopReason, int32_t aBaseMs, int32_t aScaledMs, uint8_t aActivation)
{
	if (aStopReason == ArcDps::ANIMSTOP_RETURN_CONTROL || aActivation == ArcDps::ACTV_RESET)
	{
		return true;
	}
	int32_t effectiveMs = aBaseMs > 0 ? aBaseMs : aScaledMs;
	return effectiveMs >= aSkill.CastMs;
}

uint8_t GroupTargets(ReviveGroup aGroup)
{
	if (const ReviveSkill* skill = PrimarySkillForGroup(aGroup)) { return skill->WvwTargets; }
	return 0;
}

const ReviveSkill* FindReviveSkill(uint32_t aSkillId)
{
	for (const ReviveSkill& skill : kReviveSkills)
	{
		if (skill.Id == aSkillId) { return &skill; }
	}
	return nullptr;
}

const ReviveSkill* PrimarySkillForGroup(ReviveGroup aGroup)
{
	for (const ReviveSkill& skill : kReviveSkills)
	{
		if (skill.Group == aGroup && skill.IsPlayerCast) { return &skill; }
	}
	return nullptr;
}

ReviveGroup DefaultGroupForProfession(uint32_t aProfession)
{
	switch (aProfession)
	{
	case 1: return ReviveGroup::SignetOfMercy;
	case 2: return ReviveGroup::BattleStandard;
	case 4: return ReviveGroup::SpiritOfNature;
	case 6: return ReviveGroup::GlyphOfRenewal;
	case 7: return ReviveGroup::IllusionOfLife;
	case 8: return ReviveGroup::SignetOfUndeath;
	default: return ReviveGroup::Count;
	}
}

ReviveGroup FindReviveGroupByName(const char* aName)
{
	if (aName == nullptr || aName[0] == '\0') { return ReviveGroup::Count; }
	std::string_view name(aName);

	if (name.find("Signet of Mercy") != std::string_view::npos) { return ReviveGroup::SignetOfMercy; }
	if (name.find("Signet of Undeath") != std::string_view::npos) { return ReviveGroup::SignetOfUndeath; }
	if (name.find("Illusion of Life") != std::string_view::npos) { return ReviveGroup::IllusionOfLife; }
	if (name.find("Battle Standard") != std::string_view::npos) { return ReviveGroup::BattleStandard; }
	if (name.find("Spirit of Nature") != std::string_view::npos ||
		name.find("Nature's Renewal") != std::string_view::npos)
	{
		return ReviveGroup::SpiritOfNature;
	}
	if (name.find("Glyph of Renewal") != std::string_view::npos ||
		name.find("Renewal of Fire") != std::string_view::npos ||
		name.find("Renewal of Water") != std::string_view::npos ||
		name.find("Renewal of Air") != std::string_view::npos ||
		name.find("Renewal of Earth") != std::string_view::npos)
	{
		return ReviveGroup::GlyphOfRenewal;
	}
	return ReviveGroup::Count;
}

const ReviveGroupInfo& GetGroupInfo(ReviveGroup aGroup)
{
	return kReviveGroups[static_cast<int>(aGroup)];
}

bool IsReviveRelatedName(const char* aName)
{
	if (aName == nullptr || aName[0] == '\0') { return false; }

	static constexpr std::string_view keywords[] = {
		"Mercy", "Renewal", "Illusion of Life", "Undeath", "Battle Standard", "Spirit of Nature"
	};
	std::string_view name(aName);
	for (std::string_view keyword : keywords)
	{
		if (name.find(keyword) != std::string_view::npos) { return true; }
	}
	return false;
}