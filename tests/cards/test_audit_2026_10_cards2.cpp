/// @file test_audit_2026_10_cards2.cpp
/// Card fixes from the 2026-10-28 audit, batch "cards2".

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kFlash = 309, kLux = 312, kRecruit = 313, kTibbers = 316,
                    kDecisiveStrike = 322, kAgainstTheOdds = 323, kBushwhack = 327,
                    kDetonate = 328, kGemJammer = 330, kMinotaur = 337, kRumble = 349,
                    kRibbonDancer = 361, kThwonk = 363, kPoroSnax = 369, kBigYordle = 378,
                    kIrelia = 380, kOrnn = 381, kEngineer = 384, kForecaster = 388,
                    kBreakneck = 394, kHexplate = 396, kTemporalPortal = 401,
                    kCardSharp = 403, kHextechAnomaly = 405, kJayce = 406, kZeroDrive = 412,
                    kBuhru = 413, kYordleExplorer = 422, kPetricite = 426, kHereToHelp = 433,
                    kSivir = 441, kCalledShot = 443, kFan = 449, kWarmonger = 452,
                    kBeastBelow = 453;

class Audit2610Cards2 : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.keywords = def.keywords;
            obj.might_bonus = def.might_bonus;
            obj.tags = def.tags;
        }
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::find(h.begin(), h.end(), id));
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    void control(BattlefieldId bf, PlayerId p) { state.battlefields[bf].controller = p; }

    // "Yes" for a yes/no prompt; otherwise the option choosing `want` if
    // offered; otherwise the first option.
    static std::function<Intent(const std::vector<Intent>&)> yesOr(GameObjectId want = kInvalidId) {
        return [want](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (i.chosen_objects.empty() && i.chosen_value.value_or(-1) == 1 &&
                    legal.size() == 2 && legal[0].chosen_value.value_or(-1) == 0)
                    return i;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == want) return i;
            return legal.front();
        };
    }
    static Intent pickValue(const std::vector<Intent>& legal, int v) {
        for (const auto& i : legal)
            if (i.chosen_objects.empty() && i.chosen_value.value_or(-99) == v) return i;
        return legal.front();
    }
    static Intent pickObj(const std::vector<Intent>& legal, GameObjectId id) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return i;
        return legal.front();
    }
    int goldTokens(PlayerId p) {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.isGear() && o.name == "Gold" && o.controller == p && o.location) ++n;
        return n;
    }
};

// ── Flash: "up to 2" — the player may stop after one ──
TEST_F(Audit2610Cards2, FlashCanMoveJustOneUnit) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 1);
    auto spell = addToHand(P1, kFlash);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumable(kFlash, P1, spell, [&](const std::vector<Intent>& legal) {
        return call++ == 0 ? pickObj(legal, a) : pickValue(legal, 0);   // a, then Done
    }, exec);
    EXPECT_TRUE(state.getObject(a).isAtBase());
    EXPECT_FALSE(state.getObject(b).isAtBase());
}

TEST_F(Audit2610Cards2, FlashMovesTwo) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 1);
    auto spell = addToHand(P1, kFlash);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumable(kFlash, P1, spell, [&](const std::vector<Intent>& legal) {
        return pickObj(legal, call++ == 0 ? a : b);
    }, exec);
    EXPECT_TRUE(state.getObject(a).isAtBase());
    EXPECT_TRUE(state.getObject(b).isAtBase());
}

// ── Lux, Crownguard: the added Energy is spell-only ──
TEST_F(Audit2610Cards2, LuxAddsSpellOnlyEnergy) {
    auto lux = addUnit(P1, kLux, 0, -1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, lux};
    card_registry.get(kLux)->onActivate(ctx, {});
    EXPECT_EQ(state.player(P1).rune_pool.restrictedAmount(false, SpendPurpose::PlaySpell), 2);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

// ── Recruit the Vanguard: tokens may go to a battlefield you control ──
TEST_F(Audit2610Cards2, RecruitTokensToControlledBattlefield) {
    control(0, P1);
    auto spell = addToHand(P1, kRecruit);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kRecruit, P1, spell,
                   [](const std::vector<Intent>& legal) { return pickValue(legal, 1); }, exec);
    int at_bf = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.name == "Recruit" && o.battlefieldId() == BattlefieldId{0}) ++at_bf;
    EXPECT_EQ(at_bf, 4);
}

