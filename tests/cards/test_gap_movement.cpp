/// @file test_gap_movement.cpp
/// Movement / location gaps (2026-10-28 audit, round 2):
///   - moves vs recalls (CR 448 / 457-458): one move-restriction check
///     (core/move_rules.h) for standard and effect moves; recalls bypass it
///   - group standard moves (CR 144.3) and applied move costs (CR 204.4)
///   - token play locations (CR 185.2.a / 355.2.a), face-down play location
///     permission, per-location affordability
/// Cards: Minotaur Reckoner (337), Entrenched Boar (1011), Hunker Down (962),
/// Vex, Apathetic (712), Gravity Field (1042), Mageseeker Investigator (725),
/// Void Assault (758), Arise! (513), Last Caress (982), Hunters' Circle (966),
/// Zhonya's Hourglass (77), Tricksy Tentacles (616).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "core/move_rules.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

namespace riftbound::test {
namespace {

constexpr CardDefId kZhonya = 77, kMinotaur = 337, kArise = 513, kTricksy = 616,
    kVexApathetic = 712, kMageseeker = 725, kVoidAssault = 758, kHunkerDown = 962,
    kHuntersCircle = 966, kLastCaress = 982, kEntrenchedBoar = 1011, kGravityField = 1042;

class MoveAgent : public AgentInterface {
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

std::optional<Intent> pickObject(const std::vector<Intent>& legal, GameObjectId want) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
            i.chosen_objects.front() == want) return i;
    return std::nullopt;
}
std::optional<Intent> pickValue(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.chosen_value.has_value() && *i.chosen_value == v)
            return i;
    return std::nullopt;
}

class MoveGapTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    MoveAgent a1_, a2_;
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
        st.turn.turn_number = 3;
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
    EffectExecutor& exec() { return engine_->testHook_executor(); }
    GameObjectId unit(PlayerId p, CardDefId def_id, int might = 0, int at_bf = -1) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            u.name = d.name; u.tags = d.tags; u.keywords = d.keywords; u.domains = d.domains;
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
    GameObjectId rune(PlayerId p, Domain d = Domain::Fury) {
        auto id = s().createObject();
        auto& r = s().getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        return id;
    }
    GameObjectId gearAt(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = s().createObject();
        auto& g = s().getObject(id);
        g.owner = g.controller = p;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        g.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestGear";
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId bfCard(BattlefieldId bf, CardDefId def_id) {
        auto src = s().createObject();
        auto& o = s().getObject(src);
        o.card_def_id = def_id;
        o.card_type = CardType::Battlefield;
        o.name = card_db.get(def_id).name;
        s().battlefields[bf].card_object_id = src;
        return src;
    }
    GameObjectId handCard(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& d = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = d.card_type;
        c.name = d.name; c.domains = d.domains; c.keywords = d.keywords; c.tags = d.tags;
        c.zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    /// A spell or ability of `controller` is resolving (effect-move attribution).
    void resolvingAs(PlayerId controller) {
        ChainItem ri;
        ri.id = s().chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        s().chain.resuming = ri;
    }
    bool at(GameObjectId id, const LocationId& loc) {
        return s().getObject(id).location.has_value() && *s().getObject(id).location == loc;
    }
    std::vector<Intent> movesOf(GameObjectId u) {
        std::vector<Intent> out;
        for (const auto& a : engine_->generateLegalActions())
            if (a.type == IntentType::StandardMove &&
                std::find(a.units_to_move.begin(), a.units_to_move.end(), u) != a.units_to_move.end())
                out.push_back(a);
        return out;
    }
    /// Run a resumable onResolve with `targets` through the requestChoice loop.
    void resolveWith(CardDefId def_id, PlayerId controller, const std::vector<GameObjectId>& targets,
                     EffectExecutor& ex, std::function<Intent(int, const std::vector<Intent>&)> picker) {
        ChainItem ri;
        ri.id = s().chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.card_def_id = def_id;
        ri.source = s().createObject();
        ri.targets = targets;
        s().chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{s(), events, ex, controller, ri.source};
        card->onResolve(ctx, targets);
        int n = 0;
        while (ex.hasPendingChoice()) {
            auto pending = ex.consumePendingChoice();
            ex.recordChoice(picker(n++, pending.legal));
            card->onResolve(ctx, targets);
        }
        s().chain.resuming.reset();
    }
};

// ═══ Moves vs recalls (CR 458.3) ═════════════════════════════════════════════

TEST_F(MoveGapTest, Minotaur_BlocksEffectMovesToBaseButNotRecalls) {
    makeEngine();
    unit(P2, kMinotaur, 0, 1);
    auto mine = unit(P1, kInvalidId, 2, 0);
    auto other = unit(P1, kInvalidId, 2, 0);
    engine_->testHook_cleanup();
    ASSERT_TRUE(s().getObject(mine).aura_cant_move_to_base);
    for (const auto& a : movesOf(mine))
        EXPECT_FALSE(std::holds_alternative<BaseLocation>(*a.move_destination))
            << "no standard move to base";
    resolvingAs(P1);
    EXPECT_FALSE(exec().moveToBase(mine)) << "an effect move to base is a move: blocked";
    EXPECT_TRUE(at(mine, BattlefieldLocation{0}));
    EXPECT_TRUE(exec().moveToBattlefield(mine, 1)) << "moves elsewhere are fine";
    // A recall is not a move (CR 458.3): Zhonya's Hourglass still saves to base.
    exec().recallToBase(other);
    EXPECT_TRUE(at(other, BaseLocation{P1}));
    s().chain.resuming.reset();
}

TEST_F(MoveGapTest, Recall_KeepsStatusesAndFiresNoMoveEvent) {
    makeEngine();
    auto u = unit(P1, kInvalidId, 3, 0);
    s().getObject(u).damage_marked = 2;
    s().getObject(u).cant_move_turn = s().turn.turn_number;   // can't move: irrelevant to recalls
    int moved_events = 0;
    auto conn = events.on_unit_moved.connect([&](const UnitMovedEvent&) { ++moved_events; });
    exec().recallToBase(u);
    conn.disconnect();
    EXPECT_TRUE(at(u, BaseLocation{P1}));
    EXPECT_EQ(s().getObject(u).damage_marked, 2) << "CR 460.1";
    EXPECT_EQ(moved_events, 0) << "CR 458.1: no move triggers";
    EXPECT_EQ(s().getObject(u).moves_this_turn, 0);
}

TEST_F(MoveGapTest, Zhonya_RecallsPastMinotaur) {
    makeEngine();
    unit(P2, kMinotaur, 0, 1);
    auto hourglass = gearAt(P1, kZhonya);
    auto u = unit(P1, kInvalidId, 2, 0);
    engine_->testHook_cleanup();
    ASSERT_TRUE(s().getObject(u).aura_cant_move_to_base);
    Card* z = card_registry.get(kZhonya);
    CardContext ctx{s(), events, exec(), P1, hourglass};
    ASSERT_TRUE(z->applyReplacement(ctx, u));
    EXPECT_TRUE(at(u, BaseLocation{P1})) << "recalled despite \"units can't move to base\"";
}

// ═══ "Can't move this turn" blocks effect moves too ═════════════════════════

TEST_F(MoveGapTest, EntrenchedBoar_CantBeMovedByEffectsThisTurn) {
    makeEngine();
    auto boar = unit(P1, kEntrenchedBoar, 0, 0);
    s().getObject(boar).forbidMovesThisTurn(s().turn.turn_number);
    EXPECT_TRUE(movesOf(boar).empty());
    resolvingAs(P1);
    EXPECT_FALSE(exec().moveToBase(boar));
    resolvingAs(P2);
    EXPECT_FALSE(exec().moveToBattlefield(boar, 1)) << "nobody can move it";
    EXPECT_TRUE(at(boar, BattlefieldLocation{0}));
    s().turn.turn_number++;
    EXPECT_TRUE(exec().moveToBattlefield(boar, 1)) << "next turn it can";
    s().chain.resuming.reset();
}

TEST_F(MoveGapTest, HunkerDown_BlocksEffectMoves) {
    makeEngine();
    auto u = unit(P1, kInvalidId, 2, 0);
    s().getObject(u).is_exhausted = true;
    auto src = s().createObject();
    CardContext ctx{s(), events, exec(), P1, src};
    card_registry.get(kHunkerDown)->onResolve(ctx, {u});
    EXPECT_FALSE(s().getObject(u).is_exhausted);
    EXPECT_TRUE(movesOf(u).empty());
    resolvingAs(P1);
    EXPECT_FALSE(exec().moveToBase(u));
    s().chain.resuming.reset();
}

TEST_F(MoveGapTest, VexApathetic_OnlyItsControllerCantMoveIt) {
    makeEngine();
    auto vex = unit(P1, kVexApathetic, 0, 0);
    auto played = unit(P2, kInvalidId, 3);
    s().getObject(vex).card_counters["__opp_played_unit_id"] = static_cast<int>(played);
    CardContext ctx{s(), events, exec(), P1, vex};
    card_registry.get(kVexApathetic)->onTrigger(ctx, {});
    resolvingAs(P2);
    EXPECT_FALSE(exec().moveToBattlefield(played, 1)) << "\"They can't move it\"";
    resolvingAs(P1);
    EXPECT_TRUE(exec().moveToBattlefield(played, 1)) << "Vex's controller still can";
    s().chain.resuming.reset();
}

TEST_F(MoveGapTest, ForbidMovesThisTurn_UnionsRestrictions) {
    GameObject o;
    o.forbidMovesThisTurn(5, P2);
    EXPECT_EQ(o.cant_move_only_by, P2);
    o.forbidMovesThisTurn(5);
    EXPECT_EQ(o.cant_move_only_by, PlayerId::None) << "anyone now";
    o.forbidMovesThisTurn(6, P1);
    EXPECT_EQ(o.cant_move_turn, 6);
    EXPECT_EQ(o.cant_move_only_by, P1) << "a new turn replaces it";
}

// ═══ Gravity Field: applied cost on effect moves ════════════════════════════

TEST_F(MoveGapTest, GravityField_TaxesOpponentsEffectMoves) {
    makeEngine();
    gearAt(P1, kGravityField, 0);
    engine_->testHook_cleanup();
    ASSERT_EQ(s().battlefields[0].move_tax_energy[playerIndex(P2)], 1);
    auto theirs = unit(P2, kInvalidId, 2);
    auto mine = unit(P1, kInvalidId, 2);
    a2_.pick = [](const std::vector<Intent>& l) { return pickValue(l, 1); };   // pay

    resolvingAs(P2);
    EXPECT_FALSE(exec().moveToBattlefield(theirs, 0)) << "can't pay: no move (CR 204.4.c)";
    EXPECT_TRUE(at(theirs, BaseLocation{P2}));

    rune(P2); rune(P2);
    EXPECT_TRUE(exec().moveToBattlefield(theirs, 0));
    EXPECT_TRUE(at(theirs, BattlefieldLocation{0}));
    int total = 0;
    for (auto& [id, o] : s().objects)
        if (o.isRune() && o.controller == P2 && o.location.has_value()) ++total;
    EXPECT_EQ(total, 1) << "one rune recycled for [A] (and [1] paid by exhausting)";

    resolvingAs(P1);
    EXPECT_TRUE(exec().moveToBattlefield(mine, 0)) << "its controller pays nothing";
    s().chain.resuming.reset();
}

TEST_F(MoveGapTest, GravityField_MoverMayDecline) {
    makeEngine();
    gearAt(P1, kGravityField, 0);
    engine_->testHook_cleanup();
    auto theirs = unit(P2, kInvalidId, 2);
    rune(P2); rune(P2);
    a2_.pick = [](const std::vector<Intent>& l) { return pickValue(l, 0); };   // decline
    resolvingAs(P2);
    EXPECT_FALSE(exec().moveToBattlefield(theirs, 0));
    EXPECT_TRUE(at(theirs, BaseLocation{P2}));
    s().chain.resuming.reset();
}

// ═══ Group standard move (CR 144.3) + Mageseeker Investigator ═══════════════

TEST_F(MoveGapTest, StandardMove_GroupOfUnitsFromDifferentOrigins) {
    makeEngine(3);
    auto a = unit(P1, kInvalidId, 2);
    auto b = unit(P1, kInvalidId, 2);
    auto c = unit(P1, kInvalidId, 2, 2);       // at BF2: can't reach BF1 without Ganking
    s().getObject(c).keywords.set(Keyword::Ganking);
    auto tired = unit(P1, kInvalidId, 2);
    s().getObject(tired).is_exhausted = true;
    int asked = 0;
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        ++asked;
        EXPECT_TRUE(pickValue(l, 0).has_value()) << "\"move only these\" is always offered";
        EXPECT_FALSE(pickObject(l, tired).has_value()) << "exhausted units can't join";
        if (auto i = pickObject(l, b)) return i;
        if (auto i = pickObject(l, c)) return i;
        return pickValue(l, 0);
    };
    engine_->testHook_executeStandardMove(Intent::standardMove(P1, {a}, BattlefieldLocation{1}));
    EXPECT_EQ(asked, 2) << "no third prompt: nobody left to add";
    for (auto u : {a, b, c}) {
        EXPECT_TRUE(at(u, BattlefieldLocation{1}));
        EXPECT_TRUE(s().getObject(u).is_exhausted);
    }
    EXPECT_EQ(s().player(P1).last_exhaust_group_size, 3) << "exhausted at the same time";
    EXPECT_TRUE(at(tired, BaseLocation{P1}));
}

