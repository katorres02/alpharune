/// @file test_radiance_2.cpp
/// Radiance batch 2 (cards 1007-1025): each card's main effect and its key
/// boundary, driven through the Card hooks; the engine pieces they rely on
/// (kill-a-gear cost, gear Bonus Damage, "win a combat here" on gear, "reduce
/// the Might of an enemy here", "can't move this turn") through the engine.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace riftbound::test {
namespace {

constexpr CardDefId kTwirligig = 1007;
constexpr CardDefId kZiggs = 1008;
constexpr CardDefId kDreamroot = 1009;
constexpr CardDefId kConsult = 1010;
constexpr CardDefId kBoar = 1011;
constexpr CardDefId kAvian = 1012;
constexpr CardDefId kLifeFromStone = 1013;
constexpr CardDefId kConservator = 1014;
constexpr CardDefId kFerryman = 1015;
constexpr CardDefId kCommandAttack = 1016;
constexpr CardDefId kBloom = 1017;
constexpr CardDefId kRaptor = 1018;
constexpr CardDefId kCurator = 1019;
constexpr CardDefId kAhri = 1020;
constexpr CardDefId kWard = 1021;
constexpr CardDefId kHeartening = 1022;
constexpr CardDefId kOrianna = 1023;
constexpr CardDefId kHuntingBlind = 1024;
constexpr CardDefId kKSante = 1025;

using Picker = std::function<Intent(const std::vector<Intent>&)>;

/// Answers int-coded prompts with `value` and object prompts with `object`.
Picker answer(int value, GameObjectId object = kInvalidId) {
    return [value, object](const std::vector<Intent>& legal) {
        for (const auto& i : legal) {
            if (!i.chosen_objects.empty()) {
                if (i.chosen_objects[0] == object) return i;
            } else if (i.chosen_value.has_value() && *i.chosen_value == value) {
                return i;
            }
        }
        return legal.empty() ? Intent{} : legal.front();
    };
}

enum class Hook { Resolve, Trigger, Activate };

class Radiance2Test : public CardTestFixture {
protected:
    /// Drive a card callback through its choices like ChainManager would. With
    /// `finalize_first` the engine's finalization pass runs first (choices +
    /// costs), then resolution.
    void drive(Hook hook, CardDefId def_id, PlayerId controller, GameObjectId source,
               Picker picker, EffectExecutor& exec, GameObjectId subject = kInvalidId,
               bool finalize_first = false, int ability_index = 0) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.is_spell = hook == Hook::Resolve;
        ri.is_ability = hook != Hook::Resolve;
        ri.is_activated_ability = hook == Hook::Activate;
        ri.ability_index = ability_index;
        ri.triggering_subject = subject;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.registry = &card_registry;
        auto run = [&] {
            if (hook == Hook::Resolve) card->onResolve(ctx, {});
            else if (hook == Hook::Trigger) card->onTrigger(ctx, {});
            else card->onActivate(ctx, ability_index, {});
        };
        auto loop = [&] {
            run();
            while (exec.hasPendingChoice()) {
                auto pending = exec.consumePendingChoice();
                exec.recordChoice(picker(pending.legal));
                run();
            }
        };
        if (finalize_first) {
            ri.finalizing = true;
            state.chain.resuming = ri;
            loop();
            ChainItem done = *state.chain.resuming;
            done.finalizing = false;
            done.choices_finalized = true;
            done.finalize_checked = 0;
            state.chain.resuming = done;
            finalized_hook();
        } else {
            state.chain.resuming = ri;
        }
        loop();
        state.chain.resuming.reset();
    }
    std::function<void()> finalized_hook = [] {};

    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        const auto& def = card_db.get(def_id);
        o.owner = o.controller = owner;
        o.card_def_id = def_id;
        o.card_type = CardType::Gear;
        o.name = def.name;
        o.keywords = def.keywords;
        o.tags = def.tags;
        if (at_bf >= 0) { o.zone = ZoneType::BattlefieldZone; o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)}; }
        else { o.zone = ZoneType::Base; o.location = BaseLocation{owner}; }
        return id;
    }
    GameObjectId addLegend(PlayerId owner) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = owner;
        o.card_type = CardType::Legend;
        o.name = "Test Legend";
        o.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }
    void addRuneToRuneDeck(PlayerId p) {
        auto id = state.createObject();
        auto& r = state.getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Calm Rune";
        r.domains = {Domain::Calm};
        r.zone = ZoneType::RuneDeck;
        state.player(p).rune_deck.push_back(id);
    }
    int unitsNamed(PlayerId p, const std::string& name) {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == p && o.location.has_value() && o.name == name) ++n;
        return n;
    }
    bool hasAuraKeyword(const GameObject& o, Keyword kw, int value) {
        for (const auto& ae : o.aura_effects)
            if (ae.keyword == kw && ae.keyword_value == value) return true;
        return false;
    }
    int auraMightSum(const GameObject& o) {
        int n = 0;
        for (const auto& ae : o.aura_effects) n += ae.might_bonus;
        return n;
    }
    bool suppressed(const GameObject& o) {
        for (const auto& ae : o.aura_effects) if (ae.suppress_combat_damage) return true;
        return false;
    }
    void clearAuras() { for (auto& [id, o] : state.objects) o.aura_effects.clear(); }
};