// ── Tibbers: only units at battlefields ──
TEST_F(Audit2610Cards2, TibbersSparesBaseUnits) {
    auto tib = addUnit(P1, kTibbers, 0, -1);
    auto base_enemy = addUnit(P2, kInvalidId, 3, -1);
    auto bf_enemy = addUnit(P2, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kTibbers, P1, tib, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.getObject(tib).damage_marked, 0);
    EXPECT_EQ(state.getObject(base_enemy).damage_marked, 0);
    EXPECT_EQ(state.getObject(bf_enemy).zone, ZoneType::Trash);
}

// ── Decisive Strike: all friendly units ──
TEST_F(Audit2610Cards2, DecisiveStrikeBuffsAllFriendlyUnits) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, -1);
    auto e = addUnit(P2, kInvalidId, 2, 0);
    EXPECT_EQ(card_registry.get(kDecisiveStrike)->getTargetRequirements().count, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(addToHand(P1, kDecisiveStrike), kDecisiveStrike, P1, {}, exec);
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(b).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(e).temp_might_bonus, 0);
}

// ── Against the Odds: +2 per enemy unit there ──
TEST_F(Audit2610Cards2, AgainstTheOddsScalesWithEnemies) {
    auto u = addUnit(P1, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 1);   // elsewhere: doesn't count
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(addToHand(P1, kAgainstTheOdds), kAgainstTheOdds, P1, {u}, exec);
    EXPECT_EQ(state.getObject(u).temp_might_bonus, 4);
}

// ── Bushwhack: units entering later this turn enter ready ──
TEST_F(Audit2610Cards2, BushwhackMakesLaterUnitsEnterReady) {
    auto old = addUnit(P1, kInvalidId, 2, -1);
    state.getObject(old).is_exhausted = true;
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(addToHand(P1, kBushwhack), kBushwhack, P1, {}, exec);
    EXPECT_TRUE(state.getObject(old).is_exhausted) << "units already in play are not readied";
    EXPECT_TRUE(state.player(P1).units_enter_ready_this_turn);
    EXPECT_EQ(goldTokens(P1), 1);
    auto tok = exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, {},
                                LocationId{BaseLocation{P1}}, false);
    EXPECT_FALSE(state.getObject(tok).is_exhausted);
    auto card = addToHand(P1, kInvalidId);
    state.getObject(card).card_type = CardType::Unit;
    state.player(P1).hand.clear();
    exec.playIgnoringCost(P1, card);
    EXPECT_FALSE(state.getObject(card).is_exhausted);
    state.player(P1).resetTurnTracking();
    EXPECT_FALSE(state.player(P1).units_enter_ready_this_turn);
}

// ── Detonate: the gear's controller draws 2 ──
TEST_F(Audit2610Cards2, DetonateControllerDraws2) {
    auto gear = addGear(P2, kPoroSnax, -1);
    addToDeck(P2, kInvalidId);
    addToDeck(P2, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(addToHand(P1, kDetonate), kDetonate, P1, {gear}, exec);
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Trash);
    EXPECT_EQ(handSize(P2), 2);
}

// ── Gem Jammer: chosen unit gets Ganking; no printed Ganking ──
TEST_F(Audit2610Cards2, GemJammerGivesGankingToChosenUnit) {
    EXPECT_FALSE(card_db.get(kGemJammer).keywords.has(Keyword::Ganking));
    EXPECT_EQ(card_registry.get(kGemJammer)->getTargetRequirements().count, 0);
    auto gj = addUnit(P1, kGemJammer, 0, -1);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kGemJammer, P1, gj, yesOr(other), exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(other).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(state.getObject(gj).hasKeyword(Keyword::Ganking));
}

