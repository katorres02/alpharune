/// @file test_radiance_5.cpp
/// Radiance batch 5: RAD-084 .. RAD-104 (ids 1064-1082).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kCataclysm = 1064;
constexpr CardDefId kGraves = 1065;
constexpr CardDefId kKSante = 1066;
constexpr CardDefId kPortablePortal = 1067;
constexpr CardDefId kStaunchSurvivor = 1068;
constexpr CardDefId kTitanicShrine = 1069;
constexpr CardDefId kEvelynn = 1070;
constexpr CardDefId kJarvan = 1071;
constexpr CardDefId kWyvern = 1072;
constexpr CardDefId kDisposalExpert = 1073;
constexpr CardDefId kExplosimian = 1074;
constexpr CardDefId kMomentaryCompanion = 1075;
constexpr CardDefId kSatchelCharge = 1076;
constexpr CardDefId kSirensSong = 1077;
constexpr CardDefId kSuaveNegotiator = 1078;
constexpr CardDefId kWatersWage = 1079;
constexpr CardDefId kDemonShade = 1080;
constexpr CardDefId kGutterGuardian = 1081;
constexpr CardDefId kPitilessStalker = 1082;

class Radiance5Test : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            g.name = d.name;
            g.keywords = d.keywords;
            g.tags = d.tags;
            g.domains = d.domains;
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
    GameObjectId addRuneToDeck(PlayerId owner, Domain d = Domain::Body) {
        auto id = state.createObject();
        auto& r = state.getObject(id);
        r.owner = r.controller = owner;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::RuneDeck;
        state.player(owner).rune_deck.push_back(id);
        return id;
    }
    int runesInBase(PlayerId p, bool exhausted) {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.isRune() && o.controller == p && o.isAtBase() && o.is_exhausted == exhausted) ++n;
        return n;
    }
    int countAt(CardDefId def, PlayerId p, int bf) {
        int n = 0;
        for (auto& [id, o] : state.objects) {
            if (o.card_def_id != def || o.controller != p) continue;
            auto b = o.battlefieldId();
            if (b && static_cast<int>(*b) == bf) ++n;
        }
        return n;
    }
    static Intent pickValue(const std::vector<Intent>& legal, int v) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
        return legal.front();
    }
    static Intent pickObject(const std::vector<Intent>& legal, GameObjectId id) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
        return legal.front();
    }
    Card* card(CardDefId id) { return card_registry.get(id); }
};

// ── Cataclysm ──
TEST_F(Radiance5Test, CataclysmLocksOpponentSpellsAndMovesToOccupiedEnemyBattlefield) {
    auto mine = addUnit(P1, kInvalidId, 3);                 // at base
    addUnit(P2, kInvalidId, 2, /*at_bf=*/1);
    auto spell = pushSpellOnChain(P1, kCataclysm);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kCataclysm, P1, spell, [&](const std::vector<Intent>& legal) {
        if (!legal.front().chosen_objects.empty()) return pickObject(legal, mine);
        return pickValue(legal, 1 + 1);
    }, exec);
    EXPECT_TRUE(state.player(P2).cant_play_spells_this_turn);
    EXPECT_FALSE(state.player(P1).cant_play_spells_this_turn);
    ASSERT_TRUE(state.getObject(mine).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(mine).battlefieldId(), 1u);
}

TEST_F(Radiance5Test, CataclysmNeedsAnOccupiedEnemyBattlefieldElsewhere) {
    auto there = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);
    addUnit(P2, kInvalidId, 2, /*at_bf=*/1);
    auto legal = card(kCataclysm)->enumerateLegalTargets(state, P1);
    EXPECT_TRUE(std::find(legal.begin(), legal.end(), there) == legal.end());
    auto home = addUnit(P1, kInvalidId, 3);
    legal = card(kCataclysm)->enumerateLegalTargets(state, P1);
    EXPECT_TRUE(std::find(legal.begin(), legal.end(), home) != legal.end());
}

