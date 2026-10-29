/// @file test_audit_2026_10_cards0.cpp
/// Card fixes from the 2026-10-28 audit, batch cards0 (Origins 011-173).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <memory>

namespace riftbound::test {
namespace {

constexpr CardDefId kMagmaWurm = 11;
constexpr CardDefId kCaptainFarron = 15;
constexpr CardDefId kDangerousDuo = 16;
constexpr CardDefId kNoxusSaboteur = 18;
constexpr CardDefId kSunDisc = 21;
constexpr CardDefId kShakedown = 33;
constexpr CardDefId kDefy = 45;
constexpr CardDefId kSolariShieldbearer = 51;
constexpr CardDefId kStandUnited = 53;
constexpr CardDefId kAdaptatron = 56;
constexpr CardDefId kSpiritsRefuge = 63;
constexpr CardDefId kWizenedElder = 65;
constexpr CardDefId kSolariShrine = 72;
constexpr CardDefId kTaric = 74;
constexpr CardDefId kYasuo = 76;
constexpr CardDefId kMysticReversal = 80;
constexpr CardDefId kWhiteflame = 82;
constexpr CardDefId kRiptideRex = 92;
constexpr CardDefId kBlastconeFae = 97;
constexpr CardDefId kGemcraftSeer = 100;
constexpr CardDefId kSpriteMother = 106;
constexpr CardDefId kEkko = 110;
constexpr CardDefId kTimeWarp = 122;
constexpr CardDefId kArenaBar = 124;
constexpr CardDefId kCannonBarrage = 127;
constexpr CardDefId kConfront = 129;
constexpr CardDefId kCrackshotCorsair = 130;
constexpr CardDefId kFirstMate = 132;
constexpr CardDefId kSpoilsOfWar = 144;
constexpr CardDefId kWildclawShaman = 147;
constexpr CardDefId kAnivia = 148;
constexpr CardDefId kOvertOperation = 153;
constexpr CardDefId kUdyr = 157;
constexpr CardDefId kEmberMonk = 167;
constexpr CardDefId kFightOrFlight = 168;
constexpr CardDefId kRideTheWind = 173;

using Picker = std::function<Intent(const std::vector<Intent>&)>;

Picker pickValue(int v) {
    return [v](const std::vector<Intent>& legal) -> Intent {
        for (auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
        return legal.empty() ? Intent{} : legal.front();
    };
}
Picker pickObject(GameObjectId id) {
    return [id](const std::vector<Intent>& legal) -> Intent {
        for (auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return i;
        return legal.empty() ? Intent{} : legal.front();
    };
}
/// Answers each prompt with the matching value or object, in order.
Picker script(std::vector<Picker> steps) {
    auto shared = std::make_shared<std::pair<std::vector<Picker>, size_t>>(std::move(steps), 0);
    return [shared](const std::vector<Intent>& legal) -> Intent {
        auto& [s, i] = *shared;
        if (i < s.size()) return s[i++](legal);
        return legal.empty() ? Intent{} : legal.front();
    };
}

class AuditCards0Test : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) obj.name = card_db.get(def_id).name;
        obj.zone = ZoneType::Base;
        obj.location = BaseLocation{owner};
        return id;
    }

    /// onResolve through the resume loop, keeping `targets`.
    void driveResolve(CardDefId def_id, PlayerId controller, GameObjectId source,
                      const std::vector<GameObjectId>& targets, Picker picker,
                      EffectExecutor& exec, int* prompts = nullptr) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onResolve(ctx, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            if (prompts) ++*prompts;
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }

    /// Picker that records the options it was offered.
    Picker recording(std::vector<GameObjectId>& seen, Picker inner) {
        return [&seen, inner](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal)
                if (!i.chosen_objects.empty()) seen.push_back(i.chosen_objects[0]);
            return inner(legal);
        };
    }

    int auraMight(GameObjectId id) {
        int m = 0;
        for (auto& ae : state.getObject(id).aura_effects) m += ae.might_bonus;
        return m;
    }
    bool auraHas(GameObjectId id, Keyword kw) {
        for (auto& ae : state.getObject(id).aura_effects)
            if (ae.keyword == kw) return true;
        return false;
    }
};

