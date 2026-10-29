/// @file test_audit_2026_10_cards3.cpp
/// Card fixes from the 2026-10-28 audit, batch cards3 (ids 458..600).

#include "tests/cards/card_test_fixture.h"

#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kHarpoonSquad = 458, kWindsinger = 459, kSivir = 464, kEzreal = 470,
    kLastRites = 471, kEyeOfTheHerald = 474, kSandshifter = 479, kGlasc = 486,
    kRally = 487, kAltar = 490, kDangerZone = 501, kRabadon = 508, kProdigal = 514,
    kArcaneShift = 515, kBattleMistress = 517, kOnTheHunt = 518, kVeiledTemple = 535,
    kAhri = 539, kSealOfStrength = 542, kDarius = 547, kGrandmaster = 554,
    kChemBaroness = 556, kNeophyte = 563, kSmite = 568, kBroadmane = 574,
    kRightOfConquest = 577, kScorchclaw = 578, kSquareUp = 579, kYeti = 580,
    kDancingGrenade = 582, kInviolusVox = 589, kBrambleback = 591, kDoubleTrouble = 594,
    kFriskyHunter = 595, kHerald = 596, kSkywardStrike = 600, kGold = 326, kBird = 559;

Intent yesOrPick(const std::vector<Intent>& legal, GameObjectId want = kInvalidId) {
    for (const auto& i : legal)
        if (want != kInvalidId && !i.chosen_objects.empty() && i.chosen_objects[0] == want)
            return i;
    for (const auto& i : legal)
        if (want == kInvalidId && i.chosen_value.has_value() && *i.chosen_value == 1) return i;
    return legal.front();
}
Intent pickValue(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
    return legal.front();
}
bool offers(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return true;
    return false;
}

class Cards3Test : public CardTestFixture {
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
            obj.tags = def.tags;
            obj.domains = def.domains;
            obj.might_bonus = def.might_bonus;
        } else {
            obj.name = "TestGear";
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
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = state.getObject(gear);
        auto& u = state.getObject(unit);
        g.attached_to = unit;
        g.location = u.location;
        u.attachments.push_back(gear);
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Legend;
        obj.name = card_db.get(def_id).name;
        obj.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id, CardType type = CardType::Unit) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = def_id != kInvalidId ? card_db.get(def_id).card_type : type;
        obj.name = def_id != kInvalidId ? card_db.get(def_id).name : "TrashCard";
        obj.zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    GameObjectId installBattlefieldCard(BattlefieldId bf, CardDefId def_id) {
        auto src = state.createObject();
        auto& obj = state.getObject(src);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        obj.name = card_db.get(def_id).name;
        state.battlefields[bf].card_object_id = src;
        return src;
    }
    int auraMight(GameObjectId id) {
        int b = 0;
        for (const auto& ae : state.getObject(id).aura_effects) b += ae.might_bonus;
        return b;
    }
    int chainItemsFrom(GameObjectId src) {
        int n = 0;
        for (const auto& ci : state.chain.items) if (ci.source == src) ++n;
        return n;
    }
    // Resumable onResolve with play-time targets.
    void driveResolve(CardDefId def_id, PlayerId controller, GameObjectId source,
                      const std::vector<GameObjectId>& targets,
                      std::function<Intent(const std::vector<Intent>&)> picker,
                      EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{state, events, exec, controller, source};
        card->onResolve(ctx, targets);
        for (int g = 0; g < 16 && exec.hasPendingChoice(); ++g) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }
    void driveActivate(CardDefId def_id, PlayerId controller, GameObjectId source, int index,
                       std::function<Intent(const std::vector<Intent>&)> picker,
                       EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.ability_index = index;
        ri.source = source;
        ri.card_def_id = def_id;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{state, events, exec, controller, source};
        card->onActivate(ctx, index, {});
        for (int g = 0; g < 16 && exec.hasPendingChoice(); ++g) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onActivate(ctx, index, {});
        }
        state.chain.resuming.reset();
    }
};