// ── Graves, Blasting Through ──
TEST_F(Radiance5Test, GravesDealsHisMightToAllEnemiesHereAndDealsNoCombatDamage) {
    auto graves = addUnit(P1, kGraves, 0, /*at_bf=*/0);
    auto e1 = addUnit(P2, kInvalidId, 6, 0);
    auto e2 = addUnit(P2, kInvalidId, 6, 0);
    auto away = addUnit(P2, kInvalidId, 6, 1);
    auto ally = addUnit(P1, kInvalidId, 6, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kGraves, P1, graves, TriggerType::WhenIAttackOrDefend, exec);
    EXPECT_EQ(state.getObject(e1).damage_marked, 4);
    EXPECT_EQ(state.getObject(e2).damage_marked, 4);
    EXPECT_EQ(state.getObject(away).damage_marked, 0);
    EXPECT_EQ(state.getObject(ally).damage_marked, 0);

    card(kGraves)->applyPassiveAura(state, P1);
    bool suppressed = false;
    for (const auto& ae : state.getObject(graves).aura_effects)
        if (ae.suppress_combat_damage) suppressed = true;
    EXPECT_TRUE(suppressed);
}

// ── K'Sante, Courageous ──
TEST_F(Radiance5Test, KSanteKeywordsAndHoldChannels) {
    const auto& d = card_db.get(kKSante);
    EXPECT_TRUE(d.keywords.has(Keyword::Accelerate));
    EXPECT_TRUE(d.keywords.has(Keyword::Shield));
    EXPECT_EQ(d.shield_value, 2);
    auto k = addUnit(P1, kKSante, 0, 0);
    addRuneToDeck(P1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kKSante, P1, k, TriggerType::WhenIHold, exec);
    EXPECT_EQ(runesInBase(P1, /*exhausted=*/false), 1);
}

// ── Portable Portal ──
TEST_F(Radiance5Test, PortablePortalPaysBodyToReadyAUnitHere) {
    auto portal = addGear(P1, kPortablePortal, 0);
    auto u = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(u).is_exhausted = true;
    addRune(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumableTrigger(kPortablePortal, P1, portal, [&](const std::vector<Intent>& legal) {
        if (call++ == 0) return legal.back();                // "yes"
        return pickObject(legal, u);
    }, exec, TriggerType::WhenYouPlayThis);
    EXPECT_FALSE(state.getObject(u).is_exhausted);
    EXPECT_EQ(runesInBase(P1, false) + runesInBase(P1, true), 0) << "[O] paid by recycling";
}

TEST_F(Radiance5Test, PortablePortalDeclinedDoesNothing) {
    auto portal = addGear(P1, kPortablePortal, 0);
    auto u = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(u).is_exhausted = true;
    addRune(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kPortablePortal, P1, portal,
                          [](const std::vector<Intent>& legal) { return legal.front(); },
                          exec, TriggerType::WhenYouPlayThis);
    EXPECT_TRUE(state.getObject(u).is_exhausted);
    EXPECT_EQ(runesInBase(P1, false), 1);
}

TEST_F(Radiance5Test, PortablePortalGivesFriendlyUnitsHereGanking) {
    EXPECT_TRUE(card(kPortablePortal)->hasDeploy());
    auto portal = addGear(P1, kPortablePortal, 0);
    auto here = addUnit(P1, kInvalidId, 3, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 3, 1);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    card(kPortablePortal)->applyPassiveAura(state, P1);
    auto granted = [&](GameObjectId id) {
        for (const auto& ae : state.getObject(id).aura_effects)
            if (ae.source == portal && ae.keyword == Keyword::Ganking) return true;
        return false;
    };
    EXPECT_TRUE(granted(here));
    EXPECT_FALSE(granted(elsewhere));
    EXPECT_FALSE(granted(enemy));
}

// ── Staunch Survivor ──
TEST_F(Radiance5Test, StaunchSurvivorDef) {
    const auto& d = card_db.get(kStaunchSurvivor);
    EXPECT_EQ(d.name, "Staunch Survivor");
    EXPECT_TRUE(d.keywords.has(Keyword::Assault));
    EXPECT_TRUE(d.keywords.has(Keyword::Shield));
    EXPECT_EQ(d.assault_value, 2);
    EXPECT_EQ(d.shield_value, 2);
    EXPECT_EQ(d.might, 2);
}

// ── Titanic Shrine ──
TEST_F(Radiance5Test, TitanicShrineTriggersOnlyForTheBiggestUnit) {
    auto shrine = addGear(P1, kTitanicShrine);
    addUnit(P2, kInvalidId, 4, 0);
    auto big = addUnit(P1, kInvalidId, 5, 1);
    auto tie = addUnit(P1, kInvalidId, 4, 1);
    EXPECT_TRUE(card(kTitanicShrine)->triggerCondition(state, shrine, P1,
                                                       TriggerType::WhenYouPlayAUnit, big));
    EXPECT_FALSE(card(kTitanicShrine)->triggerCondition(state, shrine, P1,
                                                        TriggerType::WhenYouPlayAUnit, tie));
}

TEST_F(Radiance5Test, TitanicShrineExhaustsToDraw) {
    auto shrine = addGear(P1, kTitanicShrine);
    addToDeck(P1, kStaunchSurvivor);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kTitanicShrine, P1, shrine,
                          [](const std::vector<Intent>& legal) { return legal.back(); },
                          exec, TriggerType::WhenYouPlayAUnit);
    EXPECT_TRUE(state.getObject(shrine).is_exhausted);
    EXPECT_EQ(handSize(P1), 1);
}