TEST_F(MoveGapTest, StandardMove_FirstOptionMovesJustTheChosenUnit) {
    makeEngine();
    auto a = unit(P1, kInvalidId, 2);
    auto b = unit(P1, kInvalidId, 2);
    engine_->testHook_executeStandardMove(Intent::standardMove(P1, {a}, BattlefieldLocation{0}));
    EXPECT_TRUE(at(a, BattlefieldLocation{0}));
    EXPECT_TRUE(at(b, BaseLocation{P1}));
    ASSERT_EQ(a1_.seen.size(), 1u);
    EXPECT_TRUE(a1_.seen[0].front().chosen_value.has_value()) << "\"done\" first";
}

TEST_F(MoveGapTest, StandardMove_GroupChoiceEncodingIsCollisionFree) {
    makeEngine();
    auto a = unit(P1, kInvalidId, 2);
    unit(P1, kMinotaur, 0);
    unit(P1, kEntrenchedBoar, 0);
    engine_->testHook_executeStandardMove(Intent::standardMove(P1, {a}, BattlefieldLocation{0}));
    ASSERT_EQ(a1_.seen.size(), 1u);
    const auto& opts = a1_.seen[0];
    ASSERT_EQ(opts.size(), 3u);
    // "done" is an int answer; each candidate is keyed by its card: distinct.
    EXPECT_TRUE(opts[0].chosen_value.has_value());
    EXPECT_FALSE(opts[1].chosen_value.has_value());
    EXPECT_NE(s().getObject(opts[1].chosen_objects[0]).card_def_id,
              s().getObject(opts[2].chosen_objects[0]).card_def_id);
}