// ── 458 Harpoon Squad: only a move FROM a battlefield triggers ──
TEST_F(Cards3Test, HarpoonSquad_OnlyMovesFromABattlefieldTrigger) {
    auto u = addUnit(P1, kHarpoonSquad, 0, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(UnitMovedEvent{u, P1, BaseLocation{P1}, BattlefieldLocation{0}, true});
    EXPECT_EQ(chainItemsFrom(u), 0) << "base -> battlefield is not 'from a battlefield'";
    events.emit(UnitMovedEvent{u, P1, BattlefieldLocation{0}, BattlefieldLocation{1}, true});
    EXPECT_EQ(chainItemsFrom(u), 1);
}

// ── 459 Windsinger: prints [Hidden] ──
TEST_F(Cards3Test, Windsinger_HasHidden) {
    EXPECT_TRUE(card_db.get(kWindsinger).keywords.has(Keyword::Hidden));
}

// ── 464 Sivir: Ganking only through the conditional aura ──
TEST_F(Cards3Test, Sivir_GankingOnlyWhenTwoPowerSpent) {
    EXPECT_FALSE(card_db.get(kSivir).keywords.has(Keyword::Ganking));
    auto s = addUnit(P1, kSivir, 0, 0);
    Card* c = card_registry.get(kSivir);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(s).aura_effects.empty());
    state.player(P1).power_spent_this_turn = 2;
    c->applyPassiveAura(state, P1);
    bool gank = false;
    for (auto& ae : state.getObject(s).aura_effects) if (ae.keyword == Keyword::Ganking) gank = true;
    EXPECT_TRUE(gank);
}

// ── 470 Ezreal: optional additional costs discounted while he's on board ──
TEST_F(Cards3Test, Ezreal_SetsOptionalCostDiscount) {
    Card* c = card_registry.get(kEzreal);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).optional_cost_discount, 0) << "not on board";
    addUnit(P1, kEzreal, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).optional_cost_discount, 1);
}

// ── 471 Last Rites: the unit from trash is paid for ──
TEST_F(Cards3Test, LastRites_UnitFromTrashStillCostsItsCost) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P1, kLastRites, 0);
    auto pricey = addToTrash(P1, kHarpoonSquad);   // [4], no runes to pay with
    driveResumableTrigger(kLastRites, P1, gear,
        [&](const std::vector<Intent>& l) { return yesOrPick(l, offers(l, pricey) ? pricey : kInvalidId); },
        exec, TriggerType::WhenIConquerOrHold);
    EXPECT_TRUE(inTrash(P1, pricey)) << "can't pay [4]: the unit stays in the trash";

    for (int i = 0; i < 4; ++i) addRune(P1, Domain::Chaos);
    driveResumableTrigger(kLastRites, P1, gear,
        [&](const std::vector<Intent>& l) { return yesOrPick(l, offers(l, pricey) ? pricey : kInvalidId); },
        exec, TriggerType::WhenIConquerOrHold);
    EXPECT_FALSE(inTrash(P1, pricey));
    EXPECT_TRUE(state.getObject(pricey).location.has_value());
    EXPECT_EQ(readyRuneCount(P1), 0) << "its [4] was paid";
}

// ── 474 Eye of the Herald: the Recruit enters exhausted ──
TEST_F(Cards3Test, EyeOfTheHerald_RecruitEntersExhausted) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    auto gear = addGear(P1, kEyeOfTheHerald, 0);
    attach(gear, u);
    CardContext ctx{state, events, exec, P1, gear};
    card_registry.get(kEyeOfTheHerald)->onEquippedTrigger(ctx, u, {});
    bool found = false;
    for (auto& [id, o] : state.objects)
        if (o.name == "Recruit") { found = true; EXPECT_TRUE(o.is_exhausted); }
    EXPECT_TRUE(found);
}

