// Audit follow-up 2026-10 — batch cards4 (cards 604..702).
#include "tests/cards/card_test_fixture.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <memory>

using namespace riftbound;
using namespace riftbound::test;

namespace {

using Picker = std::function<Intent(const std::vector<Intent>&)>;

class Cards4Test : public CardTestFixture {
protected:
    static Intent pickBack(const std::vector<Intent>& legal) {
        return legal.empty() ? Intent{} : legal.back();
    }
    static Intent pickFront(const std::vector<Intent>& legal) {
        return legal.empty() ? Intent{} : legal.front();
    }
    static Picker pickValue(int v) {
        return [v](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
            return legal.empty() ? Intent{} : legal.front();
        };
    }
    // Picks the option choosing one of `ids` (in order of preference), else a
    // yes (chosen_value 1), else back().
    static Picker pickObjs(std::vector<GameObjectId> ids) {
        return [ids](const std::vector<Intent>& legal) -> Intent {
            for (auto id : ids)
                for (auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
            for (auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
            return legal.empty() ? Intent{} : legal.back();
        };
    }
    // Scripted pickers, one per prompt, then back().
    static Picker seq(std::vector<Picker> steps) {
        auto idx = std::make_shared<size_t>(0);
        return [steps, idx](const std::vector<Intent>& legal) -> Intent {
            if (*idx < steps.size()) return steps[(*idx)++](legal);
            return legal.empty() ? Intent{} : legal.back();
        };
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        obj.name = card_db.get(def_id).name;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }
    // Drive an activated ability's onActivate through the choice loop.
    void driveActivate(CardDefId def_id, PlayerId controller, GameObjectId source,
                       int ability_index, Picker picker, EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.ability_index = ability_index;
        ri.source = source;
        ri.card_def_id = def_id;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onActivate(ctx, ability_index, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onActivate(ctx, ability_index, {});
        }
        state.chain.resuming.reset();
    }
    bool hasAuraKeyword(GameObjectId id, Keyword kw) {
        for (auto& ae : state.getObject(id).aura_effects)
            if (ae.keyword == kw) return true;
        return false;
    }
    int auraMight(GameObjectId id) {
        int m = 0;
        for (auto& ae : state.getObject(id).aura_effects) m += ae.might_bonus;
        return m;
    }
    bool chainHasItemFrom(GameObjectId source) {
        for (auto& it : state.chain.items) if (it.source == source) return true;
        return false;
    }
};

// Engine-driven fixture (GameEngine owns its own state).
class Cards4EngineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    FirstChoiceAgent a1, a2;
    GameState& es() { return engine_->mutableState(); }
    void SetUp() override {
        CardTestFixture::SetUp();
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        engine_->testHook_setAgents(&a1, &a2);
        auto& s = es();
        s.mode = ModeOfPlay{};
        s.players[0].id = P1;
        s.players[1].id = P2;
        for (int i = 0; i < 2; ++i) {
            BattlefieldState bf;
            bf.id = i;
            s.battlefields.push_back(bf);
        }
        for (int p = 0; p < 2; ++p) {
            auto leg = s.createObject();
            auto& lo = s.getObject(leg);
            lo.owner = lo.controller = s.players[p].id;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            s.players[p].legend_zone = leg;
        }
        s.turn.turn_player = P1;
    }
    GameObjectId unitAt(PlayerId owner, CardDefId def_id, int might, int bf,
                        bool temporary = false) {
        auto& s = es();
        auto id = s.createObject();
        auto& u = s.getObject(id);
        u.owner = u.controller = owner;
        u.card_def_id = def_id;
        u.card_type = CardType::Unit;
        u.name = def_id != kInvalidId ? card_db.get(def_id).name : "Unit";
        if (def_id != kInvalidId) u.keywords = card_db.get(def_id).keywords;
        u.base_might = u.current_might = might;
        if (temporary) u.keywords.set(Keyword::Temporary);
        if (bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{static_cast<BattlefieldId>(bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{owner};
        }
        return id;
    }
};

// ─── [604] Back Off ─────────────────────────────────────────────────────────

TEST_F(Cards4Test, BackOff_FromHandStunsAndDraws) {
    EffectExecutor exec(state, events, card_db);
    auto spell = addToHand(P1, 604);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    addToDeck(P1, kInvalidId);
    int hand = handSize(P1);
    driveResumable(604, P1, spell, pickObjs({enemy}), exec);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_EQ(handSize(P1), hand + 1);
}

TEST_F(Cards4Test, BackOff_FromFaceDownDoesNotDraw) {
    EffectExecutor exec(state, events, card_db);
    auto spell = addToHand(P1, 604);
    state.getObject(spell).card_counters["__played_from_facedown"] = 1;
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    addToDeck(P1, kInvalidId);
    int hand = handSize(P1);
    driveResumable(604, P1, spell, pickObjs({enemy}), exec);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_EQ(handSize(P1), hand) << "played from face down -> no draw";
    EXPECT_EQ(state.getObject(spell).card_counters["__played_from_facedown"], 0)
        << "marker consumed";
}

// ─── [607] Forgotten Signpost ───────────────────────────────────────────────

TEST_F(Cards4Test, ForgottenSignpost_ExhaustsChosenUnitAndMovesAnother) {
    EffectExecutor exec(state, events, card_db);
    auto gear = addGear(P1, 607);
    auto payer = addUnit(P1, kInvalidId, 2, 1);
    auto mover = addUnit(P1, kInvalidId, 2, -1);
    driveActivate(607, P1, gear, 0, pickObjs({payer, mover}), exec);
    EXPECT_TRUE(state.getObject(payer).is_exhausted) << "the additional cost is paid";
    ASSERT_TRUE(state.getObject(mover).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(mover).battlefieldId(), 1u);
}

TEST_F(Cards4Test, ForgottenSignpost_NotActivatableWithoutASecondUnit) {
    auto gear = addGear(P1, 607);
    addUnit(P1, kInvalidId, 2, 1);
    Card* c = card_registry.get(607);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, gear, 0));
    addUnit(P1, kInvalidId, 2, -1);
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, gear, 0));
    EXPECT_EQ(c->choicesAtFinalization() & Card::kFinalizeActivate,
              static_cast<unsigned>(Card::kFinalizeActivate))
        << "the exhaust cost is chosen and paid as the ability is played";
}