// ── Minotaur Reckoner: restriction is an aura flag, not the permanent one ──
TEST_F(Audit2610Cards2, MinotaurUsesClearableAuraFlag) {
    addUnit(P1, kMinotaur, 0, 0);
    auto e = addUnit(P2, kInvalidId, 2, 0);
    card_registry.get(kMinotaur)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(e).aura_cant_move_to_base);
    EXPECT_FALSE(state.getObject(e).cant_move_to_base);
}

// ── Rumble, Hotheaded: player-chosen recycle + Mech; no printed Assault ──
TEST_F(Audit2610Cards2, RumbleRecyclesChosenUnitAndPlaysMech) {
    EXPECT_FALSE(card_db.get(kRumble).keywords.has(Keyword::Assault));
    EXPECT_EQ(card_db.get(kRumble).assault_value, 0);
    auto rumble = addUnit(P1, kRumble, 0, 0);
    auto fodder = addUnit(P1, kInvalidId, 2, -1);
    auto mech = addToTrash(P1, kGemJammer);   // Mech, [2]: free after -2
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRumble, P1, rumble, yesOr(fodder), exec, TriggerType::WhenIConquer);
    EXPECT_TRUE(inDeck(P1, fodder));
    EXPECT_TRUE(state.getObject(mech).location.has_value());
    EXPECT_FALSE(inTrash(P1, mech));
}

TEST_F(Audit2610Cards2, RumbleGrantsAssaultToMechsIncludingItself) {
    auto rumble = addUnit(P1, kRumble, 0, 0);
    state.getObject(rumble).combat_designation = CombatDesignation::Attacker;
    card_registry.get(kRumble)->applyPassiveAura(state, P1);
    int might = 0;
    for (auto& ae : state.getObject(rumble).aura_effects)
        if (ae.keyword == Keyword::Assault) might += ae.might_bonus;
    EXPECT_EQ(might, 1);
}

// ── Ribbon Dancer: another friendly unit gets +1 ──
TEST_F(Audit2610Cards2, RibbonDancerBuffsAnotherFriendly) {
    auto rd = addUnit(P1, kRibbonDancer, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, -1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kRibbonDancer, P1, rd, [&](const std::vector<Intent>& legal) {
        for (auto& i : legal) offered.push_back(i.chosen_objects[0]);
        return legal.front();
    }, exec, TriggerType::WhenIMoveToFB);
    EXPECT_EQ(offered, std::vector<GameObjectId>{other});
    EXPECT_EQ(state.getObject(other).temp_might_bonus, 1);
    EXPECT_EQ(state.getObject(rd).temp_might_bonus, 0);
}

// ── Thwonk!: only attacking units ──
TEST_F(Audit2610Cards2, ThwonkTargetsOnlyAttackers) {
    auto att = addUnit(P2, kInvalidId, 2, 0);
    state.getObject(att).combat_designation = CombatDesignation::Attacker;
    addUnit(P2, kInvalidId, 2, 1);
    auto t = card_registry.get(kThwonk)->enumerateLegalTargets(state, P1);
    EXPECT_EQ(t, std::vector<GameObjectId>{att});
}

// ── Poro Snax: [1][G] cost + kill this ──
TEST_F(Audit2610Cards2, PoroSnaxKillsItselfAndChargesPower) {
    auto cost = card_registry.get(kPoroSnax)->getActivationCost();
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Calm);
    EXPECT_FALSE(cost.recycle_self);
    auto snax = addGear(P1, kPoroSnax);
    addToDeck(P1, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, snax};
    card_registry.get(kPoroSnax)->onActivate(ctx, {});
    EXPECT_TRUE(inTrash(P1, snax));
    EXPECT_EQ(handSize(P1), 1);
}

