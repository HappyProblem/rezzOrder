#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "Arc.h"
#include "FightStats.h"
#include "Tracker.h"

namespace Rezz
{
	enum class SquadRole : uint8_t { Leader = 0, Lieutenant = 1, Member = 2, Invited = 3, Applied = 4, None = 5, Unknown = 6 };

	struct RosterMember
	{
		std::string Account;
		std::string Character;
		uint32_t    Profession  = 0;
		uint32_t    Elite       = 0;
		uint16_t    Subgroup    = 0;
		SquadRole   Role        = SquadRole::Unknown;
		bool        OnMap       = false;
		bool        InSquad     = true;
		bool        IsSelf      = false;
		uint32_t    SeenGroups  = 0;
		uint64_t    LastEventMs = 0;
	};

	enum class NoticeKind : uint8_t { LeftSquad, LeftMap, Returned, ChangedProfession, OrderCleared,
		ShareApplied, ShareOffered, ShareRequested, Substituted, FightSummary };

	struct JoinRequest
	{
		std::string Account;
		int         Place = 0;
		uint64_t    TimeMs = 0;
	};

	struct SharedOrder
	{
		std::string              From;
		SquadRole                FromRole = SquadRole::Unknown;
		std::vector<std::string> Accounts;
		std::vector<std::string> Precast;
		std::vector<std::string> Unknown;
		int                      OurPlaceNow  = 0;
		int                      OurPlaceThen = 0;
		uint64_t                 TimeMs   = 0;
		uint16_t                 Bench    = 0;
	};

	struct IllusionTarget
	{
		std::string Account;
		uint64_t    EndsInMs = 0;
		bool        OurCast  = false;
	};

	struct Notice
	{
		NoticeKind  Kind;
		std::string Account;
		std::string Text;
	};

	struct SessionView
	{
		TurnView                  Turn;
		int                       BackupIndex = -1;
		std::vector<RosterMember> Roster;
		std::vector<std::string>  Order;
		std::vector<std::string>  Precast;
		uint16_t                  Bench = 0;
		bool                      SquadInCombat = false;
		uint64_t                  NowMs         = 0;
		std::string               SelfAccount;
		bool                      HasShare = false;
		SharedOrder               Share;
		std::vector<JoinRequest>  Requests;
		std::vector<IllusionTarget> Illusions;
		bool                      SelfCanRevive = false;
	};

	bool IsReviveProfession(uint32_t aProfession);
	const char* ProfessionShortName(uint32_t aProfession);
	bool IsUsableCharacterName(const std::string& aName);
	std::string DisplayAccount(const std::string& aAccount);

	class Session
	{
	public:
		static constexpr uint64_t kLeaveGraceMs = 8 * 1000;
		static constexpr uint64_t kResyncAfterMs = 90 * 1000;
		static constexpr uint64_t kOutOfRangeMs = 15 * 1000;
		static constexpr uint64_t kRequestShowMs = 60 * 1000;
		static constexpr size_t kMaxRequests = 6;
		static constexpr uint64_t kAskAgainMs = 30 * 1000;
		static constexpr uint64_t kRotationResetMs = 10 * 1000;
		static constexpr uint64_t kSubstituteWindowMs = 120 * 1000;

		void OnCombat(const ArcDps::EvCombatData& aData, uint64_t aNowMs);
		void OnAgentUpdate(const ArcDps::EvAgentUpdate& aUpdate, uint64_t aNowMs);
		void OnRole(const std::string& aAccount, SquadRole aRole, uint16_t aSubgroup, uint64_t aNowMs);
		void Tick(uint64_t aNowMs);

		void SetOrder(std::vector<std::string> aAccounts);
		void SetPrecast(std::vector<std::string> aAccounts);
		const std::vector<std::string>& Precast() const { return m_Precast; }
		bool OrderIsOurs() const { return m_OrderFrom.empty(); }
		int SelfPlace() const;
		void OnChatMessage(const std::string& aAccount, const std::string& aText, uint64_t aNowMs);
		void SetShareRules(bool aFromLeaders, bool aFromAnyone);
		void AcceptShare();
		void DismissShare();
		void ClearRequest(const std::string& aAccount = {});
		void MarkPlayerUsed(const std::string& aAccount, uint64_t aNowMs);
		void MarkPlayerReady(const std::string& aAccount, uint64_t aNowMs);

		enum class AnswerRule : uint8_t { Never = 0, WhenOrderIsOurs = 1, Always = 2 };
		void SetAnswerRule(AnswerRule aRule);
		void SetSubstitutes(bool aEnabled);
		void SetBench(uint16_t aBench);
		const std::vector<std::string>& Order() const { return m_Tracker.Order(); }

		SessionView GetView(uint64_t aNowMs) const;
		std::vector<Notice> TakeNotices();

		const Tracker& GetTracker() const { return m_Tracker; }
		const std::vector<FightStat>& Fights() const { return m_Fights; }

	private:
		struct AgentRef
		{
			std::string Account;
			uint16_t    InstanceId = 0;
		};

