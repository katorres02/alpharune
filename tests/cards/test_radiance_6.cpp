/// @file test_radiance_6.cpp
/// Radiance batch 6 (ids 1083-1100) + reprint text updates (Aphelios,
/// Exalted; Viktor, Innovator).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kGoodStuff = 1083, kBushwhack = 1084, kEvelynn = 1085, kLulu = 1086,
                    kBandit = 1087, kZiggs = 1088, kAngler = 1089, kRivenD = 1090,
                    kTranspos = 1091, kPoster = 1092, kForDemacia = 1093, kHerald = 1094,
                    kHuntingDog = 1095, kRestorer = 1096, kOneTwo = 1097, kPartyCrasher = 1098,
                    kProtShield = 1099, kBurnAway = 1100;

Intent pickObj(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
    ADD_FAILURE() << "object " << id << " not offered";
    return legal.front();
}
Intent pickVal(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
    ADD_FAILURE() << "value " << v << " not offered";
    return legal.front();
}
bool offered(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return true;
    return false;
}

class Radiance6 : public CardTestFixture {
protected:
    EffectExecutor exec{state, events, card_db, &card_registry};

    GameObjectId addGear(PlayerId owner, CardDefId def, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_def_id = def;
        g.card_type = CardType::Gear;
        if (def != kInvalidId) {
            g.name = card_db.get(def).name;
            g.might_bonus = card_db.get(def).might_bonus;
        } else {
            g.name = "TestGear";
        }
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def) {
        auto id = addToHand(owner, def);
        auto& h = state.player(owner).hand;
        h.erase(std::find(h.begin(), h.end(), id));
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    // Run an activated ability like ChainManager::stepResolve would.
    void driveActivate(CardDefId def, PlayerId controller, GameObjectId source,
                       std::function<Intent(const std::vector<Intent>&)> picker) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.source = source;
        ri.card_def_id = def;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onActivate(ctx, 0, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onActivate(ctx, 0, {});
        }
        state.chain.resuming.reset();
    }
    const ChainItem* chainItemFrom(GameObjectId source, TriggerType t) {
        for (const auto& item : state.chain.items)
            if (item.source == source && item.fired_trigger == t) return &item;
        return nullptr;
    }
};

// ── 1083 The Good Stuff ─────────────────────────────────────────────────────
TEST_F(Radiance6, GoodStuffPlayDiscardsOneThenDrawsThree) {
    auto junk = addToHand(P1, kHerald);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kHerald);
    auto gear = addGear(P1, kGoodStuff, 0);
    driveResumableTrigger(kGoodStuff, P1, gear,
                          [&](const std::vector<Intent>& l) { return pickObj(l, junk); }, exec,
                          TriggerType::WhenYouPlayThis);
    EXPECT_TRUE(inTrash(P1, junk));
    EXPECT_EQ(handSize(P1), 3);
}

TEST_F(Radiance6, GoodStuffReturnsToHandWhenYouHoldHere) {
    auto gear = addGear(P1, kGoodStuff, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ScoreEvent{P1, 1, ScoreMethod::Hold, 1});   // another battlefield
    EXPECT_EQ(chainItemFrom(gear, TriggerType::WhenYouHoldHere), nullptr);
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 1});
    ASSERT_NE(chainItemFrom(gear, TriggerType::WhenYouHoldHere), nullptr);
    fireTriggerAs(kGoodStuff, P1, gear, TriggerType::WhenYouHoldHere, exec);
    EXPECT_TRUE(inHand(P1, gear));
}

// ── 1084 Bushwhack Trap ─────────────────────────────────────────────────────
TEST_F(Radiance6, BushwhackTrapAddsAFacedownSlotWhileYouControlTheBattlefield) {
    addGear(P1, kBushwhack, 0);
    const Card* c = card_registry.get(kBushwhack);
    ASSERT_TRUE(c->hasDeploy());
    state.battlefields[0].controller = P2;
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.battlefields[0].facedown_max_occupancy, 1);
    for (auto& [id, o] : state.objects) o.aura_effects.clear();
    state.battlefields[0].controller = P1;
    c->applyPassiveAura(state, P1);
    c->applyPassiveAura(state, P1);   // called once per instance: still +1 total
    EXPECT_EQ(state.battlefields[0].facedown_max_occupancy, 2);
    EXPECT_EQ(state.battlefields[1].facedown_max_occupancy, 1);
}

