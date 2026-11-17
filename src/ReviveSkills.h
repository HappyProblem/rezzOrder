#pragma once

#include <cstdint>

enum class ReviveGroup : uint8_t
{
	SignetOfMercy,
	GlyphOfRenewal,
	IllusionOfLife,
	SignetOfUndeath,
	BattleStandard,
	SpiritOfNature,
	Count
};

struct ReviveGroupInfo
{
	const char* Name;
	const char* Profession;
	uint16_t    WvwRechargeS;
};

inline constexpr ReviveGroupInfo kReviveGroups[static_cast<int>(ReviveGroup::Count)] = {
	{ "Signet of Mercy",   "Guardian",     90  },
	{ "Glyph of Renewal",  "Elementalist", 90  },
	{ "Illusion of Life",  "Mesmer",       90  },
	{ "Signet of Undeath", "Necromancer",  75  },
	{ "Battle Standard",   "Warrior",      120 },
	{ "Spirit of Nature",  "Ranger",       120 },
};

struct ReviveSkill
{
	uint32_t    Id;
	const char* Name;
	ReviveGroup Group;
	bool        IsPlayerCast;
	uint8_t     WvwTargets;
	uint16_t    CastMs;
};

inline constexpr ReviveSkill kReviveSkills[] = {
	{ 9163,  "Signet of Mercy",               ReviveGroup::SignetOfMercy,   true,  1, 2000 },
	{ 9243,  "Signet of Mercy (alt id)",      ReviveGroup::SignetOfMercy,   true,  1, 2000 },
	{ 5573,  "Glyph of Renewal",              ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 5762,  "Renewal of Fire",               ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 5763,  "Renewal of Water",              ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 5760,  "Renewal of Air",                ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 5761,  "Renewal of Earth",              ReviveGroup::GlyphOfRenewal,  true,  3, 2000 },
	{ 5764,  "Renewal of Fire (effect)",      ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 5765,  "Renewal of Air (effect)",       ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 24407, "Renewal of Fire (underwater)",  ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 24410, "Renewal of Water (underwater)", ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 24409, "Renewal of Air (underwater)",   ReviveGroup::GlyphOfRenewal,  true,  1, 2000 },
	{ 24411, "Renewal of Earth (underwater)", ReviveGroup::GlyphOfRenewal,  true,  3, 2000 },
	{ 10244, "Illusion of Life",              ReviveGroup::IllusionOfLife,  true,  3, 1250 },
	{ 10611, "Signet of Undeath",             ReviveGroup::SignetOfUndeath, true,  1, 1500 },
	{ 14419, "Battle Standard",               ReviveGroup::BattleStandard,  true,  3, 2000 },
	{ 12569, "Spirit of Nature",              ReviveGroup::SpiritOfNature,  true,  2, 1500 },
	{ 12601, "Nature's Renewal",              ReviveGroup::SpiritOfNature,  false, 2, 750  },
	{ 69336, "Nature's Renewal (2nd slam)",   ReviveGroup::SpiritOfNature,  false, 2, 750  },
};

inline constexpr uint32_t kSignetOfMercyPassive   = 9162;
inline constexpr uint32_t kSignetOfUndeathPassive = 10610;
inline constexpr uint32_t kRenewalOfFireBuff      = 5764;
inline constexpr uint32_t kRenewalOfAirBuff       = 5765;
inline constexpr uint32_t kResurrectionBuff       = 848;
inline constexpr uint32_t kIllusionOfLifeEffect   = 10346;

const ReviveSkill* FindReviveSkill(uint32_t aSkillId);
const ReviveSkill* PrimarySkillForGroup(ReviveGroup aGroup);
ReviveGroup DefaultGroupForProfession(uint32_t aProfession);
ReviveGroup FindReviveGroupByName(const char* aName);
uint8_t GroupTargets(ReviveGroup aGroup);
const ReviveGroupInfo& GetGroupInfo(ReviveGroup aGroup);
bool CountsAsUsed(const ReviveSkill& aSkill, uint8_t aStopReason, int32_t aBaseMs);
bool IsReviveRelatedName(const char* aName);