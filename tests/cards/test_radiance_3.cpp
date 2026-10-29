/// @file test_radiance_3.cpp
/// Radiance batch 3 (ids 1026-1044, RAD-046..064).

#include "tests/cards/card_test_fixture.h"

#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kRostrum = 1026, kFanfare = 1027, kHighNote = 1028, kMechsmith = 1029,
                    kChronomancer = 1030, kParallel = 1031, kExtractor = 1032, kOracle = 1033,
                    kPeddler = 1034, kSentry = 1035, kProjectile = 1036, kGuidebot = 1037,
                    kRiptide = 1038, kSupernova = 1039, kEmpyrean = 1040, kEkko = 1041,
                    kGravity = 1042, kKaiSa = 1043, kScope = 1044;

class Radiance3 : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_def_id = def;
        g.card_type = CardType::Gear;
        g.name = card_db.get(def).name;
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    void fillDeck(PlayerId p, int n, CardDefId def = kSupernova) {
        for (int i = 0; i < n; ++i) addToDeck(p, def);
    }
    static std::function<Intent(const std::vector<Intent>&)> pickObj(GameObjectId want) {
        return [want](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            return legal.front();
        };
    }
    static Intent pickYes(const std::vector<Intent>& legal) { return legal.back(); }
    int countTokens(PlayerId p, const std::string& name) {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.controller == p && o.name == name && o.location.has_value()) ++n;
        return n;
    }
};

// ── Def sanity ──
TEST_F(Radiance3, DefsMatchGallery) {
    EXPECT_EQ(card_db.get(kRostrum).energy_cost, 8);
    EXPECT_EQ(card_db.get(kRostrum).power_cost, 1);
    EXPECT_EQ(card_db.get(kSupernova).domains, std::vector<Domain>{Domain::Mind});   // printed card
    EXPECT_TRUE(card_db.get(kHighNote).keywords.has(Keyword::Reaction));
    EXPECT_TRUE(card_db.get(kProjectile).keywords.has(Keyword::Action));
    EXPECT_TRUE(card_db.get(kRiptide).keywords.has(Keyword::Accelerate));
    EXPECT_TRUE(card_db.get(kEkko).keywords.has(Keyword::Ganking));
    EXPECT_EQ(card_db.get(kEkko).super_type, SuperType::Champion);
    EXPECT_EQ(card_db.get(kEmpyrean).might, 8);
    EXPECT_EQ(card_db.get(kChronomancer).def_id, "rad-050-167");
    EXPECT_EQ(card_registry.get(kChronomancer)->disarmValue(), 2);
    EXPECT_EQ(card_registry.get(kPeddler)->disarmValue(), 1);
    EXPECT_EQ(card_registry.get(kKaiSa)->disarmValue(), 1);
    EXPECT_TRUE(card_registry.get(kExtractor)->hasDeploy());
    EXPECT_TRUE(card_registry.get(kGravity)->hasDeploy());
}

// ── 1026 Resplendent Rostrum ──
TEST_F(Radiance3, RostrumActivateDrawsAndChannelsExhausted) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto g = addGear(P1, kRostrum);
    fillDeck(P1, 2);
    auto rune = state.createObject();
    { auto& r = state.getObject(rune); r.owner = r.controller = P1; r.card_type = CardType::Rune;
      r.zone = ZoneType::RuneDeck; }
    state.player(P1).rune_deck.push_back(rune);
    CardContext ctx{state, events, exec, P1, g};
    card_registry.get(kRostrum)->onActivate(ctx, {});
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(state.getObject(rune).location.has_value());
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
}

TEST_F(Radiance3, RostrumExhaustingTriggersAndGivesPlusThree) {
    auto g = addGear(P1, kRostrum);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ObjectStateChangedEvent{g, "exhausted"});
    bool queued = false;
    for (auto& it : state.chain.items) if (it.card_def_id == kRostrum && it.source == g) queued = true;
    EXPECT_TRUE(queued) << "gear WhenIBecomeExhausted fires";

    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRostrum, P1, g, pickObj(u), exec, TriggerType::WhenIBecomeExhausted);
    EXPECT_EQ(state.getObject(u).current_might, 5);
}