// ─── [609] Mosstomper ───────────────────────────────────────────────────────

TEST_F(Cards4Test, Mosstomper_DeflectOnlyAtLevel3) {
    const auto& d = card_db.get(609);
    EXPECT_FALSE(d.keywords.has(Keyword::Deflect));
    EXPECT_EQ(d.deflect_value, 0);
    auto m = addUnit(P1, 609, 0, 0);
    Card* c = card_registry.get(609);
    setXp(P1, 2);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(hasAuraKeyword(m, Keyword::Deflect));
    state.getObject(m).aura_effects.clear();
    setXp(P1, 3);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(m, Keyword::Deflect));
    EXPECT_EQ(auraMight(m), 1);
    state.getObject(m).aura_keywords.set(Keyword::Deflect);   // as recalculateAuras does
    EXPECT_EQ(deflectCostFor(state, P2, m), 1) << "Deflect 1, not 2";
}

// ─── [610] Trevor Snoozebottom / [646] Sprite Queen / [638] Petal Pixie ─────

TEST_F(Cards4Test, TemporaryOnlyOnTheTokens) {
    for (CardDefId id : {610, 646, 638, 644, 652})
        EXPECT_FALSE(card_db.get(id).keywords.has(Keyword::Temporary)) << id;
}

TEST_F(Cards4Test, Trevor_HoldPlaysReadyTemporarySpriteHere) {
    EffectExecutor exec(state, events, card_db);
    auto trevor = addUnit(P1, 610, 0, 1);
    fireTriggerAs(610, P1, trevor, TriggerType::WhenIHold, exec);
    int sprites = 0;
    for (auto& [id, o] : state.objects) {
        if (o.name != "Sprite") continue;
        ++sprites;
        EXPECT_TRUE(o.keywords.has(Keyword::Temporary));
        EXPECT_FALSE(o.is_exhausted);
        EXPECT_EQ(o.battlefieldId(), std::optional<BattlefieldId>(1));
    }
    EXPECT_EQ(sprites, 1);
}

TEST_F(Cards4Test, PetalPixie_CountsOnlyTemporaryUnits) {
    auto pixie = addUnit(P1, 638, 0, 0);
    Card* c = card_registry.get(638);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(pixie), 0) << "Pixie herself is not [Temporary]";
    auto t = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(t).keywords.set(Keyword::Temporary);
    state.getObject(pixie).aura_effects.clear();
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(pixie), 1);
}

// ─── [611] Honeyfruit ───────────────────────────────────────────────────────

TEST_F(Cards4Test, Honeyfruit_Level6AbilityGatedAndAddsEnergyPlusPower) {
    EffectExecutor exec(state, events, card_db);
    auto gear = addGear(P1, 611);
    Card* c = card_registry.get(611);
    setXp(P1, 5);
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, gear, 0));
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, gear, 1)) << "below 6 XP";
    setXp(P1, 6);
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, gear, 1));
    CardContext ctx{state, events, exec, P1, gear};
    c->onActivate(ctx, 1, {});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