// ── 1085 Evelynn, In Control ────────────────────────────────────────────────
TEST_F(Radiance6, EvelynnScoresOnHoldWithAFacedownCardHere) {
    const Card* c = card_registry.get(kEvelynn);
    EXPECT_EQ(c->disarmValue(), 1);
    auto eve = addUnit(P1, kEvelynn, 0, 0);
    EXPECT_FALSE(c->triggerCondition(state, eve, P1, TriggerType::WhenIHold, kInvalidId));
    auto hidden = addToHand(P1, kHerald);
    state.player(P1).hand.clear();
    state.getObject(hidden).is_hidden = true;
    state.battlefields[0].facedown.push_back(hidden);
    EXPECT_TRUE(c->triggerCondition(state, eve, P1, TriggerType::WhenIHold, kInvalidId));
    fireTriggerAs(kEvelynn, P1, eve, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.player(P1).score, 1);
}

// ── 1086 Lulu, Whimsical ────────────────────────────────────────────────────
TEST_F(Radiance6, LuluSetsAnotherUnitsBaseMightToFour) {
    auto lulu = addUnit(P1, kLulu, 0, 0);
    auto big = addUnit(P2, kInvalidId, 7, 0);
    bool lulu_offered = false;
    driveResumableTrigger(kLulu, P1, lulu, [&](const std::vector<Intent>& l) {
        if (l.front().chosen_value.has_value()) return pickVal(l, 1);
        lulu_offered = offered(l, lulu);
        return pickObj(l, big);
    }, exec, TriggerType::WhenIMove);
    EXPECT_FALSE(lulu_offered);
    EXPECT_EQ(state.getObject(big).current_might, 4);
}

// ── 1087 Mechanical Bandit ──────────────────────────────────────────────────
TEST_F(Radiance6, MechanicalBanditDrawsYourLowestAndRecyclesTheRest) {
    auto bandit = addGear(P1, kBandit);
    auto p1_bottom = addToDeck(P1, kAngler);
    auto mine = addToDeck(P1, kHerald);      // cost 5 on top
    auto theirs = addToDeck(P2, kAngler);    // cost 8 on top
    addToDeck(P2, kHerald);
    std::swap(state.player(P2).main_deck[0], state.player(P2).main_deck[1]);
    ASSERT_EQ(state.player(P2).main_deck.back(), theirs);
    fireTriggerAs(kBandit, P1, bandit, TriggerType::AtStartOfBeginning, exec);
    EXPECT_TRUE(inHand(P1, mine));
    EXPECT_EQ(state.player(P1).main_deck.back(), p1_bottom);
    EXPECT_EQ(state.player(P2).main_deck.front(), theirs) << "recycled to the bottom";
    EXPECT_EQ(deckSize(P2), 2);
}