// ── Evelynn, Consuming ──
TEST_F(Radiance5Test, EvelynnReadiesAndGrowsWhenACardIsPlayedFromFaceDown) {
    EXPECT_TRUE(card_db.get(kEvelynn).keywords.has(Keyword::Ambush));
    auto ev = addUnit(P1, kEvelynn, 0, 0);
    state.getObject(ev).is_exhausted = true;
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kEvelynn, P1, ev, TriggerType::WhenYouPlayFromFacedown, exec);
    EXPECT_FALSE(state.getObject(ev).is_exhausted);
    EXPECT_EQ(state.getObject(ev).current_might, 6);
}

// ── Jarvan IV, Martial Paragon ──
TEST_F(Radiance5Test, JarvanConquerChannelsExhaustedWithDeployGearHere) {
    auto j = addUnit(P1, kJarvan, 0, 0);
    addRuneToDeck(P1);
    addRuneToDeck(P1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, j};
    ctx.registry = &card_registry;
    card(kJarvan)->onTrigger(ctx, {});
    EXPECT_EQ(runesInBase(P1, true) + runesInBase(P1, false), 0) << "no Deploy gear here";

    addGear(P1, kPortablePortal, 0);
    card(kJarvan)->onTrigger(ctx, {});
    EXPECT_EQ(runesInBase(P1, true), 1);
    EXPECT_EQ(runesInBase(P1, false), 0);
}

TEST_F(Radiance5Test, JarvanLetsDeployGearBePlayedInShowdowns) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase = TurnPhase::MainPhase;
    s.turn.ns_state = NeutralShowdownState::Showdown;
    s.turn.oc_state = OpenClosedState::Open;
    for (int i = 0; i < 2; ++i) { BattlefieldState b; b.id = i; s.battlefields.push_back(b); }
    auto mk = [&](CardDefId def, CardType t) {
        auto id = s.createObject();
        auto& o = s.getObject(id);
        o.owner = o.controller = P1;
        o.card_def_id = def;
        o.card_type = t;
        if (def != kInvalidId) o.name = card_db.get(def).name;
        return id;
    };
    for (int i = 0; i < 3; ++i) {
        auto r = mk(kInvalidId, CardType::Rune);
        auto& o = s.getObject(r);
        o.domains = {Domain::Body};
        o.zone = ZoneType::Base;
        o.location = BaseLocation{P1};
    }
    auto portal = mk(kPortablePortal, CardType::Gear);
    s.getObject(portal).zone = ZoneType::Hand;
    s.player(P1).hand.push_back(portal);

    auto offered = [&]() {
        int n = 0;
        for (const auto& a : engine.generateLegalActions())
            if (a.card == portal && a.type == IntentType::PlayActionCard) ++n;
        return n;
    };
    EXPECT_EQ(offered(), 0) << "no Jarvan: gear has no [Action]";

    auto jarvan = mk(kJarvan, CardType::Unit);
    s.getObject(jarvan).zone = ZoneType::BattlefieldZone;
    s.getObject(jarvan).location = BattlefieldLocation{0};
    s.player(P1).deploy_gear_have_action = false;
    card(kJarvan)->applyPassiveAura(s, P1);
    EXPECT_TRUE(s.player(P1).deploy_gear_have_action);
    EXPECT_EQ(offered(), 2) << "one play per battlefield";
}

// ── Seraphic Wyvern ──
TEST_F(Radiance5Test, SeraphicWyvernAddsOneExhaustedRunePerChannel) {
    for (int i = 0; i < 5; ++i) addRuneToDeck(P1);
    addUnit(P1, kWyvern, 0, 0);
    card(kWyvern)->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).channel_extra_exhausted, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.channelRunes(P1, 2);
    EXPECT_EQ(runesInBase(P1, false), 2);
    EXPECT_EQ(runesInBase(P1, true), 1);
    // The opponent is unaffected.
    addRuneToDeck(P2);
    addRuneToDeck(P2);
    exec.channelRunes(P2, 1);
    EXPECT_EQ(runesInBase(P2, false), 1);
    EXPECT_EQ(runesInBase(P2, true), 0);
}