// ── Needlessly Large Yordle: [G] less per hold point too ──
TEST_F(Audit2610Cards2, BigYordlePowerDiscount) {
    state.player(P1).hold_points_this_turn = 2;
    auto* c = card_registry.get(kBigYordle);
    EXPECT_EQ(c->selfCostReduction(state, P1), 4);
    EXPECT_EQ(c->selfPowerCostReduction(state, P1), 2);
}

// ── Irelia, Fervent: "when you choose ME" ──
TEST_F(Audit2610Cards2, IreliaTriggersOnlyWhenSheIsChosen) {
    auto ir = addUnit(P1, kIrelia, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    auto* c = card_registry.get(kIrelia);
    EXPECT_TRUE(c->triggerCondition(state, ir, P1, TriggerType::WhenYouChooseAFriendlyUnit, ir));
    EXPECT_FALSE(c->triggerCondition(state, ir, P1, TriggerType::WhenYouChooseAFriendlyUnit, other));
}

// ── Ornn, Blacksmith: choose which gear (or none) ──
TEST_F(Audit2610Cards2, OrnnDraftsChosenGear) {
    auto ornn = addUnit(P1, kOrnn, 0, 0);
    auto g1 = addToDeck(P1, kPoroSnax);
    auto g2 = addToDeck(P1, kTemporalPortal);
    auto top = addToDeck(P1, kInvalidId);
    (void)top;
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kOrnn, P1, ornn,
                          [&](const std::vector<Intent>& l) { return pickObj(l, g1); }, exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inHand(P1, g1));
    EXPECT_FALSE(inHand(P1, g2));
    EXPECT_EQ(state.player(P1).draws_this_turn, 1);
    EXPECT_EQ(deckSize(P1), 2);
}

TEST_F(Audit2610Cards2, OrnnMayTakeNothing) {
    auto ornn = addUnit(P1, kOrnn, 0, 0);
    addToDeck(P1, kPoroSnax);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kOrnn, P1, ornn,
                          [&](const std::vector<Intent>& l) { return pickValue(l, 0); }, exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(deckSize(P1), 1);
}

// ── Aspiring Engineer: gear from trash to hand ──
TEST_F(Audit2610Cards2, AspiringEngineerReturnsGearFromTrash) {
    EXPECT_EQ(card_registry.get(kEngineer)->getTargetRequirements().count, 0);
    auto ae = addUnit(P1, kEngineer, 0, 0);
    auto g = addToTrash(P1, kPoroSnax);
    auto board_gear = addGear(P1, kTemporalPortal);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kEngineer, P1, ae, yesOr(g), exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inHand(P1, g));
    EXPECT_TRUE(state.getObject(board_gear).location.has_value());
}

// ── Forecaster: Vision only through the Mech aura ──
TEST_F(Audit2610Cards2, ForecasterVisionComesFromAura) {
    EXPECT_FALSE(card_db.get(kForecaster).keywords.has(Keyword::Vision));
    auto f = addUnit(P1, kForecaster, 0, 0);
    card_registry.get(kForecaster)->applyPassiveAura(state, P1);
    bool vision = false;
    for (auto& ae : state.getObject(f).aura_effects) vision |= ae.keyword == Keyword::Vision;
    EXPECT_TRUE(vision);
}

// ── Breakneck Mech: no printed Deflect/Ganking; another copy counts ──
TEST_F(Audit2610Cards2, BreakneckMechKeywordsAndAnotherMech) {
    const auto& d = card_db.get(kBreakneck);
    EXPECT_FALSE(d.keywords.has(Keyword::Deflect));
    EXPECT_FALSE(d.keywords.has(Keyword::Ganking));
    EXPECT_EQ(d.deflect_value, 0);
    auto* c = card_registry.get(kBreakneck);
    auto self = addUnit(P1, kBreakneck, 0, 0);
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1, state.getObject(self)));
    addUnit(P1, kBreakneck, 0, 1);
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1, state.getObject(self)));
}