// ── 479 Sandshifter: picks an enemy unit with 3 [M] or less ──
TEST_F(Cards3Test, Sandshifter_KillsChosenSmallEnemy) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kSandshifter, 0, 0);
    auto small = addUnit(P2, kInvalidId, 3, 0);
    auto big = addUnit(P2, kInvalidId, 4, 0);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    bool offered_bad = false;
    driveResumableTrigger(kSandshifter, P1, me, [&](const std::vector<Intent>& l) {
        offered_bad = offers(l, big) || offers(l, mine);
        return yesOrPick(l, small);
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(offered_bad);
    EXPECT_FALSE(state.getObject(small).location.has_value());
    EXPECT_TRUE(state.getObject(big).location.has_value());
    EXPECT_TRUE(card_registry.get(kSandshifter)->getTargetRequirements().count == 0);
}

// ── 486 Glasc Mixologist: power cost must be no more than [A] ──
TEST_F(Cards3Test, GlascMixologist_RejectsTwoPowerUnits) {
    CardDefId two_power = kInvalidId;
    for (CardDefId id = 1; id <= 955 && two_power == kInvalidId; ++id) {
        if (!card_registry.get(id)) continue;
        const auto& d = card_db.get(id);
        if (d.card_type == CardType::Unit && d.energy_cost <= 3 && d.power_cost >= 2)
            two_power = id;
    }
    ASSERT_NE(two_power, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto glasc = addToTrash(P1, kGlasc);
    auto t = addToTrash(P1, two_power);
    int prompts = 0;
    driveResumableTrigger(kGlasc, P1, glasc, [&](const std::vector<Intent>& l) {
        ++prompts; return yesOrPick(l);
    }, exec, TriggerType::WhenIDie);
    EXPECT_EQ(prompts, 0) << "nothing eligible: no 'you may' offered";
    EXPECT_TRUE(inTrash(P1, t));
}

// ── 487 Rally the Troops: each friendly unit played this turn is buffed ──
TEST_F(Cards3Test, RallyTheTroops_BuffsEachFriendlyUnitPlayedThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    addToDeck(P1, kInvalidId);
    auto spell = pushSpellOnChain(P1, kRally);
    state.chain.items.clear();
    invokeOnResolve(spell, kRally, P1, {}, exec);
    EXPECT_EQ(handSize(P1), 1) << "draw 1";
    ASSERT_EQ(state.delayed_abilities.size(), 1u);

    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    events.emit(CardPlayedEvent{u1, P1, CardType::Unit, 1});
    events.emit(CardPlayedEvent{u2, P1, CardType::Unit, 2});
    ASSERT_EQ(state.chain.items.size(), 2u) << "fires for every friendly unit played";
    EXPECT_EQ(state.chain.items[0].triggering_subject, u1);
    EXPECT_EQ(state.chain.items[1].triggering_subject, u2);
    EXPECT_EQ(state.delayed_abilities.size(), 1u) << "lasts the turn";

    driveResumableTrigger(kRally, P1, spell, nullptr, exec, TriggerType::WhenYouPlayAUnit, u2);
    EXPECT_EQ(state.getObject(u2).buff_count, 1);
    EXPECT_EQ(state.getObject(u1).buff_count, 0);
}

// ── 490 Altar of Memories: draws once per use, every use ──
TEST_F(Cards3Test, AltarOfMemories_DrawsOnceAndCanBeUsedAgain) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto altar = addGear(P1, kAltar);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kInvalidId);
    auto keep = addToHand(P1, kInvalidId);
    for (int use = 0; use < 2; ++use) {
        state.getObject(altar).is_exhausted = false;
        int hand_before = handSize(P1);
        int calls = 0;
        driveResumableTrigger(kAltar, P1, altar, [&](const std::vector<Intent>& l) {
            ++calls;
            if (calls == 1) return pickValue(l, 1);   // yes
            if (calls == 2) return pickValue(l, 1);   // bottom
            return yesOrPick(l, keep);                // the hand card
        }, exec, TriggerType::WhenAFriendlyUnitDies);
        EXPECT_TRUE(state.getObject(altar).is_exhausted);
        EXPECT_EQ(calls, 3) << "use " << use;
        EXPECT_EQ(handSize(P1), hand_before) << "drew 1, put 1 back";
        EXPECT_EQ(state.player(P1).main_deck.front(), keep) << "on the bottom";
        state.player(P1).main_deck.erase(state.player(P1).main_deck.begin());
        state.player(P1).hand.push_back(keep);
        state.getObject(keep).zone = ZoneType::Hand;
    }
}