TEST_F(Radiance6, MechanicalBanditNoDrawWhenOpponentRevealsLower) {
    auto bandit = addGear(P1, kBandit);
    auto mine = addToDeck(P1, kAngler);    // 8
    addToDeck(P2, kHerald);                // 5
    fireTriggerAs(kBandit, P1, bandit, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_TRUE(inDeck(P1, mine));
    EXPECT_EQ(deckSize(P1), 1);
}

// ── 1088 Ziggs, Explosive ───────────────────────────────────────────────────
TEST_F(Radiance6, ZiggsActivationPlaysABombToTheChosenBattlefield) {
    auto ziggs = addUnit(P1, kZiggs);
    auto ab = card_registry.get(kZiggs)->activatedAbilities();
    ASSERT_EQ(ab.size(), 1u);
    EXPECT_TRUE(ab[0].cost.exhaust);
    EXPECT_EQ(ab[0].cost.energy, 1);
    driveActivate(kZiggs, P1, ziggs, [](const std::vector<Intent>& l) { return pickVal(l, 1); });
    int bombs = 0;
    for (auto& [id, o] : state.objects)
        if (o.card_def_id == kBombTokenCardDefId) {
            ++bombs;
            ASSERT_TRUE(o.battlefieldId().has_value());
            EXPECT_EQ(*o.battlefieldId(), 1u);
        }
    EXPECT_EQ(bombs, 1);
}

TEST_F(Radiance6, ZiggsDefendKillsAFriendlyGearForPlusTwo) {
    auto ziggs = addUnit(P1, kZiggs, 0, 0);
    auto gear = addGear(P1, kInvalidId);
    driveResumableTrigger(kZiggs, P1, ziggs, [&](const std::vector<Intent>& l) {
        if (l.front().chosen_value.has_value()) return pickVal(l, 1);
        return pickObj(l, gear);
    }, exec, TriggerType::WhenIDefend);
    EXPECT_FALSE(state.getObject(gear).location.has_value());
    EXPECT_EQ(state.getObject(ziggs).current_might, 6);
}

TEST_F(Radiance6, ZiggsDefendDeclined) {
    auto ziggs = addUnit(P1, kZiggs, 0, 0);
    auto gear = addGear(P1, kInvalidId);
    driveResumableTrigger(kZiggs, P1, ziggs,
                          [&](const std::vector<Intent>& l) { return pickVal(l, 0); }, exec,
                          TriggerType::WhenIDefend);
    EXPECT_TRUE(state.getObject(gear).location.has_value());
    EXPECT_EQ(state.getObject(ziggs).current_might, 4);
}

// ── 1089 Angler Wyrm ────────────────────────────────────────────────────────
TEST_F(Radiance6, AnglerWyrmOpponentMovesAUnitToItsBattlefield) {
    EXPECT_TRUE(card_db.get(kAngler).keywords.has(Keyword::Deflect));
    auto wyrm = addUnit(P1, kAngler, 0, 1);
    auto a = addUnit(P2, kInvalidId, 2);      // base
    auto b = addUnit(P2, kInvalidId, 3, 0);   // other battlefield
    auto already = addUnit(P2, kInvalidId, 3, 1);
    PlayerId chooser = PlayerId::None;
    bool already_offered = true;
    driveActivate(kAngler, P1, wyrm, [&](const std::vector<Intent>& l) {
        chooser = l.front().player;
        already_offered = offered(l, already);
        return pickObj(l, b);
    });
    EXPECT_EQ(chooser, P2);
    EXPECT_FALSE(already_offered);
    ASSERT_TRUE(state.getObject(b).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(b).battlefieldId(), 1u);
    EXPECT_TRUE(state.getObject(a).isAtBase());
}

// ── 1090 Riven, Deserter ────────────────────────────────────────────────────
TEST_F(Radiance6, RivenDeserterGrowsWhenAnotherFriendlyUnitExhausts) {
    auto riven = addUnit(P1, kRivenD, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ObjectStateChangedEvent{enemy, "exhausted"});
    events.emit(ObjectStateChangedEvent{riven, "exhausted"});
    EXPECT_EQ(chainItemFrom(riven, TriggerType::WhenAnotherFriendlyUnitBecomesExhausted), nullptr);
    events.emit(ObjectStateChangedEvent{other, "exhausted"});
    ASSERT_NE(chainItemFrom(riven, TriggerType::WhenAnotherFriendlyUnitBecomesExhausted), nullptr);
    fireTriggerAs(kRivenD, P1, riven, TriggerType::WhenAnotherFriendlyUnitBecomesExhausted, exec);
    EXPECT_EQ(state.getObject(riven).current_might, 6);
}

TEST_F(Radiance6, RivenDeserterExhaustsAFriendlyUnitToReadyAUnitOncePerTurn) {
    const Card* c = card_registry.get(kRivenD);
    auto riven = addUnit(P1, kRivenD, 0, 0);
    auto fodder = addUnit(P1, kInvalidId, 2, 0);
    auto tired = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(tired).is_exhausted = true;
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, riven, 0));
    bool tired_offered_as_cost = true;
    int step = 0;
    driveActivate(kRivenD, P1, riven, [&](const std::vector<Intent>& l) {
        if (step++ == 0) {
            tired_offered_as_cost = offered(l, tired);
            return pickObj(l, fodder);
        }
        return pickObj(l, tired);
    });
    EXPECT_FALSE(tired_offered_as_cost);
    EXPECT_TRUE(state.getObject(fodder).is_exhausted);
    EXPECT_FALSE(state.getObject(tired).is_exhausted);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, riven, 0)) << "once each turn";
    state.turn.turn_number += 1;
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, riven, 0));
}