/// Tests that need GameEngine::cleanup (aura recalculation).
class AuditCards0EngineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    FirstChoiceAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    void makeEngine() {
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
    GameObjectId engineUnit(PlayerId p, CardDefId def_id, int might, int at_bf) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.card_def_id = def_id;
        u.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestUnit";
        if (def_id != kInvalidId) u.keywords = card_db.get(def_id).keywords;
        u.base_might = u.current_might = might;
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        return id;
    }
};

// ── 11 Magma Wurm ──
TEST_F(AuditCards0Test, MagmaWurm_OtherFriendlyUnitsEnterReady) {
    auto wurm = addUnit(P1, kMagmaWurm, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    Card* c = card_registry.get(kMagmaWurm);
    EXPECT_TRUE(c->grantsEnterReadyTo(state, wurm, state.getObject(other)));
    EXPECT_FALSE(c->grantsEnterReadyTo(state, wurm, state.getObject(wurm))) << "other";
}

// ── 15 Captain Farron ──
TEST_F(AuditCards0Test, CaptainFarron_GivesOthersAssaultButNotHimself) {
    EXPECT_FALSE(card_db.get(kCaptainFarron).keywords.has(Keyword::Assault));
    EXPECT_EQ(card_db.get(kCaptainFarron).assault_value, 0);
    auto farron = addUnit(P1, kCaptainFarron, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    state.getObject(ally).combat_designation = CombatDesignation::Attacker;
    card_registry.get(kCaptainFarron)->applyPassiveAura(state, P1);
    EXPECT_TRUE(auraHas(ally, Keyword::Assault));
    EXPECT_EQ(auraMight(ally), 1) << "+1 [M] while attacking";
    EXPECT_FALSE(auraHas(farron, Keyword::Assault));
    EXPECT_FALSE(auraHas(elsewhere, Keyword::Assault)) << "here only";
}

// ── 16 Dangerous Duo ──
TEST_F(AuditCards0Test, DangerousDuo_LegionGivesChosenUnitTwoMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto duo = addUnit(P1, kDangerousDuo, 0, 0);
    auto u = addUnit(P2, kInvalidId, 3, 1);
    state.player(P1).cards_played_this_turn = 2;
    driveResumableTrigger(kDangerousDuo, P1, duo, pickObject(u), exec);
    EXPECT_EQ(state.getObject(u).temp_might_bonus, 2);
}
TEST_F(AuditCards0Test, DangerousDuo_NoLegionNoEffect) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto duo = addUnit(P1, kDangerousDuo, 0, 0);
    state.player(P1).cards_played_this_turn = 1;
    int prompts = 0;
    driveResumableTrigger(kDangerousDuo, P1, duo,
        [&](const std::vector<Intent>& l) { ++prompts; return l.front(); }, exec);
    EXPECT_EQ(prompts, 0);
    EXPECT_EQ(state.getObject(duo).temp_might_bonus, 0);
}

// ── 18 Noxus Saboteur ──
TEST_F(AuditCards0Test, NoxusSaboteur_BlocksOpponentHiddenHereOnly) {
    EXPECT_FALSE(card_db.get(kNoxusSaboteur).keywords.has(Keyword::Hidden));
    addUnit(P1, kNoxusSaboteur, 0, 0);
    card_registry.get(kNoxusSaboteur)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.battlefields[0].hidden_unrevealable_for[playerIndex(P2)]);
    EXPECT_FALSE(state.battlefields[0].hidden_unrevealable_for[playerIndex(P1)]);
    EXPECT_FALSE(state.battlefields[1].hidden_unrevealable_for[playerIndex(P2)]);
}

// ── 21 Sun Disc ──
TEST_F(AuditCards0Test, SunDisc_ArmedNextUnitEntersReadyThenDisarms) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.turn.turn_number = 3;
    auto disc = addGear(P1, kSunDisc);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    Card* c = card_registry.get(kSunDisc);
    EXPECT_FALSE(c->grantsEnterReadyTo(state, disc, state.getObject(unit)));
    CardContext ctx{state, events, exec, P1, disc};
    c->onActivate(ctx, {});
    EXPECT_TRUE(c->grantsEnterReadyTo(state, disc, state.getObject(unit)));
    EXPECT_TRUE(c->triggerCondition(state, disc, P1, TriggerType::WhenYouPlayAUnit, unit));
    c->onTrigger(ctx, {});
    EXPECT_FALSE(c->grantsEnterReadyTo(state, disc, state.getObject(unit))) << "one-shot";
    EXPECT_FALSE(c->triggerCondition(state, disc, P1, TriggerType::WhenYouPlayAUnit, unit));
}