// ── 501 Danger Zone: all of your Mechs, no target ──
TEST_F(Cards3Test, DangerZone_BuffsEveryFriendlyMech) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto m1 = addUnit(P1, 16, 0, 0);    // Dangerous Duo (Mech)
    auto m2 = addUnit(P1, 20, 0, -1);   // Scrapyard Champion (Mech)
    auto other = addUnit(P1, kInvalidId, 2, 0);
    auto enemy_mech = addUnit(P2, 16, 0, 0);
    int b1 = state.getObject(m1).current_might, b2 = state.getObject(m2).current_might;
    invokeOnResolve(0, kDangerZone, P1, {}, exec);
    EXPECT_EQ(state.getObject(m1).current_might, b1 + 1);
    EXPECT_EQ(state.getObject(m2).current_might, b2 + 1);
    EXPECT_EQ(state.getObject(other).current_might, 2);
    EXPECT_EQ(state.getObject(enemy_mech).current_might, b1);
}

// ── 508 Rabadon's Deathcrown: +3 Bonus Damage while attached ──
TEST_F(Cards3Test, Rabadon_BonusDamageWhileAttached) {
    auto u = addUnit(P1, kInvalidId, 2, 0);
    auto crown = addGear(P1, kRabadon, 0);
    Card* c = card_registry.get(kRabadon);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).bonus_damage_dealt, 0);
    attach(crown, u);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).bonus_damage_dealt, 3);
}

// ── 514 Prodigal Explorer: [Reaction], only after two enemy choices ──
TEST_F(Cards3Test, ProdigalExplorer_NeedsTwoEnemyChoices) {
    Card* c = card_registry.get(kProdigal);
    EXPECT_TRUE(c->isReactionAbility());
    EXPECT_FALSE(c->isActionAbility());
    EXPECT_FALSE(c->canActivateAbility(state, P1));
    auto spell = pushSpellOnChain(P1, kSmite);
    auto enemy_gear = addGear(P2, kInvalidId, 0);
    auto enemy_unit = addUnit(P2, kInvalidId, 2, 0);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    noteEnemyChoice(state, P1, spell, mine);
    noteEnemyChoice(state, P1, spell, enemy_gear);
    EXPECT_FALSE(c->canActivateAbility(state, P1));
    noteEnemyChoice(state, P1, spell, enemy_unit);
    EXPECT_TRUE(c->canActivateAbility(state, P1));
}

// ── 515 Arcane Shift: the friendly unit is replayed once ──
TEST_F(Cards3Test, ArcaneShift_ReplaysTheUnitOnce) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = pushSpellOnChain(P1, kArcaneShift);
    state.chain.items.clear();
    auto friendly = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    int plays = 0;
    auto conn = events.on_card_played.connect([&](const CardPlayedEvent& e) {
        if (e.object == friendly) ++plays;
    });
    driveResolve(kArcaneShift, P1, spell, {friendly},
        [&](const std::vector<Intent>& l) {
            if (offers(l, enemy)) return yesOrPick(l, enemy);
            return pickValue(l, 0);   // base
        }, exec);
    conn.disconnect();
    EXPECT_EQ(plays, 1);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 3);
    EXPECT_TRUE(state.getObject(friendly).location.has_value());
}

// ── 517 Battle Mistress: legend triggers on enemy deaths and rune recycles ──
TEST_F(Cards3Test, BattleMistress_LegendTriggersFire) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kBattleMistress);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    events.emit(UnitDiedEvent{enemy, P2, BattlefieldLocation{0}, 2});
    EXPECT_EQ(chainItemsFrom(legend), 1) << "enemy unit died";
    auto rune = addRune(P1, Domain::Body);
    events.emit(LeftBoardEvent{rune, P1, CardType::Rune, BaseLocation{P1},
                               ZoneType::RuneDeck, false});
    EXPECT_EQ(chainItemsFrom(legend), 2) << "rune recycled";

    driveResumableTrigger(kBattleMistress, P1, legend,
        [](const std::vector<Intent>& l) { return pickValue(l, 1); },
        exec, TriggerType::WhenYouRecycleARune);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    bool gold = false;
    for (auto& [id, o] : state.objects)
        if (o.card_def_id == kGold) { gold = true; EXPECT_TRUE(o.is_exhausted); }
    EXPECT_TRUE(gold);

    driveResumableTrigger(kBattleMistress, P1, legend, nullptr, exec,
                          TriggerType::WhenAnEnemyUnitDies);
    EXPECT_FALSE(state.getObject(legend).is_exhausted) << "ready me";
}