// ── 1007 Whirling Twirligig ─────────────────────────────────────────────────

TEST_F(Radiance2Test, Twirligig_ThreeAbilitiesMakeTokens) {
    const Card* c = card_registry.get(kTwirligig);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 3u);
    EXPECT_TRUE(abs[1].cost.kill_friendly_gear);
    EXPECT_TRUE(abs[2].cost.kill_friendly_unit);
    EXPECT_EQ(card_db.get(kTwirligig).domains, std::vector<Domain>{Domain::Fury});

    EffectExecutor exec(state, events, card_db, &card_registry);
    auto twirl = addGear(P1, kTwirligig);
    drive(Hook::Activate, kTwirligig, P1, twirl, answer(0), exec, kInvalidId, false, 0);
    drive(Hook::Activate, kTwirligig, P1, twirl, answer(0), exec, kInvalidId, false, 1);
    drive(Hook::Activate, kTwirligig, P1, twirl, answer(0), exec, kInvalidId, false, 2);
    int gold = 0, recruit = 0, mech = 0;
    for (const auto& [id, o] : state.objects) {
        if (o.card_def_id == kGoldGearCardDefId) { ++gold; EXPECT_TRUE(o.is_exhausted); }
        if (o.isUnit() && o.name == "Recruit") { ++recruit; EXPECT_EQ(o.card_def_id, 954); EXPECT_EQ(o.current_might, 1); }
        if (o.isUnit() && o.name == "Mech") { ++mech; EXPECT_EQ(o.current_might, 3); }
    }
    EXPECT_EQ(gold, 1);
    EXPECT_EQ(recruit, 1);
    EXPECT_EQ(mech, 1);

    // Ability 1 may kill any friendly gear (itself included), never a unit.
    auto other_gear = addGear(P1, kWard);
    auto unit = addUnit(P1, kInvalidId, 2);
    auto legal = c->enumerateLegalTargets(state, P1, 1);
    EXPECT_NE(std::find(legal.begin(), legal.end(), other_gear), legal.end());
    EXPECT_NE(std::find(legal.begin(), legal.end(), twirl), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), unit), legal.end());
}

// ── Engine fixture ──────────────────────────────────────────────────────────

class Radiance2EngineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    FirstChoiceAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }

    void SetUp() override {
        CardTestFixture::SetUp();
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < 2; ++i) {
            BattlefieldState bf;
            bf.id = i;
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.phase = TurnPhase::MainPhase;
    }
    GameObjectId obj(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = s().createObject();
        auto& o = s().getObject(id);
        const auto& def = card_db.get(def_id);
        o.owner = o.controller = p;
        o.card_def_id = def_id;
        o.card_type = def.card_type;
        o.name = def.name;
        o.keywords = def.keywords;
        o.base_might = o.current_might = def.might;
        if (at_bf >= 0) { o.zone = ZoneType::BattlefieldZone; o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)}; }
        else { o.zone = ZoneType::Base; o.location = BaseLocation{p}; }
        return id;
    }
};

