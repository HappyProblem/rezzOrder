#include "Session.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "Share.h"

namespace Rezz
{
	namespace
	{
		constexpr uint64_t kSelfLeaveSlackMs = 3000;
		constexpr size_t   kMaxSeparateNotices = 3;
		constexpr uint8_t  CBTB_MANUAL = 3;

		constexpr uint32_t kBoonRegeneration = 718;
		constexpr uint32_t kBoonSwiftness    = 719;
		constexpr uint32_t kBoonFury         = 725;
		constexpr uint32_t kBoonMight        = 740;

		uint32_t GroupBit(ReviveGroup aGroup) { return 1u << static_cast<uint32_t>(aGroup); }

		bool LessNoCase(const std::string& aLeft, const std::string& aRight)
		{
			size_t shared = std::min(aLeft.size(), aRight.size());
			for (size_t i = 0; i < shared; i++)
			{
				unsigned char left = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(aLeft[i])));
				unsigned char right = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(aRight[i])));
				if (left != right) { return left < right; }
			}
			return aLeft.size() < aRight.size();
		}
	}

	bool IsReviveProfession(uint32_t aProfession)
	{
		switch (aProfession)
		{
			case 1:
			case 2:
			case 4:
			case 6:
			case 7:
			case 8:
				return true;
			default:
				return false;
		}
	}

	const char* ProfessionShortName(uint32_t aProfession)
	{
		static constexpr const char* names[] = { "???", "Guard", "War", "Engi", "Ranger", "Thief", "Ele", "Mes", "Necro", "Rev" };
		return aProfession < std::size(names) ? names[aProfession] : "???";
	}

	bool IsUsableCharacterName(const std::string& aName)
	{
		if (aName.empty()) { return false; }
		for (unsigned char c : aName) { if (std::isdigit(c)) { return false; } }

		static const char* kTiers[] = { "Bronze", "Silver", "Gold", "Platinum", "Mithril", "Diamond" };
		static const char* kRanks[] = { "Invader", "Assaulter", "Raider", "Recruit", "Scout", "Soldier",
			"Squire", "Footman", "Knight", "Major", "Colonel", "General", "Veteran", "Champion", "Legend",
			"Dominator", "Conqueror", "Vanquisher", "Warlord", "Marshal" };
		size_t space = aName.find(' ');
		if (space == std::string::npos || aName.find(' ', space + 1) != std::string::npos) { return true; }
		std::string first = aName.substr(0, space), second = aName.substr(space + 1);
		bool tier = false, rank = false;
		for (const char* text : kTiers) { tier = tier || first == text; }
		for (const char* text : kRanks) { rank = rank || second == text; }
		return !(tier && rank);
	}

	std::string DisplayAccount(const std::string& aAccount)
	{
		std::string name = !aAccount.empty() && aAccount[0] == ':' ? aAccount.substr(1) : aAccount;
		size_t dot = name.rfind('.');
		bool numbered = dot != std::string::npos && dot + 1 < name.size() &&
			std::all_of(name.begin() + dot + 1, name.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
		return numbered ? name.substr(0, dot) : name;
	}

	RosterMember& Session::GetMember(const std::string& aAccount)
	{
		RosterMember& member = m_Roster[aAccount];
		if (member.Account.empty()) { member.Account = aAccount; }
		return member;
	}

	bool Session::InOrder(const std::string& aAccount) const
	{
		const std::vector<std::string>& order = m_Tracker.Order();
		return std::find(order.begin(), order.end(), aAccount) != order.end();
	}

	std::string Session::Named(const std::string& aAccount) const
	{
		const std::vector<std::string>& order = m_Tracker.Order();
		auto it = std::find(order.begin(), order.end(), aAccount);
		std::string name = DisplayAccount(aAccount);
		if (it == order.end()) { return name; }
		return std::to_string(it - order.begin() + 1) + ". " + name;
	}

	int Session::SelfPlace() const
	{
		if (m_SelfAccount.empty()) { return 0; }
		const std::vector<std::string>& order = m_Tracker.Order();
		auto it = std::find(order.begin(), order.end(), m_SelfAccount);
		return it == order.end() ? 0 : static_cast<int>(it - order.begin()) + 1;
	}

	std::string Session::PlaceChange(int aPlaceBefore) const
	{
		int now = SelfPlace();
		if (now == aPlaceBefore) { return {}; }
		if (now == 0) { return " - you are not in it any more"; }
		if (aPlaceBefore == 0) { return " - you are now " + std::to_string(now) + "."; }
		return " - you are now " + std::to_string(now) + ". (was " + std::to_string(aPlaceBefore) + ".)";
	}

	int Session::SelfStanding(uint64_t aNowMs) const
	{
		if (m_SelfAccount.empty()) { return 0; }
		TurnView turn = m_Tracker.GetTurn(aNowMs);
		if (turn.UpIndex >= 0 && turn.Rows[turn.UpIndex].Account == m_SelfAccount) { return 1; }
		if (turn.BackupIndex >= 0 && turn.Rows[turn.BackupIndex].Account == m_SelfAccount) { return 2; }
		return 0;
	}

	std::string Session::StandingChange(int aBefore, int aAfter) const
	{
		if (aAfter == aBefore || aAfter == 0) { return {}; }
		return aAfter == 1 ? " - you are up now" : " - you are the backup now";
	}

	void Session::Notify(NoticeKind aKind, const std::string& aAccount, std::string aText)
	{
		m_Notices.push_back(Notice{ aKind, aAccount, std::move(aText) });
	}

	std::string Session::AccountByInstance(uint16_t aInstanceId) const
	{
		if (aInstanceId == 0) { return {}; }
		for (const auto& [id, agent] : m_Agents)
		{
			if (agent.InstanceId == aInstanceId) { return agent.Account; }
		}
		return {};
	}

	std::string Session::ResolveAccount(uint64_t aId, uint16_t aInstanceId)
	{
		if (aId == 0) { return {}; }
		auto it = m_Agents.find(aId);
		if (it != m_Agents.end()) { return it->second.Account; }

		auto alias = m_Aliases.find(aId);
		if (alias != m_Aliases.end())
		{
			auto aliased = m_Agents.find(alias->second);
			if (aliased != m_Agents.end()) { return aliased->second.Account; }
		}

		if (aInstanceId == 0) { return {}; }
		for (const auto& [id, agent] : m_Agents)
		{
			if (agent.InstanceId == aInstanceId)
			{
				m_Aliases[aId] = id;
				return agent.Account;
			}
		}
		return {};
	}

	void Session::RecordSkillUse(uint64_t aTimeMs, const std::string& aAccount, ReviveGroup aGroup)
	{
		if (aAccount.empty() || aGroup == ReviveGroup::Count) { return; }
		TurnView turn = m_Tracker.GetTurn(aTimeMs);
		bool inTurn = turn.UpIndex >= 0 && turn.Rows[turn.UpIndex].Account == aAccount;
		if (!m_Tracker.MarkUsed(aTimeMs, aAccount, aGroup)) { return; }
		GetMember(aAccount).SeenGroups |= GroupBit(aGroup);
		m_Stats.OnCast(aTimeMs, aAccount, true, CastEnd::Full, inTurn);
		TakeAttributions();
	}

	void Session::MarkPlayerUsed(const std::string& aAccount, uint64_t aNowMs)
	{
		std::string target = aAccount.empty() ? m_SelfAccount : aAccount;
		if (target.empty()) { return; }
		ReviveGroup group = ReviveGroup::Count;
		if (const PlayerStatus* status = m_Tracker.FindPlayer(target))
		{
			if (!status->Skills.empty()) { group = status->Skills.front().Group; }
		}
		if (group == ReviveGroup::Count)
		{
			group = DefaultGroupForProfession(GetMember(target).Profession);
		}
		RecordSkillUse(aNowMs, target, group);
	}

	void Session::MarkPlayerReady(const std::string& aAccount, uint64_t aNowMs)
	{
		std::string target = aAccount.empty() ? m_SelfAccount : aAccount;
		if (target.empty()) { return; }
		if (const PlayerStatus* status = m_Tracker.FindPlayer(target))
		{
			for (const SkillStatus& skill : status->Skills)
			{
				m_Tracker.MarkSkillReady(aNowMs, target, skill.Group);
			}
		}
		ReviveGroup def = DefaultGroupForProfession(GetMember(target).Profession);
		if (def != ReviveGroup::Count) { m_Tracker.MarkSkillReady(aNowMs, target, def); }
	}

	void Session::OnCombat(const ArcDps::EvCombatData& aData, uint64_t aNowMs)
	{
		if (aData.Ev == nullptr) { return; }
		const ArcDps::CombatEvent& ev = *aData.Ev;
		uint8_t sc = ev.IsStatechange;
		uint64_t srcId = aData.Src ? aData.Src->Id : ev.SrcAgent;
		uint64_t dstId = aData.Dst ? aData.Dst->Id : ev.DstAgent;

		switch (sc)
		{
			case ArcDps::CBTS_SQCOMBATSTART: m_CombatSinceMs = aNowMs; m_OutOfCombatSinceMs = 0; return;
			case ArcDps::CBTS_SQCOMBATEND:   m_CombatSinceMs = 0; m_OutOfCombatSinceMs = aNowMs; return;
			case ArcDps::CBTS_ENTERCOMBAT:
			{
				if (m_CombatSinceMs == 0) { m_CombatSinceMs = aNowMs; m_OutOfCombatSinceMs = 0; }
				std::string account = ResolveAccount(srcId, ev.SrcInstId);
				if (!account.empty()) { GetMember(account).LastEventMs = aNowMs; }
				return;
			}
			case ArcDps::CBTS_ANIMATIONSTART:
			{
				std::string account = ResolveAccount(srcId, ev.SrcInstId);
				if (account.empty()) { return; }
				GetMember(account).LastEventMs = aNowMs;
				m_Tracker.OnActivity(ev.Time, account);
				const ReviveSkill* skill = FindReviveSkill(ev.SkillId);
				if (skill && skill->IsPlayerCast)
				{
					m_Tracker.OnCastStart(ev.Time, account, ev.SkillId);
					GetMember(account).SeenGroups |= GroupBit(skill->Group);
				}
				return;
			}
			case ArcDps::CBTS_ANIMATIONSTOP:
			{
				const ReviveSkill* skill = FindReviveSkill(ev.SkillId);
				if (skill == nullptr) { return; }
				if (!skill->IsPlayerCast)
				{
					std::string owner = AccountByInstance(ev.SrcMasterInstId);
					if (!owner.empty()) { m_Tracker.OnOwnedEffect(ev.Time, owner, ev.SkillId); }
					TakeAttributions();
					return;
				}
				std::string account = ResolveAccount(srcId, ev.SrcInstId);
				if (account.empty()) { return; }

				bool used = CountsAsUsed(*skill, ev.Result, ev.BuffDmg);
				TurnView turn = m_Tracker.GetTurn(ev.Time);
				bool inTurn = turn.UpIndex >= 0 && turn.Rows[turn.UpIndex].Account == account;
				const PlayerStatus* caster = m_Tracker.FindPlayer(account);
				CastEnd ending = CastEnd::Full;
				if (!used)
				{
					switch (ev.Result)
					{
						case ArcDps::ANIMSTOP_COMMAND:   ending = CastEnd::ByHand; break;
						case ArcDps::ANIMSTOP_INTERRUPT: ending = CastEnd::Interrupted; break;
						case ArcDps::ANIMSTOP_MOVEDODGE: ending = CastEnd::Movement; break;
						default: ending = CastEnd::ByHand; break;
					}
					if (caster && caster->Life != LifeState::Alive) { ending = CastEnd::WentDown; }
				}
				m_Stats.OnCast(ev.Time, account, used, ending, inTurn);

				m_Tracker.OnCastStop(ev.Time, account, ev.SkillId, ev.Result, ev.BuffDmg);
				TakeAttributions();
				if (!m_IllusionApplySeen && skill->Group == ReviveGroup::IllusionOfLife && CountsAsUsed(*skill, ev.Result, ev.BuffDmg))
				{
					MatchIllusion(ev.Time, account, true);
				}
				return;
			}
			case ArcDps::CBTS_CHANGEDOWN:
			case ArcDps::CBTS_CHANGEUP:
			case ArcDps::CBTS_CHANGEDEAD:
			{
				std::string account = ResolveAccount(srcId, ev.SrcInstId);
				if (account.empty()) { return; }
				GetMember(account).LastEventMs = aNowMs;
				LifeState state = sc == ArcDps::CBTS_CHANGEDOWN ? LifeState::Downed
					: sc == ArcDps::CBTS_CHANGEDEAD ? LifeState::Dead : LifeState::Alive;
				m_Tracker.OnLifeState(ev.Time, account, state);
				TakeAttributions();
				if (state == LifeState::Downed) { m_Stats.OnDown(ev.Time, account, ChanceNow(ev.Time)); }
				else if (state == LifeState::Dead) { m_Stats.OnDead(ev.Time, account); }
				else { m_PendingUps.push_back(PendingUp{ account, ev.Time, aNowMs }); }
				if (state != LifeState::Alive) { m_Illusions.erase(account); }
				if (sc == ArcDps::CBTS_CHANGEUP && !m_IllusionApplySeen) { MatchIllusion(ev.Time, account, false); }
				return;
			}
			case ArcDps::CBTS_COMBAT:
			case ArcDps::CBTS_BUFFAPPLY:
			case ArcDps::CBTS_BUFFINITIAL:
			{
				std::string srcAccount = ResolveAccount(srcId, ev.SrcInstId);
				std::string dstAccount = ResolveAccount(dstId, ev.DstInstId);
				if (!srcAccount.empty())
				{
					GetMember(srcAccount).LastEventMs = aNowMs;
					m_Tracker.OnActivity(ev.Time, srcAccount);
				}
				if (!dstAccount.empty()) { GetMember(dstAccount).LastEventMs = aNowMs; }

				if (sc == ArcDps::CBTS_COMBAT && ev.IsBuffRemove != 0)
				{
					if (ev.SkillId == kIllusionOfLifeEffect && !srcAccount.empty())
					{
						m_Illusions.erase(srcAccount);
						return;
					}
					if ((ev.SkillId == kSignetOfMercyPassive || ev.SkillId == kSignetOfUndeathPassive) && ev.IsBuffRemove != CBTB_MANUAL)
					{
						std::string holder = !srcAccount.empty() ? srcAccount : dstAccount;
						const PlayerStatus* status = m_Tracker.FindPlayer(holder);
						if (!holder.empty() && GetMember(holder).OnMap && (status == nullptr || (status->Life == LifeState::Alive && !status->Away)))
						{
							ReviveGroup group = ev.SkillId == kSignetOfMercyPassive ? ReviveGroup::SignetOfMercy : ReviveGroup::SignetOfUndeath;
							RecordSkillUse(ev.Time, holder, group);
						}
					}
					return;
				}

				if (const ReviveSkill* skill = FindReviveSkill(ev.SkillId))
				{
					if (skill->IsPlayerCast)
					{
						std::string caster = !srcAccount.empty() ? srcAccount : dstAccount;
						if (caster.empty() && ev.SrcMasterInstId != 0) { caster = AccountByInstance(ev.SrcMasterInstId); }
						RecordSkillUse(ev.Time, caster, skill->Group);
					}
					else
					{
						std::string owner = AccountByInstance(ev.SrcMasterInstId);
						if (owner.empty()) { owner = srcAccount; }
						if (!owner.empty())
						{
							RecordSkillUse(ev.Time, owner, skill->Group);
							m_Tracker.OnOwnedEffect(ev.Time, owner, ev.SkillId);
							TakeAttributions();
						}
					}
					return;
				}

				ReviveGroup byName = FindReviveGroupByName(aData.SkillName);
				if (byName == ReviveGroup::Count && aData.Src && aData.Src->Name)
				{
					byName = FindReviveGroupByName(aData.Src->Name);
				}
				if (byName != ReviveGroup::Count)
				{
					std::string caster = srcAccount;
					if (caster.empty() && ev.SrcMasterInstId != 0) { caster = AccountByInstance(ev.SrcMasterInstId); }
					if (caster.empty()) { caster = dstAccount; }
					RecordSkillUse(ev.Time, caster, byName);
					return;
				}

				bool isBoonApply = sc == ArcDps::CBTS_BUFFAPPLY ||
					(sc == ArcDps::CBTS_COMBAT && ev.Buff != 0 && ev.IsBuffRemove == 0 && ev.Value != 0);
				if (isBoonApply && ev.SrcMasterInstId != 0 && srcAccount.empty() && srcId != 0)
				{
					std::string owner = AccountByInstance(ev.SrcMasterInstId);
					if (!owner.empty())
					{
						uint32_t prof = GetMember(owner).Profession;
						if (prof == 2 && (ev.SkillId == kBoonMight || ev.SkillId == kBoonFury || ev.SkillId == kBoonSwiftness))
						{
							MinionBoonState& state = m_MinionBoons[srcId];
							if (state.FirstMs == 0 || ev.Time > state.FirstMs + 1500) { state = MinionBoonState{ ev.Time, 0 }; }
							if (ev.SkillId == kBoonMight) { state.Mask |= 1; }
							else if (ev.SkillId == kBoonFury) { state.Mask |= 2; }
							else if (ev.SkillId == kBoonSwiftness) { state.Mask |= 4; }
							if ((state.Mask & 7) == 7)
							{
								RecordSkillUse(ev.Time, owner, ReviveGroup::BattleStandard);
								return;
							}
						}
						else if (prof == 4 && ev.SkillId == kBoonRegeneration && ev.Value >= 1500 && ev.Value <= 6500)
						{
							bool isPet = aData.Src && aData.Src->Name && std::strncmp(aData.Src->Name, "Juvenile ", 9) == 0;
							if (!isPet)
							{
								RecordSkillUse(ev.Time, owner, ReviveGroup::SpiritOfNature);
								return;
							}
						}
					}
				}

				if (sc != ArcDps::CBTS_BUFFAPPLY && sc != ArcDps::CBTS_BUFFINITIAL && !isBoonApply) { return; }

				if (ev.SkillId == kIllusionOfLifeEffect && !dstAccount.empty())
				{
					m_IllusionApplySeen = true;
					uint64_t length = ev.Value > 0 ? static_cast<uint64_t>(ev.Value) : 15000;
					m_Illusions[dstAccount] = IllusionState{ ev.Time + length, srcAccount };
					RecordSkillUse(ev.Time, srcAccount, ReviveGroup::IllusionOfLife);
					return;
				}

				if (ev.SkillId == kResurrectionBuff && !srcAccount.empty() && srcAccount != dstAccount)
				{
					ReviveGroup group = DefaultGroupForProfession(GetMember(srcAccount).Profession);
					if (group == ReviveGroup::GlyphOfRenewal)
					{
						RecordSkillUse(ev.Time, srcAccount, group);
						return;
					}
				}

				if (!dstAccount.empty())
				{
					ReviveGroup group = ev.SkillId == kSignetOfMercyPassive ? ReviveGroup::SignetOfMercy
						: ev.SkillId == kSignetOfUndeathPassive ? ReviveGroup::SignetOfUndeath : ReviveGroup::Count;
					if (group != ReviveGroup::Count)
					{
						GetMember(dstAccount).SeenGroups |= GroupBit(group);
						m_Tracker.MarkSkillReady(ev.Time, dstAccount, group);
					}
				}
				return;
			}
			case ArcDps::CBTS_BUFFREMOVE_ALL:
			case ArcDps::CBTS_BUFFREMOVE_SINGLE:
			{
				std::string srcAccount = ResolveAccount(srcId, ev.SrcInstId);
				if (ev.SkillId == kIllusionOfLifeEffect)
				{
					if (!srcAccount.empty()) { m_Illusions.erase(srcAccount); }
					return;
				}
				if ((ev.SkillId == kSignetOfMercyPassive || ev.SkillId == kSignetOfUndeathPassive) && ev.IsBuffRemove != CBTB_MANUAL)
				{
					std::string holder = !srcAccount.empty() ? srcAccount : ResolveAccount(dstId, ev.DstInstId);
					const PlayerStatus* status = m_Tracker.FindPlayer(holder);
					if (!holder.empty() && GetMember(holder).OnMap && (status == nullptr || (status->Life == LifeState::Alive && !status->Away)))
					{
						ReviveGroup group = ev.SkillId == kSignetOfMercyPassive ? ReviveGroup::SignetOfMercy : ReviveGroup::SignetOfUndeath;
						RecordSkillUse(ev.Time, holder, group);
					}
				}
				return;
			}
			default:
				return;
		}
	}

	void Session::OnAgentUpdate(const ArcDps::EvAgentUpdate& aUpdate, uint64_t aNowMs)
	{
		std::string account = aUpdate.Account;
		if (account.empty()) { return; }
		bool isSelf = aUpdate.Self != 0;
		bool added = aUpdate.Added != 0;
		m_Aliases.clear();

		RosterMember& member = GetMember(account);
		if (isSelf)
		{
			m_SelfAccount = account;
			if (!member.IsSelf && member.Role == SquadRole::None) { OnSelfLeftSquad(); }
			member.IsSelf = true;
			if (added) { m_ResyncAtMs = aNowMs + kResyncAfterMs; }
			if (added) { m_IllusionApplySeen = false; }
			if (!added)
			{
				m_LastSelfLeaveMs = aNowMs;
				m_ResyncAtMs = 0;
				std::erase_if(m_PendingLeaves, [&](const PendingLeave& aLeave) { return aLeave.TimeMs + kSelfLeaveSlackMs >= aNowMs; });
			}
		}

		if (!added)
		{
			m_Agents.erase(aUpdate.Id);
			member.OnMap = false;
			int standing = SelfStanding(aNowMs);
			m_Tracker.SetAway(aNowMs, account, true);
			if (!isSelf && member.InSquad && InOrder(account))
			{
				m_PendingLeaves.push_back(PendingLeave{ account, aNowMs, member.Profession, standing });
			}
			return;
		}

		m_Agents[aUpdate.Id] = AgentRef{ account, static_cast<uint16_t>(aUpdate.InstanceId) };
		if (!member.OnMap) { m_Tracker.MarkSeenSince(aNowMs, account); }
		member.LastEventMs = aNowMs;
		uint32_t previousProfession = member.Profession;
		std::string previousCharacter = member.Character;
		member.Character = aUpdate.Character;
		if (aUpdate.Profession != 0)
		{
			member.Profession = aUpdate.Profession;
			member.Elite = aUpdate.Elite;
		}
		if (!m_Subgroups.count(account)) { member.Subgroup = aUpdate.Subgroup; }
		member.OnMap = true;
		m_Tracker.SetAway(aNowMs, account, false);
		std::erase_if(m_PendingLeaves, [&](const PendingLeave& aLeave) { return aLeave.Account == account; });

		bool swappedProfession = previousProfession != 0 && aUpdate.Profession != 0 && previousProfession != aUpdate.Profession;
		bool swappedCharacter = !swappedProfession && previousCharacter != member.Character &&
			IsUsableCharacterName(previousCharacter) && IsUsableCharacterName(member.Character);
		bool changed = swappedProfession || swappedCharacter;
		if (changed) { m_Tracker.ForgetSkills(account); member.SeenGroups = 0; }
		if (m_Substitutes && !m_Vacancies.empty())
		{
			if (!isSelf && !InOrder(account) && SureReviver(member)) { ReclaimPlace(account, aNowMs, SubgroupOf(account)); }
			MatchSubstitutes(aNowMs);
		}
		if (isSelf || !InOrder(account)) { m_Reported.erase(account); return; }

		if (changed)
		{
			int standing = SelfStanding(aNowMs);
			std::string text = Named(account) + (swappedProfession
				? " swapped to " + std::string(ProfessionShortName(member.Profession))
				: " swapped character");
			if (m_Substitutes && SureReviver(member))
			{
				text += ", keeps their place";
			}
			else
			{
				if (!IsReviveProfession(member.Profession)) { text += " (no revive skill)"; }
				DropFromOrder(account, SubgroupOf(account), aNowMs);
				text += ", out of the order";
			}
			Notify(NoticeKind::ChangedProfession, account, text + StandingChange(standing, SelfStanding(aNowMs)));
			MatchSubstitutes(aNowMs);
		}
		else if (m_Reported.count(account))
		{
			Notify(NoticeKind::Returned, account, Named(account) + " is back (" + ProfessionShortName(member.Profession) + ")");
		}
		m_Reported.erase(account);
	}

	void Session::OnRole(const std::string& aAccount, SquadRole aRole, uint16_t aSubgroup, uint64_t aNowMs)
	{
		if (aAccount.empty() || aRole == SquadRole::Unknown) { return; }
		m_HasRoles = true;
		RosterMember& member = GetMember(aAccount);
		member.Role = aRole;
		if (aSubgroup != 0) { member.Subgroup = aSubgroup; }

		if (member.IsSelf && aRole == SquadRole::None) { OnSelfLeftSquad(); }

		bool inSquad = aRole == SquadRole::Leader || aRole == SquadRole::Lieutenant || aRole == SquadRole::Member;
		uint16_t previousGroup = SubgroupOf(aAccount);
		if (inSquad && aSubgroup != 0 && aSubgroup != previousGroup)
		{
			bool toBench = previousGroup != 0 && IsBench(aSubgroup, aAccount);
			m_Subgroups[aAccount] = aSubgroup;
			std::erase_if(m_Arrivals, [&](const Arrival& aArrival) { return aArrival.Account == aAccount; });
			if (m_Substitutes)
			{
				if (previousGroup == 0)
				{
					ReclaimPlace(aAccount, aNowMs, 0);
				}
				else if (InOrder(aAccount))
				{
					if (toBench)
					{
						m_LastBenched = aSubgroup;
						int standing = SelfStanding(aNowMs);
						std::string name = Named(aAccount);
						DropFromOrder(aAccount, previousGroup, aNowMs);
						Notify(NoticeKind::Substituted, aAccount, name + " went to the bench, out of the order" +
							StandingChange(standing, SelfStanding(aNowMs)));
					}
				}
				else if (!ReclaimPlace(aAccount, aNowMs, aSubgroup))
				{
					m_Arrivals.push_back(Arrival{ aAccount, aSubgroup, aNowMs, IsBench(previousGroup, aAccount) });
				}
				MatchSubstitutes(aNowMs);
			}
		}

		if (inSquad == member.InSquad) { return; }
		member.InSquad = inSquad;
		if (inSquad || member.IsSelf || aRole != SquadRole::None) { return; }

		int standing = SelfStanding(aNowMs);
		m_Tracker.SetAway(aNowMs, aAccount, true);
		std::erase_if(m_PendingLeaves, [&](const PendingLeave& aLeave) { return aLeave.Account == aAccount; });
		std::erase_if(m_Arrivals, [&](const Arrival& aArrival) { return aArrival.Account == aAccount; });
		m_Subgroups.erase(aAccount);
		if (InOrder(aAccount))
		{
			std::string name = Named(aAccount);
			bool report = !m_Reported.count(aAccount);
			DropFromOrder(aAccount, previousGroup, aNowMs);
			if (report)
			{
				Notify(NoticeKind::LeftSquad, aAccount, name + " left the squad, out of the order" +
					StandingChange(standing, SelfStanding(aNowMs)));
				m_Reported[aAccount] = true;
			}
			MatchSubstitutes(aNowMs);
		}
	}

	uint16_t Session::SubgroupOf(const std::string& aAccount) const
	{
		auto it = m_Subgroups.find(aAccount);
		return it == m_Subgroups.end() ? 0 : it->second;
	}

	bool Session::SureReviver(const RosterMember& aMember) const
	{
		constexpr uint32_t kDruid = 5, kTroubadour = 73;
		if (aMember.Profession == 0) { return false; }
		return aMember.Elite == kDruid || aMember.Elite == kTroubadour || aMember.SeenGroups != 0;
	}

	void Session::SetBench(uint16_t aBench)
	{
		uint16_t bench = Share::BenchFromName(Share::BenchName(aBench));
		if (bench != m_BenchGroup) { m_LastBenched = 0; }
		m_BenchGroup = bench;
	}

	bool Session::IsBench(uint16_t aGroup, const std::string& aMover) const
	{
		if (m_BenchGroup == 0 || aGroup == 0) { return false; }
		if (m_BenchGroup == Share::kBenchLast)
		{
			uint16_t last = 0;
			for (const auto& [account, group] : m_Subgroups) { last = std::max(last, group); }
			bool emptiedBench = aGroup > last && aGroup == m_LastBenched;
			if (aGroup != last && !emptiedBench) { return false; }
		}
		else if (aGroup != m_BenchGroup) { return false; }

		for (const std::string& other : m_Tracker.Order())
		{
			if (other != aMover && SubgroupOf(other) == aGroup) { return false; }
		}
		return true;
	}

	void Session::SetSubstitutes(bool aEnabled)
	{
		m_Substitutes = aEnabled;
		if (!aEnabled)
		{
			m_Vacancies.clear();
			m_Arrivals.clear();
		}
	}

	void Session::DropFromOrder(const std::string& aAccount, uint16_t aOpenFor, uint64_t aNowMs)
	{
		const std::vector<std::string>& order = m_Tracker.Order();
		auto it = std::find(order.begin(), order.end(), aAccount);
		if (it == order.end()) { return; }
		size_t place = static_cast<size_t>(it - order.begin());
		bool precast = std::find(m_Precast.begin(), m_Precast.end(), aAccount) != m_Precast.end();
		bool open = m_Substitutes && aOpenFor != 0;

		uint32_t ahead = 0;
		for (const Vacancy& vacancy : m_Vacancies) { if (vacancy.Index == place) { ahead++; } }
		for (Vacancy& vacancy : m_Vacancies)
		{
			if (vacancy.Index <= place) { continue; }
			if (vacancy.Index == place + 1) { vacancy.Tie += ahead + (open ? 1 : 0); }
			vacancy.Index--;
		}

		m_Tracker.RemoveFromOrder(aAccount);
		std::erase(m_Precast, aAccount);
		std::erase_if(m_Vacancies, [&](const Vacancy& aVacancy) { return aVacancy.Account == aAccount; });
		if (open)
		{
			auto member = m_Roster.find(aAccount);
			uint32_t profession = member == m_Roster.end() ? 0 : member->second.Profession;
			m_Vacancies.push_back(Vacancy{ aAccount, aOpenFor, profession, aNowMs, place, ahead, precast });
		}
	}

	void Session::FillPlace(size_t aIndex, const std::string& aAccount)
	{
		Vacancy filled = m_Vacancies[aIndex];
		m_Vacancies.erase(m_Vacancies.begin() + static_cast<std::ptrdiff_t>(aIndex));
		m_Tracker.InsertIntoOrder(filled.Index, aAccount);
		if (filled.Precast && std::find(m_Precast.begin(), m_Precast.end(), aAccount) == m_Precast.end())
		{
			m_Precast.push_back(aAccount);
		}
		for (Vacancy& vacancy : m_Vacancies)
		{
			if (vacancy.Index > filled.Index) { vacancy.Index++; }
			else if (vacancy.Index == filled.Index && vacancy.Tie > filled.Tie)
			{
				vacancy.Index++;
				vacancy.Tie -= filled.Tie + 1;
			}
		}
		std::erase_if(m_Arrivals, [&](const Arrival& aArrival) { return aArrival.Account == aAccount; });
		m_Reported.erase(aAccount);
	}

	bool Session::ReclaimPlace(const std::string& aAccount, uint64_t aNowMs, uint16_t aSubgroup)
	{
		if (InOrder(aAccount)) { return false; }
		for (size_t i = 0; i < m_Vacancies.size(); i++)
		{
			const Vacancy& vacancy = m_Vacancies[i];
			if (vacancy.Account != aAccount || vacancy.TimeMs + kSubstituteWindowMs < aNowMs) { continue; }
			if (aSubgroup != 0 && vacancy.Subgroup != aSubgroup) { continue; }
			int standing = SelfStanding(aNowMs);
			FillPlace(i, aAccount);
			Notify(NoticeKind::Returned, aAccount, Named(aAccount) + " is back in their place" +
				StandingChange(standing, SelfStanding(aNowMs)));
			return true;
		}
		return false;
	}

	void Session::MatchSubstitutes(uint64_t aNowMs)
	{
		auto stale = [aNowMs](uint64_t aTimeMs) { return aTimeMs + kSubstituteWindowMs < aNowMs; };
		std::erase_if(m_Vacancies, [&](const Vacancy& aVacancy) { return stale(aVacancy.TimeMs) || InOrder(aVacancy.Account); });
		std::erase_if(m_Arrivals, [&](const Arrival& aArrival) { return stale(aArrival.TimeMs); });

		auto professionOf = [this](const std::string& aAccount)
		{
			auto it = m_Roster.find(aAccount);
			return it == m_Roster.end() ? 0u : it->second.Profession;
		};

		for (size_t i = 0; i < m_Vacancies.size(); )
		{
			const Vacancy vacancy = m_Vacancies[i];

			auto sure = [this](const std::string& aAccount)
			{
				auto member = m_Roster.find(aAccount);
				return member != m_Roster.end() && SureReviver(member->second);
			};
			std::vector<std::string> candidates;
			for (const Arrival& arrival : m_Arrivals)
			{
				if (arrival.Subgroup != vacancy.Subgroup || SubgroupOf(arrival.Account) != vacancy.Subgroup ||
					InOrder(arrival.Account) || !sure(arrival.Account)) { continue; }
				candidates.push_back(arrival.Account);
			}
			bool offBench = candidates.empty();
			if (offBench)
			{
				for (const Arrival& arrival : m_Arrivals)
				{
					if (!arrival.FromBench || InOrder(arrival.Account) || !sure(arrival.Account)) { continue; }
					candidates.push_back(arrival.Account);
				}
			}
			if (candidates.empty()) { i++; continue; }

			int holes = 0, sameHoles = 0;
			for (const Vacancy& other : m_Vacancies)
			{
				if (!offBench && other.Subgroup != vacancy.Subgroup) { continue; }
				holes++;
				if (other.Profession == vacancy.Profession) { sameHoles++; }
			}

			std::string pick;
			if (candidates.size() == 1 && holes == 1) { pick = candidates[0]; }
			else
			{
				std::vector<std::string> same;
				for (const std::string& candidate : candidates)
				{
					if (professionOf(candidate) == vacancy.Profession) { same.push_back(candidate); }
				}
				if (same.size() == 1 && sameHoles == 1) { pick = same[0]; }
				else if (static_cast<int>(candidates.size()) >= holes)
				{
					uint16_t group = vacancy.Subgroup;
					std::string names;
					for (const Vacancy& other : m_Vacancies)
					{
						if (offBench || other.Subgroup == group) { names += (names.empty() ? "" : ", ") + DisplayAccount(other.Account); }
					}
					Notify(NoticeKind::Substituted, vacancy.Account, "Can't tell who replaces " + names +
						", add them by hand");
					bool all = offBench;
					std::erase_if(m_Vacancies, [group, all](const Vacancy& aOther) { return all || aOther.Subgroup == group; });
					std::erase_if(m_Arrivals, [group, all](const Arrival& aOther) { return all || aOther.Subgroup == group; });
					i = 0;
					continue;
				}
				else { i++; continue; }
			}

			int standing = SelfStanding(aNowMs);
			FillPlace(i, pick);
			Notify(NoticeKind::Substituted, pick, Named(pick) + " took the place of " + DisplayAccount(vacancy.Account) +
				StandingChange(standing, SelfStanding(aNowMs)));
			i = 0;
		}
	}

	FightStats::Chance Session::ChanceNow(uint64_t aTimeMs) const
	{
		FightStats::Chance chance;
		TurnView turn = m_Tracker.GetTurn(aTimeMs);
		if (turn.UpIndex >= 0) { chance.UpAccount = turn.Rows[turn.UpIndex].Account; }
		for (const OrderRow& row : turn.Rows)
		{
			if (row.Status != Eligibility::Ready) { continue; }
			ReviveGroup group = ReviveGroup::Count;
			if (row.Player != nullptr && !row.Player->Skills.empty()) { group = row.Player->Skills.front().Group; }
			uint8_t targets = group == ReviveGroup::Count ? 0 : GroupTargets(group);
			chance.Capacity += targets > 0 ? targets : 1;
		}
		return chance;
	}

	void Session::TakeAttributions()
	{
		for (Attribution& attribution : m_Tracker.TakeAttributions())
		{
			m_Stats.OnRevived(attribution.TimeMs, attribution.Reviver);
			m_Attributions.push_back(std::move(attribution));
		}
		constexpr size_t kKeep = 32;
		if (m_Attributions.size() > kKeep) { m_Attributions.erase(m_Attributions.begin(), m_Attributions.end() - kKeep); }
	}

	void Session::SettleUps(uint64_t aNowMs)
	{
		constexpr uint64_t kSameUpMs = 1500;
		for (size_t i = 0; i < m_PendingUps.size(); )
		{
			const PendingUp& up = m_PendingUps[i];
			if (aNowMs < up.ArrivedMs + kUpSettleMs) { i++; continue; }
			std::string reviver;
			for (const Attribution& attribution : m_Attributions)
			{
				uint64_t apart = attribution.TimeMs > up.TimeMs ? attribution.TimeMs - up.TimeMs : up.TimeMs - attribution.TimeMs;
				if (attribution.Revived == up.Account && apart <= kSameUpMs) { reviver = attribution.Reviver; break; }
			}
			m_Stats.OnUp(up.TimeMs, up.Account, reviver);
			m_PendingUps.erase(m_PendingUps.begin() + static_cast<std::ptrdiff_t>(i));
		}
	}

	void Session::MatchIllusion(uint64_t aTimeMs, const std::string& aAccount, bool aIsCast)
	{
		constexpr uint64_t kSameInstantMs = 50;
		constexpr uint64_t kKeepMs        = 2000;
		constexpr uint64_t kLengthMs      = 15000;
		constexpr int      kTargets       = 3;
		auto stale = [aTimeMs](const RecentEvent& aEvent) { return aEvent.TimeMs + kKeepMs < aTimeMs; };
		std::erase_if(m_RecentIllusionCasts, stale);
		std::erase_if(m_RecentGotUp, stale);
		auto sameInstant = [aTimeMs](uint64_t aOther) { return (aOther > aTimeMs ? aOther - aTimeMs : aTimeMs - aOther) <= kSameInstantMs; };

		if (aIsCast)
		{
			int targets = 0;
			for (const RecentEvent& up : m_RecentGotUp)
			{
				if (targets == kTargets || !sameInstant(up.TimeMs) || up.Account == aAccount) { continue; }
				m_Illusions[up.Account] = IllusionState{ aTimeMs + kLengthMs, aAccount };
				targets++;
			}
			m_RecentIllusionCasts.push_back(RecentEvent{ aTimeMs, aAccount });
			return;
		}

		for (const RecentEvent& cast : m_RecentIllusionCasts)
		{
			if (!sameInstant(cast.TimeMs) || cast.Account == aAccount) { continue; }
			m_Illusions[aAccount] = IllusionState{ cast.TimeMs + kLengthMs, cast.Account };
			break;
		}
		m_RecentGotUp.push_back(RecentEvent{ aTimeMs, aAccount });
	}

	bool Session::SelfCanRevive(uint64_t aNowMs) const
	{
		if (m_SelfAccount.empty()) { return false; }
		auto member = m_Roster.find(m_SelfAccount);
		if (member == m_Roster.end() || !IsReviveProfession(member->second.Profession)) { return false; }
		const PlayerStatus* self = m_Tracker.FindPlayer(m_SelfAccount);
		if (self == nullptr) { return true; }
		if (self->Away || self->Life != LifeState::Alive) { return false; }
		if (self->Skills.empty()) { return true; }
		for (const SkillStatus& skill : self->Skills)
		{
			if (skill.State == SkillState::Ready || (skill.State == SkillState::Cooldown && skill.ReadyAtMs <= aNowMs)) { return true; }
		}
		return false;
	}

	void Session::OnSelfLeftSquad()
	{
		if (m_Tracker.Order().empty()) { return; }
		m_Tracker.SetOrder({});
		m_PendingLeaves.clear();
		m_Reported.clear();
		m_Vacancies.clear();
		m_Arrivals.clear();
		m_Subgroups.clear();
		m_BenchGroup = 0;
		m_LastBenched = 0;
		ClearRequest();
		DismissShare();
		Notify(NoticeKind::OrderCleared, m_SelfAccount, "You left the squad: revive order cleared");
	}

	void Session::Tick(uint64_t aNowMs)
	{
		std::erase_if(m_Illusions, [aNowMs](const auto& aEntry) { return aEntry.second.EndsAt <= aNowMs; });
		std::erase_if(m_MinionBoons, [aNowMs](const auto& aEntry) { return aEntry.second.FirstMs + 10000 < aNowMs; });
		if (!m_Vacancies.empty()) { MatchSubstitutes(aNowMs); }

		SettleUps(aNowMs);
		if (m_CombatSinceMs == 0)
		{
			if (m_OutOfCombatSinceMs == 0) { m_OutOfCombatSinceMs = aNowMs; }
			if (aNowMs >= m_OutOfCombatSinceMs + kRotationResetMs)
			{
				m_Tracker.ResetRotation();
				if (m_Stats.Active())
				{
					FightStat fight;
					if (m_Stats.Close(aNowMs, m_FightNumber + 1, fight) && fight.Downs > 0)
					{
						m_FightNumber++;
						m_Fights.push_back(fight);
						if (m_Fights.size() > kKeepFights) { m_Fights.erase(m_Fights.begin()); }
						Notify(NoticeKind::FightSummary, {}, "Fight " + std::to_string(fight.Number) + ": " + fight.Summary());
					}
				}
			}
		}

		if (m_ResyncAtMs != 0 && aNowMs >= m_ResyncAtMs)
		{
			m_ResyncAtMs = 0;
			if (m_HasRoles) { return; }
			for (const std::string& account : m_Tracker.Order())
			{
				auto it = m_Roster.find(account);
				if (it == m_Roster.end() || it->second.OnMap || it->second.IsSelf || m_Reported.count(account)) { continue; }
				Notify(NoticeKind::LeftMap, account, Named(account) + " left while you were loading");
				m_Reported[account] = true;
			}
		}

		while (!m_PendingLeaves.empty() && m_PendingLeaves.front().TimeMs + kLeaveGraceMs <= aNowMs)
		{
			PendingLeave leave = m_PendingLeaves.front();
			m_PendingLeaves.pop_front();
			if (m_LastSelfLeaveMs + kSelfLeaveSlackMs >= leave.TimeMs && m_LastSelfLeaveMs <= leave.TimeMs + kLeaveGraceMs) { continue; }

			auto it = m_Roster.find(leave.Account);
			if (it == m_Roster.end() || it->second.OnMap || !InOrder(leave.Account) || m_Reported.count(leave.Account)) { continue; }
			Notify(NoticeKind::LeftMap, leave.Account, Named(leave.Account) + " left the map" +
				StandingChange(leave.OurStanding, SelfStanding(aNowMs)));
			m_Reported[leave.Account] = true;
		}
	}

	void Session::SetOrder(std::vector<std::string> aAccounts)
	{
		if (aAccounts != m_Tracker.Order()) { m_Vacancies.clear(); }
		m_Tracker.SetOrder(std::move(aAccounts));
		m_OrderFrom.clear();
	}

	void Session::SetPrecast(std::vector<std::string> aAccounts)
	{
		m_Precast = std::move(aAccounts);
	}

	void Session::SetAnswerRule(AnswerRule aRule)
	{
		m_AnswerRule = aRule;
	}

	void Session::SetShareRules(bool aFromLeaders, bool aFromAnyone)
	{
		m_ShareFromLeaders = aFromLeaders;
		m_ShareFromAnyone = aFromAnyone;
	}

	void Session::OnChatMessage(const std::string& aAccount, const std::string& aText, uint64_t aNowMs)
	{
		Share::Message message = Share::Parse(aText);
		if (message.What == Share::Kind::None) { return; }
		bool ours = !aAccount.empty() && aAccount == m_SelfAccount;

		std::string name = DisplayAccount(aAccount);
		if (message.What == Share::Kind::Remove)
		{
			std::erase_if(m_Requests, [&](const JoinRequest& aRequest) { return aRequest.Account == aAccount; });
			if (!InOrder(aAccount)) { return; }
			int standing = SelfStanding(aNowMs);
			std::string named = Named(aAccount);
			DropFromOrder(aAccount);
			Notify(NoticeKind::LeftSquad, aAccount, named + " asked to be taken out of the order" +
				StandingChange(standing, SelfStanding(aNowMs)));
			return;
		}
		if (message.What == Share::Kind::Add)
		{
			if (ours || !AnswersRequests()) { return; }
			auto member = m_Roster.find(aAccount);
			if (member == m_Roster.end() || !member->second.InSquad) { return; }
			if (InOrder(aAccount)) { return; }

			auto waiting = std::find_if(m_Requests.begin(), m_Requests.end(),
				[&](const JoinRequest& aRequest) { return aRequest.Account == aAccount; });
			if (waiting != m_Requests.end())
			{
				waiting->Place = message.Place;
				waiting->TimeMs = aNowMs;
			}
			else
			{
				if (m_Requests.size() >= kMaxRequests) { m_Requests.pop_front(); }
				m_Requests.push_back(JoinRequest{ aAccount, message.Place, aNowMs });
			}

			uint64_t& announced = m_AskedNoticeMs[aAccount];
			if (aNowMs >= announced + kAskAgainMs || announced == 0)
			{
				announced = aNowMs;
				std::string where = message.Place > 0 ? " at " + std::to_string(message.Place) + "." : "";
				Notify(NoticeKind::ShareRequested, aAccount, name + " asked to be in the revive order" + where);
			}
			return;
		}

		std::vector<RosterMember> roster;
		roster.reserve(m_Roster.size());
		for (const auto& [account, member] : m_Roster) { roster.push_back(member); }
		Share::Resolved resolved = Share::Resolve(message.Entries, roster);
		if (resolved.Accounts.empty()) { return; }

		if (ours)
		{
			if (resolved.Accounts == m_Tracker.Order() && resolved.Precast == m_Precast && message.Bench == m_BenchGroup) { return; }
			int before = SelfPlace();
			SetOrder(resolved.Accounts);
			SetPrecast(resolved.Precast);
			SetBench(message.Bench);
			Notify(NoticeKind::ShareApplied, m_SelfAccount, "using the revive order you pasted (" +
				std::to_string(resolved.Accounts.size()) + " players)" + PlaceChange(before));
			return;
		}

		SquadRole role = SquadRole::Unknown;
		if (auto it = m_Roster.find(aAccount); it != m_Roster.end()) { role = it->second.Role; }

		std::string what = name + " shared a revive order (" + std::to_string(resolved.Accounts.size()) + " players)";
		if (!resolved.Unknown.empty()) { what += ", " + std::to_string(resolved.Unknown.size()) + " not in the squad"; }

		bool trusted = m_ShareFromAnyone ||
			(m_ShareFromLeaders && (role == SquadRole::Leader || role == SquadRole::Lieutenant));
		ClearRequest();

		if (trusted)
		{
			int before = SelfPlace();
			SetOrder(resolved.Accounts);
			SetPrecast(resolved.Precast);
			SetBench(message.Bench);
			m_OrderFrom = aAccount;
			m_HasShare = false;
			Notify(NoticeKind::ShareApplied, aAccount, what + PlaceChange(before));
			return;
		}

		m_Share = SharedOrder{ aAccount, role, resolved.Accounts, resolved.Precast, resolved.Unknown, SelfPlace(), 0, aNowMs,
			message.Bench };
		auto mine = std::find(resolved.Accounts.begin(), resolved.Accounts.end(), m_SelfAccount);
		m_Share.OurPlaceThen = mine == resolved.Accounts.end() ? 0
			: static_cast<int>(mine - resolved.Accounts.begin()) + 1;
		m_HasShare = true;
		Notify(NoticeKind::ShareOffered, aAccount, what);
	}

	void Session::AcceptShare()
	{
		if (!m_HasShare) { return; }
		int before = SelfPlace();
		SetOrder(m_Share.Accounts);
		SetPrecast(m_Share.Precast);
		SetBench(m_Share.Bench);
		m_OrderFrom = m_Share.From;
		m_HasShare = false;
		Notify(NoticeKind::ShareApplied, m_Share.From, "using " + DisplayAccount(m_Share.From) + "'s revive order (" +
			std::to_string(m_Share.Accounts.size()) + " players)" + PlaceChange(before));
	}

	void Session::DismissShare()
	{
		m_HasShare = false;
		m_Share = SharedOrder{};
	}

	void Session::ClearRequest(const std::string& aAccount)
	{
		if (aAccount.empty()) { m_Requests.clear(); return; }
		std::erase_if(m_Requests, [&](const JoinRequest& aRequest) { return aRequest.Account == aAccount; });
	}

	bool Session::AnswersRequests() const
	{
		if (m_AnswerRule == AnswerRule::Never) { return false; }
		if (m_AnswerRule == AnswerRule::Always) { return true; }
		if (!m_Tracker.Order().empty()) { return OrderIsOurs(); }

		auto self = m_Roster.find(m_SelfAccount);
		if (self == m_Roster.end()) { return false; }
		if (self->second.Role == SquadRole::Leader) { return true; }
		if (self->second.Role != SquadRole::Lieutenant) { return false; }
		return std::none_of(m_Roster.begin(), m_Roster.end(), [](const auto& aEntry)
		{
			return aEntry.second.Role == SquadRole::Leader && aEntry.second.InSquad;
		});
	}

	SessionView Session::GetView(uint64_t aNowMs) const
	{
		SessionView view;
		view.HasShare = m_HasShare;
		view.Share = m_Share;
		if (AnswersRequests())
		{
			for (const JoinRequest& request : m_Requests)
			{
				if (aNowMs < request.TimeMs + kRequestShowMs && !InOrder(request.Account)) { view.Requests.push_back(request); }
			}
		}
		view.Turn = m_Tracker.GetTurn(aNowMs);
		view.BackupIndex = view.Turn.BackupIndex;
		view.Order = m_Tracker.Order();
		view.Precast = m_Precast;
		view.Bench = m_BenchGroup;
		view.SelfAccount = m_SelfAccount;
		view.SquadInCombat = m_CombatSinceMs != 0;
		view.NowMs = aNowMs;
		view.SelfCanRevive = SelfCanRevive(aNowMs);
		for (const auto& [account, illusion] : m_Illusions)
		{
			if (account == m_SelfAccount || illusion.EndsAt <= aNowMs) { continue; }
			bool ourCast = !m_SelfAccount.empty() && illusion.Caster == m_SelfAccount;
			view.Illusions.push_back(IllusionTarget{ account, illusion.EndsAt - aNowMs, ourCast });
		}
		std::sort(view.Illusions.begin(), view.Illusions.end(),
			[](const IllusionTarget& a, const IllusionTarget& b) { return a.EndsInMs < b.EndsInMs; });

		view.Roster.reserve(m_Roster.size());
		for (const auto& [account, member] : m_Roster) { view.Roster.push_back(member); }
		std::sort(view.Roster.begin(), view.Roster.end(), [](const RosterMember& a, const RosterMember& b)
		{
			bool aRevive = IsReviveProfession(a.Profession), bRevive = IsReviveProfession(b.Profession);
			if (aRevive != bRevive) { return aRevive; }
			return LessNoCase(a.Account, b.Account);
		});
		return view;
	}

	std::vector<Notice> Session::TakeNotices()
	{
		std::vector<Notice> notices;
		notices.swap(m_Notices);
		if (notices.size() > kMaxSeparateNotices)
		{
			std::string text = std::to_string(notices.size()) + " players in the order changed:";
			for (const Notice& notice : notices) { text += " " + notice.Text + ";"; }
			text.pop_back();
			notices.assign(1, Notice{ notices.front().Kind, "", text });
		}
		return notices;
	}
}