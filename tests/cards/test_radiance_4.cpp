/// @file test_radiance_4.cpp
/// Radiance batch 4 (RAD-065 .. RAD-083, card ids 1045-1063).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

namespace riftbound::test {
namespace {

constexpr CardDefId kSeraphine = 1045;
constexpr CardDefId kSpiritForge = 1046;
constexpr CardDefId kClocktower = 1047;
constexpr CardDefId kOrianna = 1048;
constexpr CardDefId kZeroPoint = 1049;
constexpr CardDefId kHoldEm = 1050;
constexpr CardDefId kNazuman = 1051;
constexpr CardDefId kTraveler = 1052;
constexpr CardDefId kPillaged = 1053;
constexpr CardDefId kRaving = 1054;
constexpr CardDefId kSkystrike = 1055;
constexpr CardDefId kZeebles = 1056;
constexpr CardDefId kWrath = 1057;
constexpr CardDefId kTusker = 1058;
constexpr CardDefId kAvalanche = 1059;
constexpr CardDefId kBloodscent = 1060;
constexpr CardDefId kElnuk = 1061;
constexpr CardDefId kRoar = 1062;
constexpr CardDefId kGoatherd = 1063;
constexpr CardDefId kBladeOfExile = 958;   // a 3-Energy gear with no triggers

using Picker = std::function<Intent(const std::vector<Intent>&)>;
Picker pickValue(int v) {
    return [v](const std::vector<Intent>& legal) -> Intent {
        for (auto& i : legal) if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
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
Picker script(std::vector<Picker> steps) {
    auto shared = std::make_shared<std::pair<std::vector<Picker>, size_t>>(std::move(steps), 0);
    return [shared](const std::vector<Intent>& legal) -> Intent {
        auto& [s, i] = *shared;
        if (i < s.size()) return s[i++](legal);
        return legal.empty() ? Intent{} : legal.front();
    };
}

/// Picks the option choosing `want` when offered, else passes priority, else
/// the first option.
class WantAgent : public AgentInterface {
public:
    GameObjectId want = kInvalidId;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

class Radiance4Test : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) obj.name = card_db.get(def_id).name;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }
    void driveResolve(CardDefId def_id, PlayerId controller, GameObjectId source,
                      Picker picker, EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onResolve(ctx, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, {});
        }
        state.chain.resuming.reset();
    }
    bool chainHasSource(GameObjectId src) {
        for (const auto& i : state.chain.items) if (i.source == src) return true;
        return false;
    }
};

/// Engine-driven tests (cost payment, standard moves).
class Radiance4EngineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    WantAgent a1_, a2_;
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
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        for (auto p : {P1, P2}) {
            auto id = st.createObject();
            auto& lo = st.getObject(id);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = id;
        }
    }
    GameObjectId cardInHand(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = def.card_type;
        c.name = def.name;
        c.domains = def.domains;
        c.keywords = def.keywords;
        c.tags = def.tags;
        c.zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId gearInBase(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& g = s().getObject(id);
        g.owner = g.controller = p;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        g.name = card_db.get(def_id).name;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        return id;
    }
    GameObjectId unitAt(PlayerId p, CardDefId def_id, int might, int at_bf = -1) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.card_def_id = def_id;
        u.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestUnit";
        u.base_might = u.current_might = might;
        if (at_bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{p};
        }
        return id;
    }
    void play(GameObjectId card, std::optional<LocationId> loc = std::nullopt) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = P1;
        i.card = card;
        i.play_location = loc;
        engine_->testHook_executeIntent(i);
    }
};

