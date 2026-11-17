#include "Live.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include <Windows.h>
#include <mmsystem.h>

#include "Share.h"
#include "unofficial_extras/Definitions.h"

namespace Live
{
	namespace
	{
		constexpr uint8_t CBTS_BUFFACTIVE = 28;
		std::mutex        s_Mutex;
		Rezz::Session     s_Session;

		bool IsRelevantCombatEvent(const ArcDps::EvCombatData* aData)
		{
			if (aData == nullptr || aData->Ev == nullptr) { return false; }
			uint8_t sc = aData->Ev->IsStatechange;
			uint32_t skillId = aData->Ev->SkillId;

			if (sc == ArcDps::CBTS_BUFFREMOVE_ALL || sc == ArcDps::CBTS_BUFFREMOVE_SINGLE)
			{
				return skillId == kIllusionOfLifeEffect ||
					skillId == kSignetOfMercyPassive ||
					skillId == kSignetOfUndeathPassive ||
					FindReviveSkill(skillId) != nullptr;
			}

			return sc == ArcDps::CBTS_COMBAT ||
				sc == CBTS_BUFFACTIVE ||
				sc == ArcDps::CBTS_BUFFCHANGE ||
				sc == ArcDps::CBTS_ANIMATIONSTART ||
				sc == ArcDps::CBTS_ANIMATIONSTOP ||
				sc == ArcDps::CBTS_CHANGEDOWN ||
				sc == ArcDps::CBTS_CHANGEUP ||
				sc == ArcDps::CBTS_CHANGEDEAD ||
				sc == ArcDps::CBTS_BUFFAPPLY ||
				sc == ArcDps::CBTS_BUFFINITIAL ||
				sc == ArcDps::CBTS_ENTERCOMBAT ||
				sc == ArcDps::CBTS_SQCOMBATSTART ||
				sc == ArcDps::CBTS_SQCOMBATEND;
		}
	}

	void OnCombatSquad(const ArcDps::EvCombatData* aData)
	{
		if (!IsRelevantCombatEvent(aData)) { return; }
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.OnCombat(*aData, now, false);
	}

	void OnCombatLocal(const ArcDps::EvCombatData* aData)
	{
		if (!IsRelevantCombatEvent(aData)) { return; }
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.OnCombat(*aData, now, true);
	}

	void OnAgentUpdate(const ArcDps::EvAgentUpdate* aUpdate)
	{
		if (aUpdate == nullptr) { return; }
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.OnAgentUpdate(*aUpdate, now);
	}

	void OnSquadUpdate(const UserInfo* aUsers, uint64_t aCount)
	{
		if (aUsers == nullptr) { return; }
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		for (uint64_t i = 0; i < aCount; i++)
		{
			const UserInfo& user = aUsers[i];
			if (user.AccountName == nullptr) { continue; }
			s_Session.OnRole(user.AccountName, static_cast<Rezz::SquadRole>(user.Role), user.Subgroup, now);
		}
	}

	void OnChatMessage(const SquadMessageInfo* aMessage)
	{
		if (aMessage == nullptr || aMessage->AccountName == nullptr || aMessage->Text == nullptr) { return; }
		std::string account = aMessage->AccountName;
		size_t length = ::strnlen(aMessage->Text, Rezz::Share::kMaxTextChars);
		if (aMessage->TextLength > 0 && static_cast<size_t>(aMessage->TextLength) < length)
		{
			length = static_cast<size_t>(aMessage->TextLength);
		}
		std::string text(aMessage->Text, length);
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.OnChatMessage(account, text, now);
	}

	void SetShareRules(bool aFromLeaders, bool aFromAnyone)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.SetShareRules(aFromLeaders, aFromAnyone);
	}

	void SetSubstitutes(bool aEnabled)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.SetSubstitutes(aEnabled);
	}

	void SetAnswerRule(int aRule)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.SetAnswerRule(static_cast<Rezz::Session::AnswerRule>(aRule));
	}

	void AcceptShare()
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.AcceptShare();
	}

	void DismissShare()
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.DismissShare();
	}

	void ClearRequest(const std::string& aAccount)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.ClearRequest(aAccount);
	}

	void MarkPlayerUsed(const std::string& aAccount)
	{
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.MarkPlayerUsed(aAccount, now);
	}

	void MarkPlayerReady(const std::string& aAccount)
	{
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.MarkPlayerReady(aAccount, now);
	}

	void Tick()
	{
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		s_Session.Tick(now);
	}

	void SetOrder(const std::vector<std::string>& aAccounts)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.SetOrder(aAccounts);
	}

	std::vector<Rezz::FightStat> GetFights()
	{
		std::scoped_lock lock(s_Mutex);
		return s_Session.Fights();
	}

	void SetBench(int aSubgroup)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.SetBench(static_cast<uint16_t>(std::clamp(aSubgroup, 0, 255)));
	}

	void SetPrecast(const std::vector<std::string>& aAccounts)
	{
		std::scoped_lock lock(s_Mutex);
		s_Session.SetPrecast(aAccounts);
	}

	Rezz::SessionView GetView()
	{
		uint32_t now = timeGetTime();
		std::scoped_lock lock(s_Mutex);
		return s_Session.GetView(now);
	}

	std::vector<Rezz::Notice> TakeNotices()
	{
		std::scoped_lock lock(s_Mutex);
		return s_Session.TakeNotices();
	}
}