TEST_F(MoveGapTest, MageseekerInvestigator_SurchargesGroupMoves) {
    makeEngine();
    unit(P2, kMageseeker, 0, 0);
    engine_->testHook_cleanup();
    ASSERT_EQ(s().battlefields[0].multi_move_surcharge_power[playerIndex(P1)], 1);
    auto a = unit(P1, kInvalidId, 2);
    auto b = unit(P1, kInvalidId, 2);
    auto c = unit(P1, kInvalidId, 2);
    rune(P1);   // pays [A] for ONE unit beyond the first
    std::vector<std::vector<GameObjectId>> offered;
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        std::vector<GameObjectId> ids;
        for (const auto& i : l) if (!i.chosen_objects.empty()) ids.push_back(i.chosen_objects[0]);
        offered.push_back(ids);
        if (auto i = pickObject(l, b)) return i;
        return pickValue(l, 0);
    };
    engine_->testHook_executeStandardMove(Intent::standardMove(P1, {a}, BattlefieldLocation{0}));
    ASSERT_EQ(offered.size(), 1u) << "after adding b, c would cost a second [A]: not offered";
    EXPECT_EQ(offered[0].size(), 2u);
    EXPECT_TRUE(at(a, BattlefieldLocation{0}));
    EXPECT_TRUE(at(b, BattlefieldLocation{0}));
    EXPECT_TRUE(at(c, BaseLocation{P1}));
    int runes = 0;
    for (auto& [id, o] : s().objects)
        if (o.isRune() && o.controller == P1 && o.location.has_value()) ++runes;
    EXPECT_EQ(runes, 0) << "paid [A] by recycling the rune";
}