// ── Card data ──
TEST_F(Radiance4Test, AllCardsRegisteredWithRadianceData) {
    for (CardDefId id = kSeraphine; id <= kGoatherd; ++id) {
        Card* c = card_registry.get(id);
        ASSERT_NE(c, nullptr) << id;
        const auto& d = c->def();
        EXPECT_EQ(d.id, id);
        EXPECT_EQ(d.set_code, "RAD");
        EXPECT_EQ(d.collector_number, static_cast<int>(id) - 980);
    }
    EXPECT_EQ(card_db.get(kSeraphine).super_type, SuperType::Champion);
    EXPECT_EQ(card_registry.get(kSeraphine)->disarmValue(), 2);
    EXPECT_TRUE(card_db.get(kSkystrike).keywords.has(Keyword::Deflect));
    EXPECT_TRUE(card_db.get(kTusker).keywords.has(Keyword::Tank));
    EXPECT_EQ(card_db.get(kTusker).shield_value, 3);
    EXPECT_EQ(card_db.get(kGoatherd).shield_value, 2);
    EXPECT_TRUE(card_db.get(kElnuk).keywords.has(Keyword::Hidden));
    EXPECT_TRUE(card_db.get(kClocktower).keywords.has(Keyword::Deathknell));
    EXPECT_TRUE(card_registry.get(kPillaged)->hasDeploy());
    EXPECT_TRUE(card_db.get(kHoldEm).keywords.has(Keyword::Action));
    EXPECT_FALSE(card_db.get(kAvalanche).keywords.has(Keyword::Action));
}

// ── 1045 Seraphine, Inspiring ──
TEST_F(Radiance4Test, Seraphine_TriggersOnThreeUnitsExhaustedTogether) {
    auto sera = addUnit(P1, kSeraphine, 0, 0);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    state.player(P1).last_exhaust_group_size = 2;
    events.emit(ObjectStateChangedEvent{u, "exhausted_together"});
    EXPECT_FALSE(chainHasSource(sera)) << "two units is not enough";
    state.player(P1).last_exhaust_group_size = 3;
    events.emit(ObjectStateChangedEvent{u, "exhausted_together"});
    EXPECT_TRUE(chainHasSource(sera));
}
TEST_F(Radiance4Test, Seraphine_MayDrawOnceEachTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto sera = addUnit(P1, kSeraphine, 0, 0);
    state.turn.turn_number = 4;
    state.player(P1).last_exhaust_group_size = 3;
    Card* c = card_registry.get(kSeraphine);
    EXPECT_TRUE(c->triggerCondition(state, sera, P1, TriggerType::WhenYouExhaustUnitsTogether, kInvalidId));
    driveResumableTrigger(kSeraphine, P1, sera, pickValue(1), exec);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_FALSE(c->triggerCondition(state, sera, P1, TriggerType::WhenYouExhaustUnitsTogether, kInvalidId))
        << "once each turn";
    state.turn.turn_number = 5;
    EXPECT_TRUE(c->triggerCondition(state, sera, P1, TriggerType::WhenYouExhaustUnitsTogether, kInvalidId));
}
TEST_F(Radiance4EngineTest, Seraphine_StandardMoveOfThreeUnitsTriggersHer) {
    makeEngine();
    auto sera = unitAt(P1, kSeraphine, 3);
    auto a = unitAt(P1, kInvalidId, 1);
    auto b = unitAt(P1, kInvalidId, 1);
    auto seen = std::make_shared<int>(0);
    auto conn = events.on_object_state_changed.connect([seen](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "exhausted_together") ++*seen;
    });
    engine_->testHook_executeStandardMove(Intent::standardMove(P1, {sera, a, b}, BattlefieldLocation{0}));
    conn.disconnect();
    EXPECT_EQ(*seen, 1);
    EXPECT_EQ(s().player(P1).last_exhaust_group_size, 3);
    bool queued = false;
    for (const auto& i : s().chain.items) if (i.source == sera) queued = true;
    EXPECT_TRUE(queued);
}

// ── 1046 Spirit of the Forge ──
TEST_F(Radiance4EngineTest, SpiritOfTheForge_ReturnsGearReadiesAndGainsItsCost) {
    makeEngine();
    auto gear = gearInBase(P1, kBladeOfExile);   // Energy cost 3
    a1_.want = gear;
    s().player(P1).rune_pool.energy = 6;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
    auto spirit = cardInHand(P1, kSpiritForge);
    play(spirit);
    const auto& sp = s().getObject(spirit);
    ASSERT_TRUE(sp.location.has_value());
    EXPECT_EQ(s().getObject(gear).zone, ZoneType::Hand) << "the gear was returned as the cost";
    EXPECT_FALSE(sp.is_exhausted) << "ready me";
    EXPECT_EQ(sp.temp_might_bonus, 3);
}
TEST_F(Radiance4EngineTest, SpiritOfTheForge_DecliningKeepsGearAndEntersExhausted) {
    makeEngine();
    auto gear = gearInBase(P1, kBladeOfExile);
    a1_.want = kInvalidId;   // decline (first option)
    s().player(P1).rune_pool.energy = 6;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
    auto spirit = cardInHand(P1, kSpiritForge);
    play(spirit);
    EXPECT_TRUE(s().getObject(gear).location.has_value());
    EXPECT_TRUE(s().getObject(spirit).is_exhausted);
    EXPECT_EQ(s().getObject(spirit).temp_might_bonus, 0);
}

