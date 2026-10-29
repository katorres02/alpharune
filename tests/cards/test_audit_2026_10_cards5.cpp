/// @file test_audit_2026_10_cards5.cpp
/// 2026-10-28 card audit, batch "cards5": behavioral tests for the fixed cards
/// (Unleashed 706-786, Vendetta 879-935) and for every implemented Radiance
/// card (956-968).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>

namespace riftbound::test {
namespace {

constexpr CardDefId kMaduli = 706, kBaron = 709, kVexApathetic = 712, kCarrionDredger = 715,
    kUltrasoftPoro = 722, kMageseekerInvestigator = 725, kSafetyInspector = 726,
    kShadowsCall = 727, kUndyingLoyalty = 730, kAsheFocused = 731, kAtakhan = 732,
    kLeBlancFragmented = 734, kShardOfUndoing = 736, kViPeacekeeper = 738, kPoppy = 740,
    kRiftHerald = 741, kCurtainCall = 743, kThrill = 745, kGreenFather = 753, kDaisy = 754,
    kKeeperOfTheHammer = 759, kKeepersVerdict = 760, kAmateurRecital = 763, kWujuMaster = 784,
    kScornOfTheMoon = 786, kRenektonBrute = 879, kSpiderling = 884, kTwilightStep = 892,
    kShadowDash = 935, kTheExile = 956, kRivenBrokenBlade = 957, kBladeOfTheExile = 958,
    kBedevilingStranger = 959, kSirenOfTheShallows = 960, kRecklessInnovator = 961, kHush = 963,
    kHunkerDown = 962, kKiBurst = 964, kOfferYouCantRefuse = 965, kHuntersCircle = 966,
    kBattlePreparations = 967, kRakelstake = 968;

/// Answers every decision with `pick`, or: pass priority when offered, else
/// the first option.
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

std::optional<Intent> chooseObject(const std::vector<Intent>& legal, GameObjectId want) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
            i.chosen_objects.front() == want) return i;
    return std::nullopt;
}
std::optional<Intent> chooseValue(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.chosen_value.has_value() && *i.chosen_value == v)
            return i;
    return std::nullopt;
}
bool offersObject(const std::vector<Intent>& legal, GameObjectId id) {
    return chooseObject(legal, id).has_value();
}

class Cards5Test : public CardTestFixture {
protected:
    // ── Engine-backed scenarios ──
    std::unique_ptr<GameEngine> engine_;
    LambdaAgent a1_, a2_;
    GameState& s() { return engine_ ? engine_->mutableState() : state; }

    void makeEngine(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = engine_->mutableState();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = static_cast<int>(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
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
    GameObjectId unit(PlayerId p, CardDefId def_id, int might = 0, int at_bf = -1) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            u.name = d.name; u.tags = d.tags; u.keywords = d.keywords; u.domains = d.domains;
            u.super_type = d.super_type;
            u.base_might = might > 0 ? might : d.might;
        } else {
            u.name = "TestUnit";
            u.base_might = might > 0 ? might : 1;
        }
        u.current_might = u.base_might;
        if (at_bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId handCard(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& d = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = d.card_type;
        c.super_type = d.super_type;
        c.name = d.name; c.domains = d.domains; c.keywords = d.keywords; c.tags = d.tags;
        c.zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId inTrashOf(PlayerId p, CardDefId def_id) {
        auto id = handCard(p, def_id);
        auto& h = s().player(p).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        s().getObject(id).zone = ZoneType::Trash;
        s().player(p).trash.push_back(id);
        return id;
    }
    GameObjectId deckCard(PlayerId p, CardDefId def_id = kInvalidId, bool is_unit = false) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = is_unit ? CardType::Unit : CardType::Spell;
        if (def_id != kInvalidId) {
            c.card_type = card_db.get(def_id).card_type;
            c.name = card_db.get(def_id).name;
        } else {
            c.name = is_unit ? "DeckUnit" : "DeckSpell";
        }
        c.zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);   // top = back
        return id;
    }
    GameObjectId bfCard(BattlefieldId bf, CardDefId def_id) {
        auto src = s().createObject();
        auto& o = s().getObject(src);
        o.card_def_id = def_id;
        o.card_type = CardType::Battlefield;
        o.name = def_id != kInvalidId ? card_db.get(def_id).name : "Plain Battlefield";
        s().battlefields[bf].card_object_id = src;
        return src;
    }
    GameObjectId gear(PlayerId p, CardDefId def_id = kInvalidId) {
        auto id = s().createObject();
        auto& g = s().getObject(id);
        g.owner = g.controller = p;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        g.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestGear";
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        return id;
    }
    GameObjectId rune(PlayerId p, Domain d, bool exhausted = false) {
        auto id = s().createObject();
        auto& r = s().getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        r.is_exhausted = exhausted;
        return id;
    }
    /// Smallest card id matching `pred` (deterministic), or kInvalidId.
    CardDefId findCard(const std::function<bool(const CardDef&)>& pred) {
        CardDefId best = kInvalidId;
        for (const auto& [id, d] : card_db.all())
            if (pred(d) && (best == kInvalidId || id < best)) best = id;
        return best;
    }
    CardDefId vanillaUnit(int max_energy = 2) {
        return findCard([&](const CardDef& d) {
            return d.card_type == CardType::Unit && d.ability_text.empty() &&
                   d.energy_cost <= max_energy && d.power_cost == 0 &&
                   d.super_type == SuperType::None && d.keywords.bits == 0;
        });
    }
    Intent playIntent(PlayerId p, GameObjectId card, std::optional<LocationId> where = std::nullopt) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = where;
        return i;
    }
    int chainItemsOf(CardDefId def_id) {
        int n = 0;
        for (const auto& it : s().chain.items) if (it.card_def_id == def_id) ++n;
        return n;
    }

    /// Resolve a spell whose target was chosen as it was played (as the
    /// engine's executePlaySpell does): the chain item carries the target and
    /// the "chosen at finalization" flag Card::pickTarget reads.
    void resolveWithPlayTarget(CardDefId def_id, PlayerId controller, GameObjectId source,
                               GameObjectId target, EffectExecutor& exec,
                               std::function<Intent(const std::vector<Intent>&)> picker,
                               std::vector<std::vector<Intent>>* prompts = nullptr) {
        ChainItem ri;
        ri.id = s().chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = {target};
        ri.resume_data = {0, 0, 0, 0, 0, 1};
        s().chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{s(), events, exec, controller, source};
        card->onResolve(ctx, ri.targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            if (prompts) prompts->push_back(pending.legal);
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, ri.targets);
        }
        s().chain.resuming.reset();
    }
};

// ═══ [706] Maduli the Gatekeeper — "I can't be readied." ═══════════════════
TEST_F(Cards5Test, Maduli_CantBeReadied) {
    makeEngine();
    auto m = unit(P1, kMaduli, 0, 0);
    s().getObject(m).is_exhausted = true;
    auto other = unit(P1, kInvalidId, 2, 0);
    s().getObject(other).is_exhausted = true;
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().getObject(m).cant_be_readied);
    engine_->testHook_executor().readyObject(m);
    engine_->testHook_executor().readyObject(other);
    EXPECT_TRUE(s().getObject(m).is_exhausted) << "effects can't ready Maduli";
    EXPECT_FALSE(s().getObject(other).is_exhausted);
}