// ─── [612] Iascylla ─────────────────────────────────────────────────────────

TEST_F(Cards4Test, Iascylla_PullsEnemyFromBaseNotOneAlreadyHere) {
    EffectExecutor exec(state, events, card_db);
    auto ias = addUnit(P1, 612, 0, 0);
    auto at_base = addUnit(P2, kInvalidId, 2, -1);
    auto already = addUnit(P2, kInvalidId, 2, 0);
    fireTriggerAs(612, P1, ias, TriggerType::WhenIHold, exec);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    // Iascylla leaves the board before the delayed ability resolves.
    state.getObject(ias).location.reset();
    state.getObject(ias).zone = ZoneType::Trash;
    std::vector<std::vector<GameObjectId>> offered;
    driveResumableTrigger(612, P1, ias,
        [&](const std::vector<Intent>& legal) -> Intent {
            std::vector<GameObjectId> objs;
            for (auto& i : legal) if (!i.chosen_objects.empty()) objs.push_back(i.chosen_objects[0]);
            if (!objs.empty()) offered.push_back(objs);
            return pickObjs({at_base})(legal);
        }, exec, TriggerType::AtStartOfMain);
    ASSERT_EQ(offered.size(), 1u);
    EXPECT_EQ(offered[0], std::vector<GameObjectId>{at_base}) << "unit already here excluded";
    EXPECT_EQ(state.getObject(at_base).battlefieldId(), std::optional<BattlefieldId>(0));
    (void)already;
}

// ─── [613] Ivern, Nurturer ──────────────────────────────────────────────────

TEST_F(Cards4Test, IvernNurturer_PlayerChoosesRevealedUnitAndBuffTarget) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, 613, 4, 0);
    auto buffed_already = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(buffed_already).buff_count = 1;
    auto plain = addUnit(P1, kInvalidId, 2, 1);
    auto yone = addToDeck(P1, 544);     // non-themed unit
    auto rengar = addToDeck(P1, 682);   // Cat
    auto spell = addToDeck(P1, 466);
    int draws = state.player(P1).draws_this_turn;
    driveResumableTrigger(613, P1, ivern,
        seq({pickValue(1), pickObjs({rengar}), pickObjs({plain})}), exec,
        TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inHand(P1, rengar)) << "the chosen unit (not the first) is drawn";
    EXPECT_EQ(state.player(P1).draws_this_turn, draws + 1) << "a real draw";
    EXPECT_TRUE(inDeck(P1, yone));
    EXPECT_TRUE(inDeck(P1, spell));
    EXPECT_EQ(state.getObject(plain).buff_count, 1) << "the chosen friendly unit is buffed";
    EXPECT_EQ(state.getObject(buffed_already).buff_count, 1);
}

// ─── [614] Nami, Headstrong ─────────────────────────────────────────────────

TEST_F(Cards4Test, Nami_PaidStunTakesTargetWithPickTarget) {
    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(614);
    EXPECT_EQ(c->getTargetRequirements().count, 0);
    auto nami = addUnit(P1, 614, 0, 0);
    state.getObject(nami).card_counters["__nami_paid"] = 1;
    addUnit(P2, kInvalidId, 2, 0);
    auto e2 = addUnit(P2, kInvalidId, 2, 1);
    driveResumableTrigger(614, P1, nami, pickObjs({e2}), exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(e2).is_stunned);
}

// ─── [616] Tricksy Tentacles ────────────────────────────────────────────────