// ── 518 On the Hunt: readies all of your units ──
TEST_F(Cards3Test, OnTheHunt_ReadiesAllYourUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, -1);
    auto e = addUnit(P2, kInvalidId, 2, 0);
    for (auto id : {a, b, e}) state.getObject(id).is_exhausted = true;
    invokeOnResolve(0, kOnTheHunt, P1, {}, exec);
    EXPECT_FALSE(state.getObject(a).is_exhausted);
    EXPECT_FALSE(state.getObject(b).is_exhausted);
    EXPECT_TRUE(state.getObject(e).is_exhausted);
}

// ── 535 Veiled Temple: ready a chosen gear, then may detach it ──
TEST_F(Cards3Test, VeiledTemple_ReadiesChosenGearAndDetaches) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf = installBattlefieldCard(0, kVeiledTemple);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    auto other = addGear(P1, kInvalidId);
    auto eq = addGear(P1, kInvalidId, 0);
    state.getObject(eq).tags = {"Equipment"};
    attach(eq, u);
    state.getObject(eq).is_exhausted = true;
    state.getObject(other).is_exhausted = true;
    driveResumableTrigger(kVeiledTemple, P1, bf, [&](const std::vector<Intent>& l) {
        if (offers(l, eq)) return yesOrPick(l, eq);
        return pickValue(l, 1);
    }, exec, TriggerType::WhenYouConquerHere);
    EXPECT_FALSE(state.getObject(eq).is_exhausted);
    EXPECT_TRUE(state.getObject(other).is_exhausted);
    EXPECT_FALSE(state.getObject(eq).attached_to.has_value()) << "detached";
}

// ── 539 Ahri: only enemy units at her battlefield ──
TEST_F(Cards3Test, Ahri_OnlyEnemiesHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ahri = addUnit(P1, kAhri, 0, 0);
    auto here = addUnit(P2, kInvalidId, 5, 0);
    auto there = addUnit(P2, kInvalidId, 5, 1);
    bool offered_there = false;
    driveResumableTrigger(kAhri, P1, ahri, [&](const std::vector<Intent>& l) {
        offered_there = offers(l, there);
        return yesOrPick(l, here);
    }, exec, TriggerType::WhenIAttackOrDefend);
    EXPECT_FALSE(offered_there);
    EXPECT_EQ(state.getObject(here).current_might, 3);
}

// ── 542 Seal of Strength: adds Body power ──
TEST_F(Cards3Test, SealOfStrength_AddsBody) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto seal = addGear(P1, kSealOfStrength);
    CardContext ctx{state, events, exec, P1, seal};
    card_registry.get(kSealOfStrength)->onActivate(ctx, std::vector<GameObjectId>{});
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)], 1);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)], 0);
}

// ── 547 Darius: other friendly units here get +1 [M] ──
TEST_F(Cards3Test, Darius_OtherFriendlyUnitsHere) {
    auto darius = addUnit(P1, kDarius, 0, 0);
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    Card* c = card_registry.get(kDarius);
    c->applyPassiveAura(state, P1);
    c->applyPassiveAura(state, P1);   // a second pass call doesn't stack
    EXPECT_EQ(auraMight(here), 1);
    EXPECT_EQ(auraMight(darius), 0);
    EXPECT_EQ(auraMight(elsewhere), 0);
    EXPECT_EQ(auraMight(enemy), 0);
}