// ── 1047 Clocktower Guardian ──
TEST_F(Radiance4Test, Clocktower_DeathknellBanishesTopAndMayPlayIt) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto top = addToDeck(P1, kZeebles);   // 2 Energy, ignored
    auto ct = addUnit(P1, kClocktower, 0, 0);
    driveResumableTrigger(kClocktower, P1, ct, pickValue(1), exec, TriggerType::WhenIDie);
    const auto& o = state.getObject(top);
    EXPECT_TRUE(o.location.has_value()) << "played ignoring its Energy cost";
    EXPECT_TRUE(state.player(P1).banishment.empty());
}
TEST_F(Radiance4Test, Clocktower_DecliningLeavesItBanished) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto top = addToDeck(P1, kZeebles);
    auto ct = addUnit(P1, kClocktower, 0, 0);
    driveResumableTrigger(kClocktower, P1, ct, pickValue(0), exec, TriggerType::WhenIDie);
    EXPECT_EQ(state.getObject(top).zone, ZoneType::Banishment);
    EXPECT_EQ(deckSize(P1), 0);
}
TEST_F(Radiance4Test, Clocktower_PredictsOnPlayAndHold) {
    Card* c = card_registry.get(kClocktower);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIHold));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIDie));
}

// ── 1048 Orianna, Mechanical Marvel ──
TEST_F(Radiance4Test, Orianna_GearDiscountOnceEachTurnWhileAtBattlefield) {
    auto ori = addUnit(P1, kOrianna, 0, 0);
    Card* c = card_registry.get(kOrianna);
    auto pass = [&] {
        auto& mods = state.player(P1).cost_modifiers;
        mods.erase(std::remove_if(mods.begin(), mods.end(),
                                  [](const PlayerState::CostModifier& m) { return m.from_aura; }),
                   mods.end());
        for (auto& [id, o] : state.objects) o.aura_effects.clear();
        c->applyPassiveAura(state, P1);
        int n = 0;
        for (const auto& m : state.player(P1).cost_modifiers)
            if (m.source == ori && m.gear_only && m.energy_reduction == 1 && m.power_reduction == 1) ++n;
        return n;
    };
    state.turn.turn_number = 2;
    EXPECT_EQ(pass(), 1);
    state.player(P1).gears_played_this_turn = 1;   // a gear was played, discount declined
    EXPECT_EQ(pass(), 1) << "declining keeps it for a later gear this turn";
    for (const auto& m : state.player(P1).cost_modifiers)
        if (m.source == ori) EXPECT_TRUE(m.optional) << "the player chooses whether to use it";
    state.getObject(ori).card_counters["__ori_used_turn"] = 2;   // taken (decideOptionalCostModifiers)
    EXPECT_EQ(pass(), 0) << "used this turn";
    state.turn.turn_number = 3;
    state.player(P1).gears_played_this_turn = 0;
    EXPECT_EQ(pass(), 1) << "new turn";
    state.getObject(ori).location = BaseLocation{P1};
    EXPECT_EQ(pass(), 0) << "only while at a battlefield";
}