// ── Experimental Hexplate: the bearer is a Mech while equipped ──
TEST_F(Audit2610Cards2, HexplateMakesBearerAMech) {
    auto u = addUnit(P1, kInvalidId, 2, -1);
    auto g = addGear(P1, kHexplate);
    addRune(P1, Domain::Mind);
    addRune(P1, Domain::Mind);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, g};
    auto* c = card_registry.get(kHexplate);
    ASSERT_TRUE(c->onEquip(ctx, u));
    EXPECT_TRUE(hasTag(state.getObject(u), "Mech"));
    c->onUnattached(ctx, u);
    EXPECT_FALSE(hasTag(state.getObject(u), "Mech"));
}

// ── Temporal Portal: [A] cost; grants Repeat to the next spell ──
TEST_F(Audit2610Cards2, TemporalPortalGrantsNextSpellRepeat) {
    auto* c = card_registry.get(kTemporalPortal);
    EXPECT_TRUE(c->getActivationCost().power_any_domain);
    EXPECT_FALSE(card_db.get(kTemporalPortal).keywords.has(Keyword::Repeat));
    auto g = addGear(P1, kTemporalPortal);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, g};
    c->onActivate(ctx, {});
    EXPECT_TRUE(state.player(P1).grant_repeat_base_to_next_spell);
}

