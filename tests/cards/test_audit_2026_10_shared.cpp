/// @file test_audit_2026_10_shared.cpp
/// 2026-10 audit follow-up, "shared" batch: the [Weaponmaster] base
/// (units/weaponmaster_base.h), the [A] universal equip base
/// (gear/equip_base.h), attached-Equipment [Temporary], and the CostModifier
/// lifetime / type filters.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kRagingFirebrand  = 31;
constexpr CardDefId kEagerApprentice  = 84;
constexpr CardDefId kHeraldOfScales   = 140;
constexpr CardDefId kArmedAssailant   = 324;
constexpr CardDefId kSentinelAdept    = 331;
constexpr CardDefId kLongSword        = 345;   // [Equip] [R], +2
constexpr CardDefId kOrnnForgeGod     = 407;
constexpr CardDefId kCombatChef       = 414;
constexpr CardDefId kDoransBlade      = 417;   // [Equip] [O]
constexpr CardDefId kVeteranPoro421   = 421;
constexpr CardDefId kAkshan           = 431;
constexpr CardDefId kLucian           = 435;
constexpr CardDefId kJax              = 440;
constexpr CardDefId kMasterBingwen    = 448;
constexpr CardDefId kHardBargain      = 457;   // spell, [2]
constexpr CardDefId kVexCheerless     = 467;
constexpr CardDefId kSpinningAxe      = 504;
constexpr CardDefId kForgefireCape    = 507;
constexpr CardDefId kShurelya         = 509;
constexpr CardDefId kOrnnsForge       = 527;
constexpr CardDefId kYone             = 544;
constexpr CardDefId kBlightedBattleaxe = 581; // [Equip] [1][R], +4
constexpr CardDefId kVaultsOfHelia    = 775;
constexpr CardDefId kVeteranPoro779   = 779;
constexpr CardDefId kRiven            = 828;
constexpr CardDefId kAstralHeron      = 831;
constexpr CardDefId kBlazingScorcher  = 1;     // Dragon unit, [5]
constexpr CardDefId kDefy             = 45;    // spell, [1][G]