// ── 1027 Fanfare ──
TEST_F(Radiance3, FanfareDrawsOnNextThreeUnitPlays) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = addToHand(P1, kFanfare);
    invokeOnResolve(spell, kFanfare, P1, {}, exec);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);

    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    for (int i = 0; i < 4; ++i) {
        auto u = addUnit(P1, kInvalidId, 1);
        events.emit(CardPlayedEvent{u, P1, CardType::Unit, i + 1, 0});
    }
    int fanfare_items = 0;
    for (auto& it : state.chain.items) if (it.card_def_id == kFanfare) ++fanfare_items;
    EXPECT_EQ(fanfare_items, 3);
    EXPECT_TRUE(state.delayed_abilities.empty());

    fillDeck(P1, 1);
    const int before = handSize(P1);
    CardContext ctx{state, events, exec, P1, spell};
    card_registry.get(kFanfare)->onTrigger(ctx, {});
    EXPECT_EQ(handSize(P1), before + 1);
}

TEST_F(Radiance3, FanfareIgnoresOpponentUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = addToHand(P1, kFanfare);
    invokeOnResolve(spell, kFanfare, P1, {}, exec);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto u = addUnit(P2, kInvalidId, 1);
    events.emit(CardPlayedEvent{u, P2, CardType::Unit, 1, 0});
    for (auto& it : state.chain.items) EXPECT_NE(it.card_def_id, kFanfare);
    EXPECT_EQ(state.delayed_abilities.size(), 1u);
}

// ── 1028 High Note ──
TEST_F(Radiance3, HighNoteMinusExhaustedFriendlyUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2); state.getObject(a).is_exhausted = true;
    auto b = addUnit(P1, kInvalidId, 2, 0); state.getObject(b).is_exhausted = true;
    addUnit(P1, kInvalidId, 2);   // ready: doesn't count
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    auto spell = addToHand(P1, kHighNote);
    driveResumable(kHighNote, P1, spell, pickObj(enemy), exec);
    EXPECT_EQ(state.getObject(enemy).current_might, 3);
}

// ── 1029 Industrious Mechsmith ──
TEST_F(Radiance3, MechsmithPlaysMechTokenHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kMechsmith, 0, 1);
    fireTriggerAs(kMechsmith, P1, me, TriggerType::WhenYouPlayMe, exec);
    int found = 0;
    for (auto& [id, o] : state.objects) {
        if (o.name != "Mech" || !o.isUnit()) continue;
        ++found;
        EXPECT_EQ(o.current_might, 3);
        EXPECT_TRUE(hasTag(o, "Mech"));
        ASSERT_TRUE(o.battlefieldId().has_value());
        EXPECT_EQ(*o.battlefieldId(), 1u);
    }
    EXPECT_EQ(found, 1);
}

// ── 1030 Mysterious Chronomancer ──
TEST_F(Radiance3, ChronomancerPredictsTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<int> seen;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& legal) {
        seen.push_back(static_cast<int>(legal.size()));
        return legal.front();
    });
    fillDeck(P1, 3);
    auto me = addUnit(P1, kChronomancer);
    fireTriggerAs(kChronomancer, P1, me, TriggerType::WhenYouPlayMe, exec);
    EXPECT_FALSE(seen.empty()) << "Predict asks the player";
    EXPECT_EQ(deckSize(P1), 3);
}

// ── 1031 Parallel Convergence ──
TEST_F(Radiance3, ParallelConvergenceReturnsThenDealsFive) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = addToHand(P1, kParallel);
    EXPECT_FALSE(card_registry.get(kParallel)->canBePlayed(state, P1)) << "needs a friendly unit";
    auto mine = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    EXPECT_TRUE(card_registry.get(kParallel)->canBePlayed(state, P1));
    EXPECT_TRUE(card_registry.get(kParallel)->hasLegalTargets(state, P1));
    int call = 0;
    driveResumable(kParallel, P1, spell, [&](const std::vector<Intent>& legal) {
        return pickObj(call++ == 0 ? mine : enemy)(legal);
    }, exec);
    EXPECT_TRUE(inHand(P1, mine));
    EXPECT_FALSE(state.getObject(enemy).location.has_value()) << "5 damage kills a 5 [M] unit";
}

