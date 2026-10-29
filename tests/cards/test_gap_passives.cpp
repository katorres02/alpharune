/// @file test_gap_passives.cpp
/// Engine gaps closed for continuous / passive card effects (2026-10-28 audit,
/// round 2): one "enters ready" decision point for every way a unit enters the
/// board (Magma Wurm, Wuju Master), Zilean's token copy as a choice, groups of
/// units exhausted together by effects and costs (Seraphine, Inspiring),
/// optional cost reductions (Orianna, Mechanical Marvel), every [E] ability
/// proxied by Heimerdinger, target discounts for targets chosen at
/// finalization (Irelia, Graceful), Void Hatchling's look before every reveal
/// from a deck, and battlefield play triggers that know the played unit
/// (Valley of Idols, Star Spring).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

namespace riftbound::test {
namespace {

constexpr CardDefId kMagmaWurm = 11, kSunDisc = 21, kHeimer = 111, kUncheckedPower = 123,
    kVoidHatchling = 341, kApprenticeSmith = 364, kIrelia = 462, kHoneyfruit = 611,
    kZilean = 648, kConscription = 702, kStarSpring = 771, kValley = 774, kWujuMaster = 784,
    kStarryEyed = 979, kSeraphine = 1045, kOrianna = 1048;

class LambdaAgent : public AgentInterface {
public:
    std::function<std::optional<Intent>(const std::vector<Intent>&)> pick;
    std::vector<std::vector<Intent>> seen;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        seen.push_back(legal);
        if (pick) if (auto i = pick(legal)) return *i;
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

std::optional<Intent> chooseValue(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.chosen_value.has_value() && *i.chosen_value == v)
            return i;
    return std::nullopt;
}

class GapPassivesTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    LambdaAgent a1_, a2_;
    GameState& s() { return engine_ ? engine_->mutableState() : state; }
    EffectExecutor& ex() { return engine_->testHook_executor(); }