class SharedBatchTest : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = 0) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        const auto& def = card_db.get(def_id);
        obj.name = def.name;
        obj.keywords = def.keywords;
        obj.might_bonus = def.might_bonus;
        obj.tags = def.tags;
        obj.domains = def.domains;
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
        u.attachment_might_bonus += g.might_bonus;
        u.recomputeMight();
    }

    static Intent yesOr(GameObjectId pick, const std::vector<Intent>& legal) {
        for (auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        for (auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == pick) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
    std::function<Intent(const std::vector<Intent>&)> yesThen(GameObjectId pick,
                                                              int* prompts = nullptr,
                                                              std::vector<GameObjectId>* offered = nullptr) {
        return [pick, prompts, offered](const std::vector<Intent>& legal) {
            if (prompts) ++*prompts;
            if (offered)
                for (auto& i : legal)
                    if (!i.chosen_objects.empty()) offered->push_back(i.chosen_objects[0]);
            return yesOr(pick, legal);
        };
    }
    static Intent no(const std::vector<Intent>& legal) {
        for (auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.front();
    }

    /// Finalize a triggered ability (choices), run `between`, then resolve it.
    void finalizeThenResolve(CardDefId def_id, GameObjectId source, TriggerType firing,
                             std::function<Intent(const std::vector<Intent>&)> picker,
                             EffectExecutor& exec, std::function<void()> between = nullptr) {
        Card* card = card_registry.get(def_id);
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = P1;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        ri.finalizing = true;
        state.chain.resuming = ri;
        CardContext ctx{state, events, exec, P1, source};
        ctx.firing_trigger = firing;
        auto drive = [&] {
            card->onTrigger(ctx, {});
            while (exec.hasPendingChoice()) {
                auto pending = exec.consumePendingChoice();
                exec.recordChoice(picker(pending.legal));
                card->onTrigger(ctx, {});
            }
        };
        drive();
        ChainItem done = *state.chain.resuming;
        done.finalizing = false;
        done.choices_finalized = true;
        done.finalize_checked = 0;
        state.chain.resuming.reset();
        if (between) between();
        state.chain.resuming = done;
        drive();
        state.chain.resuming.reset();
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [Weaponmaster] — every Weaponmaster unit: optional, chosen, Equip cost - [A]
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SharedBatchTest, Weaponmaster_AllUnitsEquipChosenEquipmentForReducedCost) {
    for (auto wm_id : {kArmedAssailant, kSentinelAdept, kOrnnForgeGod, kCombatChef,
                       kVeteranPoro421, kLucian, kJax, kMasterBingwen, kYone,
                       kVeteranPoro779, kRiven}) {
        SCOPED_TRACE(wm_id);
        state.objects.clear();
        EffectExecutor exec(state, events, card_db, &card_registry);
        auto wm = addUnit(P1, wm_id, 0, 0);
        auto sword = addGear(P1, kLongSword);        // [R] - [A] = free
        auto axe = addGear(P1, kBlightedBattleaxe);  // [1][R] - [A] = [1]
        addRune(P1, Domain::Calm);
        std::vector<GameObjectId> offered;
        driveResumableTrigger(wm_id, P1, wm, yesThen(axe, nullptr, &offered), exec,
                              TriggerType::WhenYouPlayMe);
        EXPECT_NE(std::find(offered.begin(), offered.end(), sword), offered.end());
        EXPECT_NE(std::find(offered.begin(), offered.end(), axe), offered.end());
        EXPECT_EQ(state.getObject(axe).attached_to, std::optional<GameObjectId>(wm))
            << "the controller's pick, not the first Equipment";
        EXPECT_FALSE(state.getObject(sword).attached_to.has_value());
        EXPECT_EQ(exhaustedRuneCount(P1), 1) << "[1][R] less [A] = [1] paid";
    }
}

TEST_F(SharedBatchTest, Weaponmaster_IsOptional) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto wm = addUnit(P1, kCombatChef, 0, 0);
    auto sword = addGear(P1, kLongSword);
    driveResumableTrigger(kCombatChef, P1, wm, &SharedBatchTest::no, exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(sword).attached_to.has_value());
}

TEST_F(SharedBatchTest, Weaponmaster_UnpayableEquipmentNotOffered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto wm = addUnit(P1, kArmedAssailant, 0, 0);
    auto axe = addGear(P1, kBlightedBattleaxe);   // needs [1]: no runes
    int prompts = 0;
    driveResumableTrigger(kArmedAssailant, P1, wm, yesThen(axe, &prompts), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_EQ(prompts, 0) << "nothing payable to choose: the ability isn't offered";
    EXPECT_FALSE(state.getObject(axe).attached_to.has_value()) << "no free equip";
}

TEST_F(SharedBatchTest, Weaponmaster_ChoiceAtFinalizationCostAtResolution) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto wm = addUnit(P1, kMasterBingwen, 0, 0);
    auto axe = addGear(P1, kBlightedBattleaxe);
    auto rune = addRune(P1, Domain::Fury);
    finalizeThenResolve(kMasterBingwen, wm, TriggerType::WhenYouPlayMe, yesThen(axe), exec,
        [&] {
            EXPECT_FALSE(state.getObject(axe).attached_to.has_value()) << "nothing paid yet";
            EXPECT_EQ(exhaustedRuneCount(P1), 0);
            state.getObject(rune).is_exhausted = true;   // can't pay the [1] any more
        });
    EXPECT_FALSE(state.getObject(axe).attached_to.has_value())
        << "CR 821.1.c.5: unpaid, it stays where it is";
}

TEST_F(SharedBatchTest, Weaponmaster_TakesEquipmentOffAnotherUnitAndPays) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto holder = addUnit(P1, kInvalidId, 2, 0);
    auto axe = addGear(P1, kBlightedBattleaxe);
    attach(axe, holder);
    auto ornn = addUnit(P1, kOrnnForgeGod, 0, 1);
    addRune(P1, Domain::Mind);
    finalizeThenResolve(kOrnnForgeGod, ornn, TriggerType::WhenYouPlayMe, yesThen(axe), exec);
    EXPECT_EQ(state.getObject(axe).attached_to, std::optional<GameObjectId>(ornn));
    EXPECT_TRUE(state.getObject(holder).attachments.empty());
    EXPECT_EQ(state.getObject(holder).attachment_might_bonus, 0);
    EXPECT_EQ(state.getObject(axe).battlefieldId().value_or(kInvalidId), 1u);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
}

TEST_F(SharedBatchTest, Lucian_ConquerBranchStillReadiesOnce) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto lucian = addUnit(P1, kLucian, 0, 0);
    state.getObject(lucian).is_exhausted = true;
    fireTriggerAs(kLucian, P1, lucian, TriggerType::WhenIConquer, exec);
    EXPECT_FALSE(state.getObject(lucian).is_exhausted);
    state.getObject(lucian).is_exhausted = true;
    fireTriggerAs(kLucian, P1, lucian, TriggerType::WhenIConquer, exec);
    EXPECT_TRUE(state.getObject(lucian).is_exhausted) << "first time each turn only";
}