// ═══ [709] Baron Nashor — "Other friendly units have +2 [M]." ══════════════
TEST_F(Cards5Test, Baron_OtherFriendlyUnitsGetPlusTwo) {
    makeEngine();
    auto baron = unit(P1, kBaron, 0, 0);
    auto ally = unit(P1, kInvalidId, 2, 1);
    auto foe = unit(P2, kInvalidId, 2, 0);
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(ally).current_might, 4);
    EXPECT_EQ(s().getObject(foe).current_might, 2) << "enemies are not affected";
    EXPECT_EQ(s().getObject(baron).current_might, 12) << "Baron doesn't buff himself";
}

// ═══ [712] Vex, Apathetic — "They can't move it this turn." ════════════════
TEST_F(Cards5Test, VexApathetic_StunnedUnitCantMoveThisTurn) {
    makeEngine();
    auto vex = unit(P1, kVexApathetic, 0, 0);
    auto played = unit(P2, kInvalidId, 3);           // just played to P2's base
    auto free_unit = unit(P2, kInvalidId, 3);
    s().getObject(vex).card_counters["__opp_played_unit_id"] = static_cast<int>(played);
    auto& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, vex};
    card_registry.get(kVexApathetic)->onTrigger(ctx, {});
    EXPECT_TRUE(s().getObject(played).is_stunned);
    EXPECT_TRUE(s().getObject(played).cant_move_this_turn);

    s().turn.turn_player = P2;
    auto actions = engine_->generateLegalActions();
    bool moves_played = false, moves_free = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::StandardMove) continue;
        for (auto u : a.units_to_move) {
            if (u == played) moves_played = true;
            if (u == free_unit) moves_free = true;
        }
    }
    EXPECT_FALSE(moves_played) << "its controller can't move it this turn";
    EXPECT_TRUE(moves_free);
}

// ═══ [715] Carrion Dredger — the Bird token has [Deflect], not the Dredger ═
TEST_F(Cards5Test, CarrionDredger_OnlyTheBirdHasDeflect) {
    const auto& d = card_db.get(kCarrionDredger);
    EXPECT_FALSE(d.keywords.has(Keyword::Deflect));
    EXPECT_EQ(d.deflect_value, 0);
    auto dredger = addUnit(P1, kCarrionDredger);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kCarrionDredger, P1, dredger, TriggerType::WhenIDie, exec);
    int birds = 0;
    for (auto& [id, o] : state.objects)
        if (o.isToken() && o.name == "Bird") {
            ++birds;
            EXPECT_TRUE(o.hasKeyword(Keyword::Deflect));
        }
    EXPECT_EQ(birds, 1);
}

// ═══ [722] Ultrasoft Poro — no [Deflect]; only the copy at a battlefield ═══
TEST_F(Cards5Test, UltrasoftPoro_PerObjectBattlefieldGate) {
    EXPECT_FALSE(card_db.get(kUltrasoftPoro).keywords.has(Keyword::Deflect));
    auto at_base = addUnit(P1, kUltrasoftPoro);
    auto at_bf = addUnit(P1, kUltrasoftPoro, 0, 0);
    const Card* c = card_registry.get(kUltrasoftPoro);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, at_base, 0))
        << "a second copy at base can't use it";
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, at_bf, 0));
}

// ═══ [725] Mageseeker Investigator — surcharge flag on its battlefield ═════
TEST_F(Cards5Test, MageseekerInvestigator_MarksItsBattlefield) {
    makeEngine();
    unit(P1, kMageseekerInvestigator, 0, 1);
    engine_->testHook_cleanup();
    EXPECT_FALSE(s().battlefields[0].surcharge_enemy_multi_move);
    EXPECT_TRUE(s().battlefields[1].surcharge_enemy_multi_move);
}

// ═══ [726] Safety Inspector — each player picks their own unit ═════════════
TEST_F(Cards5Test, SafetyInspector_EachPlayerChoosesTheirOwnUnit) {
    auto me = addUnit(P1, kSafetyInspector);
    auto my_a = addUnit(P1, kInvalidId, 1);
    auto my_b = addUnit(P1, kInvalidId, 5);
    auto their_a = addUnit(P2, kInvalidId, 1, 0);
    auto their_b = addUnit(P2, kInvalidId, 6, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<PlayerId> asked;
    driveResumableTrigger(kSafetyInspector, P1, me, [&](const std::vector<Intent>& legal) {
        asked.push_back(legal.front().player);
        return *chooseObject(legal, legal.front().player == P1 ? my_b : their_b);
    }, exec, TriggerType::WhenYouPlayMe);
    ASSERT_EQ(asked.size(), 2u);
    EXPECT_EQ(asked[0], P1);
    EXPECT_EQ(asked[1], P2) << "the opponent chooses their own unit";
    EXPECT_FALSE(state.getObject(my_b).location.has_value());
    EXPECT_FALSE(state.getObject(their_b).location.has_value()) << "not the lowest-Might one";
    EXPECT_TRUE(state.getObject(my_a).location.has_value());
    EXPECT_TRUE(state.getObject(their_a).location.has_value());
}

TEST_F(Cards5Test, SafetyInspector_PaidXpSparesTheController) {
    auto me = addUnit(P1, kSafetyInspector);
    state.getObject(me).card_counters["__safety_inspector_xp_paid"] = 1;
    auto mine = addUnit(P1, kInvalidId, 1);
    auto theirs = addUnit(P2, kInvalidId, 1, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<PlayerId> asked;
    driveResumableTrigger(kSafetyInspector, P1, me, [&](const std::vector<Intent>& legal) {
        asked.push_back(legal.front().player);
        return legal.front();
    }, exec, TriggerType::WhenYouPlayMe);
    ASSERT_EQ(asked.size(), 1u);
    EXPECT_EQ(asked[0], P2);
    EXPECT_TRUE(state.getObject(mine).location.has_value());
    EXPECT_TRUE(state.getObject(me).location.has_value());
    EXPECT_FALSE(state.getObject(theirs).location.has_value());
}

TEST_F(Cards5Test, SafetyInspector_XpIsAnAdditionalCostDecidedAtPlay) {
    makeEngine();
    s().player(P1).xp = 3;
    s().player(P1).rune_pool.energy = 5;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    auto card = handCard(P1, kSafetyInspector);
    a1_.pick = [](const std::vector<Intent>& legal) { return chooseValue(legal, 1); };
    engine_->testHook_executeIntent(playIntent(P1, card));
    EXPECT_EQ(s().player(P1).xp, 0) << "3 XP spent as the card was played";
}

// ═══ [727] Shadow's Call — a lasting [Temporary], "without [Temporary]" ═══
TEST_F(Cards5Test, ShadowsCall_TemporaryLastsUntilBeginningPhase) {
    makeEngine();
    auto plain = unit(P1, kInvalidId, 2, 0);
    auto temp = unit(P1, kInvalidId, 2, 0);
    s().getObject(temp).keywords.set(Keyword::Temporary);
    deckCard(P1); deckCard(P1);
    const Card* c = card_registry.get(kShadowsCall);
    auto legal = c->enumerateLegalTargets(s(), P1);
    EXPECT_NE(std::find(legal.begin(), legal.end(), plain), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), temp), legal.end())
        << "units that already have [Temporary] can't be chosen";

    auto spell = s().createObject();
    auto& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, spell};
    card_registry.get(kShadowsCall)->onResolve(ctx, {});   // no chain: first legal
    EXPECT_TRUE(s().getObject(plain).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(s().getObject(plain).temp_keywords.has(Keyword::Temporary))
        << "not a 'this turn' grant";
    EXPECT_EQ(s().player(P1).hand.size(), 2u) << "Draw 2";
    engine_->testHook_beginningStep();
    EXPECT_FALSE(s().getObject(plain).location.has_value()) << "killed at its Beginning Phase";
}