    void makeEngine() {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = engine_->mutableState();
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
        st.turn.turn_number = 3;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        setLegend(P1);
        setLegend(P2);
    }
    GameObjectId setLegend(PlayerId p, CardDefId def_id = kInvalidId) {
        auto id = s().createObject();
        auto& lo = s().getObject(id);
        lo.owner = lo.controller = p;
        lo.card_type = CardType::Legend;
        lo.card_def_id = def_id;
        if (def_id != kInvalidId) lo.name = card_db.get(def_id).name;
        lo.zone = ZoneType::LegendZone;
        s().player(p).legend_zone = id;
        return id;
    }
    GameObjectId obj(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = s().createObject();
        auto& o = s().getObject(id);
        o.owner = o.controller = p;
        o.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            o.card_type = d.card_type; o.super_type = d.super_type; o.name = d.name;
            o.keywords = d.keywords; o.tags = d.tags; o.domains = d.domains;
            o.base_might = o.current_might = d.might;
        } else {
            o.card_type = CardType::Unit; o.name = "TestUnit";
            o.base_might = o.current_might = 2;
        }
        if (at_bf >= 0) {
            o.zone = ZoneType::BattlefieldZone;
            o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            o.zone = ZoneType::Base;
            o.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId handCard(PlayerId p, CardDefId def_id) {
        auto id = obj(p, def_id);
        auto& c = s().getObject(id);
        c.zone = ZoneType::Hand;
        c.location = std::nullopt;
        s().player(p).hand.push_back(id);
        return id;
    }
    CardDefId vanillaUnit() {
        CardDefId best = kInvalidId;
        for (const auto& [id, d] : card_db.all())
            if (d.card_type == CardType::Unit && d.ability_text.empty() && d.energy_cost <= 2 &&
                d.power_cost == 0 && d.super_type == SuperType::None && d.keywords.bits == 0 &&
                (best == kInvalidId || id < best))
                best = id;
        return best;
    }
    GameObjectId token(PlayerId p) {
        return ex().createToken(p, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                BaseLocation{p});
    }
    GameObjectId installBattlefieldCard(BattlefieldId bf, CardDefId def_id) {
        auto src = state.createObject();
        auto& o = state.getObject(src);
        o.card_def_id = def_id;
        o.card_type = CardType::Battlefield;
        o.name = card_db.get(def_id).name;
        state.battlefields[bf].card_object_id = src;
        return src;
    }
    int chainItemsFrom(GameObjectId source) {
        int n = 0;
        for (const auto& it : s().chain.items) if (it.source == source) ++n;
        return n;
    }
};

// ═══ Enters ready: one decision point for every entry ═══════════════════════

TEST_F(GapPassivesTest, MagmaWurm_TokensAndEffectPlaysEnterReady) {
    makeEngine();
    auto before = token(P1);
    EXPECT_TRUE(s().getObject(before).is_exhausted) << "CR 143.4: units enter exhausted";

    obj(P1, kMagmaWurm);
    auto tok = token(P1);
    EXPECT_FALSE(s().getObject(tok).is_exhausted) << "a token is a friendly unit entering";
    auto enemy_tok = token(P2);
    EXPECT_TRUE(s().getObject(enemy_tok).is_exhausted) << "only friendly units";

    const CardDefId vanilla = vanillaUnit();
    ASSERT_NE(vanilla, kInvalidId);
    auto card = handCard(P1, vanilla);
    ex().playIgnoringCost(P1, card);
    EXPECT_FALSE(s().getObject(card).is_exhausted) << "a unit played by an effect enters ready";
}

TEST_F(GapPassivesTest, EffectPlayWithoutGranterEntersExhausted) {
    makeEngine();
    auto card = handCard(P1, vanillaUnit());
    ex().playIgnoringCost(P1, card);
    EXPECT_TRUE(s().getObject(card).is_exhausted);
}

TEST_F(GapPassivesTest, WujuMaster_Level11_TokensEnterReady) {
    makeEngine();
    auto legend = setLegend(P1, kWujuMaster);
    (void)legend;
    s().player(P1).xp = 11;
    card_registry.get(kWujuMaster)->applyPassiveAura(s(), P1);
    ASSERT_TRUE(s().player(P1).units_enter_ready);
    EXPECT_FALSE(s().getObject(token(P1)).is_exhausted);
    auto card = handCard(P1, vanillaUnit());
    ex().playIgnoringCost(P1, card);
    EXPECT_FALSE(s().getObject(card).is_exhausted);
    EXPECT_TRUE(s().getObject(token(P2)).is_exhausted) << "\"your units\" only";
}

TEST_F(GapPassivesTest, WujuMaster_BelowLevel11_NoReady) {
    makeEngine();
    setLegend(P1, kWujuMaster);
    s().player(P1).xp = 10;
    card_registry.get(kWujuMaster)->applyPassiveAura(s(), P1);
    EXPECT_TRUE(s().getObject(token(P1)).is_exhausted);
}

TEST_F(GapPassivesTest, SunDisc_ArmedOneShotIsNotSpentOnATokenItCantSee) {
    makeEngine();
    auto disc = obj(P1, kSunDisc);
    s().getObject(disc).card_counters["__sundisc_arm_turn"] = s().turn.turn_number;
    EXPECT_TRUE(s().getObject(token(P1)).is_exhausted)
        << "the next unit you PLAY: a token doesn't disarm Sun Disc, so it isn't readied by it";
    auto card = handCard(P1, vanillaUnit());
    ex().playIgnoringCost(P1, card);
    EXPECT_FALSE(s().getObject(card).is_exhausted) << "an effect play is a play";
}

// ═══ Zilean, Time Mage: the extra copy is the player's choice ═══════════════

int recruitsOf(GameState& st, PlayerId p) {
    int n = 0;
    for (const auto& [id, o] : st.objects)
        if (o.isUnit() && o.name == "Recruit" && o.controller == p && o.location) ++n;
    return n;
}

TEST_F(GapPassivesTest, Zilean_DecliningKeepsTheOncePerTurnUse) {
    makeEngine();
    s().player(P1).zilean_present = true;
    int asked = 0;
    a1_.pick = [&](const std::vector<Intent>& l) { ++asked; return chooseValue(l, 0); };
    token(P1);
    EXPECT_EQ(asked, 1) << "the player is asked";
    EXPECT_EQ(recruitsOf(s(), P1), 1);
    a1_.pick = [&](const std::vector<Intent>& l) { ++asked; return chooseValue(l, 1); };
    token(P1);
    EXPECT_EQ(asked, 2);
    EXPECT_EQ(recruitsOf(s(), P1), 3) << "yes: that token and an additional copy";
    token(P1);
    EXPECT_EQ(asked, 2) << "once each turn";
    EXPECT_EQ(recruitsOf(s(), P1), 4);
}

// ═══ Seraphine, Inspiring: units exhausted together by effects / costs ══════

TEST_F(GapPassivesTest, Seraphine_EffectExhaustingThreeUnitsTriggers) {
    makeEngine();
    auto sera = obj(P1, kSeraphine);
    std::vector<GameObjectId> units = {obj(P1, kInvalidId), obj(P1, kInvalidId), obj(P1, kInvalidId)};
    ex().exhaustTogether(P1, {units[0], units[1]});
    EXPECT_EQ(chainItemsFrom(sera), 0) << "two units";
    s().getObject(units[0]).is_exhausted = false;
    s().getObject(units[1]).is_exhausted = false;
    ex().exhaustTogether(P1, units);
    EXPECT_EQ(s().player(P1).last_exhaust_group_size, 3);
    EXPECT_EQ(chainItemsFrom(sera), 1);
}

TEST_F(GapPassivesTest, Seraphine_OnlyNewlyExhaustedFriendlyUnitsCount) {
    makeEngine();
    auto sera = obj(P1, kSeraphine);
    auto a = obj(P1, kInvalidId), b = obj(P1, kInvalidId), c = obj(P1, kInvalidId);
    s().getObject(c).is_exhausted = true;
    auto foe = obj(P2, kInvalidId);
    ex().exhaustTogether(P1, {a, b, c, foe});
    EXPECT_EQ(chainItemsFrom(sera), 0);
}

TEST_F(GapPassivesTest, UncheckedPower_ExhaustsFriendlyUnitsTogether) {
    makeEngine();
    auto sera = obj(P1, kSeraphine);
    obj(P1, kInvalidId); obj(P1, kInvalidId);
    auto spell = obj(P1, kUncheckedPower);
    CardContext ctx{s(), events, ex(), P1, spell};
    card_registry.get(kUncheckedPower)->onResolve(ctx, {});
    EXPECT_EQ(s().player(P1).last_exhaust_group_size, 3) << "Seraphine + two units";
    EXPECT_EQ(chainItemsFrom(sera), 1);
}

// Starry-Eyed Songstress's grouped exhaust is covered in test_gap_costs.cpp
// (SongstressTest.ForcedWhenOnlyTheDiscountMakesItPayable).

// ═══ Orianna, Mechanical Marvel: "you may pay [1][A] less" ══════════════════

class OriannaTest : public GapPassivesTest {
protected:
    GameObjectId ori_ = kInvalidId, disc_ = kInvalidId;
    void SetUp() override {
        GapPassivesTest::SetUp();
        makeEngine();
        ori_ = obj(P1, kOrianna, 0);
        card_registry.get(kOrianna)->applyPassiveAura(s(), P1);
        disc_ = handCard(P1, kSunDisc);   // [2][R]
        s().player(P1).rune_pool.energy = 2;
        s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    }
    void play() {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = P1;
        i.card = disc_;
        engine_->testHook_executeIntent(i);
    }
};

TEST_F(OriannaTest, TakingTheDiscountPaysLessAndUsesIt) {
    a1_.pick = [](const std::vector<Intent>& l) { return chooseValue(l, 1); };
    play();
    EXPECT_EQ(s().player(P1).rune_pool.energy, 1) << "[2] - [1]";
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 1) << "[R] - [A]";
    EXPECT_EQ(s().getObject(ori_).card_counters["__ori_used_turn"], s().turn.turn_number);
}

TEST_F(OriannaTest, DecliningPaysFullAndSavesIt) {
    a1_.pick = [](const std::vector<Intent>& l) { return chooseValue(l, 0); };
    play();
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);
    EXPECT_EQ(s().getObject(ori_).card_counters.count("__ori_used_turn"), 0u);
    for (const auto& m : s().player(P1).cost_modifiers) EXPECT_FALSE(m.declined);
}

