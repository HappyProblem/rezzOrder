#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Arc.h"
#include "Session.h"

struct UserInfo;
struct SquadMessageInfo;

namespace Live
{
	void OnCombatSquad(const ArcDps::EvCombatData* aData);
	void OnAgentUpdate(const ArcDps::EvAgentUpdate* aUpdate);
	void OnSquadUpdate(const UserInfo* aUsers, uint64_t aCount);
	void OnChatMessage(const SquadMessageInfo* aMessage);

	void Tick();
	void SetOrder(const std::vector<std::string>& aAccounts);
	void SetPrecast(const std::vector<std::string>& aAccounts);
	void SetBench(int aSubgroup);
	void SetShareRules(bool aFromLeaders, bool aFromAnyone);
	void SetAnswerRule(int aRule);
	void SetSubstitutes(bool aEnabled);
	void AcceptShare();
	void DismissShare();
	void ClearRequest(const std::string& aAccount = {});
	void MarkPlayerUsed(const std::string& aAccount = {});
	void MarkPlayerReady(const std::string& aAccount = {});

	Rezz::SessionView GetView();
	std::vector<Rezz::FightStat> GetFights();
	std::vector<Rezz::Notice> TakeNotices();
}