// ── 33 Shakedown ──
TEST_F(AuditCards0Test, Shakedown_OpponentMayHaveCasterDrawTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    auto enemy = addUnit(P2, kInvalidId, 8, 0);
    auto spell = pushSpellOnChain(P1, kShakedown);
    state.chain.items.clear();
    driveResolve(kShakedown, P1, spell, {enemy}, pickValue(1), exec);
    EXPECT_EQ(handSize(P1), 2);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0);
}
TEST_F(AuditCards0Test, Shakedown_OpponentTakesSix) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy = addUnit(P2, kInvalidId, 8, 0);
    auto spell = pushSpellOnChain(P1, kShakedown);
    state.chain.items.clear();
    driveResolve(kShakedown, P1, spell, {enemy}, pickValue(0), exec);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 6);
}

// ── 45 Defy ──
TEST_F(AuditCards0Test, Defy_CountersEligibleSpellBelowAnIneligibleTop) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto eligible = pushSpellOnChain(P2, kShakedown);         // [2] + 1 power
    auto too_much_power = pushSpellOnChain(P2, kMysticReversal);  // [4] + 3 power
    auto defy = state.createObject();
    invokeOnResolve(defy, kDefy, P1, {}, exec);
    ASSERT_EQ(state.chain.items.size(), 1u);
    EXPECT_EQ(state.chain.items.back().source, too_much_power);
    EXPECT_TRUE(inTrash(P2, eligible));
}
TEST_F(AuditCards0Test, Defy_PowerCapIsChecked) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    pushSpellOnChain(P2, kMysticReversal);
    auto defy = state.createObject();
    invokeOnResolve(defy, kDefy, P1, {}, exec);
    EXPECT_EQ(state.chain.items.size(), 1u) << "more than [A]: not counterable";
}

// ── 51 Solari Shieldbearer ──
TEST_F(AuditCards0Test, SolariShieldbearer_StunsChosenUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kSolariShieldbearer, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    driveResumableTrigger(kSolariShieldbearer, P1, me, pickObject(enemy), exec);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_EQ(card_registry.get(kSolariShieldbearer)->getTargetRequirements().count, 0);
}

// ── 53 Stand United ──
TEST_F(AuditCards0Test, StandUnited_BuffsAndSetsTheRider) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    auto spell = state.createObject();
    invokeOnResolve(spell, kStandUnited, P1, {u}, exec);
    EXPECT_EQ(state.getObject(u).buff_count, 1);
    EXPECT_EQ(state.player(P1).buff_bonus_this_turn, 1);
}
TEST_F(AuditCards0EngineTest, StandUnited_BuffsGiveAnExtraMightThisTurn) {
    makeEngine();
    auto buffed = engineUnit(P1, kInvalidId, 2, 0);
    auto plain = engineUnit(P1, kInvalidId, 2, 0);
    auto enemy = engineUnit(P2, kInvalidId, 2, 1);
    s().getObject(buffed).buff_count = 1;
    s().getObject(enemy).buff_count = 1;
    s().player(P1).buff_bonus_this_turn = 1;
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(buffed).current_might, 4);
    EXPECT_EQ(s().getObject(plain).current_might, 2);
    EXPECT_EQ(s().getObject(enemy).current_might, 3) << "friendly units only";
}

// ── 56 Adaptatron ──
TEST_F(AuditCards0Test, Adaptatron_RepeatedConquersDoNotStackBuffs) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kAdaptatron, 0, 0);
    auto g1 = addGear(P2, kInvalidId);
    auto g2 = addGear(P2, kInvalidId);
    driveResumableTrigger(kAdaptatron, P1, me, script({pickValue(1), pickObject(g1)}), exec);
    driveResumableTrigger(kAdaptatron, P1, me, script({pickValue(1), pickObject(g2)}), exec);
    EXPECT_FALSE(state.getObject(g1).location.has_value());
    EXPECT_FALSE(state.getObject(g2).location.has_value());
    EXPECT_EQ(state.getObject(me).buff_count, 1);
}