TEST_F(Cards4Test, TricksyTentacles_MovesSeveralWithinBudgetToChosenBattlefield) {
    EffectExecutor exec(state, events, card_db);
    auto spell = addToHand(P1, 616);
    auto a = addUnit(P2, kInvalidId, 5, -1);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 1, 0);
    std::vector<size_t> offers;
    int prompt = 0;
    driveResumable(616, P1, spell,
        [&](const std::vector<Intent>& legal) -> Intent {
            ++prompt;
            if (prompt == 1) return pickValue(2)(legal);   // battlefield 1
            offers.push_back(legal.size());
            if (prompt == 2) return pickObjs({a})(legal);  // 5 [M]
            if (prompt == 3) return pickObjs({b})(legal);  // 3 [M] -> total 8
            return legal.back();                            // "Done"
        }, exec);
    EXPECT_EQ(state.getObject(a).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_EQ(state.getObject(b).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_EQ(state.getObject(c).battlefieldId(), std::optional<BattlefieldId>(0))
        << "1 [M] more would exceed the total of 8";
    ASSERT_EQ(offers.size(), 2u) << "budget spent: no further prompt";
    EXPECT_EQ(offers[0], 4u) << "a, b, c + Done";
    EXPECT_EQ(offers[1], 3u) << "after 5 [M]: b, c + Done";
}

// ─── [618] Yuumi, Magical Cat ───────────────────────────────────────────────

TEST_F(Cards4Test, Yuumi_GivesAnotherUnitHereMightAndTank) {
    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(card_db.get(618).keywords.has(Keyword::Tank));
    EXPECT_EQ(card_registry.get(618)->getTargetRequirements().count, 0);
    auto yuumi = addUnit(P1, 618, 0, 0);
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(618, P1, yuumi,
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
            return pickObjs({here})(legal);
        }, exec, TriggerType::WhenIAttackOrDefend);
    EXPECT_EQ(offered, std::vector<GameObjectId>{here}) << "only other units here";
    EXPECT_EQ(state.getObject(here).current_might, 5);
    EXPECT_TRUE(state.getObject(here).keywords.has(Keyword::Tank));
    EXPECT_FALSE(state.getObject(yuumi).keywords.has(Keyword::Tank));
    (void)elsewhere;
}

// ─── [619] Alpha Wildclaw ───────────────────────────────────────────────────

TEST_F(Cards4EngineTest, AlphaWildclaw_ProtectsSmallerUnitsHere) {
    auto claw = unitAt(P1, 619, 7, 0);
    auto small_here = unitAt(P1, kInvalidId, 3, 0);
    auto big_here = unitAt(P1, kInvalidId, 9, 0);
    auto small_away = unitAt(P1, kInvalidId, 3, 1);
    engine_->testHook_cleanup();
    EXPECT_TRUE(es().getObject(small_here).untargetable_by_enemy);
    EXPECT_FALSE(es().getObject(big_here).untargetable_by_enemy);
    EXPECT_FALSE(es().getObject(small_away).untargetable_by_enemy);
    EXPECT_FALSE(es().getObject(claw).untargetable_by_enemy);
}

// ─── [620] Lillia, Protector of Dreams ──────────────────────────────────────

TEST_F(Cards4Test, LilliaProtector_TokenPlayTriggersAndNoOwnTank) {
    EXPECT_FALSE(card_db.get(620).keywords.has(Keyword::Tank));
    auto lillia = addUnit(P1, 620, 0, 0);
    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{},
                     BaseLocation{P1}, false);
    ASSERT_TRUE(chainHasItemFrom(lillia)) << "When you play a token unit";
    fireTriggerAs(620, P1, lillia, TriggerType::WhenYouPlayATokenUnit, exec);
    EXPECT_EQ(state.getObject(lillia).current_might, 5);
}

TEST_F(Cards4Test, LilliaProtector_EnemyTokenDoesNotTrigger) {
    auto lillia = addUnit(P1, 620, 0, 0);
    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    exec.createToken(P2, CardType::Unit, "Recruit", 1, {}, KeywordSet{},
                     BaseLocation{P2}, false);
    EXPECT_FALSE(chainHasItemFrom(lillia));
}

// ─── [621] Master Yi, Unstoppable ───────────────────────────────────────────

TEST_F(Cards4Test, MasterYiUnstoppable_PowerReductionAndLevel16) {
    Card* c = card_registry.get(621);
    setXp(P1, 6);
    EXPECT_EQ(c->selfCostReduction(state, P1), 4);
    EXPECT_EQ(c->selfPowerCostReduction(state, P1), 2);
    setXp(P1, 11);
    EXPECT_EQ(c->selfPowerCostReduction(state, P1), 3);
    auto yi = addUnit(P1, 621, 0, 0);
    setXp(P1, 15);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.getObject(yi).untargetable_by_enemy);
    setXp(P1, 16);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(yi).untargetable_by_enemy);
}

// ─── [625] Eclipse ──────────────────────────────────────────────────────────

TEST_F(Cards4Test, Eclipse_PredictsAfterMinusFour) {
    EffectExecutor exec(state, events, card_db);
    exec.setAgentQuery([](PlayerId, const std::vector<Intent>& legal) { return legal.front(); });
    auto u = addUnit(P2, kInvalidId, 5, 0);
    auto bottom = addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);
    invokeOnResolve(0, 625, P1, {u}, exec);
    EXPECT_EQ(state.getObject(u).current_might, 1);
    // legal.front() = recycle the looked-at card.
    EXPECT_EQ(state.player(P1).main_deck.front(), top);
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom);
}

// ─── [629] Ruined Rex ───────────────────────────────────────────────────────