TEST_F(SharedBatchTest, Riven_PlayTriggerIsWeaponmasterNotAttack) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto riven = addUnit(P1, kRiven, 0, 0);
    auto sword = addGear(P1, kLongSword);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    finalizeThenResolve(kRiven, riven, TriggerType::WhenYouPlayMe, yesThen(sword), exec);
    EXPECT_EQ(state.getObject(sword).attached_to, std::optional<GameObjectId>(riven));
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0);
}

TEST_F(SharedBatchTest, Jax_RiderCostPaidAtFinalization) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJax, 0, 0);
    state.player(P1).rune_pool.energy = 1;
    addToDeck(P1, kInvalidId);
    int hand_before = handSize(P1);
    finalizeThenResolve(kJax, jax, TriggerType::WhenEquipmentAttachedToMe,
                        yesThen(kInvalidId), exec, [&] {
        EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "[1] paid as it is finalized";
        EXPECT_EQ(handSize(P1), hand_before);
    });
    EXPECT_EQ(handSize(P1), hand_before + 1);
}

// ─── Akshan, Mischievous ───────────────────────────────────────────────────

TEST_F(SharedBatchTest, Akshan_AdditionalCostIsBodyPower) {
    const auto cost = card_registry.get(kAkshan)->optionalAdditionalCost();
    EXPECT_TRUE(cost.valid);
    EXPECT_EQ(cost.power, 2);
    EXPECT_EQ(cost.power_domain, Domain::Body) << "[O] is Body";
}

TEST_F(SharedBatchTest, Akshan_StealsTheChosenEnemyGearAndHasWeaponmaster) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto akshan = addUnit(P1, kAkshan, 0, 0);
    state.getObject(akshan).card_counters["__akshan_paid"] = 1;
    auto own_sword = addGear(P1, kLongSword);
    auto enemy_axe = addGear(P2, kSpinningAxe);
    auto enemy_blade = addGear(P2, kDoransBlade);
    std::vector<GameObjectId> offered;
    // Two abilities, two chain items: the steal (index 1) picks Doran's Blade;
    // [Weaponmaster] (index 0): yes + own Long Sword.
    EXPECT_EQ(card_registry.get(kAkshan)->triggeredAbilityCount(state, akshan,
                                                                TriggerType::WhenYouPlayMe), 2);
    driveResumableTrigger(kAkshan, P1, akshan,
        [&](const std::vector<Intent>& legal) {
            for (auto& i : legal)
                if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
            return yesOr(enemy_blade, legal);
        }, exec, TriggerType::WhenYouPlayMe, kInvalidId, /*trigger_index=*/1);
    int step = 0;
    driveResumableTrigger(kAkshan, P1, akshan,
        [&](const std::vector<Intent>& legal) {
            ++step;
            if (step == 1) return yesOr(kInvalidId, legal);
            return yesOr(own_sword, legal);
        }, exec, TriggerType::WhenYouPlayMe, kInvalidId, /*trigger_index=*/0);
    EXPECT_EQ(offered.size(), 2u) << "either enemy gear may be moved";
    EXPECT_EQ(state.getObject(enemy_blade).controller, P1);
    EXPECT_EQ(state.getObject(enemy_blade).attached_to, std::optional<GameObjectId>(akshan));
    EXPECT_EQ(state.getObject(enemy_axe).controller, P2) << "not the first gear found";
    EXPECT_EQ(state.getObject(own_sword).attached_to, std::optional<GameObjectId>(akshan))
        << "[Weaponmaster] is no longer just a keyword bit";
}