// ── 1091 Transpositionist ───────────────────────────────────────────────────
TEST_F(Radiance6, TranspositionistReturnsAUnitAndPlaysASmallerOneFree) {
    auto ab = card_registry.get(kTranspos)->activatedAbilities();
    ASSERT_EQ(ab.size(), 1u);
    EXPECT_TRUE(ab[0].cost.exhaust);
    EXPECT_EQ(ab[0].cost.power, 1);
    EXPECT_EQ(ab[0].cost.power_domain, Domain::Chaos);

    auto tp = addUnit(P1, kTranspos);
    auto herald = addUnit(P1, kHerald, 0, 0);      // printed 3
    state.getObject(herald).temp_might_bonus = 2;   // has 5 now
    state.getObject(herald).recomputeMight();
    auto small = addToHand(P1, kLulu);              // 3 < 5
    auto big = addToHand(P1, kAngler);              // 8
    bool big_offered = true;
    int step = 0;
    driveActivate(kTranspos, P1, tp, [&](const std::vector<Intent>& l) {
        if (step++ == 0) return pickObj(l, herald);
        big_offered = offered(l, big);
        return pickObj(l, small);
    });
    EXPECT_TRUE(inHand(P1, herald));
    EXPECT_FALSE(big_offered);
    EXPECT_FALSE(inHand(P1, small));
    EXPECT_TRUE(state.getObject(small).location.has_value());
}

TEST_F(Radiance6, TranspositionistMayDeclineToPlay) {
    auto tp = addUnit(P1, kTranspos);
    auto herald = addUnit(P1, kHerald, 0, 0);
    auto small = addToHand(P1, kRestorer);   // 2 < 3
    int step = 0;
    driveActivate(kTranspos, P1, tp, [&](const std::vector<Intent>& l) {
        if (step++ == 0) return pickObj(l, herald);
        return pickVal(l, -1);
    });
    EXPECT_TRUE(inHand(P1, herald));
    EXPECT_TRUE(inHand(P1, small));
}

// ── 1092 Concert Poster ─────────────────────────────────────────────────────
TEST_F(Radiance6, ConcertPosterPaysOneForARecruit) {
    auto poster = addGear(P1, kPoster, 0);
    auto rune = addRune(P1, Domain::Order);
    driveResumableTrigger(kPoster, P1, poster,
                          [](const std::vector<Intent>& l) { return pickVal(l, 1); }, exec,
                          TriggerType::WhenYouPlayThis);
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
    int recruits = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.name == "Recruit" && o.isAtBase() && o.controller == P1) ++recruits;
    EXPECT_EQ(recruits, 1);
}

TEST_F(Radiance6, ConcertPosterTriggersWhenYouConquerHere) {
    auto poster = addGear(P1, kPoster, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ScoreEvent{P2, 0, ScoreMethod::Conquer, 1});   // opponent conquers
    EXPECT_EQ(chainItemFrom(poster, TriggerType::WhenYouConquerHere), nullptr);
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Conquer, 1});
    EXPECT_NE(chainItemFrom(poster, TriggerType::WhenYouConquerHere), nullptr);
}

// ── 1093 For Demacia! ───────────────────────────────────────────────────────
TEST_F(Radiance6, ForDemaciaGivesShieldTwoToFriendlyUnitsAtTheChosenBattlefield) {
    auto here = addUnit(P1, kInvalidId, 2, 1);
    auto there = addUnit(P1, kInvalidId, 2, 0);
    auto foe = addUnit(P2, kInvalidId, 2, 1);
    auto spell = addToHand(P1, kForDemacia);
    driveResumable(kForDemacia, P1, spell, [](const std::vector<Intent>& l) { return pickVal(l, 1); },
                   exec);
    EXPECT_EQ(state.getObject(here).shield_value, 2);
    EXPECT_EQ(state.getObject(there).shield_value, 0);
    EXPECT_EQ(state.getObject(foe).shield_value, 0);
    state.getObject(here).combat_designation = CombatDesignation::Defender;
    state.getObject(here).recomputeMight();
    EXPECT_EQ(state.getObject(here).current_might, 4);
}