// ── 63 Spirit's Refuge ──
TEST_F(AuditCards0Test, SpiritsRefuge_GearHasNoDeflectAndBuffsChosenUnit) {
    EXPECT_FALSE(card_db.get(kSpiritsRefuge).keywords.has(Keyword::Deflect));
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto refuge = addGear(P1, kSpiritsRefuge);
    addGear(P1, kSpiritsRefuge);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    driveResumableTrigger(kSpiritsRefuge, P1, refuge, pickObject(b), exec);
    EXPECT_EQ(state.getObject(b).buff_count, 1);
    EXPECT_EQ(state.getObject(a).buff_count, 0);
    card_registry.get(kSpiritsRefuge)->applyPassiveAura(state, P1);
    card_registry.get(kSpiritsRefuge)->applyPassiveAura(state, P1);
    int deflects = 0;
    for (auto& ae : state.getObject(b).aura_effects) if (ae.keyword == Keyword::Deflect) ++deflects;
    EXPECT_EQ(deflects, 1) << "\"if they didn't already\"";
    EXPECT_FALSE(auraHas(a, Keyword::Deflect)) << "unbuffed";
}

// ── 65 Wizened Elder ──
TEST_F(AuditCards0EngineTest, WizenedElder_BuffedGetsExactlyOneExtraMight) {
    makeEngine();
    auto elder = engineUnit(P1, kWizenedElder, 4, 0);
    auto other = engineUnit(P1, kInvalidId, 2, 0);
    s().getObject(elder).buff_count = 1;
    s().getObject(other).buff_count = 1;
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(elder).current_might, 6) << "4 + buff + 1";
    EXPECT_EQ(s().getObject(other).current_might, 3) << "only the Elder itself";
}

// ── 72 Solari Shrine ──
TEST_F(AuditCards0Test, SolariShrine_OnlyStunnedEnemiesYouKill) {
    auto shrine = addGear(P1, kSolariShrine);
    auto dead = addUnit(P2, kInvalidId, 2, 0);
    Card* c = card_registry.get(kSolariShrine);
    auto cond = [&]() {
        return c->triggerCondition(state, shrine, P1, TriggerType::WhenAnEnemyUnitDies, dead);
    };
    ChainItem mine;
    mine.controller = P1;
    state.chain.resuming = mine;
    EXPECT_FALSE(cond()) << "not stunned";
    state.getObject(dead).is_stunned = true;
    EXPECT_TRUE(cond()) << "stunned, killed by my spell";
    state.chain.resuming->controller = P2;
    EXPECT_FALSE(cond()) << "killed by the opponent's own effect";
    state.chain.resuming.reset();
    EXPECT_FALSE(cond()) << "no damage of mine on it";
    state.getObject(dead).damage_marked_by[playerIndex(P1)] = 2;
    EXPECT_TRUE(cond()) << "killed by my damage (combat)";
}

// ── 74 Taric, Protector ──
TEST_F(AuditCards0Test, Taric_GrantedShieldAddsMightWhileDefending) {
    auto lone = addUnit(P1, kInvalidId, 2, 1);
    card_registry.get(kTaric)->applyPassiveAura(state, P1);
    EXPECT_FALSE(auraHas(lone, Keyword::Shield)) << "no Taric on the board";
    addUnit(P1, kTaric, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(ally).combat_designation = CombatDesignation::Defender;
    card_registry.get(kTaric)->applyPassiveAura(state, P1);
    EXPECT_TRUE(auraHas(ally, Keyword::Shield));
    EXPECT_EQ(auraMight(ally), 1);
    EXPECT_FALSE(auraHas(lone, Keyword::Shield)) << "here only";
}

// ── 76 Yasuo, Remorseful / 130 Crackshot Corsair ──
TEST_F(AuditCards0Test, Yasuo_DamagesAnEnemyUnitHereOnly) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto yasuo = addUnit(P1, kYasuo, 0, 0);
    auto here = addUnit(P2, kInvalidId, 10, 0);
    auto there = addUnit(P2, kInvalidId, 10, 1);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kYasuo, P1, yasuo, recording(offered, pickObject(here)), exec);
    EXPECT_EQ(offered, std::vector<GameObjectId>{here});
    EXPECT_EQ(state.getObject(here).damage_marked, 6);
    EXPECT_EQ(state.getObject(there).damage_marked, 0);
}
TEST_F(AuditCards0Test, CrackshotCorsair_DealsOneToEnemyHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kCrackshotCorsair, 0, 1);
    addUnit(P2, kInvalidId, 3, 0);
    auto here = addUnit(P2, kInvalidId, 3, 1);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kCrackshotCorsair, P1, me, recording(offered, pickObject(here)), exec);
    EXPECT_EQ(offered, std::vector<GameObjectId>{here});
    EXPECT_EQ(state.getObject(here).damage_marked, 1);
}

