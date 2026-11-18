#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "ReviveSkills.h"

namespace Rezz
{
	enum class LifeState : uint8_t { Alive, Downed, Dead };
	enum class SkillState : uint8_t { Ready, Casting, Cooldown };

	struct SkillStatus
	{
		ReviveGroup Group = ReviveGroup::Count;
		SkillState  State = SkillState::Ready;
		uint64_t    CastStartMs = 0;
		uint64_t    LastUsedMs = 0;
		uint64_t    ReadyAtMs = 0;
		uint32_t    Uses = 0;
		uint32_t    Cancels = 0;
		uint32_t    Revived = 0;
		uint32_t    LastRevived = 0;
		uint32_t    EarlyRecasts = 0;
		bool        Confirmed = false;
	};

	struct PlayerStatus
	{
		std::string              Account;
		LifeState                Life = LifeState::Alive;
		uint64_t                 LifeSinceMs = 0;
		bool                     Away = false;
		uint64_t                 SeenSinceMs = 0;
		std::vector<SkillStatus> Skills;
	};

	enum class Eligibility : uint8_t { Ready, Casting, Cooldown, Downed, Dead, Away };

	struct Attribution
	{
		uint64_t    TimeMs;
		std::string Reviver;
		std::string Revived;
	};

	struct OrderRow
	{
		std::string         Account;
		const PlayerStatus* Player = nullptr;
		Eligibility         Status = Eligibility::Ready;
		uint64_t            ReadyInMs = 0;
		uint64_t            RechargeMs = 0;
		bool                IsUp = false;
		bool                IsLastUser = false;
		bool                Unconfirmed = false;
	};

	struct TurnView
	{
		std::vector<OrderRow> Rows;
		int UpIndex = -1;
		int BackupIndex = -1;
		int NextReadyIndex = -1;
	};

	class Tracker
	{
	public:
		static constexpr uint64_t kStaleDownedMs = 120 * 1000;
		static constexpr uint64_t kRecastSlackMs = 5 * 1000;
		static constexpr uint64_t kConfirmAfterMs = 120 * 1000;
		static constexpr uint64_t kAlreadyCountedMs = 20 * 1000;

		void SetOrder(std::vector<std::string> aAccounts);
		bool RemoveFromOrder(const std::string& aAccount);
		void InsertIntoOrder(size_t aIndex, const std::string& aAccount);
		void ResetRotation() { m_LastOrderedUser.clear(); }
		const std::vector<std::string>& Order() const { return m_Order; }

		void OnCastStart(uint64_t aTimeMs, const std::string& aAccount, uint32_t aSkillId);
		void OnCastStop(uint64_t aTimeMs, const std::string& aAccount, uint32_t aSkillId, uint8_t aStopReason, int32_t aBaseMs, int32_t aScaledMs = 0, uint8_t aActivation = 0);
		void OnOwnedEffect(uint64_t aTimeMs, const std::string& aOwnerAccount, uint32_t aSkillId);
		void OnLifeState(uint64_t aTimeMs, const std::string& aAccount, LifeState aState);
		void OnActivity(uint64_t aTimeMs, const std::string& aAccount);
		void SetAway(uint64_t aTimeMs, const std::string& aAccount, bool aAway);
		void ForgetSkills(const std::string& aAccount);
		void MarkSeenSince(uint64_t aTimeMs, const std::string& aAccount);
		bool MarkUsed(uint64_t aTimeMs, const std::string& aAccount, ReviveGroup aGroup);
		void MarkSkillReady(uint64_t aTimeMs, const std::string& aAccount, ReviveGroup aGroup, bool aForce = false);
		bool IsRecentlyUsed(uint64_t aTimeMs, const std::string& aAccount, ReviveGroup aGroup) const;

		const PlayerStatus* FindPlayer(const std::string& aAccount) const;
		TurnView GetTurn(uint64_t aNowMs) const;

		const std::string& LastOrderedUser() const { return m_LastOrderedUser; }
		std::vector<Attribution> TakeAttributions() { return std::move(m_Attributions); }

	private:
		struct RecentRevive
		{
			uint64_t    TimeMs;
			std::string Account;
			uint32_t    SkillId;
			ReviveGroup Group;
			uint8_t     Attributed = 0;
		};

		struct RecentUp
		{
			uint64_t    TimeMs;
			std::string Account;
			bool        Attributed = false;
		};

		PlayerStatus& GetPlayer(const std::string& aAccount);
		SkillStatus& GetSkill(PlayerStatus& aPlayer, ReviveGroup aGroup);
		void AddRevive(uint64_t aTimeMs, const std::string& aAccount, const ReviveSkill& aSkill);
		void Attribute(RecentRevive& aRevive, const std::string& aRevived);
		void Prune(uint64_t aTimeMs);

		std::vector<std::string>                      m_Order;
		std::unordered_map<std::string, PlayerStatus> m_Players;
		std::string                                   m_LastOrderedUser;
		std::deque<RecentRevive>                      m_RecentRevives;
		std::deque<RecentUp>                          m_RecentUps;
		std::vector<Attribution>                      m_Attributions;
	};
}