// ── 1094 Herald of the Emperor ──────────────────────────────────────────────
TEST_F(Radiance6, HeraldSetsAFriendlyUnitsBaseMightToFive) {
    auto herald = addUnit(P1, kHerald, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(ally).buff_count = 1;
    state.getObject(ally).recomputeMight();
    auto foe = addUnit(P2, kInvalidId, 2, 0);
    bool foe_offered = true;
    driveResumableTrigger(kHerald, P1, herald, [&](const std::vector<Intent>& l) {
        foe_offered = offered(l, foe);
        return pickObj(l, ally);
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(foe_offered);
    EXPECT_EQ(state.getObject(ally).current_might, 6) << "base 5 + buff";
    // A second "becomes" this turn replaces the first rather than stacking.
    setBaseMightThisTurn(state, ally, 4);
    EXPECT_EQ(state.getObject(ally).current_might, 5);
}

// ── 1095 Hunting Dog ────────────────────────────────────────────────────────
TEST_F(Radiance6, HuntingDogDeathknellMakesADeflectBird) {
    EXPECT_TRUE(card_db.get(kHuntingDog).keywords.has(Keyword::Deathknell));
    auto dog = addUnit(P1, kHuntingDog, 0, 0);
    fireTriggerAs(kHuntingDog, P1, dog, TriggerType::WhenIDie, exec);
    int birds = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.name == "Bird") {
            ++birds;
            EXPECT_TRUE(o.isAtBase());
            EXPECT_TRUE(o.hasKeyword(Keyword::Deflect));
            EXPECT_EQ(o.base_might, 1);
        }
    EXPECT_EQ(birds, 1);
}

// ── 1096 Meticulous Restorationist ──────────────────────────────────────────
TEST_F(Radiance6, RestorationistTriggersFromTrashOnSevenPlusCostCard) {
    auto resto = addToTrash(P1, kRestorer);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto cheap = addUnit(P1, kHerald);   // cost 5
    events.emit(CardPlayedEvent{cheap, P1, CardType::Unit, 1, 5});
    auto big = addUnit(P1, kAngler);     // cost 8
    events.emit(CardPlayedEvent{big, P1, CardType::Unit, 2, 8});
    const ChainItem* item = nullptr;
    int items = 0;
    for (const auto& ci : state.chain.items)
        if (ci.source == resto) { item = &ci; ++items; }
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(items, 1) << "only the [7]+ card triggers it (checked as it is played)";
    EXPECT_EQ(item->triggering_subject, big);

    // Resolve it: play me for [Y].
    auto rune = addRune(P1, Domain::Order);
    driveResumableTrigger(kRestorer, P1, resto,
                          [](const std::vector<Intent>& l) { return pickVal(l, 1); }, exec,
                          TriggerType::WhenYouPlayACard, big);
    EXPECT_FALSE(inTrash(P1, resto));
    EXPECT_TRUE(state.getObject(resto).location.has_value());
    EXPECT_FALSE(state.getObject(rune).location.has_value()) << "Order rune recycled for [Y]";
}

TEST_F(Radiance6, RestorationistOnBoardDoesNotTrigger) {
    auto resto = addUnit(P1, kRestorer);
    EXPECT_FALSE(card_registry.get(kRestorer)->triggerCondition(
        state, resto, P1, TriggerType::WhenYouPlayACard, kInvalidId));
}

// ── 1097 One-Two Punch ──────────────────────────────────────────────────────
TEST_F(Radiance6, OneTwoPunchBuffsAFriendAndStunsAnEnemy) {
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto foe = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kOneTwo);
    EXPECT_TRUE(card_registry.get(kOneTwo)->hasLegalTargets(state, P1));
    int step = 0;
    driveResumable(kOneTwo, P1, spell, [&](const std::vector<Intent>& l) {
        return pickObj(l, step++ == 0 ? ally : foe);
    }, exec);
    EXPECT_EQ(state.getObject(ally).buff_count, 1);
    EXPECT_TRUE(state.getObject(foe).is_stunned);
}

TEST_F(Radiance6, OneTwoPunchNeedsBothTargets) {
    addUnit(P1, kInvalidId, 2, 0);
    EXPECT_FALSE(card_registry.get(kOneTwo)->hasLegalTargets(state, P1));
}

// ── 1098 Party Crasher ──────────────────────────────────────────────────────
TEST_F(Radiance6, PartyCrasherCostsOneLessPerExhaustedFriendlyUnit) {
    const Card* c = card_registry.get(kPartyCrasher);
    EXPECT_TRUE(card_db.get(kPartyCrasher).keywords.has(Keyword::Tank));
    auto a = addUnit(P1, kInvalidId, 1, 0);
    auto b = addUnit(P1, kInvalidId, 1);
    addUnit(P1, kInvalidId, 1);
    auto e = addUnit(P2, kInvalidId, 1);
    state.getObject(a).is_exhausted = true;
    state.getObject(b).is_exhausted = true;
    state.getObject(e).is_exhausted = true;
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
}

// ── 1099 Protector's Shield ─────────────────────────────────────────────────
TEST_F(Radiance6, ProtectorsShieldEquipsForOneAndGivesTank) {
    const auto& def = card_db.get(kProtShield);
    EXPECT_TRUE(def.keywords.has(Keyword::QuickDraw));
    EXPECT_TRUE(def.keywords.has(Keyword::Reaction));
    Card* c = card_registry.get(kProtShield);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Tank));
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto shield = addGear(P1, kProtShield);
    CardContext ctx{state, events, exec, P1, shield};
    EXPECT_FALSE(c->onEquip(ctx, unit)) << "no energy";
    auto rune = addRune(P1, Domain::Fury);
    EXPECT_TRUE(c->onEquip(ctx, unit));
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
    EXPECT_EQ(state.getObject(unit).current_might, 3);
}