// ─── Yone, Blademaster ─────────────────────────────────────────────────────

TEST_F(SharedBatchTest, Yone_ConquerOnlyFromUncontrolledAndChosenTarget) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* yone_card = card_registry.get(kYone);
    auto yone = addUnit(P1, kYone, 5, 0);
    state.battlefields[0].conquered_from_uncontrolled = false;
    EXPECT_FALSE(yone_card->triggerCondition(state, yone, P1, TriggerType::WhenIConquer, kInvalidId))
        << "conquered from the opponent";
    state.battlefields[0].conquered_from_uncontrolled = true;
    EXPECT_TRUE(yone_card->triggerCondition(state, yone, P1, TriggerType::WhenIConquer, kInvalidId));

    auto e1 = addUnit(P2, kInvalidId, 9);
    auto e2 = addUnit(P2, kInvalidId, 9);
    auto e_bf = addUnit(P2, kInvalidId, 9, 1);
    std::vector<GameObjectId> offered;
    finalizeThenResolve(kYone, yone, TriggerType::WhenIConquer, yesThen(e2, nullptr, &offered), exec);
    EXPECT_EQ(state.getObject(e2).damage_marked, 5);
    EXPECT_EQ(state.getObject(e1).damage_marked, 0) << "the player's choice";
    EXPECT_EQ(std::find(offered.begin(), offered.end(), e_bf), offered.end())
        << "only enemy units in a base";
}