// ── 80 Mystic Reversal ──
TEST_F(AuditCards0Test, MysticReversal_StealsChosenSpell) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bottom = pushSpellOnChain(P2, kShakedown);
    auto top = pushSpellOnChain(P2, kShakedown);
    auto mr = state.createObject();
    driveResolve(kMysticReversal, P1, mr, {}, script({pickObject(bottom), pickValue(0)}), exec);
    EXPECT_EQ(state.chain.items.front().controller, P1);
    EXPECT_EQ(state.getObject(bottom).controller, P1);
    EXPECT_EQ(state.chain.items.back().controller, P2);
    EXPECT_EQ(state.getObject(top).controller, P2);
}

// ── 82 / 92 / 97 / 132 one-target play triggers ──
TEST_F(AuditCards0Test, WhiteflameProtector_GivesEight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kWhiteflame, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    driveResumableTrigger(kWhiteflame, P1, me, pickObject(ally), exec);
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 8);
}
TEST_F(AuditCards0Test, RiptideRex_DealsSixToEnemyAtBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kRiptideRex, 0, 0);
    auto at_base = addUnit(P2, kInvalidId, 3);
    auto at_bf = addUnit(P2, kInvalidId, 7, 1);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kRiptideRex, P1, me, recording(offered, pickObject(at_bf)), exec);
    EXPECT_EQ(offered, std::vector<GameObjectId>{at_bf});
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 6);
    EXPECT_EQ(state.getObject(at_base).damage_marked, 0);
}
TEST_F(AuditCards0Test, BlastconeFae_MinusTwoToMinimumOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kBlastconeFae, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    driveResumableTrigger(kBlastconeFae, P1, me, pickObject(enemy), exec);
    state.getObject(enemy).recomputeMight();
    EXPECT_EQ(state.getObject(enemy).current_might, 1);
}
TEST_F(AuditCards0Test, FirstMate_ReadiesAnotherUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kFirstMate, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(me).is_exhausted = true;
    state.getObject(ally).is_exhausted = true;
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kFirstMate, P1, me, recording(offered, pickObject(ally)), exec);
    EXPECT_EQ(std::count(offered.begin(), offered.end(), me), 0) << "another";
    EXPECT_FALSE(state.getObject(ally).is_exhausted);
    EXPECT_TRUE(state.getObject(me).is_exhausted);
}

// ── 100 Gemcraft Seer ──
TEST_F(AuditCards0Test, GemcraftSeer_OtherFriendlyUnitsGetTheVisionLook) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto seer = addUnit(P1, kGemcraftSeer, 0, 0);
    auto played = addUnit(P1, kInvalidId, 2, 0);
    auto bottom = addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);
    Card* c = card_registry.get(kGemcraftSeer);
    EXPECT_TRUE(c->triggerCondition(state, seer, P1, TriggerType::WhenYouPlayAUnit, played));
    EXPECT_FALSE(c->triggerCondition(state, seer, P1, TriggerType::WhenYouPlayAUnit, seer));
    driveResumableTrigger(kGemcraftSeer, P1, seer, pickValue(1), exec,
                          TriggerType::WhenYouPlayAUnit, played);
    EXPECT_EQ(state.player(P1).main_deck.front(), top) << "recycled to the bottom";
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom);

    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(auraHas(played, Keyword::Vision));
    EXPECT_FALSE(auraHas(seer, Keyword::Vision));
}

// ── 106 Sprite Mother ──
TEST_F(AuditCards0Test, SpriteMother_IsNotTemporaryHerToken_Is) {
    EXPECT_FALSE(card_db.get(kSpriteMother).keywords.has(Keyword::Temporary));
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kSpriteMother, 0, 0);
    fireTriggerAs(kSpriteMother, P1, me, TriggerType::WhenYouPlayMe, exec);
    int sprites = 0;
    for (auto& [id, o] : state.objects)
        if (o.name == "Sprite" && o.keywords.has(Keyword::Temporary)) ++sprites;
    EXPECT_EQ(sprites, 1);
}