TEST_F(Cards4Test, RuinedRex_DamagesChosenEnemyEvenAtBase) {
    EffectExecutor exec(state, events, card_db);
    auto rex = addUnit(P1, 629, 0, 0);
    auto at_bf = addUnit(P2, kInvalidId, 6, 0);
    auto at_base = addUnit(P2, kInvalidId, 6, -1);
    driveResumableTrigger(629, P1, rex, pickObjs({at_base}), exec, TriggerType::WhenIDie);
    EXPECT_EQ(state.getObject(at_base).damage_marked, 4);
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 0);
}

// ─── [632] Turn to Dust ─────────────────────────────────────────────────────

TEST_F(Cards4Test, TurnToDust_GrantsLastingTemporary) {
    EffectExecutor exec(state, events, card_db);
    auto gear = addGear(P2, 611);
    invokeOnResolve(0, 632, P1, {gear}, exec);
    EXPECT_TRUE(state.getObject(gear).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(state.getObject(gear).temp_keywords.has(Keyword::Temporary))
        << "not a this-turn grant (it would be revoked before the kill)";
}

// ─── [633] Chakram Dancer ───────────────────────────────────────────────────

TEST_F(Cards4Test, ChakramDancer_NoOwnShieldButGrantsOthersHere) {
    const auto& d = card_db.get(633);
    EXPECT_FALSE(d.keywords.has(Keyword::Shield));
    EXPECT_EQ(d.shield_value, 0);
    EffectExecutor exec(state, events, card_db);
    auto dancer = addUnit(P1, 633, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    fireTriggerAs(633, P1, dancer, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(state.getObject(other).keywords.has(Keyword::Shield));
    EXPECT_FALSE(state.getObject(dancer).keywords.has(Keyword::Shield));
}

// ─── [634] Crescent Strike ──────────────────────────────────────────────────

TEST_F(Cards4Test, CrescentStrike_SplashesOtherEnemiesThere) {
    EffectExecutor exec(state, events, card_db);
    auto main = addUnit(P2, kInvalidId, 6, 0);
    auto other = addUnit(P2, kInvalidId, 1, 0);
    auto away = addUnit(P2, kInvalidId, 3, 1);
    auto friendly = addUnit(P1, kInvalidId, 3, 0);
    invokeOnResolve(0, 634, P1, {main}, exec);
    EXPECT_EQ(state.getObject(main).damage_marked, 4);
    EXPECT_FALSE(state.getObject(other).location.has_value()) << "1 damage killed the 1 [M] unit";
    EXPECT_EQ(state.getObject(away).damage_marked, 0);
    EXPECT_EQ(state.getObject(friendly).damage_marked, 0);
    auto base_enemy = addUnit(P2, kInvalidId, 3, -1);
    auto legal = card_registry.get(634)->enumerateLegalTargets(state, P1);
    EXPECT_EQ(std::find(legal.begin(), legal.end(), base_enemy), legal.end())
        << "the unit must be at a battlefield";
}

// ─── [635] Deadly Flourish ──────────────────────────────────────────────────

TEST_F(Cards4Test, DeadlyFlourish_EnemyOnlyAndWatchExpiresThisTurn) {
    EffectExecutor exec(state, events, card_db);
    auto friendly = addUnit(P1, kInvalidId, 5, 0);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    auto legal = card_registry.get(635)->enumerateLegalTargets(state, P1);
    EXPECT_EQ(std::find(legal.begin(), legal.end(), friendly), legal.end());
    state.turn.turn_number = 7;
    invokeOnResolve(0, 635, P1, {}, exec);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].target_filter, enemy);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, 7);
}

// ─── [636] Frigid Jewel ─────────────────────────────────────────────────────

TEST_F(Cards4Test, FrigidJewel_PlayerChoosesUnitOncePerTurn) {
    EffectExecutor exec(state, events, card_db);
    auto jewel = addGear(P1, 636);
    auto a = addUnit(P1, kInvalidId, 2, -1);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    state.player(P1).draws_this_turn = 2;
    Card* c = card_registry.get(636);
    EXPECT_TRUE(c->triggerCondition(state, jewel, P1, TriggerType::WhenYouDrawACard, kInvalidId));
    driveResumableTrigger(636, P1, jewel, pickObjs({b}), exec, TriggerType::WhenYouDrawACard);
    EXPECT_EQ(state.getObject(b).current_might, 4);
    EXPECT_EQ(state.getObject(a).current_might, 2);
    EXPECT_FALSE(c->triggerCondition(state, jewel, P1, TriggerType::WhenYouDrawACard, kInvalidId))
        << "once per turn";
}

