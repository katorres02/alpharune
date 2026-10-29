/// @file test_audit_2026_10_engine.cpp
/// Engine-wide fixes from the 2026-10-28 card audit
/// (docs/card-coverage/audit-2026-10-28.md).

#include "tests/cards/card_test_fixture.h"

#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kLeeSinAscetic = 78;   // "I can have any number of buffs."

using AuditEngineTest = CardTestFixture;

// CR 427.1.b.1 — buffing a unit that already has a buff does nothing.
TEST_F(AuditEngineTest, BuffDoesNotStack) {
    auto u = addUnit(PlayerId::Player1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(exec.buffUnit(u));
    EXPECT_FALSE(exec.buffUnit(u));
    EXPECT_EQ(state.getObject(u).buff_count, 1);
    EXPECT_EQ(state.getObject(u).current_might, 3);
}

// CR 427.1.b.2 — a card that allows any number of buffs keeps stacking.
TEST_F(AuditEngineTest, LeeSinAsceticCanHaveAnyNumberOfBuffs) {
    auto u = addUnit(PlayerId::Player1, kLeeSinAscetic, 0, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(exec.buffUnit(u));
    EXPECT_TRUE(exec.buffUnit(u));
    EXPECT_EQ(state.getObject(u).buff_count, 2);
}

// "When I become exhausted" needs a ready -> exhausted change: exhausting an
// already-exhausted unit must not emit the "exhausted" event.
TEST_F(AuditEngineTest, ExhaustingAnExhaustedUnitEmitsNothing) {
    auto u = addUnit(PlayerId::Player1, kInvalidId, 2, /*at_bf=*/0);
    int exhausted_events = 0;
    auto conn = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.object == u && e.what_changed == "exhausted") ++exhausted_events;
        });
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.exhaustObject(u);
    exec.exhaustObject(u);
    conn.disconnect();
    EXPECT_TRUE(state.getObject(u).is_exhausted);
    EXPECT_EQ(exhausted_events, 1);
}

}  // namespace
}  // namespace riftbound::test