TEST_F(Radiance2EngineTest, Twirligig_KillAFriendlyGearIsPaidAsTheCost) {
    auto twirl = obj(P1, kTwirligig);
    auto ward = obj(P1, kWard);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = twirl;
    act.ability_index = 1;
    act.targets = {ward};
    engine_->testHook_executeIntent(act);
    EXPECT_FALSE(s().getObject(ward).location.has_value()) << "the gear paid the cost";
    EXPECT_TRUE(s().getObject(twirl).is_exhausted);
    int recruits = 0;
    for (const auto& [id, o] : s().objects)
        if (o.isUnit() && o.name == "Recruit" && o.controller == P1 && o.location) ++recruits;
    EXPECT_EQ(recruits, 1);
}

// ── 1008 Ziggs, Short-Fused ─────────────────────────────────────────────────

TEST_F(Radiance2Test, Ziggs_PlaysABombToTheChosenBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ziggs = addUnit(P1, kZiggs, 0, 0);
    drive(Hook::Trigger, kZiggs, P1, ziggs, answer(1), exec);
    int bombs_at_1 = 0;
    for (const auto& [id, o] : state.objects)
        if (o.card_def_id == kBombTokenCardDefId && o.battlefieldId() == std::optional<BattlefieldId>(1))
            ++bombs_at_1;
    EXPECT_EQ(bombs_at_1, 1);
}

TEST_F(Radiance2Test, Ziggs_AbilitiesOfFriendlyGearDealOneBonusDamage) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addUnit(P1, kZiggs, 0, 0);
    card_registry.get(kZiggs)->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).gear_ability_bonus_damage, 1);

    auto gear = addGear(P1, kDreamroot, 0);
    auto unit_src = addUnit(P1, kInvalidId, 2, 0);
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    ChainItem ri;
    ri.source = gear;
    ri.controller = P1;
    state.chain.resuming = ri;
    exec.dealDamage(foe, 2, gear);
    EXPECT_EQ(state.getObject(foe).damage_marked, 3) << "gear ability: 2 + 1 Bonus Damage";
    state.chain.resuming->source = unit_src;
    exec.dealDamage(foe, 2, unit_src);
    EXPECT_EQ(state.getObject(foe).damage_marked, 5) << "a unit's ability gets no bonus";
    state.chain.resuming.reset();
}

// ── 1009 Blossoming Dreamroot ───────────────────────────────────────────────

TEST_F(Radiance2Test, Dreamroot_WinningACombatHereDraws) {
    EXPECT_TRUE(card_registry.get(kDreamroot)->hasDeploy());
    auto here = addGear(P1, kDreamroot, 0);
    auto away = addGear(P1, kDreamroot, 1);
    addUnit(P1, kInvalidId, 3, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(CombatEndedEvent{0, P2});
    for (const auto& item : state.chain.items) EXPECT_NE(item.source, here) << "P2 won";
    events.emit(CombatEndedEvent{0, P1});
    int fired_here = 0;
    for (const auto& item : state.chain.items) {
        if (item.source == here) ++fired_here;
        EXPECT_NE(item.source, away);
    }
    EXPECT_EQ(fired_here, 1);

    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kAvian);
    const int hand = handSize(P1);
    drive(Hook::Trigger, kDreamroot, P1, here, answer(0), exec);
    EXPECT_EQ(handSize(P1), hand + 1);
}

// ── 1010 Consult the Heavens ────────────────────────────────────────────────

TEST_F(Radiance2Test, ConsultTheHeavens_ShieldTwoAndPredict) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    addToDeck(P1, kAvian);
    auto spell = addToHand(P1, kConsult);
    drive(Hook::Resolve, kConsult, P1, spell, answer(0, unit), exec);
    EXPECT_EQ(state.getObject(unit).shield_value, 2);
    EXPECT_TRUE(state.getObject(unit).keywords.has(Keyword::Shield));
    EXPECT_TRUE(card_db.get(kConsult).keywords.has(Keyword::Reaction));
}

// ── 1011 Entrenched Boar ────────────────────────────────────────────────────