// ═══ [730] Undying Loyalty — Power gate and choice-bound discount ═════════
TEST_F(Cards5Test, UndyingLoyalty_PowerGateAndChosenTagDiscount) {
    const CardDefId tagged = findCard([](const CardDef& d) {
        if (d.card_type != CardType::Unit || d.energy_cost > 2 || d.power_cost > 1) return false;
        for (auto& t : d.tags) if (t == "Poro" || t == "Bird" || t == "Cat" || t == "Dog") return true;
        return false;
    });
    const CardDefId untagged = findCard([](const CardDef& d) {
        if (d.card_type != CardType::Unit || d.energy_cost > 2 || d.power_cost > 1) return false;
        if (d.super_type == SuperType::Token) return false;
        for (auto& t : d.tags) if (t == "Poro" || t == "Bird" || t == "Cat" || t == "Dog") return false;
        return true;
    });
    const CardDefId too_much_power = findCard([](const CardDef& d) {
        return d.card_type == CardType::Unit && d.energy_cost <= 2 && d.power_cost >= 2;
    });
    const CardDefId too_much_energy = findCard([](const CardDef& d) {
        return d.card_type == CardType::Unit && d.energy_cost == 3 && d.power_cost == 0;
    });
    ASSERT_NE(tagged, kInvalidId);
    ASSERT_NE(untagged, kInvalidId);
    ASSERT_NE(too_much_energy, kInvalidId);
    auto t_id = inTrashOf(P1, tagged);
    auto u_id = inTrashOf(P1, untagged);
    auto e_id = inTrashOf(P1, too_much_energy);
    GameObjectId p_id = too_much_power != kInvalidId ? inTrashOf(P1, too_much_power) : kInvalidId;
    for (int i = 0; i < 2; ++i) addRune(P1, Domain::Order);

    const Card* c = card_registry.get(kUndyingLoyalty);
    auto legal = c->enumerateLegalTargets(state, P1);
    auto has = [&](GameObjectId id) { return std::find(legal.begin(), legal.end(), id) != legal.end(); };
    EXPECT_TRUE(has(t_id));
    EXPECT_TRUE(has(u_id));
    EXPECT_FALSE(has(e_id)) << "costs more than [2]";
    if (p_id != kInvalidId) EXPECT_FALSE(has(p_id)) << "costs more than [A]";

    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    EXPECT_EQ(c->targetCostAdjustment(state, P1, {t_id}), 0) << "chose a Poro/Bird/Cat/Dog";
    EXPECT_EQ(c->targetCostAdjustment(state, P1, {u_id}), 2) << "no discount for another unit";

    // Only the discounted price is payable: only discount-giving choices.
    for (auto& [id, o] : state.objects) if (o.isRune()) o.is_exhausted = true;
    legal = c->enumerateLegalTargets(state, P1);
    EXPECT_TRUE(has(t_id));
    EXPECT_FALSE(has(u_id));
}

// ═══ [731] Ashe, Focused — the banished card comes back when they hold ════
TEST_F(Cards5Test, AsheFocused_BanishedCardReturnsWhenTheyHold) {
    makeEngine();
    auto ashe = unit(P1, kAsheFocused, 0, 0);
    ASSERT_NE(vanillaUnit(), kInvalidId);
    auto keep = handCard(P2, vanillaUnit());
    auto take = handCard(P2, kHush);
    auto& exec = engine_->testHook_executor();
    // P1 banishes `take` from P2's hand.
    {
        ChainItem ri; ri.controller = P1; ri.is_ability = true; ri.source = ashe;
        ri.card_def_id = kAsheFocused; ri.fired_trigger = TriggerType::WhenYouPlayMe;
        s().chain.resuming = ri;
        CardContext ctx{s(), events, exec, P1, ashe};
        ctx.firing_trigger = TriggerType::WhenYouPlayMe;
        Card* c = card_registry.get(kAsheFocused);
        c->onTrigger(ctx, {});
        while (exec.hasPendingChoice()) {
            auto p = exec.consumePendingChoice();
            exec.recordChoice(*chooseObject(p.legal, take));
            c->onTrigger(ctx, {});
        }
        s().chain.resuming.reset();
    }
    EXPECT_EQ(s().getObject(take).zone, ZoneType::Banishment);
    EXPECT_EQ(s().getObject(keep).zone, ZoneType::Hand);
    ASSERT_EQ(s().delayed_abilities.size(), 1u);

    // Ashe leaves the board; P1 holding does nothing; P2 holding fires it.
    s().getObject(ashe).location = std::nullopt;
    s().getObject(ashe).zone = ZoneType::Trash;
    engine_->testHook_scoreHold(P1, 1);
    EXPECT_EQ(chainItemsOf(kAsheFocused), 0);
    engine_->testHook_scoreHold(P2, 0);
    ASSERT_EQ(chainItemsOf(kAsheFocused), 1);
    EXPECT_EQ(s().chain.items.back().fired_trigger, TriggerType::WhenIHold);
    s().chain.items.clear();

    CardContext ctx{s(), events, exec, P2, ashe};
    ctx.firing_trigger = TriggerType::WhenIHold;
    card_registry.get(kAsheFocused)->onTrigger(ctx, {});
    EXPECT_EQ(s().getObject(take).zone, ZoneType::Hand);
    const auto& h = s().player(P2).hand;
    EXPECT_NE(std::find(h.begin(), h.end(), take), h.end());
}

// ═══ [732] Atakhan — defender's choice; kill a friendly unit for its cost ═
TEST_F(Cards5Test, Atakhan_DefenderChoosesWhichUnitDies) {
    auto atakhan = addUnit(P1, kAtakhan, 0, 0);
    auto small = addUnit(P2, kInvalidId, 1, 0);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 1, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<Intent> offered;
    driveResumableTrigger(kAtakhan, P1, atakhan, [&](const std::vector<Intent>& legal) {
        offered = legal;
        return *chooseObject(legal, big);
    }, exec, TriggerType::WhenIAttack);
    ASSERT_FALSE(offered.empty());
    EXPECT_EQ(offered.front().player, P2) << "the defender chooses";
    EXPECT_FALSE(offersObject(offered, elsewhere)) << "only units here";
    EXPECT_FALSE(state.getObject(big).location.has_value());
    EXPECT_TRUE(state.getObject(small).location.has_value());
}

TEST_F(Cards5Test, Atakhan_KillFriendlyUnitReducesCost) {
    makeEngine();
    const CardDefId fodder_def = findCard([](const CardDef& d) {
        return d.card_type == CardType::Unit && d.energy_cost == 5 && d.power_cost == 1 &&
               d.super_type == SuperType::None;
    });
    ASSERT_NE(fodder_def, kInvalidId);
    auto fodder = unit(P1, fodder_def, 0, 0);
    s().player(P1).rune_pool.energy = 5;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 2;
    auto card = handCard(P1, kAtakhan);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, card)) << "affordable by killing a 5/1 unit";
    bool offered_decline = false;
    a1_.pick = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (offersObject(legal, fodder) && chooseValue(legal, 0)) offered_decline = true;
        return chooseObject(legal, fodder);
    };
    engine_->testHook_executeIntent(playIntent(P1, card));
    EXPECT_FALSE(offered_decline) << "the full price isn't payable, so declining isn't offered";
    EXPECT_FALSE(s().getObject(fodder).location.has_value()) << "killed as the cost";
    EXPECT_TRUE(s().getObject(card).location.has_value()) << "Atakhan was played";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
}