TEST_F(MoveGapTest, MageseekerInvestigator_SingleMovesAndOwnerAreFree) {
    makeEngine();
    unit(P2, kMageseeker, 0, 0);
    unit(P1, kMageseeker, 0, 1);
    engine_->testHook_cleanup();
    EXPECT_EQ(moveAppliedCost(s(), P1, BattlefieldLocation{0}, 1).power, 0) << "one unit";
    EXPECT_EQ(moveAppliedCost(s(), P1, BattlefieldLocation{0}, 3).power, 2);
    EXPECT_EQ(moveAppliedCost(s(), P2, BattlefieldLocation{0}, 3).power, 0) << "not an opponent";
    EXPECT_EQ(moveAppliedCost(s(), P1, BaseLocation{P1}, 3).power, 0);
}

TEST_F(MoveGapTest, MoveUnitsTogether_ChargesTheGroupOnce) {
    makeEngine();
    unit(P1, kMageseeker, 0, 0);
    gearAt(P1, kGravityField, 0);
    engine_->testHook_cleanup();
    auto x = unit(P2, kInvalidId, 2);
    auto y = unit(P2, kInvalidId, 2);
    a2_.pick = [](const std::vector<Intent>& l) { return pickValue(l, 1); };
    // [1][A] (Gravity Field) + [A] (one unit beyond the first) = 1 energy + 2 power:
    // two runes (one exhausted for [1], both recycled).
    rune(P2);
    resolvingAs(P2);
    EXPECT_TRUE(exec().moveUnitsTogether({x, y}, BattlefieldLocation{0}).empty()) << "can't pay";
    rune(P2);
    auto moved = exec().moveUnitsTogether({x, y}, BattlefieldLocation{0});
    EXPECT_EQ(moved.size(), 2u);
    EXPECT_TRUE(at(x, BattlefieldLocation{0}));
    EXPECT_TRUE(at(y, BattlefieldLocation{0}));
    s().chain.resuming.reset();
}

TEST_F(MoveGapTest, TricksyTentacles_MovesItsGroupAsOneMove) {
    makeEngine();
    unit(P2, kMageseeker, 0, 0);
    engine_->testHook_cleanup();
    auto x = unit(P2, kInvalidId, 2, 1);
    auto y = unit(P2, kInvalidId, 2, 1);
    // P1 moves P2's units to P2's base: no applied cost there.
    int call = 0;
    resolveWith(kTricksy, P1, {}, exec(), [&](int, const std::vector<Intent>& l) {
        ++call;
        if (call == 1) return *pickValue(l, 0);                 // their base
        if (auto i = pickObject(l, x)) return *i;
        if (auto i = pickObject(l, y)) return *i;
        return l.back();                                         // done
    });
    EXPECT_TRUE(at(x, BaseLocation{P2}));
    EXPECT_TRUE(at(y, BaseLocation{P2}));
}