// ── 554 Grandmaster at Arms: the player picks the Equipment ──
TEST_F(Cards3Test, Grandmaster_PlayerPicksEquipment) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kGrandmaster);
    Card* c = card_registry.get(kGrandmaster);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, legend, 0)) << "no Equipment";
    auto e1 = addGear(P1, kInvalidId);
    auto e2 = addGear(P1, kInvalidId);
    state.getObject(e1).tags = {"Equipment"};
    state.getObject(e2).tags = {"Equipment"};
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, legend, 0));
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, legend, 1)) << "none attached";
    driveActivate(kGrandmaster, P1, legend, 0, [&](const std::vector<Intent>& l) {
        if (offers(l, e2)) return yesOrPick(l, e2);
        return yesOrPick(l, u);
    }, exec);
    EXPECT_EQ(state.getObject(e2).attached_to, std::optional<GameObjectId>(u));
    EXPECT_FALSE(state.getObject(e1).attached_to.has_value());
}

// ── 556 Chem-Baroness: hold only; Gold adds an extra [1] near victory ──
TEST_F(Cards3Test, ChemBaroness_HoldOnlyAndGoldBonus) {
    Card* c = card_registry.get(kChemBaroness);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIHold);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kChemBaroness);
    driveResumableTrigger(kChemBaroness, P1, legend,
        [](const std::vector<Intent>& l) { return pickValue(l, 1); }, exec, TriggerType::WhenIHold);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    GameObjectId gold = kInvalidId;
    for (auto& [id, o] : state.objects) if (o.card_def_id == kGold) gold = id;
    ASSERT_NE(gold, kInvalidId);

    setScore(P1, 4);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).gold_extra_energy, 0);
    setScore(P1, 5);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).gold_extra_energy, 1);
    CardContext ctx{state, events, exec, P1, gold};
    card_registry.get(kGold)->onActivate(ctx, std::vector<GameObjectId>{});
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
}

// ── 563 Prepared Neophyte: any spell of [4]+ this turn ──
TEST_F(Cards3Test, PreparedNeophyte_LaterCheapSpellKeepsBonus) {
    auto n = addUnit(P1, kNeophyte, 0, 0);
    state.player(P1).max_spell_spent_this_turn = 4;
    state.player(P1).last_spell_energy_spent = 1;
    card_registry.get(kNeophyte)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(n), 4);
}

// ── 568 Smite: a unit that dies later this turn is banished ──
TEST_F(Cards3Test, Smite_LaterDeathThisTurnBanishes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = pushSpellOnChain(P1, kSmite);
    state.chain.items.clear();
    auto u = addUnit(P2, kInvalidId, 5, 0);
    invokeOnResolve(spell, kSmite, P1, {u}, exec);
    ASSERT_TRUE(state.getObject(u).location.has_value()) << "survives 3";
    exec.killObject(u);
    EXPECT_EQ(state.getObject(u).zone, ZoneType::Banishment);
    EXPECT_FALSE(inTrash(P2, u));
}

// ── 574 Lord Broadmane: Assault is granted, not printed ──
TEST_F(Cards3Test, LordBroadmane_NoPrintedAssault) {
    const auto& d = card_db.get(kBroadmane);
    EXPECT_FALSE(d.keywords.has(Keyword::Assault));
    EXPECT_EQ(d.assault_value, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto lord = addUnit(P1, kBroadmane, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    fireTriggerAs(kBroadmane, P1, lord, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(state.getObject(ally).keywords.has(Keyword::Assault));
    EXPECT_FALSE(state.getObject(lord).keywords.has(Keyword::Assault));
}

// ── 577 Right of Conquest: 1 + battlefields you control ──
TEST_F(Cards3Test, RightOfConquest_DrawsPerControlledBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 5; ++i) addToDeck(P1, kInvalidId);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P2;
    invokeOnResolve(0, kRightOfConquest, P1, {}, exec);
    EXPECT_EQ(handSize(P1), 2);
}

// ── 578 Scorchclaw: Hunt 2 always; Level 3 = +1 [M] and enters ready ──
TEST_F(Cards3Test, Scorchclaw_HuntAndLevel) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto s = addUnit(P1, kScorchclaw, 0, 0);
    Card* c = card_registry.get(kScorchclaw);
    fireTriggerAs(kScorchclaw, P1, s, TriggerType::WhenIConquerOrHold, exec);
    EXPECT_EQ(state.player(P1).xp, 2);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(s), 0);
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1, state.getObject(s)));
    setXp(P1, 3);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(s), 1);
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1, state.getObject(s)));
}