// ═══ [734] LeBlanc, Fragmented — only YOUR Beginning Phase draws 2 ════════
TEST_F(Cards5Test, LeBlancFragmented_OwnBeginningPhaseOnly) {
    for (int i = 0; i < 6; ++i) addToDeck(P1, kInvalidId);
    auto lb = addUnit(P1, kLeBlancFragmented);
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.turn.turn_player = P2;
    state.turn.phase = TurnPhase::BeginningStep;
    fireTriggerAs(kLeBlancFragmented, P1, lb, TriggerType::WhenIDie, exec);
    EXPECT_EQ(handSize(P1), 1) << "the opponent's Beginning Phase isn't yours";
    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::ScoringStep;
    fireTriggerAs(kLeBlancFragmented, P1, lb, TriggerType::WhenIDie, exec);
    EXPECT_EQ(handSize(P1), 3) << "the Scoring Step is part of the Beginning Phase";
}

// ═══ [736] Shard of Undoing — the opponent chooses ═════════════════════════
TEST_F(Cards5Test, ShardOfUndoing_OpponentChoosesTheirUnit) {
    auto shard = state.createObject();
    auto& g = state.getObject(shard);
    g.owner = g.controller = P1; g.card_type = CardType::Gear; g.card_def_id = kShardOfUndoing;
    g.zone = ZoneType::Base; g.location = BaseLocation{P1};
    auto small = addUnit(P2, kInvalidId, 1, 0);
    auto big = addUnit(P2, kInvalidId, 7, 1);
    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::BeginningStep;
    state.turn.turn_number = 3;
    EffectExecutor exec(state, events, card_db, &card_registry);
    PlayerId chooser = PlayerId::None;
    driveResumableTrigger(kShardOfUndoing, P1, shard, [&](const std::vector<Intent>& legal) {
        chooser = legal.front().player;
        return *chooseObject(legal, big);
    }, exec, TriggerType::WhenAFriendlyUnitDies);
    EXPECT_EQ(chooser, P2);
    EXPECT_FALSE(state.getObject(big).location.has_value());
    EXPECT_TRUE(state.getObject(small).location.has_value());
}

// ═══ [738] Vi, Peacekeeper — stuns an enemy unit HERE (triggered target) ══
TEST_F(Cards5Test, ViPeacekeeper_StunsAChosenEnemyHere) {
    auto vi = addUnit(P1, kViPeacekeeper, 0, 0);
    auto here_a = addUnit(P2, kInvalidId, 2, 0);
    auto here_b = addUnit(P2, kInvalidId, 2, 0);
    auto there = addUnit(P2, kInvalidId, 2, 1);
    EXPECT_EQ(card_registry.get(kViPeacekeeper)->getTargetRequirements().count, 0)
        << "no engine-filled trigger targets";
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<Intent> offered;
    driveResumableTrigger(kViPeacekeeper, P1, vi, [&](const std::vector<Intent>& legal) {
        offered = legal;
        return *chooseObject(legal, here_b);
    }, exec, TriggerType::WhenIAttack);
    EXPECT_FALSE(offersObject(offered, there)) << "only enemy units here";
    EXPECT_TRUE(state.getObject(here_b).is_stunned);
    EXPECT_FALSE(state.getObject(here_a).is_stunned);
}

// ═══ [740] Poppy — optional 3 XP for [3] less, decided at play ═════════════
TEST_F(Cards5Test, Poppy_SpendXpForDiscount) {
    makeEngine();
    s().player(P1).xp = 3;
    s().player(P1).rune_pool.energy = 3;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    auto card = handCard(P1, kPoppy);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, card)) << "playable for [3] with the XP";
    engine_->testHook_executeIntent(playIntent(P1, card));
    EXPECT_TRUE(s().getObject(card).location.has_value());
    EXPECT_EQ(s().player(P1).xp, 0);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
}

TEST_F(Cards5Test, Poppy_MayDeclineTheXpCost) {
    makeEngine();
    s().player(P1).xp = 3;
    s().player(P1).rune_pool.energy = 6;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    auto card = handCard(P1, kPoppy);
    a1_.pick = [](const std::vector<Intent>& legal) { return chooseValue(legal, 0); };
    engine_->testHook_executeIntent(playIntent(P1, card));
    EXPECT_TRUE(s().getObject(card).location.has_value());
    EXPECT_EQ(s().player(P1).xp, 3) << "the 'you may' was declined";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "paid the full [6]";
}

// ═══ [741] Rift Herald — choose (or decline) the unit; Power still paid ═══
TEST_F(Cards5Test, RiftHerald_MoveTriggerPlayerChoosesOrDeclines) {
    auto herald = addUnit(P1, kRiftHerald, 0, 0);
    auto bottom = addToDeck(P1, kInvalidId);
    auto u1 = addToDeck(P1, kInvalidId);
    state.getObject(u1).card_type = CardType::Unit;
    auto u2 = addToDeck(P1, kInvalidId);
    state.getObject(u2).card_type = CardType::Unit;
    auto top = addToDeck(P1, kInvalidId);        // a non-unit on top
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRiftHerald, P1, herald, [&](const std::vector<Intent>& legal) {
        // looked = [top, u2, u1]: draw u1 (answer 1 + index 2), not the first unit.
        return *chooseValue(legal, 3);
    }, exec, TriggerType::WhenIMoveToFB);
    EXPECT_TRUE(inHand(P1, u1));
    EXPECT_FALSE(inHand(P1, u2));
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom) << "the rest went to the bottom";
    EXPECT_TRUE(inDeck(P1, top));

    // Declining draws nothing.
    int hand = handSize(P1);
    driveResumableTrigger(kRiftHerald, P1, herald, [&](const std::vector<Intent>& legal) {
        return *chooseValue(legal, 0);
    }, exec, TriggerType::WhenIMoveToFB);
    EXPECT_EQ(handSize(P1), hand);
}

TEST_F(Cards5Test, RiftHerald_DeathknellStillPaysPower) {
    makeEngine();
    const CardDefId pricey = findCard([](const CardDef& d) {
        return d.card_type == CardType::Unit && d.energy_cost >= 3 && d.power_cost == 1 &&
               d.domains.size() == 1 && d.super_type == SuperType::None;
    });
    ASSERT_NE(pricey, kInvalidId);
    auto herald = unit(P1, kRiftHerald, 0, 0);
    auto card = handCard(P1, pricey);
    auto& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, herald};
    ctx.firing_trigger = TriggerType::WhenIDie;
    // No Power available: not offered.
    card_registry.get(kRiftHerald)->onTrigger(ctx, {});
    EXPECT_EQ(s().getObject(card).zone, ZoneType::Hand);
    // With its Power available it is played, Energy ignored, Power paid.
    s().player(P1).rune_pool.power[static_cast<int>(card_db.get(pricey).domains[0])] = 1;
    card_registry.get(kRiftHerald)->onTrigger(ctx, {});
    EXPECT_NE(s().getObject(card).zone, ZoneType::Hand);
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(card_db.get(pricey).domains[0])], 0);
}

// ═══ [743] Curtain Call — three separate [Repeat] costs, new mode each ═════
TEST_F(Cards5Test, CurtainCall_PaysItsOwnRepeatsAndChoosesDistinctModes) {
    auto foe = addUnit(P2, kInvalidId, 5, 0);
    addToDeck(P1, kInvalidId); addToDeck(P1, kInvalidId);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    auto src = state.createObject();
    state.getObject(src).owner = state.getObject(src).controller = P1;
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<std::string> labels;
    int step = 0;
    driveResumable(kCurtainCall, P1, src, [&](const std::vector<Intent>& legal) {
        ++step;
        if (!legal.front().chosen_objects.empty()) return *chooseObject(legal, foe);
        labels.push_back(legal.back().choice_label);
        switch (step) {
        case 1: return *chooseValue(legal, 1);   // pay [1]
        case 2: return *chooseValue(legal, 0);   // don't pay [A]
        case 3: return *chooseValue(legal, 0);   // mode: draw
        default: return *chooseValue(legal, 1);  // mode: deal 2 at a battlefield
        }
    }, exec);
    // [1][A] isn't offered after [1] spent the only Energy.
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1) << "[A] declined";
    EXPECT_EQ(handSize(P1), 1) << "Draw 1 once";
    EXPECT_EQ(state.getObject(foe).damage_marked, 2);
}