// ═══════════════════════════════════════════════════════════════════════════
// UniversalEquipGear ([Equip] [A]): no free equip
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SharedBatchTest, UniversalEquip_NeedsAPowerSource) {
    for (auto gear_id : {kSpinningAxe, kForgefireCape, kShurelya}) {
        SCOPED_TRACE(gear_id);
        state.objects.clear();
        state.player(P1).rune_pool = {};
        EffectExecutor exec(state, events, card_db, &card_registry);
        auto unit = addUnit(P1, kInvalidId, 2, 0);
        auto gear = addGear(P1, gear_id);
        CardContext ctx{state, events, exec, P1, gear};
        Card* c = card_registry.get(gear_id);
        EXPECT_FALSE(c->onEquip(ctx, unit)) << "no rune, no floating power";
        EXPECT_FALSE(state.getObject(gear).attached_to.has_value());

        state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
        EXPECT_TRUE(c->onEquip(ctx, unit)) << "floating power pays the [A]";
        EXPECT_EQ(state.player(P1).rune_pool.totalPower(), 0);
        EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Engine-driven: [Temporary] on attached Equipment, CostModifier lifetimes
// ═══════════════════════════════════════════════════════════════════════════

class SharedEngineTest : public CardTestFixture {
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
    GameObjectId obj(PlayerId p, CardDefId def_id, ZoneType zone, int at_bf = -1) {
        auto id = s().createObject();
        auto& o = s().getObject(id);
        const auto& def = card_db.get(def_id);
        o.owner = o.controller = p;
        o.card_def_id = def_id;
        o.card_type = def.card_type;
        o.super_type = def.super_type;
        o.name = def.name;
        o.domains = def.domains;
        o.keywords = def.keywords;
        o.tags = def.tags;
        o.base_might = o.current_might = def.might;
        o.might_bonus = def.might_bonus;
        o.zone = zone;
        if (zone == ZoneType::Hand) s().player(p).hand.push_back(id);
        else if (at_bf >= 0) o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        else o.location = BaseLocation{p};
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId d) { return obj(p, d, ZoneType::Hand); }
    void runes(PlayerId p, int n, Domain d = Domain::Fury) {
        for (int i = 0; i < n; ++i) {
            auto id = s().createObject();
            auto& r = s().getObject(id);
            r.owner = r.controller = p;
            r.card_type = CardType::Rune;
            r.name = "Rune";
            r.domains = {d};
            r.zone = ZoneType::Base;
            r.location = BaseLocation{p};
        }
    }
    bool afford(PlayerId p, GameObjectId c) { return engine_->testHook_canAfford(p, c); }
};

TEST_F(SharedEngineTest, SpinningAxe_TemporaryOnlyWhileUnattached) {
    auto unit = obj(P1, kVeteranPoro421, ZoneType::BattlefieldZone, 0);
    auto attached = obj(P1, kSpinningAxe, ZoneType::BattlefieldZone, 0);
    auto loose = obj(P1, kSpinningAxe, ZoneType::Base);
    s().getObject(attached).attached_to = unit;
    s().getObject(unit).attachments.push_back(attached);
    engine_->testHook_beginningStep();
    EXPECT_TRUE(s().getObject(attached).location.has_value())
        << "attached: its [Temporary] text is inactive";
    EXPECT_FALSE(s().getObject(loose).location.has_value()) << "unattached: killed";
}

TEST_F(SharedEngineTest, VexCheerless_ModifiersDoNotAccumulateOrOutliveCombat) {
    auto vex = obj(P1, kVexCheerless, ZoneType::BattlefieldZone, 0);
    auto foe = obj(P2, kVeteranPoro421, ZoneType::BattlefieldZone, 0);
    s().getObject(foe).combat_designation = CombatDesignation::Defender;
    s().battlefields[0].combat_in_progress = true;
    s().battlefields[0].attacker = P1;
    s().battlefields[0].defender = P2;
    s().getObject(vex).combat_designation = CombatDesignation::Attacker;
    for (int i = 0; i < 4; ++i) engine_->testHook_cleanup();
    EXPECT_EQ(s().player(P1).cost_modifiers.size(), 2u) << "re-asserted, not stacked";

    // Enemy spell: Defy [1][G] costs [2][G][A] — three runes of which one Calm.
    auto defy = inHand(P2, kDefy);
    runes(P2, 2, Domain::Fury);
    EXPECT_FALSE(afford(P2, defy)) << "[1][A] more";
    runes(P2, 1, Domain::Calm);
    EXPECT_TRUE(afford(P2, defy));
    // Enemy unit: not a spell, unaffected.
    auto poro = inHand(P2, kVeteranPoro421);   // [2]
    EXPECT_TRUE(afford(P2, poro));

    // Vex leaves combat: the modifiers go away.
    s().getObject(vex).combat_designation = CombatDesignation::None;
    s().battlefields[0].combat_in_progress = false;
    s().battlefields[0].attacker = std::nullopt;
    s().battlefields[0].defender = std::nullopt;
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty());
}

TEST_F(SharedEngineTest, VexCheerless_FriendlySpellsCostOneAndAnyPowerLess) {
    auto vex = obj(P1, kVexCheerless, ZoneType::BattlefieldZone, 0);
    auto foe = obj(P2, kVeteranPoro421, ZoneType::BattlefieldZone, 0);
    s().getObject(foe).combat_designation = CombatDesignation::Attacker;
    s().battlefields[0].combat_in_progress = true;
    s().battlefields[0].attacker = P2;
    s().battlefields[0].defender = P1;
    s().getObject(vex).combat_designation = CombatDesignation::Defender;
    engine_->testHook_cleanup();
    auto defy = inHand(P1, kDefy);      // [1][G] -> [1] (min 1), no Power
    runes(P1, 1, Domain::Fury);         // no Calm rune at all
    EXPECT_TRUE(afford(P1, defy));
}

TEST_F(SharedEngineTest, EagerApprentice_SpellsOnlyWhileAtABattlefield) {
    auto app = obj(P1, kEagerApprentice, ZoneType::BattlefieldZone, 0);
    engine_->testHook_cleanup();
    engine_->testHook_cleanup();
    auto spell = inHand(P1, kHardBargain);   // [2] -> [1]
    auto unit = inHand(P1, kVeteranPoro421); // [2], not a spell
    runes(P1, 1);
    EXPECT_TRUE(afford(P1, spell));
    EXPECT_FALSE(afford(P1, unit)) << "units are not discounted";
    // Back at base: no discount, and nothing left over.
    s().getObject(app).location = BaseLocation{P1};
    s().getObject(app).zone = ZoneType::Base;
    engine_->testHook_cleanup();
    EXPECT_FALSE(afford(P1, spell));
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty());
}