// ── 1049 Zero Point Inflection ──
TEST_F(Radiance4Test, ZeroPoint_DrawsTwoWhenYouPlayACard) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    auto zp = addGear(P1, kZeroPoint);
    fireTriggerAs(kZeroPoint, P1, zp, TriggerType::WhenYouPlayACard, exec);
    EXPECT_EQ(handSize(P1), 2);
}
TEST_F(Radiance4Test, ZeroPoint_NineOtherCardsCondition) {
    auto zp = addGear(P1, kZeroPoint);
    for (int i = 0; i < 10; ++i) addToHand(P1, kInvalidId);
    Card* c = card_registry.get(kZeroPoint);
    state.player(P1).last_draw_count = 1;
    EXPECT_TRUE(c->triggerCondition(state, zp, P1, TriggerType::WhenYouDrawACard, kInvalidId));
    state.player(P1).last_draw_count = 2;
    EXPECT_FALSE(c->triggerCondition(state, zp, P1, TriggerType::WhenYouDrawACard, kInvalidId))
        << "only 8 other cards";
}
TEST_F(Radiance4Test, ZeroPoint_OverdrawDiscardsHandKillsItAndOpponentScores) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto zp = addGear(P1, kZeroPoint);
    for (int i = 0; i < 10; ++i) addToHand(P1, kInvalidId);
    fireTriggerAs(kZeroPoint, P1, zp, TriggerType::WhenYouDrawACard, exec);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_FALSE(state.getObject(zp).location.has_value());
    EXPECT_EQ(state.player(P2).score, 1);
    EXPECT_EQ(state.player(P1).score, 0);
}

// ── 1050 Hold 'Em ──
TEST_F(Radiance4Test, HoldEm_DiscountAndPlusThree) {
    Card* c = card_registry.get(kHoldEm);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
    state.player(P1).played_permanent_to_bf_this_turn = true;
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kHoldEm);
    driveResolve(kHoldEm, P1, spell, pickObject(u), exec);
    EXPECT_EQ(state.getObject(u).temp_might_bonus, 3);
}
TEST_F(Radiance4EngineTest, HoldEm_PlayingAUnitToABattlefieldSetsTheTracker) {
    makeEngine();
    s().player(P1).rune_pool.energy = 2;
    auto z = cardInHand(P1, kZeebles);
    EXPECT_FALSE(s().player(P1).played_permanent_to_bf_this_turn);
    play(z, LocationId{BattlefieldLocation{0}});
    EXPECT_TRUE(s().player(P1).played_permanent_to_bf_this_turn);
    s().player(P1).resetTurnTracking();
    EXPECT_FALSE(s().player(P1).played_permanent_to_bf_this_turn);
}

// ── 1051 Nazuman Ntofo ──
TEST_F(Radiance4Test, Nazuman_EquipForOneEnergyGivesShieldTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto g = addGear(P1, kNazuman);
    auto u = addUnit(P1, kInvalidId, 2, -1);
    Card* c = card_registry.get(kNazuman);
    CardContext ctx{state, events, exec, P1, g};
    EXPECT_FALSE(c->onEquip(ctx, u)) << "can't pay [1]";
    state.player(P1).rune_pool.energy = 1;
    EXPECT_TRUE(c->onEquip(ctx, u));
    EXPECT_EQ(state.getObject(g).attached_to.value_or(kInvalidId), u);
    EXPECT_EQ(c->equippedShield(), 2);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Shield));
}

// ── 1052 Overloaded Traveler ──
TEST_F(Radiance4Test, OverloadedTraveler_PlusOneWithFriendlyDeployGearHere) {
    auto t = addUnit(P1, kTraveler, 0, 0);
    auto g = addGear(P1, kPillaged, 1);
    state.getObject(g).has_deploy = true;
    Card* c = card_registry.get(kTraveler);
    auto might = [&] {
        for (auto& [id, o] : state.objects) o.aura_effects.clear();
        c->applyPassiveAura(state, P1);
        int m = 0;
        for (const auto& ae : state.getObject(t).aura_effects) m += ae.might_bonus;
        return m;
    };
    EXPECT_EQ(might(), 0) << "Deploy gear at another battlefield";
    state.getObject(g).location = BattlefieldLocation{0};
    EXPECT_EQ(might(), 1);
    state.getObject(g).controller = P2;
    EXPECT_EQ(might(), 0) << "enemy gear";
}
TEST_F(Radiance4EngineTest, HasDeployIsRefreshedByAuraPass) {
    makeEngine();
    auto g = gearInBase(P1, kPillaged);
    auto other = gearInBase(P1, kBladeOfExile);
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().getObject(g).has_deploy);
    EXPECT_FALSE(s().getObject(other).has_deploy);
}