TEST_F(Radiance2Test, EntrenchedBoar_MayEnterReadyButThenCantMove) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kBoar);
    auto boar = addToHand(P1, kBoar);
    CardContext ctx{state, events, exec, P1, boar};

    exec.setAgentQuery([](PlayerId, const std::vector<Intent>& legal) { return answer(0)(legal); });
    card_registry.get(kBoar)->onPlay(ctx);
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1, state.getObject(boar)));
    EXPECT_EQ(state.getObject(boar).cant_move_turn, -1);

    exec.setAgentQuery([](PlayerId, const std::vector<Intent>& legal) { return answer(1)(legal); });
    card_registry.get(kBoar)->onPlay(ctx);
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1, state.getObject(boar)));
    EXPECT_EQ(state.getObject(boar).cant_move_turn, state.turn.turn_number);
}

TEST_F(Radiance2EngineTest, EntrenchedBoar_NoStandardMoveOnTheTurnItCantMove) {
    auto boar = obj(P1, kBoar);
    auto other = obj(P1, kAvian);
    auto moves_of = [&](GameObjectId u) {
        int n = 0;
        for (const auto& i : engine_->generateLegalActions())
            if (i.type == IntentType::StandardMove &&
                std::find(i.units_to_move.begin(), i.units_to_move.end(), u) != i.units_to_move.end()) ++n;
        return n;
    };
    EXPECT_GT(moves_of(boar), 0);
    s().getObject(boar).cant_move_turn = s().turn.turn_number;
    EXPECT_EQ(moves_of(boar), 0);
    EXPECT_GT(moves_of(other), 0);
    s().getObject(boar).cant_move_turn = s().turn.turn_number - 1;   // a later turn
    EXPECT_GT(moves_of(boar), 0);
}

// ── 1012 Iridescent Avian ───────────────────────────────────────────────────

TEST_F(Radiance2Test, IridescentAvian_BuffsOrGivesPlusTwoIfAlreadyBuffed) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto avian = addUnit(P1, kAvian, 0, 0);
    auto target = addUnit(P1, kInvalidId, 2, 0);
    drive(Hook::Activate, kAvian, P1, avian, answer(0, target), exec);
    EXPECT_EQ(state.getObject(target).buff_count, 1);
    EXPECT_EQ(state.getObject(target).temp_might_bonus, 0);
    drive(Hook::Activate, kAvian, P1, avian, answer(0, target), exec);
    EXPECT_EQ(state.getObject(target).buff_count, 1);
    EXPECT_EQ(state.getObject(target).temp_might_bonus, 2);
}

// ── 1013 Life from Stone ────────────────────────────────────────────────────

TEST_F(Radiance2Test, LifeFromStone_ExhaustAUnitAsTheCostThenDrawAndChannel) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kLifeFromStone);
    EXPECT_FALSE(c->canBePlayed(state, P1)) << "no ready friendly unit to exhaust";
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    EXPECT_TRUE(c->canBePlayed(state, P1));
    addToDeck(P1, kAvian);
    addRuneToRuneDeck(P1);
    auto spell = addToHand(P1, kLifeFromStone);
    const int hand = handSize(P1);
    bool exhausted_at_finalize = false;
    finalized_hook = [&] { exhausted_at_finalize = state.getObject(unit).is_exhausted; };
    drive(Hook::Resolve, kLifeFromStone, P1, spell, answer(0, unit), exec, kInvalidId,
          /*finalize_first=*/true);
    EXPECT_TRUE(exhausted_at_finalize) << "the cost is paid as the spell is finalized";
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
    EXPECT_EQ(handSize(P1), hand + 1);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
    EXPECT_EQ(readyRuneCount(P1), 0);
}

// ── 1014 Sanctum Conservator ────────────────────────────────────────────────

TEST_F(Radiance2Test, SanctumConservator_ReadiesOnlyForSevenPlusCards) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kConservator, 0, 0);
    state.getObject(me).is_exhausted = true;
    const Card* c = card_registry.get(kConservator);
    auto cheap = addUnit(P1, kZiggs, 0, 0);          // [5]
    EXPECT_FALSE(c->triggerConditionOnDefs(state, card_db, me, P1,
                                           TriggerType::WhenYouPlayACard, cheap))
        << "a cheaper card doesn't trigger it";
    auto big = addUnit(P1, kHeartening, 0, 0);       // [7]
    EXPECT_TRUE(c->triggerConditionOnDefs(state, card_db, me, P1,
                                          TriggerType::WhenYouPlayACard, big));
    drive(Hook::Trigger, kConservator, P1, me, answer(0), exec, big);
    EXPECT_FALSE(state.getObject(me).is_exhausted);
}