TEST_F(OriannaTest, NotOfferedWhenOnlyAffordableWithIt) {
    s().player(P1).rune_pool.energy = 1;
    int asked = 0;
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (chooseValue(l, 0)) ++asked;
        return chooseValue(l, 0);
    };
    play();
    EXPECT_EQ(asked, 0) << "declining would leave the gear unpayable";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_EQ(s().getObject(ori_).card_counters["__ori_used_turn"], s().turn.turn_number);
}

// ═══ Heimerdinger, Inventor: every [E] ability ══════════════════════════════

TEST_F(GapPassivesTest, Heimerdinger_GetsEveryExhaustAbility) {
    makeEngine();
    s().player(P1).xp = 6;
    auto heimer = obj(P1, kHeimer, 0);
    obj(P1, kHoneyfruit);   // two [E] abilities
    card_registry.get(kHeimer)->applyPassiveAura(s(), P1);
    std::vector<int> indices;
    for (const auto& i : engine_->generateLegalActions())
        if (i.ability_source == heimer && i.granted_ability_def == kHoneyfruit)
            indices.push_back(i.ability_index);
    std::sort(indices.begin(), indices.end());
    EXPECT_EQ(indices, (std::vector<int>{0, 1}));
}

// ═══ Irelia, Graceful: targets chosen at finalization ═══════════════════════