// ── 579 Square Up: discard only to repeat ──
TEST_F(Cards3Test, SquareUp_DiscardOnlyPaysForTheRepeat) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kSquareUp);
    auto oac = c->optionalAdditionalCost();
    EXPECT_TRUE(oac.valid);
    EXPECT_EQ(oac.discard_cards, 1);
    addToHand(P1, kInvalidId);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    auto spell = pushSpellOnChain(P1, kSquareUp);
    state.chain.items.clear();
    driveResolve(kSquareUp, P1, spell, {a}, [](const std::vector<Intent>& l) { return l.front(); }, exec);
    EXPECT_EQ(handSize(P1), 1) << "base cast discards nothing";
    EXPECT_TRUE(state.getObject(a).keywords.has(Keyword::Assault));
    EXPECT_FALSE(state.getObject(b).keywords.has(Keyword::Assault));

    state.getObject(spell).card_counters[oac.paid_flag] = 1;   // repeat paid
    driveResolve(kSquareUp, P1, spell, {a}, [&](const std::vector<Intent>& l) {
        return yesOrPick(l, b);
    }, exec);
    EXPECT_TRUE(state.getObject(b).keywords.has(Keyword::Assault));
}

// ── 580 Yeti Brawler: needs 3+ excess damage; no [Reaction] ──
TEST_F(Cards3Test, YetiBrawler_ExcessDamageCondition) {
    EXPECT_FALSE(card_db.get(kYeti).keywords.has(Keyword::Reaction));
    auto y = addUnit(P1, kYeti, 0, 0);
    Card* c = card_registry.get(kYeti);
    state.player(P1).last_combat_bf = 0;
    state.player(P1).excess_combat_damage = 2;
    EXPECT_FALSE(c->triggerCondition(state, y, P1, TriggerType::WhenIConquer, kInvalidId));
    state.player(P1).excess_combat_damage = 3;
    EXPECT_TRUE(c->triggerCondition(state, y, P1, TriggerType::WhenIConquer, kInvalidId));
}

// ── 582 Dancing Grenade: the escalation is per turn ──
TEST_F(Cards3Test, DancingGrenade_EscalationResetsNextTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = pushSpellOnChain(P1, kDancingGrenade);
    state.chain.items.clear();
    auto u = addUnit(P2, kInvalidId, 20, 0);
    state.turn.turn_number = 1;
    invokeOnResolve(spell, kDancingGrenade, P1, {u}, exec);
    invokeOnResolve(spell, kDancingGrenade, P1, {u}, exec);
    EXPECT_EQ(state.getObject(u).damage_marked, 5);   // 2 + 3
    state.turn.turn_number = 2;
    invokeOnResolve(spell, kDancingGrenade, P1, {u}, exec);
    EXPECT_EQ(state.getObject(u).damage_marked, 7);   // + 2 again
}

// ── 589 Inviolus Vox: chosen friendly unit +8 ──
TEST_F(Cards3Test, InviolusVox_ChosenFriendlyUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto vox = addUnit(P1, kInviolusVox, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 1);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    bool offered_enemy = false;
    driveResumableTrigger(kInviolusVox, P1, vox, [&](const std::vector<Intent>& l) {
        offered_enemy = offers(l, enemy);
        return yesOrPick(l, ally);
    }, exec, TriggerType::WhenIConquer);
    EXPECT_FALSE(offered_enemy);
    EXPECT_EQ(state.getObject(ally).current_might, 10);
}

// ── 591 Red Brambleback: conquer effects here trigger an additional time ──
TEST_F(Cards3Test, RedBrambleback_ConquerEffectsTwice) {
    auto bramble = addUnit(P1, kBrambleback, 0, 0);
    auto vox = addUnit(P1, kInviolusVox, 0, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Conquer, 1});
    EXPECT_EQ(chainItemsFrom(bramble), 2);
    EXPECT_EQ(chainItemsFrom(vox), 2);
    state.chain.items.clear();
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 2});
    EXPECT_EQ(chainItemsFrom(vox), 0);
}