TEST_F(Cards5Test, CurtainCall_NoRepeatResolvesOneMode) {
    auto foe = addUnit(P2, kInvalidId, 5, 0);
    addToDeck(P1, kInvalidId);
    auto src = state.createObject();
    state.getObject(src).owner = state.getObject(src).controller = P1;
    EffectExecutor exec(state, events, card_db, &card_registry);
    int prompts = 0;
    driveResumable(kCurtainCall, P1, src, [&](const std::vector<Intent>& legal) {
        ++prompts;
        if (!legal.front().chosen_objects.empty()) return *chooseObject(legal, foe);
        return *chooseValue(legal, 3);          // -4 [M] at a battlefield
    }, exec);
    EXPECT_EQ(prompts, 2) << "no Repeat offered without resources: mode + target";
    EXPECT_EQ(state.getObject(foe).temp_might_bonus, -4);
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Cards5Test, CurtainCall_ChoicesAtPlayEffectsAtResolution) {
    makeEngine();
    auto foe = unit(P2, kInvalidId, 5, 0);
    deckCard(P1); deckCard(P1);
    s().player(P1).rune_pool.energy = card_db.get(kCurtainCall).energy_cost + 1;
    s().player(P1).rune_pool.universal_power = 1;
    auto spell = handCard(P1, kCurtainCall);
    int value_prompts = 0, object_prompts = 0, mode_prompts = 0;
    a1_.pick = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (legal.empty() || legal.front().type != IntentType::MakeChoice) return std::nullopt;
        if (!legal.front().chosen_objects.empty()) { ++object_prompts; return chooseObject(legal, foe); }
        ++value_prompts;
        if (legal.size() == 2 && legal.back().choice_label.rfind("Pay ", 0) == 0)
            return chooseValue(legal, legal.back().choice_label == "Pay [1] to repeat" ? 1 : 0);
        ++mode_prompts;
        return chooseValue(legal, mode_prompts == 1 ? 0 : 1);   // Draw 1, then Deal 2
    };
    engine_->testHook_executeIntent(playIntent(P1, spell));
    EXPECT_EQ(mode_prompts, 2) << "two resolutions: the spell + one paid repeat";
    EXPECT_EQ(object_prompts, 1);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "base cost + the [1] repeat";
    EXPECT_EQ(s().player(P1).rune_pool.universal_power, 1) << "the [A] repeat was declined";
    EXPECT_EQ(s().getObject(foe).damage_marked, 2);
    EXPECT_EQ(s().player(P1).hand.size(), 1u);
}

// ═══ [745] Thrill of the Hunt — owner picks the battlefield; reset; tokens ═
TEST_F(Cards5Test, ThrillOfTheHunt_OwnerChoosesBattlefieldAndUnitIsNew) {
    makeEngine();
    auto u = unit(P1, vanillaUnit(), 0, 0);
    s().getObject(u).damage_marked = 1;
    s().getObject(u).buff_count = 1;
    auto src = s().createObject();
    auto& exec = engine_->testHook_executor();
    std::vector<std::vector<Intent>> prompts;
    resolveWithPlayTarget(kThrill, P1, src, u, exec, [&](const std::vector<Intent>& legal) {
        return *chooseValue(legal, 1);
    }, &prompts);
    ASSERT_EQ(prompts.size(), 1u) << "one battlefield prompt (the unit was chosen at play)";
    EXPECT_EQ(prompts[0].front().player, P1);
    auto bf = s().getObject(u).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 1u) << "the chosen battlefield, not the first";
    EXPECT_EQ(s().getObject(u).damage_marked, 0);
    EXPECT_EQ(s().getObject(u).buff_count, 0);
}

TEST_F(Cards5Test, ThrillOfTheHunt_BanishedTokenCeasesToExist) {
    makeEngine();
    auto tok = unit(P1, kInvalidId, 1, 0);
    s().getObject(tok).is_token = true;
    auto src = s().createObject();
    auto& exec = engine_->testHook_executor();
    std::vector<std::vector<Intent>> prompts;
    resolveWithPlayTarget(kThrill, P1, src, tok, exec,
                          [&](const std::vector<Intent>& legal) { return legal.front(); }, &prompts);
    EXPECT_TRUE(prompts.empty());
    EXPECT_FALSE(s().getObject(tok).location.has_value());
}

// ═══ [753] Green Father — replaces THAT battlefield ═══════════════════════
TEST_F(Cards5Test, GreenFather_ReplacesTheScoredBattlefield) {
    auto legend = state.createObject();
    auto& lo = state.getObject(legend);
    lo.owner = lo.controller = P1; lo.card_def_id = kGreenFather;
    lo.card_type = CardType::Legend; lo.zone = ZoneType::LegendZone;
    state.players[0].legend_zone = legend;
    auto bf0 = state.createObject(); state.battlefields[0].card_object_id = bf0;
    state.getObject(bf0).card_type = CardType::Battlefield; state.getObject(bf0).name = "A";
    auto bf1 = state.createObject(); state.battlefields[1].card_object_id = bf1;
    state.getObject(bf1).card_type = CardType::Battlefield; state.getObject(bf1).name = "B";
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kGreenFather, P1, legend, [&](const std::vector<Intent>& legal) {
        return *chooseValue(legal, 1);
    }, exec, TriggerType::WhenIConquerOrHold, /*subject=*/bf1);
    EXPECT_FALSE(state.battlefields[0].was_replaced) << "not the first controlled battlefield";
    EXPECT_TRUE(state.battlefields[1].was_replaced);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
}

// ═══ [754] Daisy! — the controller chooses the enemy unit here ═══════════
TEST_F(Cards5Test, Daisy_ControllerChoosesWhichEnemyToStun) {
    auto daisy = addUnit(P1, kDaisy, 0, 0);
    for (const char* tag : {"Bird", "Cat", "Dog", "Poro"}) {
        auto u = addUnit(P1, kInvalidId, 1);
        state.getObject(u).tags = {tag};
    }
    auto a = addUnit(P2, kInvalidId, 2, 0);
    auto b = addUnit(P2, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kDaisy, P1, daisy, [&](const std::vector<Intent>& legal) {
        return *chooseObject(legal, std::max(a, b));
    }, exec, TriggerType::WhenIAttack);
    EXPECT_TRUE(state.getObject(std::max(a, b)).is_stunned);
    EXPECT_FALSE(state.getObject(std::min(a, b)).is_stunned);
}

// ═══ [759] Keeper of the Hammer — XP on hold only ═══════════════════════
TEST_F(Cards5Test, KeeperOfTheHammer_XpOnHoldNotConquer) {
    makeEngine();
    setLegend(P1, kKeeperOfTheHammer);
    unit(P1, kInvalidId, 2, 0);
    unit(P1, kInvalidId, 2, 1);
    engine_->testHook_scoreConquer(P1, 0);
    EXPECT_EQ(chainItemsOf(kKeeperOfTheHammer), 0) << "conquering gives no XP";
    engine_->testHook_scoreHold(P1, 1);
    EXPECT_EQ(chainItemsOf(kKeeperOfTheHammer), 1);
}