// ── 110 Ekko, Recurrent ──
TEST_F(AuditCards0Test, Ekko_RecyclesHimselfToReadyRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ekko = addUnit(P1, kEkko, 0, 0);
    auto& e = state.getObject(ekko);
    e.location.reset();
    e.zone = ZoneType::Trash;
    state.player(P1).trash.push_back(ekko);
    auto rune = addRune(P1, Domain::Mind, /*exhausted=*/true);
    driveResumableTrigger(kEkko, P1, ekko, nullptr, exec, TriggerType::WhenIDie);
    EXPECT_FALSE(inTrash(P1, ekko));
    ASSERT_FALSE(state.player(P1).main_deck.empty());
    EXPECT_EQ(state.player(P1).main_deck.front(), ekko) << "bottom of the Main Deck";
    EXPECT_FALSE(state.getObject(rune).is_exhausted);
}
TEST_F(AuditCards0Test, Ekko_NotInTrashNoReady) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ekko = addUnit(P1, kEkko, 0, 0);
    state.getObject(ekko).location.reset();
    state.getObject(ekko).zone = ZoneType::Banishment;
    auto rune = addRune(P1, Domain::Mind, /*exhausted=*/true);
    driveResumableTrigger(kEkko, P1, ekko, nullptr, exec, TriggerType::WhenIDie);
    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "the cost couldn't be paid";
}

// ── 122 Time Warp ──
TEST_F(AuditCards0Test, TimeWarp_BanishedByTheChainNotDuringResolve) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto tw = pushSpellOnChain(P1, kTimeWarp);
    state.chain.items.clear();
    invokeOnResolve(tw, kTimeWarp, P1, {}, exec);
    EXPECT_TRUE(state.getObject(tw).banish_on_leave_chain);
    EXPECT_TRUE(state.player(P1).banishment.empty()) << "not banished twice";
    EXPECT_EQ(state.player(P1).additional_turns.size(), 1u);
}

// ── 124 Arena Bar ──
TEST_F(AuditCards0Test, ArenaBar_OnlyExhaustedFriendlyUnits) {
    auto ready = addUnit(P1, kInvalidId, 2, 0);
    auto tired = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(tired).is_exhausted = true;
    auto legal = card_registry.get(kArenaBar)->enumerateLegalTargets(state, P1);
    EXPECT_EQ(legal, std::vector<GameObjectId>{tired});
    (void)ready;
}

// ── 127 Cannon Barrage ──
TEST_F(AuditCards0Test, CannonBarrage_OnlyEnemyUnitsInCombat) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.battlefields[0].combat_in_progress = true;
    auto fighting = addUnit(P2, kInvalidId, 5, 0);
    auto idle = addUnit(P2, kInvalidId, 5, 1);
    auto home = addUnit(P2, kInvalidId, 5);
    auto mine = addUnit(P1, kInvalidId, 5, 0);
    auto spell = state.createObject();
    invokeOnResolve(spell, kCannonBarrage, P1, {}, exec);
    EXPECT_EQ(state.getObject(fighting).damage_marked, 2);
    EXPECT_EQ(state.getObject(idle).damage_marked, 0);
    EXPECT_EQ(state.getObject(home).damage_marked, 0);
    EXPECT_EQ(state.getObject(mine).damage_marked, 0);
}

// ── 129 Confront ──
TEST_F(AuditCards0Test, Confront_UnitsEnterReadyThisTurnAndDraw) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kInvalidId);
    auto spell = state.createObject();
    invokeOnResolve(spell, kConfront, P1, {}, exec);
    EXPECT_TRUE(state.player(P1).units_enter_ready_this_turn);
    EXPECT_EQ(handSize(P1), 1);
    state.player(P1).resetTurnTracking();
    EXPECT_FALSE(state.player(P1).units_enter_ready_this_turn) << "this turn only";
}

// ── 144 Spoils of War ──
TEST_F(AuditCards0Test, SpoilsOfWar_DiscountOnlyAfterAnEnemyDiedThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kSpoilsOfWar);
    auto mine = addUnit(P1, kInvalidId, 1, 0);
    exec.killObject(mine);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0) << "a friendly death doesn't count";
    auto enemy = addUnit(P2, kInvalidId, 1, 0);
    exec.killObject(enemy);
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    state.player(P2).resetTurnTracking();
    EXPECT_EQ(c->selfCostReduction(state, P1), 0) << "next turn";
}