TEST_F(SharedEngineTest, HeraldOfScales_DragonsOnly) {
    obj(P1, kHeraldOfScales, ZoneType::Base);
    engine_->testHook_cleanup();
    engine_->testHook_cleanup();
    EXPECT_EQ(s().player(P1).cost_modifiers.size(), 1u) << "not stacked";
    auto dragon = inHand(P1, kBlazingScorcher);  // [5] -> [3]
    auto poro = inHand(P1, kVeteranPoro421);     // [2], not a Dragon
    auto spell = inHand(P1, kHardBargain);       // [2], not a Dragon
    runes(P1, 1);
    EXPECT_FALSE(afford(P1, poro));
    EXPECT_FALSE(afford(P1, spell));
    runes(P1, 2);
    EXPECT_TRUE(afford(P1, dragon));
}

TEST_F(SharedEngineTest, OrnnsForge_DoesNotStackAcrossCleanups) {
    auto bf_card = s().createObject();
    s().getObject(bf_card).card_def_id = kOrnnsForge;
    s().getObject(bf_card).card_type = CardType::Battlefield;
    s().battlefields[0].card_object_id = bf_card;
    s().battlefields[0].controller = P1;
    auto holder = obj(P1, kVeteranPoro421, ZoneType::BattlefieldZone, 0);   // holds it
    for (int i = 0; i < 3; ++i) engine_->testHook_cleanup();
    EXPECT_EQ(s().player(P1).cost_modifiers.size(), 1u);
    s().battlefields[0].controller = P2;   // lost control
    s().getObject(holder).location = BaseLocation{P1};
    s().getObject(holder).zone = ZoneType::Base;
    obj(P2, kVeteranPoro421, ZoneType::BattlefieldZone, 0);
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty()) << "control lapsed";
}

TEST_F(SharedEngineTest, VaultsOfHelia_TaxesUnitsNotSpells) {
    auto bf_card = s().createObject();
    s().getObject(bf_card).card_def_id = kVaultsOfHelia;
    EffectExecutor& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, bf_card};
    card_registry.get(kVaultsOfHelia)->onTrigger(ctx, {});
    auto spell = inHand(P1, kHardBargain);   // [2]
    auto unit = inHand(P1, kVeteranPoro421); // [2] -> [3]
    runes(P1, 2);
    EXPECT_TRUE(afford(P1, spell));
    EXPECT_FALSE(afford(P1, unit));
}

TEST_F(SharedEngineTest, RagingFirebrand_NextSpellOnly) {
    auto src = obj(P1, kRagingFirebrand, ZoneType::BattlefieldZone, 0);
    EffectExecutor& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, src};
    card_registry.get(kRagingFirebrand)->onTrigger(ctx, {});
    auto spell = inHand(P1, kHardBargain);   // [2] -> [0]
    auto unit = inHand(P1, kVeteranPoro421);
    EXPECT_TRUE(afford(P1, spell));
    EXPECT_FALSE(afford(P1, unit));
}

TEST_F(SharedEngineTest, AstralHeron_NextCardThisTurnOnly) {
    auto heron = obj(P1, kAstralHeron, ZoneType::BattlefieldZone, 0);
    s().player(P1).cards_played_this_turn = 1;
    EffectExecutor& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, heron};
    card_registry.get(kAstralHeron)->onTrigger(ctx, {});
    ASSERT_EQ(s().player(P1).cost_modifiers.size(), 1u);
    EXPECT_TRUE(s().player(P1).cost_modifiers[0].this_turn_only) << "errata: \"this turn\"";
    auto unit = inHand(P1, kVeteranPoro421);  // [2] -> [0]
    EXPECT_TRUE(afford(P1, unit));
}

}  // namespace