TEST_F(Radiance3, ParallelConvergenceCantTargetTheReturnedUnit) {
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    EXPECT_FALSE(card_registry.get(kParallel)->hasLegalTargets(state, P1))
        << "the only battlefield unit is the one returned as the cost";
    (void)mine;
}

// ── 1032 Resource Extractor ──
TEST_F(Radiance3, ResourceExtractorGoldWhenControllingBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto g = addGear(P1, kExtractor, 0);
    Card* c = card_registry.get(kExtractor);
    EXPECT_FALSE(c->triggerCondition(state, g, P1, TriggerType::AtEndOfTurn, kInvalidId));
    state.battlefields[0].controller = P1;
    EXPECT_TRUE(c->triggerCondition(state, g, P1, TriggerType::AtEndOfTurn, kInvalidId));
    fireTriggerAs(kExtractor, P1, g, TriggerType::AtEndOfTurn, exec);
    bool gold = false;
    for (auto& [id, o] : state.objects)
        if (o.name == "Gold" && o.controller == P1) { gold = true; EXPECT_TRUE(o.is_exhausted); }
    EXPECT_TRUE(gold);
}

// ── 1033 Strigian Oracle ──
TEST_F(Radiance3, StrigianOracleTriggersOnExhaustAndPredicts) {
    auto me = addUnit(P1, kOracle, 0, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ObjectStateChangedEvent{me, "exhausted"});
    bool queued = false;
    for (auto& it : state.chain.items) if (it.card_def_id == kOracle) queued = true;
    EXPECT_TRUE(queued);

    EffectExecutor exec(state, events, card_db, &card_registry);
    int asked = 0;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& legal) { ++asked; return legal.front(); });
    fillDeck(P1, 2);
    fireTriggerAs(kOracle, P1, me, TriggerType::WhenIBecomeExhausted, exec);
    EXPECT_GT(asked, 0);
}

// ── 1035 Clanging Sentry ──
TEST_F(Radiance3, ClangingSentryGivesMechsDisarm) {
    addUnit(P1, kSentry);
    auto mech = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(mech).tags = {"Mech"};
    auto other = addUnit(P1, kInvalidId, 3, 0);
    auto enemy_mech = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(enemy_mech).tags = {"Mech"};
    card_registry.get(kSentry)->applyPassiveAura(state, P1);
    EXPECT_EQ(state.getObject(mech).aura_disarm_value, 1);
    EXPECT_EQ(state.getObject(other).aura_disarm_value, 0);
    EXPECT_EQ(state.getObject(enemy_mech).aura_disarm_value, 0);
}

// ── 1036 Improvised Projectile ──
TEST_F(Radiance3, ImprovisedProjectileDealsGearEnergyCost) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = addToHand(P1, kProjectile);
    EXPECT_FALSE(card_registry.get(kProjectile)->hasLegalTargets(state, P1));
    auto gear = addGear(P1, kScope);   // Energy cost 2
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    EXPECT_TRUE(card_registry.get(kProjectile)->hasLegalTargets(state, P1));
    int call = 0;
    driveResumable(kProjectile, P1, spell, [&](const std::vector<Intent>& legal) {
        return pickObj(call++ == 0 ? gear : enemy)(legal);
    }, exec);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 2);
}

// ── 1037 Piltovan Guidebot ──
TEST_F(Radiance3, GuidebotBaseMightBecomesPlayedCardCost) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kGuidebot, 0, 0);
    ASSERT_EQ(state.getObject(me).current_might, 3);
    auto big = addUnit(P1, kSupernova);   // stands in for the played card: Energy 9
    driveResumableTrigger(kGuidebot, P1, me, pickYes, exec, TriggerType::WhenYouPlayACard, big);
    EXPECT_EQ(state.getObject(me).current_might, 9);
    auto two = addUnit(P1, kScope);       // Energy 2: a later "become" replaces the earlier one
    driveResumableTrigger(kGuidebot, P1, me, pickYes, exec, TriggerType::WhenYouPlayACard, two);
    EXPECT_EQ(state.getObject(me).current_might, 2);
    // Declining leaves it unchanged.
    driveResumableTrigger(kGuidebot, P1, me, [](const std::vector<Intent>& l) { return l.front(); },
                          exec, TriggerType::WhenYouPlayACard, big);
    EXPECT_EQ(state.getObject(me).current_might, 2);
}

