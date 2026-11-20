#include "ReviveSkills.h"

#include <string_view>

#include "Arc.h"

bool CountsAsUsed(const ReviveSkill& aSkill, uint8_t aStopReason, int32_t aBaseMs, int32_t aScaledMs, uint8_t aActivation)
{
	// Natural full-cast completion states in ArcDPS:
	// 5 = ENDED, 15 = MOTIONSKILL_VIA_RESET, 19 = GADGET_VIA_RESET, 22 = RETURN_CONTROL, 23 = READY
	if (aStopReason == ArcDps::ANIMSTOP_RETURN_CONTROL ||
		aStopReason == 5 ||
		aStopReason == 15 ||
		aStopReason == 19 ||
		aStopReason == 23 ||
		aActivation == ArcDps::ACTV_RESET)
	{
		return true;
	}

	// Hard interrupts, CC, or going down/dead mid-cast: only count if the full unscaled cast time elapsed.
	if (aStopReason == ArcDps::ANIMSTOP_INTERRUPT ||
		aStopReason == 9 ||  // DEATH
		aStopReason == 10 || // DOWNED
		aStopReason == 11)   // CROWDCONTROL
	{
		return aBaseMs >= aSkill.CastMs;
	}

	// Explicit manual cancel (6 = CANCEL, 17 = STOW): require full cast time (or full Quickness duration if unscaled is missing).
	if (aStopReason == 6 || aStopReason == 17)
	{
		if (aBaseMs > 0) { return aBaseMs >= aSkill.CastMs; }
		if (aScaledMs > 0) { return aScaledMs >= (static_cast<int32_t>(aSkill.CastMs) * 66) / 100; }
		return false;
	}

	// Unscaled base duration (ev.BuffDmg) is already normalized for Quickness/Slow by ArcDPS.
	// Allow a 10% margin (90% of CastMs) for cancelling post-trigger aftercast by moving or queueing the next skill.
	if (aBaseMs > 0)
	{
		return aBaseMs >= (static_cast<int32_t>(aSkill.CastMs) * 90) / 100;
	}

	// When aBaseMs == 0 (e.g. ACTV_NODATA), only real-time duration (aScaledMs / ev.Value) is available.
	// With Quickness (+50% speed), a full cast takes 66.7% of CastMs (~833 ms for Illusion of Life).
	// Allow a small aftercast-cancel margin down to 62% (~775 ms for IoL, ~930 ms for 1.5s, ~1240 ms for 2.0s).
	if (aScaledMs > 0)
	{
		return aScaledMs >= (static_cast<int32_t>(aSkill.CastMs) * 62) / 100;
	}

	return false;
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