// ─── [637] Gustwalker / [675] Master Yi, Tempered ───────────────────────────

TEST_F(Cards4Test, Gustwalker_GankingOnlyAtLevel3) {
    EXPECT_FALSE(card_db.get(637).keywords.has(Keyword::Ganking));
    auto g = addUnit(P1, 637, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    Card* c = card_registry.get(637);
    setXp(P1, 3);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(g, Keyword::Ganking));
    EXPECT_FALSE(hasAuraKeyword(other, Keyword::Ganking));
}

TEST_F(Cards4Test, MasterYiTempered_DeflectAndGankingOnlyAtLevel6) {
    const auto& d = card_db.get(675);
    EXPECT_FALSE(d.keywords.has(Keyword::Deflect));
    EXPECT_FALSE(d.keywords.has(Keyword::Ganking));
    EXPECT_EQ(d.deflect_value, 0);
    auto yi = addUnit(P1, 675, 0, 0);
    Card* c = card_registry.get(675);
    setXp(P1, 5);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(hasAuraKeyword(yi, Keyword::Deflect));
    setXp(P1, 6);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(yi, Keyword::Deflect));
    EXPECT_TRUE(hasAuraKeyword(yi, Keyword::Ganking));
    state.getObject(yi).aura_keywords.set(Keyword::Deflect);   // as recalculateAuras does
    EXPECT_EQ(deflectCostFor(state, P2, yi), 1) << "Deflect 1, not 2";
}

// ─── [644] Lillia, Fae Fawn ─────────────────────────────────────────────────