// ── Disposal Expert ──
TEST_F(Radiance5Test, DisposalExpertEntersReadyOnlyWhenAlone) {
    EXPECT_EQ(card(kDisposalExpert)->disarmValue(), 2);
    auto me = addUnit(P1, kDisposalExpert, 0, 0);
    addUnit(P2, kInvalidId, 3, 0);
    EXPECT_TRUE(card(kDisposalExpert)->entersReadyOnPlay(state, P1, state.getObject(me)));
    addUnit(P1, kInvalidId, 3);
    EXPECT_FALSE(card(kDisposalExpert)->entersReadyOnPlay(state, P1, state.getObject(me)));
}

// ── Explosimian ──
TEST_F(Radiance5Test, ExplosimianPlaysThreeBombsToTheChosenBattlefield) {
    auto ex = addUnit(P1, kExplosimian, 0, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kExplosimian, P1, ex,
                          [](const std::vector<Intent>& legal) { return pickValue(legal, 1 + 1); },
                          exec, TriggerType::WhenIHold);
    EXPECT_EQ(countAt(kBombTokenCardDefId, P1, 1), 3);
    EXPECT_EQ(countAt(kBombTokenCardDefId, P1, 0), 0);
}

// ── Momentary Companion ──
TEST_F(Radiance5Test, MomentaryCompanionIsTemporaryWhenPlayedOnOpponentsTurn) {
    EXPECT_TRUE(card_db.get(kMomentaryCompanion).keywords.has(Keyword::Hidden));
    auto mc = addUnit(P1, kMomentaryCompanion, 0, 0);
    state.turn.turn_player = P1;
    EXPECT_FALSE(card(kMomentaryCompanion)->triggerCondition(state, mc, P1,
                                                            TriggerType::WhenYouPlayMe, mc));
    state.turn.turn_player = P2;
    EXPECT_TRUE(card(kMomentaryCompanion)->triggerCondition(state, mc, P1,
                                                           TriggerType::WhenYouPlayMe, mc));
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kMomentaryCompanion, P1, mc, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(state.getObject(mc).hasKeyword(Keyword::Temporary));
}

// ── Satchel Charge ──
TEST_F(Radiance5Test, SatchelChargeMovesAUnitThenPlaysABomb) {
    EXPECT_TRUE(card_db.get(kSatchelCharge).keywords.has(Keyword::Action));
    auto u = addUnit(P1, kInvalidId, 3);
    auto spell = pushSpellOnChain(P1, kSatchelCharge);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumable(kSatchelCharge, P1, spell, [&](const std::vector<Intent>& legal) {
        switch (call++) {
        case 0: return pickObject(legal, u);
        case 1: return pickValue(legal, 1 + 1);   // move to battlefield 1
        default: return pickValue(legal, 1 + 0);  // Bomb at battlefield 0
        }
    }, exec);
    ASSERT_TRUE(state.getObject(u).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(u).battlefieldId(), 1u);
    EXPECT_EQ(countAt(kBombTokenCardDefId, P1, 0), 1);
    EXPECT_EQ(countAt(kBombTokenCardDefId, P1, 1), 0);
}

// ── Siren's Song ──
TEST_F(Radiance5Test, SirensSongMovesOnlyAnEnemyThatMovedThisTurn) {
    auto still = addUnit(P2, kInvalidId, 3, 0);
    auto moved = addUnit(P2, kInvalidId, 3);
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.moveToBattlefield(moved, 0);   // an effect move counts as moving
    EXPECT_EQ(state.getObject(moved).moves_this_turn, 1);
    auto legal = card(kSirensSong)->enumerateLegalTargets(state, P1);
    ASSERT_EQ(legal.size(), 1u);
    EXPECT_EQ(legal.front(), moved);

    auto spell = pushSpellOnChain(P1, kSirensSong);
    state.chain.items.clear();
    int call = 0;
    driveResumable(kSirensSong, P1, spell, [&](const std::vector<Intent>& l) {
        return call++ == 0 ? pickObject(l, moved) : pickValue(l, 1 + 1);
    }, exec);
    ASSERT_TRUE(state.getObject(moved).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(moved).battlefieldId(), 1u);
    EXPECT_EQ(*state.getObject(still).battlefieldId(), 0u);
}