// ── 1100 Burn Away ──────────────────────────────────────────────────────────
TEST_F(Radiance6, BurnAwayBanishesASmallUnitOrGearThenBurnsTwo) {
    auto small = addUnit(P2, kInvalidId, 3, 0);
    auto large = addUnit(P2, kInvalidId, 4, 0);
    auto gear = addGear(P2, kInvalidId);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kHerald);
    auto spell = addToHand(P1, kBurnAway);
    bool large_offered = true, small_offered = false;
    driveResumable(kBurnAway, P1, spell, [&](const std::vector<Intent>& l) {
        large_offered = offered(l, large);
        small_offered = offered(l, small);
        return pickObj(l, gear);
    }, exec);
    EXPECT_FALSE(large_offered);
    EXPECT_TRUE(small_offered);
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Banishment);
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_EQ(trashSize(P1), 2);
}

// ── Domain corrections from the printed cards ───────────────────────────────
TEST_F(Radiance6, DefsMatchPrintedCards) {
    EXPECT_EQ(card_db.get(kGoodStuff).domains, std::vector<Domain>{Domain::Chaos});
    EXPECT_EQ(card_db.get(kBushwhack).domains, std::vector<Domain>{Domain::Chaos});
    EXPECT_EQ(card_db.get(kBandit).domains, std::vector<Domain>{Domain::Chaos});
    EXPECT_EQ(card_db.get(kOneTwo).domains, std::vector<Domain>{Domain::Order});
    EXPECT_EQ(card_db.get(kProtShield).domains, std::vector<Domain>{Domain::Order});
    EXPECT_EQ(card_db.get(kBurnAway).domains, std::vector<Domain>{Domain::Order});
    EXPECT_EQ(card_db.get(kProtShield).might_bonus, 1);
    EXPECT_EQ(card_db.get(kZiggs).def_id, "rad-112-167");
}

// ── Reprints: newest printing's text ────────────────────────────────────────
TEST_F(Radiance6, ApheliosReprintTextAndBuffMode) {
    constexpr CardDefId kAphelios = 372;
    EXPECT_NE(card_db.get(kAphelios).ability_text.find("[Buff] a friendly unit"), std::string::npos);
    auto aph = addUnit(P1, kAphelios, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    driveResumableTrigger(kAphelios, P1, aph, [&](const std::vector<Intent>& l) {
        if (l.front().chosen_value.has_value()) return pickVal(l, 2);
        return pickObj(l, ally);
    }, exec, TriggerType::WhenEquipmentAttachedToMe);
    EXPECT_EQ(state.getObject(ally).buff_count, 1);
}

TEST_F(Radiance6, ViktorReprintTextAndOffTurnRecruit) {
    constexpr CardDefId kViktor = 117;
    EXPECT_NE(card_db.get(kViktor).ability_text.find("Recruit unit token to your base"),
              std::string::npos);
    auto vik = addUnit(P1, kViktor);
    state.turn.turn_player = P2;
    fireTriggerAs(kViktor, P1, vik, TriggerType::WhenYouPlayASpell, exec);
    int recruits = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.name == "Recruit" && o.isAtBase() && o.controller == P1) ++recruits;
    EXPECT_EQ(recruits, 1);
    state.turn.turn_player = P1;
    fireTriggerAs(kViktor, P1, vik, TriggerType::WhenYouPlayASpell, exec);
    recruits = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.name == "Recruit") ++recruits;
    EXPECT_EQ(recruits, 1) << "not on your own turn";
}

}  // namespace
}  // namespace riftbound::test