// ── 1015 Stoic Ferryman ─────────────────────────────────────────────────────

TEST_F(Radiance2Test, StoicFerryman_MayMoveAnotherUnitHereToBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kFerryman);
    EXPECT_EQ(card_db.get(kFerryman).shield_value, 3);
    auto at_base = addUnit(P1, kFerryman);
    EXPECT_FALSE(c->triggerCondition(state, at_base, P1, TriggerType::WhenYouPlayMe, kInvalidId));
    auto ferry = addUnit(P1, kFerryman, 0, 0);
    EXPECT_TRUE(c->triggerCondition(state, ferry, P1, TriggerType::WhenYouPlayMe, kInvalidId));
    auto buddy = addUnit(P1, kInvalidId, 2, 0);
    auto foe = addUnit(P2, kInvalidId, 2, 0);

    drive(Hook::Trigger, kFerryman, P1, ferry, answer(0), exec);   // decline
    EXPECT_TRUE(state.getObject(buddy).isAtBattlefield());
    drive(Hook::Trigger, kFerryman, P1, ferry, answer(1, buddy), exec, kInvalidId, true);
    EXPECT_TRUE(state.getObject(buddy).isAtBase());
    EXPECT_TRUE(state.getObject(ferry).isAtBattlefield());
    EXPECT_TRUE(state.getObject(foe).isAtBattlefield());
}

// ── 1016 Command: Attack ────────────────────────────────────────────────────

TEST_F(Radiance2Test, CommandAttack_MovesADeployGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto root = addGear(P1, kDreamroot, 0);
    auto spell = addToHand(P1, kCommandAttack);
    int prompt = 0;
    drive(Hook::Resolve, kCommandAttack, P1, spell, [&](const std::vector<Intent>& legal) {
        ++prompt;
        if (prompt == 1) return answer(0)(legal);         // mode: move a gear
        if (prompt == 2) return answer(0, root)(legal);   // the gear
        return answer(1)(legal);                          // battlefield #1
    }, exec, kInvalidId, true);
    EXPECT_EQ(state.getObject(root).battlefieldId(), std::optional<BattlefieldId>(1));
}

TEST_F(Radiance2Test, CommandAttack_DrawsARevealedGearAndRecyclesTheRest) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bottom = addToDeck(P1, kAvian);   // 6th from the top: not looked at
    std::vector<GameObjectId> top;
    top.push_back(addToDeck(P1, kAvian));
    auto gear = addToDeck(P1, kWard);
    top.push_back(gear);
    for (int i = 0; i < 3; ++i) top.push_back(addToDeck(P1, kAvian));
    auto spell = addToHand(P1, kCommandAttack);
    // No Deploy gear on the board: only the second mode is offered.
    int prompt = 0;
    bool first_mode_offered = false;
    drive(Hook::Resolve, kCommandAttack, P1, spell, [&](const std::vector<Intent>& legal) {
        ++prompt;
        if (prompt == 1) {
            for (const auto& i : legal) if (i.chosen_value == 0) first_mode_offered = true;
            return answer(1)(legal);
        }
        return answer(-1, gear)(legal);
    }, exec, kInvalidId, true);
    EXPECT_FALSE(first_mode_offered);
    EXPECT_TRUE(inHand(P1, gear));
    auto& deck = state.player(P1).main_deck;
    ASSERT_EQ(deck.size(), 5u);
    EXPECT_NE(deck.back(), top.back()) << "the looked-at cards went to the bottom";
    EXPECT_EQ(std::find(deck.begin(), deck.end(), bottom) - deck.begin(), 4)
        << "the unseen card is now on top";
}

// ── 1017 Invigorating Bloom ─────────────────────────────────────────────────

TEST_F(Radiance2Test, InvigoratingBloom_PlusThreeOnPlay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bloom = addGear(P1, kBloom);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    drive(Hook::Trigger, kBloom, P1, bloom, answer(0, unit), exec, kInvalidId, true);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 3);
}

