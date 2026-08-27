/**
 * @file CCharDeathState.h
 * Persistent state for the resumable character death workflow.
 */
#ifndef _INC_CCHARDEATHSTATE_H
#define _INC_CCHARDEATHSTATE_H

#include <cstdint>
#include <limits>
#ifdef UNIT_TESTING
#    include <stdexcept>
#endif

enum class CCharDeathPhase : std::uint8_t
{
    None = 0,
    Accepted,
    InteractionsDetached,
    AttributionDispatched,
    DeadCommitted,
    CoreEffectsCleaned,
    LootDispatched,
    CorpseEnsured,
    CorpseTriggerDispatched,
    WorldNotified,
    EntityFinalized,
    Completed,
    EmergencyFinalization
};

[[nodiscard]]
constexpr const char *CCharDeathPhaseName(const CCharDeathPhase phase) noexcept
{
    switch (phase)
    {
        case CCharDeathPhase::None:
            return "None";
        case CCharDeathPhase::Accepted:
            return "Accepted";
        case CCharDeathPhase::InteractionsDetached:
            return "InteractionsDetached";
        case CCharDeathPhase::AttributionDispatched:
            return "AttributionDispatched";
        case CCharDeathPhase::DeadCommitted:
            return "DeadCommitted";
        case CCharDeathPhase::CoreEffectsCleaned:
            return "CoreEffectsCleaned";
        case CCharDeathPhase::LootDispatched:
            return "LootDispatched";
        case CCharDeathPhase::CorpseEnsured:
            return "CorpseEnsured";
        case CCharDeathPhase::CorpseTriggerDispatched:
            return "CorpseTriggerDispatched";
        case CCharDeathPhase::WorldNotified:
            return "WorldNotified";
        case CCharDeathPhase::EntityFinalized:
            return "EntityFinalized";
        case CCharDeathPhase::Completed:
            return "Completed";
        case CCharDeathPhase::EmergencyFinalization:
            return "EmergencyFinalization";
    }
    return "Invalid";
}

enum class CCharDeathFaultBoundary : std::uint8_t
{
    BeforePhase,
    AfterSideEffect
};

[[nodiscard]]
constexpr bool CCharShouldStartLegacyDeathRecovery(const bool dead, const bool npc, const bool bonded, const bool transactionActive) noexcept
{
    return dead && npc && !bonded && !transactionActive;
}

class CCharDeathFaultInjector {
public:
#ifdef UNIT_TESTING
    static inline CCharDeathPhase sm_phase            = CCharDeathPhase::None;
    static inline CCharDeathFaultBoundary sm_boundary = CCharDeathFaultBoundary::BeforePhase;
    static inline unsigned sm_remaining               = 0;

    static void Arm(CCharDeathPhase phase, CCharDeathFaultBoundary boundary, unsigned count = 1) noexcept
    {
        sm_phase     = phase;
        sm_boundary  = boundary;
        sm_remaining = count;
    }

    static void Reset() noexcept
    {
        sm_remaining = 0;
    }

    static void Check(CCharDeathPhase phase, CCharDeathFaultBoundary boundary)
    {
        if (sm_remaining && sm_phase == phase && sm_boundary == boundary)
        {
            --sm_remaining;
            throw std::runtime_error("Injected death transaction failure");
        }
    }
#else
    static constexpr void Check(CCharDeathPhase, CCharDeathFaultBoundary) noexcept
    {}
#endif
};

struct CCharDeathTransaction
{
    static constexpr std::uint8_t kSaveVersion = 1;
    static constexpr std::uint16_t kMaxRetries = 8;

    CCharDeathPhase phase             = CCharDeathPhase::None;
    std::uint32_t transactionId       = 0;
    std::uint32_t killerUid           = 0;
    std::uint32_t corpseUid           = 0;
    std::uint32_t dispatchedCallbacks = 0;
    std::uint16_t retryCount          = 0;
    std::int64_t nextRetryTime        = 0;
    bool frontFall                    = false;
    bool legacyRecovery               = false;
    bool executing                    = false; // transient; never serialized

    [[nodiscard]]
    constexpr bool IsActive() const noexcept
    {
        return phase != CCharDeathPhase::None && phase != CCharDeathPhase::Completed;
    }

    [[nodiscard]]
    constexpr bool DispatchOnce(const std::uint32_t mask) noexcept
    {
        if ((dispatchedCallbacks & mask) != 0)
            return false;
        dispatchedCallbacks |= mask;
        return true;
    }

    [[nodiscard]]
    constexpr bool RegisterFailure(const bool fCanEmergencyFinalize) noexcept
    {
        if (retryCount < std::numeric_limits<std::uint16_t>::max())
            ++retryCount;
        if (fCanEmergencyFinalize && retryCount >= kMaxRetries)
        {
            phase         = CCharDeathPhase::EmergencyFinalization;
            nextRetryTime = 0;
            return true;
        }
        return false;
    }

    constexpr void Reset() noexcept
    {
        *this = {};
    }
};

class CCharDeathExecutionGuard {
public:
    explicit CCharDeathExecutionGuard(CCharDeathTransaction &transaction) noexcept :
        m_transaction(transaction)
    {
        m_transaction.executing = true;
    }

    ~CCharDeathExecutionGuard()
    {
        m_transaction.executing = false;
    }

    CCharDeathExecutionGuard(const CCharDeathExecutionGuard &)            = delete;
    CCharDeathExecutionGuard &operator=(const CCharDeathExecutionGuard &) = delete;

private:
    CCharDeathTransaction &m_transaction;
};

namespace CCharDeathCallback
{
static constexpr std::uint32_t Death       = 1u << 0;
static constexpr std::uint32_t Attribution = 1u << 1;
static constexpr std::uint32_t CreateLoot  = 1u << 2;
static constexpr std::uint32_t DeathCorpse = 1u << 3;
static constexpr std::uint32_t Penalties   = 1u << 4;
static constexpr std::uint32_t WorldNotify = 1u << 5;
} // namespace CCharDeathCallback

#endif