		struct PendingLeave
		{
			std::string Account;
			uint64_t    TimeMs;
			uint32_t    Profession;
			int         OurStanding;
		};

		struct MinionBoonState
		{
			uint64_t FirstMs = 0;
			uint8_t  Mask    = 0;
		};

		std::string ResolveAccount(uint64_t aId, uint16_t aInstanceId);
		std::string AccountByInstance(uint16_t aInstanceId) const;
		RosterMember& GetMember(const std::string& aAccount);
		bool InOrder(const std::string& aAccount) const;
		void Notify(NoticeKind aKind, const std::string& aAccount, std::string aText);
		std::string Named(const std::string& aAccount) const;
		std::string PlaceChange(int aPlaceBefore) const;
		int SelfStanding(uint64_t aNowMs) const;
		std::string StandingChange(int aBefore, int aAfter) const;
		void OnSelfLeftSquad();
		void RecordSkillUse(uint64_t aTimeMs, const std::string& aAccount, ReviveGroup aGroup);
		FightStats::Chance ChanceNow(uint64_t aTimeMs) const;
		void SettleUps(uint64_t aNowMs);
		void TakeAttributions();
		bool AnswersRequests() const;
		bool SelfCanRevive(uint64_t aNowMs) const;
		void MatchIllusion(uint64_t aTimeMs, const std::string& aAccount, bool aIsCast);
		void DropFromOrder(const std::string& aAccount, uint16_t aOpenFor = 0, uint64_t aNowMs = 0);
		void MatchSubstitutes(uint64_t aNowMs);
		void FillPlace(size_t aIndex, const std::string& aAccount);
		bool ReclaimPlace(const std::string& aAccount, uint64_t aNowMs, uint16_t aSubgroup);
		bool SureReviver(const RosterMember& aMember) const;
		bool IsBench(uint16_t aGroup, const std::string& aMover) const;
		uint16_t SubgroupOf(const std::string& aAccount) const;

		Tracker                                          m_Tracker;
		std::unordered_map<std::string, RosterMember>    m_Roster;
		std::unordered_map<uint64_t, AgentRef>           m_Agents;
		std::unordered_map<uint64_t, uint64_t>           m_Aliases;
		std::unordered_map<uint64_t, MinionBoonState>    m_MinionBoons;
		std::deque<PendingLeave>                         m_PendingLeaves;
		std::unordered_map<std::string, bool>            m_Reported;
		std::vector<Notice>                              m_Notices;
		std::string                                      m_SelfAccount;
		uint64_t                                         m_LastSelfLeaveMs = 0;
		uint64_t                                         m_ResyncAtMs      = 0;
		bool                                             m_HasRoles        = false;
		uint64_t                                         m_CombatSinceMs   = 0;
		uint64_t                                         m_OutOfCombatSinceMs = 0;
		struct IllusionState
		{
			uint64_t    EndsAt = 0;
			std::string Caster;
		};
		struct RecentEvent
		{
			uint64_t    TimeMs;
			std::string Account;
		};
		std::unordered_map<std::string, IllusionState> m_Illusions;
		bool                                           m_IllusionApplySeen = false;
		std::deque<RecentEvent>                        m_RecentIllusionCasts;
		std::deque<RecentEvent>                        m_RecentGotUp;
		SharedOrder                                    m_Share;
		bool                                           m_HasShare        = false;
		std::deque<JoinRequest>                        m_Requests;
		std::unordered_map<std::string, uint64_t>      m_AskedNoticeMs;
		struct Vacancy
		{
			std::string Account;
			uint16_t    Subgroup   = 0;
			uint32_t    Profession = 0;
			uint64_t    TimeMs     = 0;
			size_t      Index      = 0;
			uint32_t    Tie        = 0;
			bool        Precast    = false;
		};
		struct Arrival
		{
			std::string Account;
			uint16_t    Subgroup  = 0;
			uint64_t    TimeMs    = 0;
			bool        FromBench = false;
		};
		std::deque<Vacancy>                           m_Vacancies;
		std::deque<Arrival>                           m_Arrivals;
		struct PendingUp
		{
			std::string Account;
			uint64_t    TimeMs;
			uint64_t    ArrivedMs;
		};
		static constexpr uint64_t kUpSettleMs = 1500;
		static constexpr size_t   kKeepFights = 30;
		FightStats                                    m_Stats;
		std::vector<FightStat>                        m_Fights;
		std::vector<PendingUp>                        m_PendingUps;
		std::vector<Attribution>                      m_Attributions;
		int                                           m_FightNumber = 0;
		std::unordered_map<std::string, uint16_t>     m_Subgroups;
		uint16_t                                      m_BenchGroup  = 0;
		uint16_t                                      m_LastBenched = 0;
		bool                                          m_Substitutes = true;
		std::string                                   m_OrderFrom;
		std::vector<std::string>                      m_Precast;
		AnswerRule                                    m_AnswerRule   = AnswerRule::WhenOrderIsOurs;
		bool                                          m_ShareFromLeaders = true;
		bool                                          m_ShareFromAnyone  = false;
	};
}