TEST_F(Radiance2Test, InvigoratingBloom_ExhaustAMightyUnitToDraw) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kBloom);
    auto bloom = addGear(P1, kBloom);
    auto small = addUnit(P1, kInvalidId, 4, 0);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, bloom, 0)) << "no Mighty unit";
    auto mighty = addUnit(P1, kInvalidId, 5, 0);
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, bloom, 0));
    addToDeck(P1, kAvian);
    const int hand = handSize(P1);
    drive(Hook::Activate, kBloom, P1, bloom, answer(0, mighty), exec, kInvalidId, true);
    EXPECT_TRUE(state.getObject(mighty).is_exhausted);
    EXPECT_FALSE(state.getObject(small).is_exhausted);
    EXPECT_EQ(handSize(P1), hand + 1);
}

// ── 1018 Storm Raptor ───────────────────────────────────────────────────────

TEST_F(Radiance2Test, StormRaptor_DrawsWhenAStunnedEnemyHereDies) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kRaptor);
    EXPECT_EQ(card_db.get(kRaptor).deflect_value, 1);
    auto raptor = addUnit(P1, kRaptor, 0, 0);
    auto foe = addUnit(P2, kInvalidId, 2, 0);
    EXPECT_FALSE(c->triggerCondition(state, raptor, P1, TriggerType::WhenAnEnemyUnitDiesHere, foe));
    state.getObject(foe).is_stunned = true;
    EXPECT_TRUE(c->triggerCondition(state, raptor, P1, TriggerType::WhenAnEnemyUnitDiesHere, foe));

    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    exec.killObject(foe);
    int fired = 0;
    for (const auto& item : state.chain.items) if (item.source == raptor) ++fired;
    EXPECT_EQ(fired, 1);

    addToDeck(P1, kAvian);
    const int hand = handSize(P1);
    drive(Hook::Trigger, kRaptor, P1, raptor, answer(0), exec, foe);
    EXPECT_EQ(handSize(P1), hand + 1);
}

// ── 1019 Watchful Curator ───────────────────────────────────────────────────

TEST_F(Radiance2Test, WatchfulCurator_FriendlyGearHaveDeflect) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addUnit(P1, kCurator, 0, 0);
    auto mine = addGear(P1, kWard);
    auto theirs = addGear(P2, kWard);
    card_registry.get(kCurator)->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(state.getObject(mine), Keyword::Deflect, 1));
    EXPECT_FALSE(hasAuraKeyword(state.getObject(theirs), Keyword::Deflect, 1));
    // Fold the aura keywords as the engine's aura pass does.
    for (auto& [id, o] : state.objects)
        for (const auto& ae : o.aura_effects)
            if (ae.keyword != Keyword::Count) o.aura_keywords.set(ae.keyword);
    EXPECT_EQ(exec.deflectCostFor(P2, mine), 1);
    EXPECT_EQ(exec.deflectCostFor(P1, theirs), 0);
}

// ── 1020 Ahri, Confident ────────────────────────────────────────────────────

TEST_F(Radiance2Test, AhriConfident_ReducingAnEnemyHereGivesHerPlusOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_EQ(card_registry.get(kAhri)->disarmValue(), 1);
    auto ahri = addUnit(P1, kAhri, 0, 0);
    auto foe_here = addUnit(P2, kInvalidId, 4, 0);
    auto foe_away = addUnit(P2, kInvalidId, 4, 1);
    auto friend_here = addUnit(P1, kInvalidId, 4, 0);

    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto ahri_items = [&] {
        int n = 0;
        for (const auto& item : state.chain.items) if (item.source == ahri) ++n;
        return n;
    };
    ChainItem ri;
    ri.controller = P1;
    state.chain.resuming = ri;
    exec.giveTemporaryMight(foe_away, -1);
    exec.giveTemporaryMight(friend_here, -1);
    exec.giveTemporaryMight(foe_here, 2);
    EXPECT_EQ(ahri_items(), 0);
    exec.giveTemporaryMight(foe_here, -1);
    EXPECT_EQ(ahri_items(), 1);
    state.chain.resuming->controller = P2;   // the opponent's effect: not "you"
    exec.giveTemporaryMight(foe_here, -1);
    EXPECT_EQ(ahri_items(), 1);
    state.chain.resuming.reset();

    drive(Hook::Trigger, kAhri, P1, ahri, answer(0), exec, foe_here);
    EXPECT_EQ(state.getObject(ahri).temp_might_bonus, 1);
}