// ── 147 Wildclaw Shaman ──
TEST_F(AuditCards0Test, WildclawShaman_SpendsTheChosenBuff) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kWildclawShaman, 0, 0);
    auto donor = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(me).is_exhausted = true;
    state.getObject(donor).buff_count = 1;
    driveResumableTrigger(kWildclawShaman, P1, me, script({pickValue(1), pickObject(donor)}), exec);
    EXPECT_EQ(state.getObject(donor).buff_count, 0) << "the buff was spent";
    EXPECT_EQ(state.getObject(me).buff_count, 1);
    EXPECT_FALSE(state.getObject(me).is_exhausted);
}
TEST_F(AuditCards0Test, WildclawShaman_NoBuffToSpendNoEffect) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kWildclawShaman, 0, 0);
    state.getObject(me).is_exhausted = true;
    driveResumableTrigger(kWildclawShaman, P1, me, pickValue(1), exec);
    EXPECT_EQ(state.getObject(me).buff_count, 0);
    EXPECT_TRUE(state.getObject(me).is_exhausted);
}

// ── 148 Anivia, Primal ──
TEST_F(AuditCards0Test, Anivia_OnlyEnemyUnitsHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kAnivia, 0, 0);
    auto here = addUnit(P2, kInvalidId, 5, 0);
    auto there = addUnit(P2, kInvalidId, 5, 1);
    fireTriggerAs(kAnivia, P1, me, TriggerType::WhenIAttack, exec);
    EXPECT_EQ(state.getObject(here).damage_marked, 3);
    EXPECT_EQ(state.getObject(there).damage_marked, 0);
}

// ── 153 Overt Operation ──
TEST_F(AuditCards0Test, OvertOperation_PerUnitSpendAndNoStacking) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    auto c = addUnit(P1, kInvalidId, 2, 0);
    for (auto id : {a, b}) {
        state.getObject(id).buff_count = 1;
        state.getObject(id).is_exhausted = true;
        state.getObject(id).recomputeMight();
    }
    auto spell = state.createObject();
    int prompts = 0;
    driveResolve(kOvertOperation, P1, spell, {}, script({pickValue(1), pickValue(0)}), exec,
                 &prompts);
    EXPECT_EQ(prompts, 2) << "one question per buffed unit";
    EXPECT_FALSE(state.getObject(a).is_exhausted);
    EXPECT_TRUE(state.getObject(b).is_exhausted);
    for (auto id : {a, b, c}) {
        EXPECT_EQ(state.getObject(id).buff_count, 1);
        EXPECT_EQ(state.getObject(id).current_might, 3);
    }
}

// ── 157 / 167 keyword sweep ──
TEST_F(AuditCards0Test, Udyr_HasNoPrintedGanking) {
    EXPECT_FALSE(card_db.get(kUdyr).keywords.has(Keyword::Ganking));
}
TEST_F(AuditCards0Test, EmberMonk_IsNotHidden) {
    EXPECT_FALSE(card_db.get(kEmberMonk).keywords.has(Keyword::Hidden));
}

// ── 168 Fight or Flight ──
TEST_F(AuditCards0Test, FightOrFlight_TargetsUnitsAtBattlefieldsOnly) {
    auto at_base = addUnit(P2, kInvalidId, 2);
    auto at_bf = addUnit(P2, kInvalidId, 2, 0);
    auto legal = card_registry.get(kFightOrFlight)->enumerateLegalTargets(state, P1);
    EXPECT_EQ(std::count(legal.begin(), legal.end(), at_base), 0);
    EXPECT_EQ(std::count(legal.begin(), legal.end(), at_bf), 1);
}

// ── 173 Ride the Wind ──
TEST_F(AuditCards0Test, RideTheWind_MovesToTheChosenBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u = addUnit(P1, kInvalidId, 2);
    state.getObject(u).is_exhausted = true;
    auto spell = state.createObject();
    driveResolve(kRideTheWind, P1, spell, {}, script({pickObject(u), pickValue(2)}), exec);
    EXPECT_EQ(state.getObject(u).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_FALSE(state.getObject(u).is_exhausted);
}

}  // namespace
}  // namespace riftbound::test
