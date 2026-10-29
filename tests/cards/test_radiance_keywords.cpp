/// @file test_radiance_keywords.cpp
/// Engine support for the Radiance keywords (CR 830 Deploy, 831 Disarm).
/// Show Off (CR 832) is covered by the card tests that use it.

#include "tests/cards/card_test_fixture.h"

#include "cards/gear/deploy_base.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/keyword_abilities.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kFakeDeployGear = 999201;

class FakeDeployGear : public DeployGear {
public:
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef d;
            d.id = kFakeDeployGear;
            d.name = "Fake Deploy Gear";
            d.card_type = CardType::Gear;
            return d;
        }();
        return d;
    }
};

class RadianceKeywordTest : public CardTestFixture {
protected:
    size_t chainSize() { return state.chain.items.size(); }

    // Resolve an engine-owned keyword ability like ChainManager::stepResolve
    // would: run it, answer each pending choice with `pick`, run it again.
    void driveKeyword(const ChainItem& item, EffectExecutor& exec,
                      std::function<Intent(const std::vector<Intent>&)> pick) {
        state.chain.resuming = item;
        Card* card = keywordAbilityCard(item.keyword_ability);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, item.controller, item.source};
        card->onTrigger(ctx, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(pick(pending.legal));
            card->onTrigger(ctx, {});
        }
        state.chain.resuming.reset();
    }
};

// CR 831.1.d.1 / 831.2: one attack trigger using the summed Disarm Value.
TEST_F(RadianceKeywordTest, DisarmValuesSumIntoOneAttackTrigger) {
    auto attacker = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto& a = state.getObject(attacker);
    a.combat_designation = CombatDesignation::Attacker;
    a.aura_disarm_value = 1;   // "Your Mechs have [Disarm]"
    a.temp_disarm_value = 1;   // given [Disarm] this turn
    addUnit(P2, kInvalidId, 4, 0);

    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    const size_t before = chainSize();
    events.emit(CombatStartedEvent{0, P1, P2});

    int disarm_items = 0;
    for (const auto& item : state.chain.items) {
        if (item.keyword_ability != KeywordAbility::Disarm) continue;
        ++disarm_items;
        EXPECT_EQ(item.source, attacker);
        EXPECT_EQ(item.keyword_value, 2);
    }
    EXPECT_EQ(disarm_items, 1);
    EXPECT_GT(chainSize(), before);
}

TEST_F(RadianceKeywordTest, NoDisarmNoTrigger) {
    auto attacker = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(CombatStartedEvent{0, P1, P2});
    for (const auto& item : state.chain.items)
        EXPECT_NE(item.keyword_ability, KeywordAbility::Disarm);
}

// "give an enemy unit HERE -X [M] this turn": only enemies at the attacker's
// battlefield are offered, and the chosen one loses X.
TEST_F(RadianceKeywordTest, DisarmGivesMinusXToAnEnemyHere) {
    auto attacker = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy_here = addUnit(P2, kInvalidId, 5, /*at_bf=*/0);
    auto enemy_away = addUnit(P2, kInvalidId, 5, /*at_bf=*/1);
    auto friend_here = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);

    ChainItem item;
    item.id = state.chain.allocateId();
    item.controller = P1;
    item.source = attacker;
    item.is_ability = true;
    item.keyword_ability = KeywordAbility::Disarm;
    item.keyword_value = 2;

    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<GameObjectId> offered;
    driveKeyword(item, exec, [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            for (auto o : i.chosen_objects) offered.push_back(o);
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == enemy_here) return i;
        return legal.front();
    });

    EXPECT_EQ(state.getObject(enemy_here).current_might, 3);
    EXPECT_EQ(state.getObject(enemy_away).current_might, 5);
    EXPECT_EQ(state.getObject(friend_here).current_might, 2);
    EXPECT_EQ(std::count(offered.begin(), offered.end(), enemy_away), 0);
    EXPECT_EQ(std::count(offered.begin(), offered.end(), friend_here), 0);
}

// CR 830.1.b.1: Deploy gear is played only to battlefields (any of them).
TEST_F(RadianceKeywordTest, DeployGearPlaysOnlyToBattlefields) {
    FakeDeployGear g;
    EXPECT_TRUE(g.hasDeploy());
    EXPECT_TRUE(g.restrictsPlayLocations());
    auto locs = g.getPlayLocations(state, P1);
    ASSERT_EQ(locs.size(), state.battlefields.size());
    for (const auto& l : locs) EXPECT_TRUE(std::holds_alternative<BattlefieldLocation>(l));
}

// CR 830.1.b: "When an opponent holds here, kill this."
TEST_F(RadianceKeywordTest, OpponentHoldingKillsDeployGear) {
    card_registry.registerCard(kFakeDeployGear, std::make_unique<FakeDeployGear>());
    auto gear = state.createObject();
    {
        auto& g = state.getObject(gear);
        g.owner = g.controller = P2;
        g.card_def_id = kFakeDeployGear;
        g.card_type = CardType::Gear;
        g.name = "Fake Deploy Gear";
        g.location = BattlefieldLocation{0};
        g.zone = ZoneType::BattlefieldZone;
    }
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();

    events.emit(ScoreEvent{P2, 0, ScoreMethod::Hold, 1});   // its controller holds: nothing
    for (const auto& item : state.chain.items)
        EXPECT_NE(item.keyword_ability, KeywordAbility::DeployHoldKill);

    events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 1});   // an opponent holds
    const ChainItem* kill = nullptr;
    for (const auto& item : state.chain.items)
        if (item.keyword_ability == KeywordAbility::DeployHoldKill) kill = &item;
    ASSERT_NE(kill, nullptr);
    EXPECT_EQ(kill->source, gear);
    EXPECT_EQ(kill->controller, P2);

    EffectExecutor exec(state, events, card_db, &card_registry);
    driveKeyword(*kill, exec, [](const std::vector<Intent>& l) { return l.front(); });
    EXPECT_FALSE(state.getObject(gear).location.has_value()) << "killed";
}

}  // namespace
}  // namespace riftbound::test