TEST_F(Cards4Test, LilliaFaeFawn_SpriteAtOriginNotReady) {
    EffectExecutor exec(state, events, card_db);
    auto lillia = addUnit(P1, 644, 0, 1);
    state.getObject(lillia).card_counters["__moved_from_bf"] = 0;   // moved 0 -> 1
    driveResumableTrigger(644, P1, lillia, pickBack, exec, TriggerType::WhenIMove);
    GameObjectId sprite = kInvalidId;
    for (auto& [id, o] : state.objects) if (o.name == "Sprite") sprite = id;
    ASSERT_NE(sprite, kInvalidId);
    EXPECT_EQ(state.getObject(sprite).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_TRUE(state.getObject(sprite).is_exhausted) << "the text doesn't say ready";
}

// ─── [646] Sprite Queen ─────────────────────────────────────────────────────

TEST_F(Cards4EngineTest, SpriteQueen_SurvivesHerBeginningPhase) {
    auto queen = unitAt(P1, 646, 6, -1);
    engine_->testHook_beginningStep();
    EXPECT_TRUE(es().getObject(queen).location.has_value());
}

// ─── [649] Blue Sentinel ────────────────────────────────────────────────────

TEST_F(Cards4EngineTest, BlueSentinel_HoldEffectsHereTriggerTwiceAndAddLater) {
    auto sentinel = unitAt(P1, 649, 4, 0);
    es().battlefields[0].controller = P1;
    engine_->testHook_scoreHold(P1, 0);
    int sentinel_items = 0;
    for (auto& it : es().chain.items) if (it.source == sentinel) ++sentinel_items;
    EXPECT_EQ(sentinel_items, 2) << "its own hold effect triggers an additional time";

    // The [Add] [A] is delayed to the next Main Phase.
    EffectExecutor exec(state, events, card_db);
    auto s = addUnit(P1, 649, 4, 0);
    fireTriggerAs(649, P1, s, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::AtStartOfMain);
    fireTriggerAs(649, P1, s, TriggerType::AtStartOfMain, exec);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

// ─── [652] LeBlanc, Everywhere at Once ──────────────────────────────────────

TEST_F(Cards4EngineTest, LeBlanc_TemporaryUnitsAtHerBattlefieldAreNotKilled) {
    auto leblanc = unitAt(P1, 652, 4, 0);
    auto temp_here = unitAt(P1, kInvalidId, 3, 0, /*temporary=*/true);
    auto temp_away = unitAt(P1, kInvalidId, 3, 1, /*temporary=*/true);
    engine_->testHook_beginningStep();
    EXPECT_TRUE(es().getObject(leblanc).location.has_value()) << "LeBlanc has no [Temporary]";
    EXPECT_TRUE(es().getObject(temp_here).location.has_value());
    EXPECT_FALSE(es().getObject(temp_away).location.has_value());
}

// ─── [666] Gentle Gemdragon ─────────────────────────────────────────────────

TEST_F(Cards4Test, GentleGemdragon_OnlyAnotherDragonTriggers) {
    auto gem = addUnit(P1, 666, 0, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto plain = addUnit(P1, kInvalidId, 2, -1);
    events.emit(CardPlayedEvent{plain, P1, CardType::Unit, 1, 0});
    EXPECT_FALSE(chainHasItemFrom(gem)) << "a non-Dragon unit does not trigger";
    auto dragon = addUnit(P1, kInvalidId, 2, -1);
    state.getObject(dragon).tags = {"Dragon"};
    events.emit(CardPlayedEvent{dragon, P1, CardType::Unit, 2, 0});
    EXPECT_TRUE(chainHasItemFrom(gem));
}

TEST_F(Cards4Test, GentleGemdragon_PlayerChoosesTwoRunes) {
    EffectExecutor exec(state, events, card_db);
    auto gem = addUnit(P1, 666, 0, 0);
    auto r1 = addRune(P1, Domain::Body, true);
    auto r2 = addRune(P1, Domain::Fury, true);
    auto r3 = addRune(P1, Domain::Calm, true);
    driveResumableTrigger(666, P1, gem, pickObjs({r3, r2}), exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(r1).is_exhausted);
    EXPECT_FALSE(state.getObject(r2).is_exhausted);
    EXPECT_FALSE(state.getObject(r3).is_exhausted);
}

// ─── [669] Stare Down ───────────────────────────────────────────────────────

TEST_F(Cards4Test, StareDown_OneXpAndChosenBattlefield) {
    EffectExecutor exec(state, events, card_db);
    auto spell = addToHand(P1, 669);
    auto mine = addUnit(P1, kInvalidId, 4, -1);
    auto e0 = addUnit(P2, kInvalidId, 2, 0);
    auto e0b = addUnit(P2, kInvalidId, 1, 0);
    auto e1 = addUnit(P2, kInvalidId, 3, 1);
    setXp(P1, 0);
    driveResumable(669, P1, spell, seq({pickValue(1), pickObjs({mine})}), exec);
    EXPECT_EQ(state.player(P1).xp, 1) << "exactly 1 XP";
    EXPECT_TRUE(state.getObject(e1).isAtBase()) << "battlefield 1 was chosen";
    EXPECT_FALSE(state.getObject(e0).isAtBase());
    EXPECT_FALSE(state.getObject(e0b).isAtBase());
}

// ─── [670] Wily Newtfish ────────────────────────────────────────────────────

TEST_F(Cards4EngineTest, WilyNewtfish_XpGainIsTrackedAndBuffs) {
    auto newt = unitAt(P1, 670, 4, 0);
    engine_->testHook_cleanup();
    EXPECT_EQ(es().player(P1).xp_gained_this_turn, 0);
    EXPECT_FALSE(es().getObject(newt).hasKeyword(Keyword::Ganking));
    es().player(P1).xp += 2;    // a card gains XP the usual way
    engine_->testHook_cleanup();
    EXPECT_EQ(es().player(P1).xp_gained_this_turn, 2);
    EXPECT_TRUE(es().getObject(newt).hasKeyword(Keyword::Ganking));
    EXPECT_EQ(es().getObject(newt).current_might, 5);
    es().player(P1).xp -= 1;    // spending is not a gain
    engine_->testHook_cleanup();
    EXPECT_EQ(es().player(P1).xp_gained_this_turn, 2);
}

// ─── [671] Blood Rose ───────────────────────────────────────────────────────

TEST_F(Cards4Test, BloodRose_CanReadyAnEnemyUnit) {
    EffectExecutor exec(state, events, card_db);
    auto gear = addGear(P1, 671);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    state.getObject(enemy).is_exhausted = true;
    auto legal = card_registry.get(671)->enumerateLegalTargets(state, P1, 0, gear);
    EXPECT_NE(std::find(legal.begin(), legal.end(), enemy), legal.end());
    driveActivate(671, P1, gear, 0, pickObjs({enemy}), exec);
    EXPECT_FALSE(state.getObject(enemy).is_exhausted);
}

// ─── [674] Irresistible Faefolk ─────────────────────────────────────────────

TEST_F(Cards4Test, IrresistibleFaefolk_PlayerChoosesEnemy) {
    EffectExecutor exec(state, events, card_db);
    auto fae = addUnit(P1, 674, 0, 0);
    auto first = addUnit(P2, kInvalidId, 2, 1);
    auto second = addUnit(P2, kInvalidId, 2, -1);
    driveResumableTrigger(674, P1, fae, pickObjs({second}), exec, TriggerType::WhenIMoveToFB);
    EXPECT_EQ(state.getObject(second).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_EQ(state.getObject(first).battlefieldId(), std::optional<BattlefieldId>(1));
}

// ─── [679] Arachnoid Horror ─────────────────────────────────────────────────

TEST_F(Cards4Test, ArachnoidHorror_PlayableWhereAnEnemyIsAlone) {
    Card* c = card_registry.get(679);
    state.battlefields[0].controller = P2;
    state.battlefields[1].controller = P2;
    addUnit(P2, kInvalidId, 3, 0);                 // alone at BF 0
    addUnit(P2, kInvalidId, 3, 1);
    addUnit(P2, kInvalidId, 3, 1);                 // two at BF 1
    auto locs = c->getPlayLocations(state, P1);
    auto has = [&](LocationId l) { return std::find(locs.begin(), locs.end(), l) != locs.end(); };
    EXPECT_TRUE(has(BaseLocation{P1}));
    EXPECT_TRUE(has(BattlefieldLocation{0}));
    EXPECT_FALSE(has(BattlefieldLocation{1}));
    addUnit(P1, 679, 0, -1);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).grant_friendly_units_lone_enemy_bf);
}

// ─── [684] Crescent Guardian ────────────────────────────────────────────────

TEST_F(Cards4Test, CrescentGuardian_CostOfferedOnlyAfterASpell) {
    Card* c = card_registry.get(684);
    EXPECT_FALSE(c->optionalAdditionalCost(state, P1).valid);
    state.player(P1).played_spell_this_turn = true;   // e.g. a 0-cost spell
    EXPECT_TRUE(c->optionalAdditionalCost(state, P1).valid);
    EffectExecutor exec(state, events, card_db);
    auto g = addUnit(P1, 684, 0, 0);
    state.getObject(g).is_exhausted = true;
    state.getObject(g).card_counters["__crescent_paid"] = 1;
    fireTriggerAs(684, P1, g, TriggerType::WhenYouPlayMe, exec);
    EXPECT_FALSE(state.getObject(g).is_exhausted);
}

// ─── [688] Megatusk ─────────────────────────────────────────────────────────

TEST_F(Cards4Test, Megatusk_XpIsTheActivationCost) {
    Card* c = card_registry.get(688);
    EXPECT_FALSE(card_db.get(688).keywords.has(Keyword::Ganking));
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_EQ(abs[0].cost.xp_cost, 3);
}

// ─── [695] Blast Cone ───────────────────────────────────────────────────────

TEST_F(Cards4Test, BlastCone_MoveTriggerFiresForAnyEnemyMoveYouMake) {
    auto cone = addGear(P1, 695);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    // An effect P1 controls (e.g. a spell) moves the enemy unit.
    ChainItem resolving;
    resolving.controller = P1;
    state.chain.resuming = resolving;
    exec.moveToBase(enemy);
    state.chain.resuming.reset();
    ASSERT_TRUE(chainHasItemFrom(cone));
    EXPECT_EQ(state.chain.items.back().triggering_subject, enemy);
    // Resolve it: yes -> exhaust Blast Cone, stun the unit.
    state.chain.items.clear();
    driveResumableTrigger(695, P1, cone, pickBack, exec,
                          TriggerType::WhenYouMoveAnEnemyUnit, enemy);
    EXPECT_TRUE(state.getObject(cone).is_exhausted);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
}

TEST_F(Cards4Test, BlastCone_OwnUnitMoveOrStandardMoveDoesNotTrigger) {
    auto cone = addGear(P1, 695);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    ChainItem resolving;
    resolving.controller = P1;
    state.chain.resuming = resolving;
    exec.moveToBase(mine);
    state.chain.resuming.reset();
    EXPECT_FALSE(chainHasItemFrom(cone));
    EXPECT_EQ(card_registry.get(695)->getTargetRequirements().count, 0);
}

// ─── [702] Conscription ─────────────────────────────────────────────────────

TEST_F(Cards4Test, Conscription_PaysFiveXpOnceAndRecallsWithoutMoving) {
    EffectExecutor exec(state, events, card_db);
    auto spell = addToHand(P1, 702);
    auto big = addUnit(P2, kInvalidId, 7, 0);
    setXp(P1, 6);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    driveResumable(702, P1, spell, pickObjs({big}), exec);
    EXPECT_EQ(state.player(P1).xp, 1) << "5 XP paid exactly once";
    EXPECT_EQ(state.getObject(big).controller, P1);
    EXPECT_TRUE(state.getObject(big).is_exhausted);
    EXPECT_TRUE(state.getObject(big).isAtBase());
    EXPECT_EQ(state.getObject(big).card_counters.count("__moved_from_bf"), 0u)
        << "a recall is not a move";
}

}  // namespace