// ═══ [760] Keeper's Verdict — the OWNER picks top or bottom ═══════════════
TEST_F(Cards5Test, KeepersVerdict_OwnerChoosesTopOrBottom) {
    makeEngine();
    auto foe = unit(P2, kInvalidId, 3, 0);
    deckCard(P2); deckCard(P2);
    auto src = s().createObject();
    auto& exec = engine_->testHook_executor();
    std::vector<std::vector<Intent>> prompts;
    resolveWithPlayTarget(kKeepersVerdict, P1, src, foe, exec, [&](const std::vector<Intent>& legal) {
        return *chooseValue(legal, 1);   // bottom
    }, &prompts);
    ASSERT_EQ(prompts.size(), 1u);
    EXPECT_EQ(prompts[0].front().player, P2) << "its owner chooses";
    EXPECT_EQ(s().player(P2).main_deck.front(), foe) << "placed on the bottom";
}

TEST_F(Cards5Test, KeepersVerdict_TokenCeasesToExist) {
    makeEngine();
    auto tok = unit(P2, kInvalidId, 1, 0);
    s().getObject(tok).is_token = true;
    auto src = s().createObject();
    auto& exec = engine_->testHook_executor();
    resolveWithPlayTarget(kKeepersVerdict, P1, src, tok, exec,
                          [&](const std::vector<Intent>& legal) { return legal.front(); });
    EXPECT_FALSE(s().getObject(tok).location.has_value());
    EXPECT_FALSE(inDeck(P2, tok));
    const auto& d = s().player(P2).main_deck;
    EXPECT_EQ(std::find(d.begin(), d.end(), tok), d.end());
}

// ═══ [763] Amateur Recital — "you may move a unit at a battlefield" ═══════
TEST_F(Cards5Test, AmateurRecital_MayMoveAUnitToItsBase) {
    auto bfsrc = state.createObject();
    state.battlefields[0].card_object_id = bfsrc;
    auto foe = addUnit(P2, kInvalidId, 3, 1);
    EXPECT_EQ(card_registry.get(kAmateurRecital)->getTargetRequirements().count, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kAmateurRecital, P1, bfsrc, [&](const std::vector<Intent>& legal) {
        return *chooseValue(legal, 0);   // decline
    }, exec, TriggerType::WhenYouHoldHere);
    EXPECT_TRUE(state.getObject(foe).isAtBattlefield());
    driveResumableTrigger(kAmateurRecital, P1, bfsrc, [&](const std::vector<Intent>& legal) {
        if (legal.front().chosen_value.has_value()) return *chooseValue(legal, 1);
        return *chooseObject(legal, foe);
    }, exec, TriggerType::WhenYouHoldHere);
    EXPECT_TRUE(state.getObject(foe).isAtBase());
}

// ═══ [784] Wuju Master — [Level 11] your units enter ready ════════════════
TEST_F(Cards5Test, WujuMaster_Level11UnitsEnterReady) {
    makeEngine();
    setLegend(P1, kWujuMaster);
    const CardDefId vanilla = vanillaUnit();
    ASSERT_NE(vanilla, kInvalidId);
    s().player(P1).xp = 10;
    engine_->testHook_cleanup();
    EXPECT_FALSE(s().player(P1).units_enter_ready);
    s().player(P1).xp = 11;
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().player(P1).units_enter_ready);
    s().player(P1).rune_pool.energy = card_db.get(vanilla).energy_cost;
    auto card = handCard(P1, vanilla);
    engine_->testHook_executeIntent(playIntent(P1, card));
    ASSERT_TRUE(s().getObject(card).location.has_value());
    EXPECT_FALSE(s().getObject(card).is_exhausted);
}

// ═══ [786] Scorn of the Moon — Energy only for showdowns ═══════════════════
TEST_F(Cards5Test, ScornOfTheMoon_EnergyOnlyDuringShowdowns) {
    makeEngine();
    const CardDefId one_cost = findCard([](const CardDef& d) {
        return d.card_type == CardType::Spell && d.energy_cost == 1 && d.power_cost == 0 &&
               d.keywords.has(Keyword::Reaction);
    });
    ASSERT_NE(one_cost, kInvalidId);
    auto legend = setLegend(P1, kScornOfTheMoon);
    auto& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, legend};
    card_registry.get(kScornOfTheMoon)->onActivate(ctx, {});
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(false, SpendPurpose::Any, true), 1);
    auto spell = handCard(P1, one_cost);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, spell)) << "not outside a showdown";
    s().turn.ns_state = NeutralShowdownState::Showdown;
    EXPECT_TRUE(engine_->testHook_canAfford(P1, spell));
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell));
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(false, SpendPurpose::Any, true), 0);
}

// ═══ [879] Renekton, Brute — empowered when his Might reaches 10 ══════════
TEST_F(Cards5Test, RenektonBrute_EmpowersAtTenMight) {
    makeEngine();
    auto ren = unit(P1, kRenektonBrute, 0, 0);
    engine_->testHook_cleanup();
    EXPECT_EQ(chainItemsOf(kRenektonBrute), 0);
    s().getObject(ren).temp_might_bonus = 6;   // 4 + 6 = 10
    s().getObject(ren).recomputeMight();
    engine_->testHook_cleanup();
    ASSERT_EQ(chainItemsOf(kRenektonBrute), 1);
    EXPECT_EQ(s().chain.items.back().fired_trigger, TriggerType::WhenMyMightBecomes10OrMore);
    engine_->testHook_cleanup();
    EXPECT_EQ(chainItemsOf(kRenektonBrute), 1) << "fires on the edge only";
    s().chain.items.clear();
    auto& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, ren};
    card_registry.get(kRenektonBrute)->onTrigger(ctx, {});
    EXPECT_TRUE(s().getObject(ren).isEmpowered());
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().getObject(ren).hasKeyword(Keyword::Ganking));
}

// ═══ [884] Spiderling — +1 [M] per other Spiderling you control here ═════
TEST_F(Cards5Test, Spiderling_CountsOtherSpiderlingsHere) {
    makeEngine();
    auto a = unit(P1, kSpiderling, 0, 0);
    auto b = unit(P1, kSpiderling, 0, 0);
    auto c = unit(P1, kSpiderling, 0, 0);
    auto lone = unit(P1, kSpiderling, 0, 1);
    auto foe = unit(P2, kSpiderling, 0, 0);
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(a).current_might, 3);
    EXPECT_EQ(s().getObject(b).current_might, 3);
    EXPECT_EQ(s().getObject(c).current_might, 3);
    EXPECT_EQ(s().getObject(lone).current_might, 1);
    EXPECT_EQ(s().getObject(foe).current_might, 1) << "only units its controller controls";
}

// ═══ [892] Twilight Step / [935] Shadow Dash — the play-time target is used ═
TEST_F(Cards5Test, TwilightStep_UsesTheTargetChosenAtPlay) {
    auto small_a = addUnit(P1, kInvalidId, 2, 0);
    auto small_b = addUnit(P1, kInvalidId, 3, 0);
    auto src = state.createObject();
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<std::vector<Intent>> prompts;
    resolveWithPlayTarget(kTwilightStep, P1, src, small_b, exec,
                          [&](const std::vector<Intent>& legal) { return *chooseValue(legal, 0); },
                          &prompts);
    ASSERT_EQ(prompts.size(), 1u) << "only the destination is asked";
    EXPECT_TRUE(prompts[0].front().chosen_value.has_value());
    EXPECT_TRUE(state.getObject(small_b).isAtBase());
    EXPECT_TRUE(state.getObject(small_a).isAtBattlefield());
}