// ── Card Sharp: the opponent decides ──
TEST_F(Audit2610Cards2, CardSharpOpponentMayDecline) {
    auto cs = addUnit(P1, kCardSharp, 0, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumableTrigger(kCardSharp, P1, cs, [&](const std::vector<Intent>& legal) {
        return call++ == 0 ? pickValue(legal, 1) : pickValue(legal, 0);   // me yes, opp no
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(goldTokens(P1), 1);
    EXPECT_EQ(goldTokens(P2), 0);
}

TEST_F(Audit2610Cards2, CardSharpOpponentAccepts) {
    auto cs = addUnit(P1, kCardSharp, 0, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kCardSharp, P1, cs,
                          [&](const std::vector<Intent>& legal) { return pickValue(legal, 1); },
                          exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(goldTokens(P1), 2);
    EXPECT_EQ(goldTokens(P2), 1);
}

// ── Hextech Anomaly: [Reaction] ability ──
TEST_F(Audit2610Cards2, HextechAnomalyIsReaction) {
    auto abs = card_registry.get(kHextechAnomaly)->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].is_reaction);
}

// ── Jayce, Man of Progress: one chosen gear dies; the play may be declined ──
TEST_F(Audit2610Cards2, JayceKillsOnlyTheChosenGear) {
    auto jayce = addUnit(P1, kJayce, 0, 0);
    auto g1 = addGear(P1, kPoroSnax);
    auto g2 = addGear(P1, kPoroSnax);
    auto hand_gear = addToHand(P1, kTemporalPortal);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumableTrigger(kJayce, P1, jayce, [&](const std::vector<Intent>& legal) {
        switch (call++) {
        case 0: return pickValue(legal, 1);    // yes
        case 1: return pickObj(legal, g2);     // kill g2
        default: return pickValue(legal, 0);  // don't play
        }
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(g1).location.has_value());
    EXPECT_TRUE(inTrash(P1, g2));
    EXPECT_TRUE(inHand(P1, hand_gear));
}

TEST_F(Audit2610Cards2, JayceGrantsAGearPlayForTheTurn) {
    auto jayce = addUnit(P1, kJayce, 0, 0);
    auto g1 = addGear(P1, kPoroSnax);
    auto hand_gear = addToHand(P1, kTemporalPortal);   // [3], no Power
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumableTrigger(kJayce, P1, jayce, [&](const std::vector<Intent>& legal) {
        return call++ == 0 ? pickValue(legal, 1) : pickObj(legal, g1);
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inTrash(P1, g1));
    EXPECT_TRUE(inHand(P1, hand_gear)) << "a permission for the turn, not an immediate play";
    const auto& grants = state.player(P1).ignore_energy_play_grants;
    ASSERT_EQ(grants.size(), 1u);
    EXPECT_EQ(grants[0].card_type, CardType::Gear);
    EXPECT_EQ(grants[0].max_energy_cost, 7);
}

// ── The Zero Drive: [3][B] replay cost ──
TEST_F(Audit2610Cards2, ZeroDriveReplayChargesPower) {
    auto cost = card_registry.get(kZeroDrive)->getActivationCost();
    EXPECT_EQ(cost.energy, 3);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Mind);
}

// ── Buhru Captain: draw 1 OR buff me ──
TEST_F(Audit2610Cards2, BuhruCaptainCanBuffInstead) {
    auto bc = addUnit(P1, kBuhru, 0, 0);
    addToDeck(P1, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kBuhru, P1, bc,
                          [](const std::vector<Intent>& legal) { return legal[1]; },  // "Buff me"
                          exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.getObject(bc).buff_count, 1);
    EXPECT_EQ(handSize(P1), 0);
}

// ── Yordle Explorer: reads the played card from the trigger's subject ──
TEST_F(Audit2610Cards2, YordleExplorerDrawsForPowerTwoCard) {
    auto ye = addUnit(P1, kYordleExplorer, 0, 0);
    auto played = addUnit(P1, kTibbers, 0, 0);       // power cost 2
    auto cheap = addUnit(P1, kGemJammer, 0, 0);      // power cost 0
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kYordleExplorer, P1, ye, nullptr, exec,
                          TriggerType::WhenYouPlayAUnit, cheap);
    EXPECT_EQ(handSize(P1), 0);
    driveResumableTrigger(kYordleExplorer, P1, ye, nullptr, exec,
                          TriggerType::WhenYouPlayAUnit, played);
    EXPECT_EQ(handSize(P1), 1);
}

// ── Petricite Monument: the gear itself has no Deflect ──
TEST_F(Audit2610Cards2, PetriciteMonumentHasNoOwnDeflect) {
    const auto& d = card_db.get(kPetricite);
    EXPECT_FALSE(d.keywords.has(Keyword::Deflect));
    EXPECT_EQ(d.deflect_value, 0);
    EXPECT_TRUE(d.keywords.has(Keyword::Temporary));
}

// ── Here to Help: battlefield you control; may decline; none -> nothing ──
TEST_F(Audit2610Cards2, HereToHelpPlaysToControlledBattlefield) {
    control(1, P1);
    auto unit = addToHand(P1, kGemJammer);   // [2] -> free after -[3]
    auto spell = addToHand(P1, kHereToHelp);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kHereToHelp, P1, spell,
                   [&](const std::vector<Intent>& l) { return pickObj(l, unit); }, exec);
    EXPECT_EQ(state.getObject(unit).battlefieldId(), std::optional<BattlefieldId>{1});
}

TEST_F(Audit2610Cards2, HereToHelpNeedsAControlledBattlefield) {
    auto unit = addToHand(P1, kGemJammer);
    auto spell = addToHand(P1, kHereToHelp);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kHereToHelp, P1, spell,
                   [&](const std::vector<Intent>& l) { return pickObj(l, unit); }, exec);
    EXPECT_TRUE(inHand(P1, unit));
}

TEST_F(Audit2610Cards2, HereToHelpCanBeDeclined) {
    control(0, P1);
    auto unit = addToHand(P1, kGemJammer);
    auto spell = addToHand(P1, kHereToHelp);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kHereToHelp, P1, spell,
                   [&](const std::vector<Intent>& l) { return pickValue(l, 0); }, exec);
    EXPECT_TRUE(inHand(P1, unit));
}