// ═══ Void Assault: "you're the attacker" ════════════════════════════════════

TEST_F(MoveGapTest, VoidAssault_BothMoveToEnemyBattlefield_CasterAttacks) {
    makeEngine();
    s().battlefields[0].controller = P2;
    auto friendly = unit(P1, kInvalidId, 2);
    auto enemy = unit(P2, kInvalidId, 2, 1);
    resolveWith(kVoidAssault, P1, {friendly, enemy}, exec(),
                [&](int, const std::vector<Intent>& l) { return *pickValue(l, 1); });   // BF0
    EXPECT_TRUE(at(friendly, BattlefieldLocation{0}));
    EXPECT_TRUE(at(enemy, BattlefieldLocation{0}));
    EXPECT_TRUE(s().battlefields[0].is_contested);
    EXPECT_EQ(s().battlefields[0].contested_by, P1) << "the caster is the attacker";
    engine_->testHook_cleanup();
    EXPECT_EQ(s().battlefields[0].contested_by, P1);
}

TEST_F(MoveGapTest, VoidAssault_OwnBattlefieldOrBlockedMove_NoDesignation) {
    makeEngine();
    s().battlefields[0].controller = P1;
    auto friendly = unit(P1, kInvalidId, 2);
    auto enemy = unit(P2, kInvalidId, 2, 1);
    resolveWith(kVoidAssault, P1, {friendly, enemy}, exec(),
                [&](int, const std::vector<Intent>& l) { return *pickValue(l, 1); });
    EXPECT_FALSE(s().battlefields[0].is_contested && s().battlefields[0].contested_by == P1)
        << "a battlefield you control: not the attacker by this";

    makeEngine();
    auto f2 = unit(P1, kInvalidId, 2);
    auto e2 = unit(P2, kInvalidId, 2, 1);
    s().getObject(f2).forbidMovesThisTurn(s().turn.turn_number);   // the friendly can't move
    resolveWith(kVoidAssault, P1, {f2, e2}, exec(),
                [&](int, const std::vector<Intent>& l) { return *pickValue(l, 1); });
    EXPECT_TRUE(at(f2, BaseLocation{P1}));
    EXPECT_FALSE(s().battlefields[0].contested_by == P1 && at(f2, BattlefieldLocation{0}));
}

// ═══ Arise!: token play locations ═══════════════════════════════════════════

TEST_F(MoveGapTest, Arise_ControllerSplitsTokensBetweenBaseAndControlledBattlefields) {
    makeEngine();
    s().battlefields[0].controller = P1;
    for (int i = 0; i < 3; ++i) {
        auto g = gearAt(P1, kInvalidId);
        s().getObject(g).tags = {"Equipment"};
    }
    resolveWith(kArise, P1, {}, exec(), [&](int n, const std::vector<Intent>& l) {
        if (n == 0) {
            EXPECT_EQ(l.size(), 4u) << "0..3 at base";
            return *pickValue(l, 1);          // one at base, the rest at BF0
        }
        return *pickValue(l, 0);              // ready none
    });
    int at_base = 0, at_bf0 = 0;
    for (auto& [id, o] : s().objects) {
        if (o.name != "Sand Soldier" || !o.location.has_value()) continue;
        if (*o.location == LocationId{BaseLocation{P1}}) ++at_base;
        if (*o.location == LocationId{BattlefieldLocation{0}}) ++at_bf0;
    }
    EXPECT_EQ(at_base, 1);
    EXPECT_EQ(at_bf0, 2);
}