// ── 1038 Riptide Mage ──
TEST_F(Radiance3, RiptideMageGivesEnemyMinusThree) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kRiptide);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    driveResumableTrigger(kRiptide, P1, me, pickObj(enemy), exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.getObject(enemy).current_might, 2);
}

// ── 1039 Supernova ──
TEST_F(Radiance3, SupernovaDealsTenAndDrawsTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    fillDeck(P1, 3);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto spell = addToHand(P1, kSupernova);
    state.player(P1).hand.clear();
    driveResumable(kSupernova, P1, spell, pickObj(enemy), exec);
    EXPECT_FALSE(state.getObject(enemy).location.has_value());
    EXPECT_EQ(handSize(P1), 2);
}

// ── 1040 The Empyrean ──
TEST_F(Radiance3, EmpyreanShowOffPredictsPowerCostThenDrawsTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    int predict_asks = 0;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& legal) { ++predict_asks; return legal.front(); });
    fillDeck(P1, 4);
    auto me = addUnit(P1, kEmpyrean);
    Card* c = card_registry.get(kEmpyrean);
    EXPECT_TRUE(c->hasShowOff());
    auto shown = addToHand(P1, kRiptide);   // Power cost 1
    EXPECT_TRUE(c->showOffMatches(state, P1, state.getObject(shown)));
    state.getObject(me).showed_off_object = shown;
    fireTriggerAs(kEmpyrean, P1, me, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 3);
    EXPECT_GT(predict_asks, 0);
}

TEST_F(Radiance3, EmpyreanWithoutShowOffDoesNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    fillDeck(P1, 4);
    auto me = addUnit(P1, kEmpyrean);
    fireTriggerAs(kEmpyrean, P1, me, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 0);
}

// ── 1041 Ekko, Ingenious ──
TEST_F(Radiance3, EkkoNeedsAndReturnsAFriendlyUnit) {
    // Engine-backed play coverage: tests/cards/test_gap_addcosts.cpp.
    Card* c = card_registry.get(kEkko);
    const auto oac = c->optionalAdditionalCost(state, P1);
    EXPECT_TRUE(oac.mandatory);
    EXPECT_EQ(oac.object_cost, Card::OptionalAdditionalCost::ObjectCost::ReturnFriendlyUnit);
    EXPECT_TRUE(c->objectCostCandidates(state, P1, kInvalidId, std::nullopt).empty());
    auto other = addUnit(P1, kInvalidId, 2, 0);
    auto me = addUnit(P1, kEkko, 0, 0);
    const auto cands = c->objectCostCandidates(state, P1, me, std::nullopt);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_EQ(cands.front(), other) << "Ekko can't return himself";
}