TEST_F(Cards5Test, ShadowDash_UsesTheTargetChosenAtPlay) {
    addUnit(P1, kInvalidId, 2, 1);
    auto foe_a = addUnit(P2, kInvalidId, 2, 0);
    auto foe_b = addUnit(P2, kInvalidId, 2, 0);
    auto src = state.createObject();
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<std::vector<Intent>> prompts;
    resolveWithPlayTarget(kShadowDash, P1, src, foe_b, exec,
                          [&](const std::vector<Intent>& legal) { return legal.front(); },
                          &prompts);
    ASSERT_EQ(prompts.size(), 1u) << "only the battlefield is asked";
    EXPECT_EQ(*state.getObject(foe_b).battlefieldId(), 1u);
    EXPECT_EQ(*state.getObject(foe_a).battlefieldId(), 0u);
}

// ═══ [956] The Exile — units played to a battlefield you control enter ready ═
TEST_F(Cards5Test, TheExile_UnitsPlayedToControlledBattlefieldEnterReady) {
    makeEngine();
    setLegend(P1, kTheExile);
    const CardDefId vanilla = vanillaUnit();
    ASSERT_NE(vanilla, kInvalidId);
    unit(P1, kInvalidId, 1, 0);
    s().battlefields[0].controller = P1;
    auto a = handCard(P1, vanilla);
    auto b = handCard(P1, vanilla);
    s().player(P1).rune_pool.energy = 2 * card_db.get(vanilla).energy_cost;
    engine_->testHook_executeIntent(playIntent(P1, a, LocationId{BattlefieldLocation{0}}));
    engine_->testHook_executeIntent(playIntent(P1, b));   // to base
    ASSERT_TRUE(s().getObject(a).location.has_value());
    ASSERT_TRUE(s().getObject(b).location.has_value());
    EXPECT_FALSE(s().getObject(a).is_exhausted);
    EXPECT_TRUE(s().getObject(b).is_exhausted);
}

// ═══ [957] Riven, Broken Blade — only BECOMING exhausted triggers ════════
TEST_F(Cards5Test, RivenBrokenBlade_TriggersOnlyOnBecomingExhausted) {
    makeEngine();
    auto riven = unit(P1, kRivenBrokenBlade, 0, 0);
    auto& exec = engine_->testHook_executor();
    exec.exhaustObject(riven);
    EXPECT_EQ(chainItemsOf(kRivenBrokenBlade), 1);
    exec.exhaustObject(riven);   // already exhausted
    EXPECT_EQ(chainItemsOf(kRivenBrokenBlade), 1);
    // The trigger: buff me, then (another time) spend the buff to ready me.
    s().chain.items.clear();
    EffectExecutor fx(state, events, card_db, &card_registry);
    auto r2 = addUnit(P1, kRivenBrokenBlade, 0, 0);
    state.getObject(r2).is_exhausted = true;
    driveResumableTrigger(kRivenBrokenBlade, P1, r2,
        [&](const std::vector<Intent>& legal) { return *chooseValue(legal, 0); },
        fx, TriggerType::WhenIBecomeExhausted);
    EXPECT_EQ(state.getObject(r2).buff_count, 1);
    driveResumableTrigger(kRivenBrokenBlade, P1, r2,
        [&](const std::vector<Intent>& legal) { return *chooseValue(legal, 2); },
        fx, TriggerType::WhenIBecomeExhausted);
    EXPECT_EQ(state.getObject(r2).buff_count, 0);
    EXPECT_FALSE(state.getObject(r2).is_exhausted);
}

// ═══ [958] Blade of the Exile — the player picks how to pay; never free ════
TEST_F(Cards5Test, BladeOfTheExile_PlayerPicksEquipCost) {
    auto target = addUnit(P1, kInvalidId, 2, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    auto blade = state.createObject();
    auto& g = state.getObject(blade);
    g.owner = g.controller = P1; g.card_type = CardType::Gear; g.card_def_id = kBladeOfTheExile;
    g.might_bonus = 3; g.zone = ZoneType::Base; g.location = BaseLocation{P1};
    addRune(P1, Domain::Calm);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<Intent> offered;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& legal) {
        offered = legal;
        return *chooseObject(legal, target);   // exhaust the unit being equipped
    });
    CardContext ctx{state, events, exec, P1, blade};
    ASSERT_TRUE(card_registry.get(kBladeOfTheExile)->onEquip(ctx, target));
    EXPECT_TRUE(offersObject(offered, blade)) << "[A] is an option";
    EXPECT_TRUE(offersObject(offered, other));
    EXPECT_TRUE(state.getObject(target).is_exhausted) << "any friendly unit, itself included";
    EXPECT_EQ(readyRuneCount(P1), 1) << "the rune was not used";
    EXPECT_EQ(state.getObject(blade).attached_to, std::optional<GameObjectId>(target));
}

TEST_F(Cards5Test, BladeOfTheExile_NoPayableCostNoEquip) {
    auto target = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(target).is_exhausted = true;
    auto blade = state.createObject();
    auto& g = state.getObject(blade);
    g.owner = g.controller = P1; g.card_type = CardType::Gear; g.card_def_id = kBladeOfTheExile;
    g.zone = ZoneType::Base; g.location = BaseLocation{P1};
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, blade};
    EXPECT_FALSE(card_registry.get(kBladeOfTheExile)->onEquip(ctx, target));
    EXPECT_FALSE(state.getObject(blade).attached_to.has_value());
}