class IreliaTest : public GapPassivesTest {
protected:
    ChainItemId spellItem(GameObjectId chosen, int applied_e, int applied_p = 0) {
        auto spell = obj(P1, kConscription);
        s().getObject(spell).zone = ZoneType::Chain;
        s().getObject(spell).location = std::nullopt;
        ChainItem it;
        it.id = s().chain.allocateId();
        it.source = spell;
        it.card_def_id = kConscription;
        it.controller = P1;
        it.is_spell = true;
        it.chosen_objects = {chosen};
        it.target_disc_energy = applied_e;
        it.target_disc_power = applied_p;
        s().chain.items.push_back(it);
        return it.id;
    }
};

TEST_F(IreliaTest, ChoosingIreliaAtFinalizationReturnsTheReduction) {
    makeEngine();
    auto irelia = obj(P1, kIrelia, 0);
    card_registry.get(kIrelia)->applyPassiveAura(s(), P1);
    auto id = spellItem(irelia, 0);
    engine_->testHook_settleChosenTargetDiscount(id);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 1);
    engine_->testHook_settleChosenTargetDiscount(id);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 1) << "settled once";
}

TEST_F(IreliaTest, NoRefundWhenAlreadyAppliedOrCostNotPaid) {
    makeEngine();
    auto irelia = obj(P1, kIrelia, 0);
    card_registry.get(kIrelia)->applyPassiveAura(s(), P1);
    engine_->testHook_settleChosenTargetDiscount(spellItem(irelia, 1));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "chosen as it was played: already paid less";
    engine_->testHook_settleChosenTargetDiscount(spellItem(irelia, -1));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "its cost was ignored";
    auto other = obj(P1, kInvalidId, 0);
    engine_->testHook_settleChosenTargetDiscount(spellItem(other, 0));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "didn't choose Irelia";
}

// ═══ Void Hatchling: look before every reveal from a deck ═══════════════════

class VoidHatchlingTest : public CardTestFixture {
protected:
    GameObjectId deck(PlayerId p, CardDefId def_id) {
        auto id = state.createObject();
        auto& c = state.getObject(id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = def_id != kInvalidId ? card_db.get(def_id).card_type : CardType::Unit;
        c.name = def_id != kInvalidId ? card_db.get(def_id).name : "DeckUnit";
        c.zone = ZoneType::MainDeck;
        state.player(p).main_deck.push_back(id);   // top = back
        return id;
    }
};

TEST_F(VoidHatchlingTest, ApprenticeSmith_RecyclingTheTopFirstChangesTheReveal) {
    for (int answer : {0, 1}) {
        SCOPED_TRACE(answer);
        state.player(P1).main_deck.clear();
        state.player(P1).hand.clear();
        EffectExecutor exec(state, events, card_db, &card_registry);
        int asked = 0;
        exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) {
            ++asked;
            return *chooseValue(l, answer);
        });
        state.player(P1).has_reveal_peek = true;
        auto gear = deck(P1, kSunDisc);      // second from the top
        auto top = deck(P1, kInvalidId);     // a unit on top
        auto smith = addUnit(P1, kApprenticeSmith, 0, 0);
        fireTriggerAs(kApprenticeSmith, P1, smith, TriggerType::WhenIMove, exec);
        EXPECT_EQ(asked, 1);
        const auto& hand = state.player(P1).hand;
        if (answer == 1) {
            EXPECT_NE(std::find(hand.begin(), hand.end(), gear), hand.end())
                << "the unit was recycled first, so the gear was revealed and drawn";
        } else {
            EXPECT_TRUE(hand.empty()) << "the unit was revealed (not a gear) and recycled";
            EXPECT_EQ(state.player(P1).main_deck.front(), top);
        }
    }
}