// ── 594 Double Trouble: the player picks the unit (or none) and draws it ──
TEST_F(Cards3Test, DoubleTrouble_ChooseOrDecline) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bottom = addToDeck(P1, kInvalidId);
    auto u1 = addToDeck(P1, kHarpoonSquad);
    auto u2 = addToDeck(P1, kSandshifter);
    auto top = addToDeck(P1, kSmite);
    driveResumable(kDoubleTrouble, P1, 0, [&](const std::vector<Intent>& l) {
        return yesOrPick(l, offers(l, u1) ? u1 : kInvalidId);
    }, exec);
    EXPECT_TRUE(inHand(P1, u1));
    EXPECT_FALSE(inHand(P1, u2));
    EXPECT_EQ(state.player(P1).draws_this_turn, 1);
    EXPECT_TRUE(inDeck(P1, u2)) << "the other two were recycled";
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(state.player(P1).main_deck.size(), 3u);
    (void)bottom;

    auto v = addToDeck(P1, kHarpoonSquad);
    driveResumable(kDoubleTrouble, P1, 0, [](const std::vector<Intent>& l) {
        return pickValue(l, 0);   // decline
    }, exec);
    EXPECT_FALSE(inHand(P1, v));
    EXPECT_TRUE(inDeck(P1, v));
}

// ── 595 Frisky Hunter: only the Bird has Deflect ──
TEST_F(Cards3Test, FriskyHunter_OnlyBirdHasDeflect) {
    const auto& d = card_db.get(kFriskyHunter);
    EXPECT_FALSE(d.keywords.has(Keyword::Deflect));
    EXPECT_EQ(d.deflect_value, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto f = addUnit(P1, kFriskyHunter, 0, 0);
    fireTriggerAs(kFriskyHunter, P1, f, TriggerType::WhenYouPlayMe, exec);
    GameObjectId bird = kInvalidId;
    for (auto& [id, o] : state.objects) if (o.name == "Bird") bird = id;
    ASSERT_NE(bird, kInvalidId);
    EXPECT_EQ(state.getObject(bird).card_def_id, kBird);
    EXPECT_EQ(deflectCostFor(state, P2, bird), 1);
    EXPECT_EQ(deflectCostFor(state, P2, f), 0);
}

// ── 596 Herald of Spring: [Hunt] gains 1 XP ──
TEST_F(Cards3Test, HeraldOfSpring_Hunt) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto h = addUnit(P1, kHerald, 0, 0);
    EXPECT_TRUE(card_registry.get(kHerald)->firesOn(TriggerType::WhenIConquerOrHold));
    fireTriggerAs(kHerald, P1, h, TriggerType::WhenIConquerOrHold, exec);
    EXPECT_EQ(state.player(P1).xp, 1);
    fireTriggerAs(kHerald, P1, h, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.player(P1).xp, 3);
}

// ── 600 Skyward Strike: move, and at Level 6 also stun ──
TEST_F(Cards3Test, SkywardStrike_MoveAndLevel6Stun) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = pushSpellOnChain(P1, kSkywardStrike);
    state.chain.items.clear();
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    setXp(P1, 6);
    int calls = 0;
    driveResumable(kSkywardStrike, P1, spell, [&](const std::vector<Intent>& l) {
        ++calls;
        if (calls == 1) return yesOrPick(l, a);   // move a
        if (calls == 2) return yesOrPick(l, b);   // stun b
        // destination: battlefield 1
        for (const auto& i : l)
            if (i.choice_label == "battlefield 1") return i;
        return l.front();
    }, exec);
    EXPECT_EQ(state.getObject(a).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_TRUE(state.getObject(b).is_stunned);
    EXPECT_FALSE(state.getObject(a).is_stunned);

    setXp(P1, 0);
    calls = 0;
    driveResumable(kSkywardStrike, P1, spell, [&](const std::vector<Intent>& l) {
        ++calls;
        if (calls == 1) return yesOrPick(l, b);
        return pickValue(l, 0);   // base
    }, exec);
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(b).location));
}

}  // namespace
}  // namespace riftbound::test