// ═══ [959] Bedeviling Stranger — the return is optional ═══════════════════
TEST_F(Cards5Test, BedevilingStranger_ReturnIsOptional) {
    auto me = addUnit(P1, kBedevilingStranger, 0, 0);
    addUnit(P1, kInvalidId, 1, 0);
    addUnit(P2, kInvalidId, 1, 0);
    auto foe = addUnit(P2, kInvalidId, 1, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kBedevilingStranger, P1, me, [&](const std::vector<Intent>& legal) {
        return *chooseValue(legal, 0);
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(foe).location.has_value()) << "declined";
    driveResumableTrigger(kBedevilingStranger, P1, me, [&](const std::vector<Intent>& legal) {
        if (legal.front().chosen_value.has_value()) return *chooseValue(legal, 1);
        return *chooseObject(legal, foe);
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inHand(P2, foe));
}

// ═══ [960] Siren of the Shallows — [Disarm] an enemy unit here ════════════
TEST_F(Cards5Test, SirenOfTheShallows_DisarmsChosenEnemyHere) {
    auto siren = addUnit(P1, kSirenOfTheShallows, 0, 0);
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto there = addUnit(P2, kInvalidId, 3, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<Intent> offered;
    driveResumableTrigger(kSirenOfTheShallows, P1, siren, [&](const std::vector<Intent>& legal) {
        offered = legal;
        return *chooseObject(legal, b);
    }, exec, TriggerType::WhenIAttack);
    EXPECT_FALSE(offersObject(offered, there));
    EXPECT_EQ(state.getObject(b).temp_might_bonus, -1);
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 0);
}

// ═══ [961] Reckless Innovator — Gold token or pay [P] to draw ═════════════
TEST_F(Cards5Test, RecklessInnovator_GoldOrPaidDraw) {
    auto ri = addUnit(P1, kRecklessInnovator, 0, 0);
    addToDeck(P1, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRecklessInnovator, P1, ri,
        [&](const std::vector<Intent>& legal) { return *chooseValue(legal, 0); },
        exec, TriggerType::WhenIBecomeExhausted);
    int golds = 0;
    for (auto& [id, o] : state.objects)
        if (o.isGear() && o.name == "Gold") { ++golds; EXPECT_TRUE(o.is_exhausted); }
    EXPECT_EQ(golds, 1);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    driveResumableTrigger(kRecklessInnovator, P1, ri,
        [&](const std::vector<Intent>& legal) { return *chooseValue(legal, 1); },
        exec, TriggerType::WhenIBecomeExhausted);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
}

// ═══ [962] Hunker Down — ready, can't move this turn, draw ═════════════════
TEST_F(Cards5Test, HunkerDown_ReadiesButCantMove) {
    makeEngine();
    auto u = unit(P1, kInvalidId, 2);
    s().getObject(u).is_exhausted = true;
    deckCard(P1);
    auto src = s().createObject();
    auto& exec = engine_->testHook_executor();
    CardContext ctx{s(), events, exec, P1, src};
    card_registry.get(kHunkerDown)->onResolve(ctx, {u});
    EXPECT_FALSE(s().getObject(u).is_exhausted);
    EXPECT_EQ(s().player(P1).hand.size(), 1u);
    for (const auto& a : engine_->generateLegalActions())
        if (a.type == IntentType::StandardMove)
            for (auto m : a.units_to_move) EXPECT_NE(m, u) << "it can't move this turn";
    EXPECT_TRUE(s().getObject(u).cant_move_this_turn);
}

// ═══ [963] Hush — counters only Power cost [A][A] or more ════════════════
TEST_F(Cards5Test, Hush_CountersOnlyTwoPowerSpells) {
    const CardDefId two_power = findCard([](const CardDef& d) {
        return d.card_type == CardType::Spell && d.power_cost >= 2;
    });
    const CardDefId one_power = findCard([](const CardDef& d) {
        return d.card_type == CardType::Spell && d.power_cost == 1;
    });
    ASSERT_NE(two_power, kInvalidId);
    ASSERT_NE(one_power, kInvalidId);
    EffectExecutor exec(state, events, card_db, &card_registry);
    pushSpellOnChain(P2, one_power);
    auto hush = state.createObject();
    invokeOnResolve(hush, kHush, P1, {}, exec);
    EXPECT_EQ(state.chain.items.size(), 1u) << "power cost 1: not countered";
    state.chain.items.clear();
    pushSpellOnChain(P2, two_power);
    invokeOnResolve(hush, kHush, P1, {}, exec);
    EXPECT_TRUE(state.chain.items.empty()) << "power cost 2+: countered";
}

// ═══ [964] Ki Burst — "[Stun] a unit": friendly units too ═══════════════
TEST_F(Cards5Test, KiBurst_CanStunAnyUnit) {
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);
    auto payer = addUnit(P1, kInvalidId, 2);
    const Card* c = card_registry.get(kKiBurst);
    auto legal = c->enumerateLegalTargets(state, P1);
    EXPECT_NE(std::find(legal.begin(), legal.end(), mine), legal.end());
    EXPECT_NE(std::find(legal.begin(), legal.end(), theirs), legal.end());
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto src = state.createObject();
    invokeOnResolve(src, kKiBurst, P1, {mine}, exec);
    EXPECT_TRUE(state.getObject(mine).is_stunned);
    (void)payer;
}

// ═══ [965] An Offer You Can't Refuse — kill gear, its controller gets Gold ═
TEST_F(Cards5Test, AnOffer_KillsGearAndGivesItsControllerGold) {
    auto g = state.createObject();
    auto& go = state.getObject(g);
    go.owner = go.controller = P2; go.card_type = CardType::Gear; go.name = "TestGear";
    go.zone = ZoneType::Base; go.location = BaseLocation{P2};
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto src = state.createObject();
    invokeOnResolve(src, kOfferYouCantRefuse, P1, {g}, exec);
    EXPECT_FALSE(state.getObject(g).location.has_value());
    int p2_gold = 0;
    for (auto& [id, o] : state.objects)
        if (o.isGear() && o.name == "Gold" && o.controller == P2 && o.is_exhausted) ++p2_gold;
    EXPECT_EQ(p2_gold, 1);
}

// ═══ [966] Hunters' Circle — first unit played here each turn costs [1] less ═
TEST_F(Cards5Test, HuntersCircle_FirstUnitPlayedHereCostsLess) {
    makeEngine();
    bfCard(0, kHuntersCircle);
    unit(P1, kInvalidId, 1, 0);
    s().battlefields[0].controller = P1;
    s().turn.turn_number = 4;
    engine_->testHook_cleanup();
    EXPECT_EQ(s().battlefields[0].first_play_here_discount, 1);
    const CardDefId two = findCard([](const CardDef& d) {
        return d.card_type == CardType::Unit && d.ability_text.empty() && d.energy_cost >= 1 &&
               d.energy_cost <= 3 && d.power_cost == 0 && d.keywords.bits == 0 &&
               d.super_type == SuperType::None;
    });
    ASSERT_NE(two, kInvalidId);
    const int c = card_db.get(two).energy_cost;
    ASSERT_GE(c, 1);
    auto a = handCard(P1, two);
    auto b = handCard(P1, two);
    s().player(P1).rune_pool.energy = 2 * c - 1;
    engine_->testHook_executeIntent(playIntent(P1, a, LocationId{BattlefieldLocation{0}}));
    EXPECT_TRUE(s().getObject(a).location.has_value());
    EXPECT_EQ(s().player(P1).rune_pool.energy, c) << "paid [1] less";
    engine_->testHook_executeIntent(playIntent(P1, b, LocationId{BattlefieldLocation{0}}));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "the second one pays full";
}

// ═══ [967] Battle Preparations — +2 [M]; readies runes if you showed off ══
TEST_F(Cards5Test, BattlePreparations_ReadiesRunesOnlyWithAGear) {
    auto u = addUnit(P1, kInvalidId, 2, 0);
    addRune(P1, Domain::Calm, true);
    addRune(P1, Domain::Calm, true);
    addRune(P1, Domain::Calm, true);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto src = state.createObject();
    invokeOnResolve(src, kBattlePreparations, P1, {u}, exec);
    EXPECT_EQ(state.getObject(u).temp_might_bonus, 2);
    EXPECT_EQ(readyRuneCount(P1), 0) << "no gear to show off";
    auto g = state.createObject();
    auto& go = state.getObject(g);
    go.owner = go.controller = P1; go.card_type = CardType::Gear;
    go.zone = ZoneType::Base; go.location = BaseLocation{P1};
    invokeOnResolve(src, kBattlePreparations, P1, {u}, exec);
    EXPECT_EQ(readyRuneCount(P1), 2) << "up to 2 runes readied";
}

// ═══ [968] Rakelstake — predict once per player per turn ══════════════════
TEST_F(Cards5Test, Rakelstake_PredictsOncePerTurn) {
    auto src = state.createObject();
    state.battlefields[0].card_object_id = src;
    state.getObject(src).card_def_id = kRakelstake;
    addToDeck(P1, kInvalidId);
    state.turn.turn_number = 2;
    EffectExecutor exec(state, events, card_db, &card_registry);
    int looks = 0;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& legal) {
        ++looks;
        return legal.back();   // keep on top
    });
    fireTriggerAs(kRakelstake, P1, src, TriggerType::WhenYouPlayAUnit, exec);
    fireTriggerAs(kRakelstake, P1, src, TriggerType::WhenYouPlayAUnit, exec);
    EXPECT_EQ(looks, 1) << "the first play here this turn only";
    state.turn.turn_number = 3;
    fireTriggerAs(kRakelstake, P1, src, TriggerType::WhenYouPlayAUnit, exec);
    EXPECT_EQ(looks, 2);
}

}  // namespace
}  // namespace riftbound::test