// ── Suave Negotiator ──
TEST_F(Radiance5Test, SuaveNegotiatorBothPlayersDraw) {
    auto sn = addUnit(P1, kSuaveNegotiator, 0, 0);
    addToDeck(P1, kStaunchSurvivor);
    addToDeck(P2, kStaunchSurvivor);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kSuaveNegotiator, P1, sn, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(handSize(P2), 1);
}

// ── The Waters' Wage ──
class WatersWageTest : public Radiance5Test {
protected:
    int gold(bool exhausted) {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.card_def_id == kGoldGearCardDefId && o.controller == P1 &&
                o.location.has_value() && o.is_exhausted == exhausted) ++n;
        return n;
    }
    void cast(bool from_facedown, PlayerId turn) {
        state.turn.turn_player = turn;
        auto spell = pushSpellOnChain(P1, kWatersWage);
        state.chain.items.clear();
        if (from_facedown) state.getObject(spell).card_counters["__played_from_facedown"] = 1;
        EffectExecutor exec(state, events, card_db, &card_registry);
        invokeOnResolve(spell, kWatersWage, P1, {}, exec);
    }
};

TEST_F(WatersWageTest, FromHandMakesOneExhaustedGold) {
    cast(false, P1);
    EXPECT_EQ(gold(true), 1);
    EXPECT_EQ(gold(false), 0);
}

TEST_F(WatersWageTest, FromFaceDownOnYourTurnMakesThreeReadyGold) {
    cast(true, P1);
    EXPECT_EQ(gold(false), 3);
    EXPECT_EQ(gold(true), 0);
}

TEST_F(WatersWageTest, FromFaceDownOnOpponentsTurnMakesOneExhaustedGold) {
    cast(true, P2);
    EXPECT_EQ(gold(true), 1);
    EXPECT_EQ(gold(false), 0);
}

// ── Demon Shade ──
TEST_F(Radiance5Test, DemonShadeMovesToAnOpenBattlefield) {
    auto u = addUnit(P1, kInvalidId, 3, 0);
    auto spell = pushSpellOnChain(P1, kDemonShade);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumable(kDemonShade, P1, spell, [&](const std::vector<Intent>& l) {
        switch (call++) {
        case 0: return pickValue(l, 0);     // mode: move
        case 1: return pickObject(l, u);
        default: return pickValue(l, 1 + 1);
        }
    }, exec);
    ASSERT_TRUE(state.getObject(u).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(u).battlefieldId(), 1u);
}

TEST_F(Radiance5Test, DemonShadeGivesGankingOnlyWithoutOpenBattlefields) {
    auto u = addUnit(P1, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto run = [&]() {
        auto spell = pushSpellOnChain(P1, kDemonShade);
        state.chain.items.clear();
        int call = 0;
        driveResumable(kDemonShade, P1, spell, [&](const std::vector<Intent>& l) {
            return call++ == 0 ? pickValue(l, 1) : pickObject(l, u);
        }, exec);
    };
    run();   // battlefield 1 is open
    EXPECT_FALSE(state.getObject(u).hasKeyword(Keyword::Ganking));
    addUnit(P2, kInvalidId, 2, 1);
    run();
    EXPECT_TRUE(state.getObject(u).hasKeyword(Keyword::Ganking));
}

// ── Gutter Guardian ──
TEST_F(Radiance5Test, GutterGuardianAddsPowerOnlyIfItShowedOff) {
    Card* gg = card(kGutterGuardian);
    EXPECT_TRUE(gg->hasShowOff());
    auto hidden_card = addToHand(P1, kWatersWage);
    auto plain = addToHand(P1, kStaunchSurvivor);
    EXPECT_TRUE(gg->showOffMatches(state, P1, state.getObject(hidden_card)));
    EXPECT_FALSE(gg->showOffMatches(state, P1, state.getObject(plain)));

    auto me = addUnit(P1, kGutterGuardian, 0, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kGutterGuardian, P1, me, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    state.getObject(me).showed_off_object = hidden_card;
    fireTriggerAs(kGutterGuardian, P1, me, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

// ── Pitiless Stalker ──
TEST_F(Radiance5Test, PitilessStalkerStunsAnEnemyUnit) {
    auto ps = addUnit(P1, kPitilessStalker, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 3, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    auto abs = card(kPitilessStalker)->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    auto legal = card(kPitilessStalker)->enumerateLegalTargets(state, P1, 0, ps);
    EXPECT_TRUE(std::find(legal.begin(), legal.end(), ally) == legal.end());
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, ps};
    card(kPitilessStalker)->onActivate(ctx, 0, {});
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_FALSE(state.getObject(ally).is_stunned);
}

}  // namespace
}  // namespace riftbound::test