TEST_F(VoidHatchlingTest, NoLookWithoutHatchling) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    int asked = 0;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { ++asked; return l.front(); });
    deck(P1, kInvalidId);
    auto smith = addUnit(P1, kApprenticeSmith, 0, 0);
    fireTriggerAs(kApprenticeSmith, P1, smith, TriggerType::WhenIMove, exec);
    EXPECT_EQ(asked, 0);
}

TEST_F(VoidHatchlingTest, AuraSetsThePeek) {
    addUnit(P1, kVoidHatchling, 0, 0);
    card_registry.get(kVoidHatchling)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).has_reveal_peek);
    EXPECT_FALSE(state.player(P2).has_reveal_peek);
}

// ═══ Valley of Idols / Star Spring: the played unit is the trigger subject ══

TEST_F(GapPassivesTest, PlayHereTriggersCarryThePlayedUnit) {
    makeEngine();
    auto bf_src = s().createObject();
    s().getObject(bf_src).card_def_id = kValley;
    s().getObject(bf_src).card_type = CardType::Battlefield;
    s().battlefields[0].card_object_id = bf_src;
    auto played = obj(P1, kInvalidId, 0);
    obj(P1, kInvalidId, 0);   // a newer unit already here
    events.emit(CardPlayedEvent{played, P1, CardType::Unit, 1});
    bool found = false;
    for (const auto& it : s().chain.items)
        if (it.source == bf_src) { found = true; EXPECT_EQ(it.triggering_subject, played); }
    EXPECT_TRUE(found);
}

TEST_F(GapPassivesTest, ValleyOfIdols_BuffsThePlayedUnitNotTheNewest) {
    auto src = installBattlefieldCard(0, kValley);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto played = addUnit(P1, kInvalidId, 2, 0);
    auto newer = addUnit(P1, kInvalidId, 2, 0);
    ASSERT_GT(newer, played);
    state.player(P1).rune_pool.energy = 1;
    driveResumableTrigger(kValley, P1, src,
        [](const std::vector<Intent>& l) { return *chooseValue(l, 1); },
        exec, TriggerType::WhenYouPlayAUnit, /*subject=*/played);
    EXPECT_EQ(state.getObject(played).buff_count, 1);
    EXPECT_EQ(state.getObject(newer).buff_count, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(GapPassivesTest, StarSpring_PlayerChoosesWhichUnitToMove) {
    auto src = installBattlefieldCard(0, kStarSpring);
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.turn.turn_number = 1;
    auto played = addUnit(P1, kInvalidId, 2, 0);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kStarSpring, P1, src,
        [&](const std::vector<Intent>& l) {
            for (const auto& i : l) if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
            for (const auto& i : l) if (!i.chosen_objects.empty() && i.chosen_objects[0] == b) return i;
            return *chooseValue(l, 1);
        },
        exec, TriggerType::WhenYouPlayAUnit, played);
    EXPECT_EQ(std::find(offered.begin(), offered.end(), played), offered.end()) << "another unit";
    EXPECT_NE(std::find(offered.begin(), offered.end(), a), offered.end());
    EXPECT_TRUE(state.getObject(b).isAtBase());
    EXPECT_TRUE(state.getObject(a).isAtBattlefield());
    EXPECT_TRUE(state.getObject(played).isAtBattlefield());
}

}  // namespace
}  // namespace riftbound::test