// ── 1053 Pillaged Armory ──
TEST_F(Radiance4Test, PillagedArmory_ConquerHereTriggersAndBuffs) {
    auto g = addGear(P1, kPillaged, 0);
    auto u = addUnit(P1, kInvalidId, 2, 1);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ScoreEvent{P1, 1, ScoreMethod::Conquer, 1});
    EXPECT_FALSE(chainHasSource(g)) << "another battlefield";
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Conquer, 2});
    EXPECT_TRUE(chainHasSource(g));
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kPillaged, P1, g, pickObject(u), exec, TriggerType::WhenYouConquerHere);
    EXPECT_EQ(state.getObject(u).buff_count, 1);
}

// ── 1054 Raving Witness ──
TEST_F(Radiance4Test, RavingWitness_DrawsMightyRecyclesOther) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto rw = addUnit(P1, kRaving, 0, 0);
    auto bottom = addToDeck(P1, kInvalidId);
    auto mighty = addToDeck(P1, kWrath);   // 7 Might
    fireTriggerAs(kRaving, P1, rw, TriggerType::WhenIMove, exec);
    EXPECT_TRUE(inHand(P1, mighty));
    auto spell = addToDeck(P1, kAvalanche);
    fireTriggerAs(kRaving, P1, rw, TriggerType::WhenIMove, exec);
    EXPECT_FALSE(inHand(P1, spell));
    ASSERT_EQ(deckSize(P1), 2);
    EXPECT_EQ(state.player(P1).main_deck.front(), spell) << "recycled to the bottom";
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom);
}

// ── 1055 Skystrike Squadron ──
TEST_F(Radiance4EngineTest, Skystrike_ReturnGearMakesItTwoCheaper) {
    makeEngine();
    auto gear = gearInBase(P1, kBladeOfExile);
    a1_.want = gear;
    s().player(P1).rune_pool.energy = 3;   // only affordable with the discount
    auto sky = cardInHand(P1, kSkystrike);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, sky));
    play(sky);
    EXPECT_TRUE(s().getObject(sky).location.has_value());
    EXPECT_EQ(s().getObject(gear).zone, ZoneType::Hand);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
}
TEST_F(Radiance4EngineTest, Skystrike_NoGearNoDiscount) {
    makeEngine();
    s().player(P1).rune_pool.energy = 3;
    auto sky = cardInHand(P1, kSkystrike);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, sky));
}

// ── 1056 Sneaky Zeebles ──
TEST_F(Radiance4Test, SneakyZeebles_ConquerAddsAnyPower) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto z = addUnit(P1, kZeebles, 0, 0);
    EXPECT_TRUE(card_registry.get(kZeebles)->firesOn(TriggerType::WhenIConquer));
    fireTriggerAs(kZeebles, P1, z, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

// ── 1057 Wrath of the Freljord ──
TEST_F(Radiance4Test, WrathOfTheFreljord_EntersReadyShieldThree) {
    EXPECT_TRUE(card_registry.get(kWrath)->entersReadyOnPlay());
    EXPECT_EQ(card_db.get(kWrath).shield_value, 3);
    EXPECT_TRUE(card_db.get(kWrath).keywords.has(Keyword::Shield));
}

// ── 1058 Armored Tusker ──
TEST_F(Radiance4Test, ArmoredTusker_MoveDealsShieldDamage) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto tusk = addUnit(P1, kTusker, 0, 0);
    state.getObject(tusk).shield_value = 3;
    auto enemy = addUnit(P2, kInvalidId, 5, 1);
    auto in_base = addUnit(P2, kInvalidId, 1, -1);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kTusker, P1, tusk,
        script({pickValue(1), [&](const std::vector<Intent>& l) {
            for (auto& i : l) if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
            return pickObject(enemy)(l);
        }}), exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 3);
    EXPECT_EQ(std::count(offered.begin(), offered.end(), in_base), 0) << "units at a battlefield only";
}
TEST_F(Radiance4Test, ArmoredTusker_Declined) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto tusk = addUnit(P1, kTusker, 0, 0);
    state.getObject(tusk).shield_value = 3;
    auto enemy = addUnit(P2, kInvalidId, 5, 1);
    driveResumableTrigger(kTusker, P1, tusk, pickValue(0), exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0);
}