TEST_F(MoveGapTest, Arise_NoControlledBattlefield_AllToBaseWithoutAsking) {
    makeEngine();
    for (int i = 0; i < 2; ++i) {
        auto g = gearAt(P1, kInvalidId);
        s().getObject(g).tags = {"Equipment"};
    }
    int asked = 0;
    resolveWith(kArise, P1, {}, exec(), [&](int, const std::vector<Intent>& l) {
        ++asked;
        for (const auto& i : l) if (!i.chosen_objects.empty()) return i;   // ready one
        return l.back();
    });
    int at_base = 0;
    for (auto& [id, o] : s().objects)
        if (o.name == "Sand Soldier" && o.location == LocationId{BaseLocation{P1}}) ++at_base;
    EXPECT_EQ(at_base, 2);
    EXPECT_EQ(asked, 2) << "only the two ready picks";
}

// ═══ Last Caress: a face-down unit can land on any battlefield ══════════════

TEST_F(MoveGapTest, LastCaress_FaceDownUnitChoosesAnyBattlefield) {
    auto hidden = addUnit(P1, kInvalidId, 2);
    auto& h = state.getObject(hidden);
    h.location.reset();
    h.zone = ZoneType::FacedownZone;
    h.is_hidden = true;
    h.hidden_at = 0;
    state.battlefields[0].facedown.push_back(hidden);
    ChainManager cm(state, events, card_db);
    int asked = 0;
    auto pick_bf1 = [&](PlayerId, const std::vector<Intent>& l) {
        ++asked;
        EXPECT_EQ(l.size(), 2u);
        EXPECT_EQ(l.front().chosen_value.value_or(-1), 1) << "the hidden-at battlefield first";
        return l.back();
    };
    EXPECT_EQ(cm.hiddenUnitPlayDestination(hidden, P1, pick_bf1),
              LocationId{BattlefieldLocation{0}}) << "no permission: where it was hidden";
    EXPECT_EQ(asked, 0);

    EffectExecutor ex(state, events, card_db, &card_registry);
    auto src = state.createObject();
    invokeOnResolve(src, kLastCaress, P1, {}, ex);
    ASSERT_TRUE(state.player(P1).next_unit_any_battlefield);
    EXPECT_EQ(cm.hiddenUnitPlayDestination(hidden, P1, pick_bf1),
              LocationId{BattlefieldLocation{1}});
    EXPECT_EQ(asked, 1);
}

// ═══ Hunters' Circle: affordability per play location ═══════════════════════

TEST_F(MoveGapTest, HuntersCircle_OffersAPlayPayableOnlyWithTheDiscount) {
    makeEngine();
    bfCard(0, kHuntersCircle);
    unit(P1, kInvalidId, 1, 0);
    s().battlefields[0].controller = P1;
    engine_->testHook_cleanup();
    CardDefId two = kInvalidId;
    for (const auto& [id, d] : card_db.all())
        if (d.card_type == CardType::Unit && d.ability_text.empty() && d.energy_cost >= 1 &&
            d.energy_cost <= 3 &&
            d.power_cost == 0 && d.keywords.bits == 0 && d.super_type == SuperType::None &&
            (two == kInvalidId || id < two)) two = id;
    ASSERT_NE(two, kInvalidId);
    auto card = handCard(P1, two);
    s().player(P1).rune_pool.energy = card_db.get(two).energy_cost - 1;
    auto offered = [&](const LocationId& loc) {
        for (const auto& a : engine_->generateLegalActions())
            if (a.type == IntentType::PlayCard && a.card == card && a.play_location == loc)
                return true;
        return false;
    };
    EXPECT_TRUE(offered(BattlefieldLocation{0})) << "costs [1] there";
    EXPECT_FALSE(offered(BaseLocation{P1})) << "full price at base";
    s().battlefields[0].first_play_here_turn[playerIndex(P1)] = s().turn.turn_number;
    EXPECT_FALSE(offered(BattlefieldLocation{0})) << "discount already used this turn";
}

}  // namespace
}  // namespace riftbound::test