// ── Sivir, Ambitious: deal the excess damage ──
TEST_F(Audit2610Cards2, SivirDealsExcessDamage) {
    auto sivir = addUnit(P1, kSivir, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 5, 1);
    auto& ps = state.player(P1);
    ps.last_combat_attacker = true;
    ps.last_combat_bf = 0;
    ps.excess_combat_damage = 6;
    auto* c = card_registry.get(kSivir);
    EXPECT_TRUE(c->triggerCondition(state, sivir, P1, TriggerType::WhenIConquer, kInvalidId));
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kSivir, P1, sivir, yesOr(enemy), exec, TriggerType::WhenIConquer);
    EXPECT_TRUE(inTrash(P2, enemy));
    ps.excess_combat_damage = 4;
    EXPECT_FALSE(c->triggerCondition(state, sivir, P1, TriggerType::WhenIConquer, kInvalidId));
}

// ── Called Shot: "Draw one" is a draw ──
TEST_F(Audit2610Cards2, CalledShotIsADraw) {
    auto a = addToDeck(P1, kInvalidId);
    auto b = addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kCalledShot);
    state.player(P1).hand.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kCalledShot, P1, spell,
                   [&](const std::vector<Intent>& l) { return pickObj(l, a); }, exec);
    EXPECT_TRUE(inHand(P1, a));
    EXPECT_TRUE(inDeck(P1, b));
    EXPECT_EQ(state.player(P1).draws_this_turn, 1);
}

// ── Overzealous Fan: moves the attacker to its base (not hand) ──
TEST_F(Audit2610Cards2, OverzealousFanMovesAttackerToBase) {
    auto fan = addUnit(P1, kFan, 0, 0);
    auto a1 = addUnit(P2, kInvalidId, 3, 0);
    auto a2 = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(a1).combat_designation = CombatDesignation::Attacker;
    state.getObject(a2).combat_designation = CombatDesignation::Attacker;
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kFan, P1, fan, yesOr(a2), exec, TriggerType::WhenIDefend);
    EXPECT_TRUE(inTrash(P1, fan));
    EXPECT_TRUE(state.getObject(a2).isAtBase());
    EXPECT_FALSE(inHand(P2, a2));
    EXPECT_FALSE(state.getObject(a1).isAtBase());
}

// ── Ancient Warmonger: Assault = enemy units here, nothing printed ──
TEST_F(Audit2610Cards2, AncientWarmongerAssaultEqualsEnemies) {
    const auto& d = card_db.get(kWarmonger);
    EXPECT_EQ(d.assault_value, 0);
    EXPECT_FALSE(d.keywords.has(Keyword::Assault));
    auto w = addUnit(P1, kWarmonger, 0, 0);
    state.getObject(w).combat_designation = CombatDesignation::Attacker;
    auto* c = card_registry.get(kWarmonger);
    c->applyPassiveAura(state, P1);
    int bonus = 0;
    for (auto& ae : state.getObject(w).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 0) << "no enemy units here: no Assault";
    state.getObject(w).aura_effects.clear();
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 0);
    c->applyPassiveAura(state, P1);
    bonus = 0;
    for (auto& ae : state.getObject(w).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 2);
}

// ── Beast Below: returns another friendly unit AND an enemy unit ──
TEST_F(Audit2610Cards2, BeastBelowReturnsFriendlyAndEnemy) {
    EXPECT_EQ(card_registry.get(kBeastBelow)->getTargetRequirements().count, 0);
    auto bb = addUnit(P1, kBeastBelow, 0, 0);
    auto f = addUnit(P1, kInvalidId, 2, 0);
    auto e = addUnit(P2, kInvalidId, 2, 0);
    for (auto id : {f, e}) state.getObject(id).zone = ZoneType::BattlefieldZone;
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kBeastBelow, P1, bb, nullptr, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inHand(P1, f));
    EXPECT_TRUE(inHand(P2, e));
    EXPECT_TRUE(state.getObject(bb).location.has_value());
}

}  // namespace
}  // namespace riftbound::test