// ── 1059 Avalanche ──
TEST_F(Radiance4Test, Avalanche_HitsOnlyUnitsInBases) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = addUnit(P1, kInvalidId, 5, -1);
    auto theirs = addUnit(P2, kInvalidId, 2, -1);
    auto at_bf = addUnit(P2, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kAvalanche);
    invokeOnResolve(spell, kAvalanche, P1, {}, exec);
    EXPECT_EQ(state.getObject(mine).damage_marked, 3);
    EXPECT_FALSE(state.getObject(theirs).location.has_value()) << "killed";
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 0);
}

// ── 1060 Bloodscent Swarm ──
TEST_F(Radiance4Test, BloodscentSwarm_TwoLessPerBattlefieldConqueredThisTurn) {
    Card* c = card_registry.get(kBloodscent);
    state.turn.turn_number = 6;
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
    state.battlefields[0].conquered_on_turn = 6;
    state.battlefields[0].conquered_by_player = P1;
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    state.battlefields[1].conquered_on_turn = 6;
    state.battlefields[1].conquered_by_player = P1;
    EXPECT_EQ(c->selfCostReduction(state, P1), 4);
    state.battlefields[1].conquered_on_turn = 5;   // last turn
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    EXPECT_EQ(c->selfCostReduction(state, P2), 0);
}

// ── 1061 Elnuk Herd ──
TEST_F(Radiance4Test, ElnukHerd_FromFaceDownBuffsThenRecyclesItself) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto elnuk = addUnit(P1, kElnuk, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(elnuk).card_counters["__played_from_facedown"] = 1;
    driveResumableTrigger(kElnuk, P1, elnuk, pickObject(ally), exec);
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 3);
    EXPECT_TRUE(inDeck(P1, elnuk)) << "recycled";
}
TEST_F(Radiance4Test, ElnukHerd_FromHandStays) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto elnuk = addUnit(P1, kElnuk, 0, 0);
    driveResumableTrigger(kElnuk, P1, elnuk, pickObject(elnuk), exec);
    EXPECT_EQ(state.getObject(elnuk).temp_might_bonus, 3) << "errata: a friendly unit (may be me)";
    EXPECT_TRUE(state.getObject(elnuk).location.has_value());
}

// ── 1062 Primordial Roar ──
TEST_F(Radiance4Test, PrimordialRoar_DamageEqualsShownOffMight) {
    Card* c = card_registry.get(kRoar);
    EXPECT_TRUE(c->hasShowOff());
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto big = addUnit(P1, kInvalidId, 5, 0);
    auto enemy = addUnit(P2, kInvalidId, 7, 0);
    EXPECT_TRUE(c->showOffMatches(state, P1, state.getObject(big)));
    auto spell = addToHand(P1, kRoar);
    driveResolve(kRoar, P1, spell, pickObject(enemy), exec);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0) << "didn't show off";
    state.getObject(spell).showed_off_object = big;
    driveResolve(kRoar, P1, spell, pickObject(enemy), exec);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 5);
}
TEST_F(Radiance4Test, PrimordialRoar_ShownFromHandUsesPrintedMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto wrath = addToHand(P1, kWrath);   // 7 Might
    auto spell = addToHand(P1, kRoar);
    state.getObject(spell).showed_off_object = wrath;
    driveResolve(kRoar, P1, spell, pickObject(enemy), exec);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 7);
}

// ── 1063 Stalwart Goatherd ──
TEST_F(Radiance4Test, StalwartGoatherd_KillsCheapGearOnly) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto goat = addUnit(P1, kGoatherd, 0, 0);
    auto cheap = addGear(P2, kNazuman);      // 1 Energy
    auto pricey = addGear(P2, kZeroPoint);   // 9 Energy
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kGoatherd, P1, goat,
        script({pickValue(1), [&](const std::vector<Intent>& l) {
            for (auto& i : l) if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
            return pickObject(cheap)(l);
        }}), exec);
    EXPECT_FALSE(state.getObject(cheap).location.has_value());
    EXPECT_TRUE(state.getObject(pricey).location.has_value());
    EXPECT_EQ(std::count(offered.begin(), offered.end(), pricey), 0);
}

}  // namespace
}  // namespace riftbound::test