// ── 1042 Gravity Field ──
TEST_F(Radiance3, GravityFieldTaxesOpponentMovesHere) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.players[0].id = P1;
    s.players[1].id = P2;
    for (int i = 0; i < 2; ++i) { s.battlefields.push_back(BattlefieldState{}); s.battlefields.back().id = i; }
    s.turn.phase = TurnPhase::MainPhase;
    s.turn.turn_player = P2;
    auto mk = [&](PlayerId p, CardType t, LocationId loc) {
        auto id = s.createObject();
        auto& o = s.getObject(id);
        o.owner = o.controller = p;
        o.card_type = t;
        o.location = loc;
        o.zone = std::holds_alternative<BaseLocation>(loc) ? ZoneType::Base : ZoneType::BattlefieldZone;
        return id;
    };
    auto field = mk(P1, CardType::Gear, BattlefieldLocation{0});
    s.getObject(field).card_def_id = kGravity;
    s.getObject(field).name = "Gravity Field";
    auto unit = mk(P2, CardType::Unit, BaseLocation{P2});
    s.getObject(unit).base_might = s.getObject(unit).current_might = 2;
    s.getObject(unit).name = "Walker";
    engine.testHook_cleanup();
    EXPECT_EQ(s.battlefields[0].move_tax_energy[playerIndex(P2)], 1);
    EXPECT_EQ(s.battlefields[0].move_tax_power[playerIndex(P2)], 1);
    EXPECT_EQ(s.battlefields[0].move_tax_energy[playerIndex(P1)], 0);

    auto movesTo = [&](BattlefieldId bf) {
        int n = 0;
        for (auto& i : engine.generateLegalActions())
            if (i.type == IntentType::StandardMove && i.move_destination &&
                *i.move_destination == LocationId{BattlefieldLocation{bf}}) ++n;
        return n;
    };
    EXPECT_EQ(movesTo(0), 0) << "can't pay [1][A]";
    EXPECT_EQ(movesTo(1), 1);

    auto r1 = mk(P2, CardType::Rune, BaseLocation{P2});
    auto r2 = mk(P2, CardType::Rune, BaseLocation{P2});
    s.getObject(r1).domains = {Domain::Mind};
    s.getObject(r2).domains = {Domain::Mind};
    EXPECT_EQ(movesTo(0), 1);
    engine.testHook_executeStandardMove(Intent::standardMove(P2, {unit}, BattlefieldLocation{0}));
    ASSERT_TRUE(s.getObject(unit).battlefieldId().has_value());
    int ready_in_base = 0;
    for (auto r : {r1, r2})
        if (s.getObject(r).location.has_value() &&
            std::holds_alternative<BaseLocation>(*s.getObject(r).location) &&
            !s.getObject(r).is_exhausted) ++ready_in_base;
    EXPECT_LT(ready_in_base, 2) << "the tax was paid with runes";
}

TEST_F(Radiance3, GravityFieldDeathknellDraws) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    fillDeck(P1, 1);
    auto g = addGear(P1, kGravity, 0);
    fireTriggerAs(kGravity, P1, g, TriggerType::WhenIDie, exec);
    EXPECT_EQ(handSize(P1), 1);
}

// ── 1043 Kai'Sa, Rebel ──
TEST_F(Radiance3, KaiSaGivesSpellsReactionInShowdown) {
    addUnit(P1, kKaiSa, 0, 0);
    card_registry.get(kKaiSa)->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P1).spells_have_reaction);
    for (auto& [id, o] : state.objects) o.aura_effects.clear();
    state.battlefields[0].showdown_in_progress = true;
    card_registry.get(kKaiSa)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).spells_have_reaction);
    EXPECT_FALSE(state.player(P2).spells_have_reaction);
}

// ── 1044 Scrutinizing Scope ──
class ScopeTest : public Radiance3 {
protected:
    void activate(GameObjectId g, int guess, EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = P1;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.source = g;
        ri.card_def_id = kScope;
        state.chain.resuming = ri;
        CardContext ctx{state, events, exec, P1, g};
        Card* c = card_registry.get(kScope);
        c->onActivate(ctx, {});
        ASSERT_TRUE(exec.hasPendingChoice());
        auto pending = exec.consumePendingChoice();
        EXPECT_EQ(pending.player, P2) << "the opponent guesses";
        for (auto& i : pending.legal)
            if (i.chosen_value && *i.chosen_value == guess) { exec.recordChoice(i); break; }
        c->onActivate(ctx, {});
        state.chain.resuming.reset();
    }
};

TEST_F(ScopeTest, RightGuessRecyclesTopCard) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bottom = addToDeck(P1, kSupernova);
    auto top = addToDeck(P1, kRiptide);   // a unit
    auto g = addGear(P1, kScope);
    activate(g, /*unit*/0, exec);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(state.player(P1).main_deck.front(), top) << "recycled to the bottom";
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom);
}

TEST_F(ScopeTest, WrongGuessDrawsTopCard) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kSupernova);
    auto top = addToDeck(P1, kRiptide);
    auto g = addGear(P1, kScope);
    activate(g, /*spell*/2, exec);
    EXPECT_TRUE(inHand(P1, top));
}

}  // namespace
}  // namespace riftbound::test
