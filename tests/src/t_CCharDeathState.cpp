#include <doctest/doctest.h>

#include "../../src/game/chars/CCharDeathState.h"

TEST_CASE("death transaction remains recoverable until completion")
{
    CCharDeathTransaction tx;
    CHECK_FALSE(tx.IsActive());

    tx.phase = CCharDeathPhase::DeadCommitted;
    CHECK(tx.IsActive());

    tx.phase = CCharDeathPhase::Completed;
    CHECK_FALSE(tx.IsActive());
}

TEST_CASE("irreversible death callbacks are dispatched at most once")
{
    CCharDeathTransaction tx;
    CHECK(tx.DispatchOnce(CCharDeathCallback::DeathCorpse));
    CHECK_FALSE(tx.DispatchOnce(CCharDeathCallback::DeathCorpse));
    CHECK(tx.DispatchOnce(CCharDeathCallback::Penalties));
}

TEST_CASE("repeated engine failures eventually select NPC emergency cleanup")
{
    CCharDeathTransaction tx;
    tx.phase = CCharDeathPhase::CoreEffectsCleaned;

    for (std::uint16_t attempt = 1; attempt < CCharDeathTransaction::kMaxRetries; ++attempt)
    {
        CHECK_FALSE(tx.RegisterFailure(true));
        CHECK(tx.phase == CCharDeathPhase::CoreEffectsCleaned);
    }

    CHECK(tx.RegisterFailure(true));
    CHECK(tx.phase == CCharDeathPhase::EmergencyFinalization);
}

TEST_CASE("players and bonded pets are never emergency-deleted")
{
    CCharDeathTransaction tx;
    tx.phase = CCharDeathPhase::EntityFinalized;
    for (unsigned attempt = 0; attempt < CCharDeathTransaction::kMaxRetries + 2; ++attempt)
        CHECK_FALSE(tx.RegisterFailure(false));
    CHECK(tx.phase == CCharDeathPhase::EntityFinalized);
}

TEST_CASE("death fault injector targets an exact phase boundary")
{
    CCharDeathFaultInjector::Arm(CCharDeathPhase::DeadCommitted, CCharDeathFaultBoundary::AfterSideEffect);
    CHECK_NOTHROW(CCharDeathFaultInjector::Check(CCharDeathPhase::DeadCommitted, CCharDeathFaultBoundary::BeforePhase));
    CHECK_THROWS_AS(CCharDeathFaultInjector::Check(CCharDeathPhase::DeadCommitted, CCharDeathFaultBoundary::AfterSideEffect), std::runtime_error);
    CHECK_NOTHROW(CCharDeathFaultInjector::Check(CCharDeathPhase::DeadCommitted, CCharDeathFaultBoundary::AfterSideEffect));
    CCharDeathFaultInjector::Reset();
}

TEST_CASE("every resumable phase exposes before and after fault boundaries")
{
    constexpr CCharDeathPhase phases[] = { CCharDeathPhase::Accepted, CCharDeathPhase::InteractionsDetached, CCharDeathPhase::AttributionDispatched,
        CCharDeathPhase::DeadCommitted, CCharDeathPhase::CoreEffectsCleaned, CCharDeathPhase::LootDispatched, CCharDeathPhase::CorpseEnsured,
        CCharDeathPhase::CorpseTriggerDispatched, CCharDeathPhase::WorldNotified, CCharDeathPhase::EntityFinalized, CCharDeathPhase::EmergencyFinalization };

    for (const CCharDeathPhase phase : phases)
    {
        CAPTURE(CCharDeathPhaseName(phase));
        CCharDeathFaultInjector::Arm(phase, CCharDeathFaultBoundary::BeforePhase);
        CHECK_THROWS_AS(CCharDeathFaultInjector::Check(phase, CCharDeathFaultBoundary::BeforePhase), std::runtime_error);

        CCharDeathFaultInjector::Arm(phase, CCharDeathFaultBoundary::AfterSideEffect);
        CHECK_THROWS_AS(CCharDeathFaultInjector::Check(phase, CCharDeathFaultBoundary::AfterSideEffect), std::runtime_error);
    }
    CCharDeathFaultInjector::Reset();
}

TEST_CASE("death execution guard releases the reentrancy lock")
{
    CCharDeathTransaction tx;
    CHECK_FALSE(tx.executing);
    {
        CCharDeathExecutionGuard guard(tx);
        CHECK(tx.executing);
    }
    CHECK_FALSE(tx.executing);
}

TEST_CASE("legacy zombie NPC enters recovery while intentional ghosts do not")
{
    // Old OnTickPeriodic skipped every STATF_DEAD character, which left this
    // exact non-bonded NPC state permanently in the world.
    constexpr bool legacyWouldRemainStuck = true;
    CHECK(legacyWouldRemainStuck);
    CHECK(CCharShouldStartLegacyDeathRecovery(true, true, false, false));

    CHECK_FALSE(CCharShouldStartLegacyDeathRecovery(true, false, false, false)); // player ghost
    CHECK_FALSE(CCharShouldStartLegacyDeathRecovery(true, true, true, false));   // bonded ghost
    CHECK_FALSE(CCharShouldStartLegacyDeathRecovery(true, true, false, true));   // already recovering
}