// ── 1021 Ephemeral Ward ─────────────────────────────────────────────────────

TEST_F(Radiance2Test, EphemeralWard_PlusTwoUntilYouPlayAUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ward = addGear(P1, kWard);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);
    card_registry.get(kWard)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(mine)), 2);
    EXPECT_EQ(auraMightSum(state.getObject(theirs)), 0);
    drive(Hook::Trigger, kWard, P1, ward, answer(0), exec);
    EXPECT_FALSE(state.getObject(ward).location.has_value());
}

// ── 1022 Heartening Soul ────────────────────────────────────────────────────

TEST_F(Radiance2Test, HeartingSoul_ReadiesTheLegendAndGrantsItAnAbility) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kHeartening);
    auto soul = addUnit(P1, kHeartening, 0, 0);
    auto legend = addLegend(P1);
    state.getObject(legend).is_exhausted = true;
    drive(Hook::Trigger, kHeartening, P1, soul, answer(0), exec);
    EXPECT_FALSE(state.getObject(legend).is_exhausted);

    c->applyPassiveAura(state, P1);
    c->applyPassiveAura(state, P1);   // a second copy doesn't grant it twice
    const auto& granted = state.getObject(legend).granted_abilities;
    ASSERT_EQ(granted.size(), 1u);
    EXPECT_EQ(granted[0].source_def_id, kHeartening);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, soul, 0)) << "the unit itself can't use it";
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, legend, 0));

    auto unit = addUnit(P1, kInvalidId, 2, 0);
    drive(Hook::Activate, kHeartening, P1, legend, answer(0, unit), exec);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 3);
}

// ── 1023 Orianna, Exquisite ─────────────────────────────────────────────────

TEST_F(Radiance2Test, Orianna_NoCombatDamageWithoutAFriendlyGearHere) {
    const Card* c = card_registry.get(kOrianna);
    auto ori = addUnit(P1, kOrianna, 0, 0);
    addGear(P1, kWard);           // at base: not here
    addGear(P2, kDreamroot, 0);   // enemy gear here: doesn't count
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(suppressed(state.getObject(ori)));

    clearAuras();
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto sword = addGear(P1, kWard);
    state.getObject(sword).attached_to = ally;   // attached to a unit here
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(suppressed(state.getObject(ori)));
}

// ── 1024 Hunting Blind ──────────────────────────────────────────────────────

TEST_F(Radiance2Test, HuntingBlind_PullsAnEnemyHereAndShieldsFriends) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto blind = addGear(P1, kHuntingBlind, 0);
    auto foe = addUnit(P2, kInvalidId, 3);       // at P2's base
    auto mine_here = addUnit(P1, kInvalidId, 2, 0);
    auto mine_away = addUnit(P1, kInvalidId, 2, 1);
    drive(Hook::Trigger, kHuntingBlind, P1, blind, answer(1, foe), exec, kInvalidId, true);
    EXPECT_EQ(state.getObject(foe).battlefieldId(), std::optional<BattlefieldId>(0));

    card_registry.get(kHuntingBlind)->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(state.getObject(mine_here), Keyword::Shield, 2));
    EXPECT_FALSE(hasAuraKeyword(state.getObject(mine_away), Keyword::Shield, 2));
    EXPECT_FALSE(hasAuraKeyword(state.getObject(foe), Keyword::Shield, 2));
}

// ── 1025 K'Sante, Dauntless ─────────────────────────────────────────────────

TEST_F(Radiance2Test, KSante_DoublesTheShieldOfAUnitHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ks = addUnit(P1, kKSante, 0, 0);
    auto ferry = addUnit(P1, kFerryman, 0, 0);
    state.getObject(ferry).shield_value = 3;
    drive(Hook::Trigger, kKSante, P1, ks, answer(0, ferry), exec, kInvalidId, true);
    EXPECT_EQ(state.getObject(ferry).shield_value, 6);
    EXPECT_EQ(state.getObject(ferry).temp_shield_value, 3);
}

}  // namespace
}  // namespace riftbound::test
