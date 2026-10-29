/// @file test_gap_addcosts.cpp
/// Object additional costs decided and paid as a card is played
/// (Card::OptionalAdditionalCost::object_cost, GameEngine::payObjectAdditionalCost):
/// "kill a friendly unit", "return a friendly unit to hand", "spend a buff" —
/// mandatory or optional, one object or any number, with a per-object
/// discount or an ignored base cost. Cards: Wallop (146), Kraken Hunter (150),
/// Call to Glory (207), Cruel Patron (208), Commander Ledros (231),
/// Stalking Wolf (728), Atakhan (732), Ekko, Ingenious (1041).

#include "tests/cards/card_test_fixture.h"

#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>

namespace riftbound::test {
namespace {

constexpr CardDefId kWallop = 146, kKrakenHunter = 150, kCallToGlory = 207,
    kCruelPatron = 208, kLedros = 231, kStalkingWolf = 728, kEkko = 1041;

class ScriptAgent : public AgentInterface {
public:
    std::function<std::optional<Intent>(const std::vector<Intent>&)> pick;
    std::vector<std::vector<Intent>> choices;   // MakeChoice decisions seen
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (!legal.empty() && legal.front().type == IntentType::MakeChoice) choices.push_back(legal);
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
std::optional<Intent> chooseStop(const std::vector<Intent>& legal) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.chosen_objects.empty() &&
            i.chosen_value.value_or(-1) == 0) return i;
    return std::nullopt;
}
bool offersStop(const std::vector<Intent>& legal) { return chooseStop(legal).has_value(); }
bool offersObject(const std::vector<Intent>& legal, GameObjectId id) {
    return chooseObject(legal, id).has_value();
}

class GapAddCostsTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    ScriptAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }

    void SetUp() override {
        CardTestFixture::SetUp();
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
    GameObjectId unit(PlayerId p, CardDefId def_id = kInvalidId, int at_bf = -1,
                      std::vector<std::string> tags = {}) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            u.name = d.name; u.tags = d.tags; u.keywords = d.keywords; u.domains = d.domains;
            u.base_might = d.might;
        } else {
            u.name = "TestUnit";
            u.base_might = 2;
        }
        for (auto& t : tags) u.tags.push_back(t);
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
    GameObjectId buffed(PlayerId p) {
        auto id = unit(p);
        auto& u = s().getObject(id);
        u.buff_count = 1;
        u.recomputeMight();
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
    void pool(PlayerId p, int energy, Domain d = Domain::Fury, int power = 0) {
        s().player(p).rune_pool.energy = energy;
        s().player(p).rune_pool.power[static_cast<int>(d)] = power;
    }
    void play(PlayerId p, GameObjectId card, std::optional<LocationId> where = std::nullopt,
              std::vector<GameObjectId> targets = {}) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = where;
        i.targets = std::move(targets);
        engine_->testHook_executeIntent(i);
    }
    bool onBoard(GameObjectId id) { return s().getObject(id).location.has_value(); }
    bool inHandOf(PlayerId p, GameObjectId id) {
        const auto& h = s().player(p).hand;
        return std::find(h.begin(), h.end(), id) != h.end();
    }
};

// ── Mechanism: an optional cost never makes a card unplayable, a mandatory one does ──
TEST_F(GapAddCostsTest, MandatoryObjectCostGatesPlayability) {
    pool(P1, 9);
    auto patron = handCard(P1, kCruelPatron);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, patron)) << "no friendly unit to kill";
    unit(P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, patron));
    // An optional object cost: Kraken Hunter stays playable with no buffs.
    pool(P1, 3, Domain::Body, 2);
    auto kraken = handCard(P1, kKrakenHunter);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, kraken));
}

// ── 208 Cruel Patron: the kill is paid as it's played, before it lands ──
TEST_F(GapAddCostsTest, CruelPatron_KillsAsAdditionalCost) {
    pool(P1, 4);
    auto a = unit(P1), b = unit(P1);
    auto patron = handCard(P1, kCruelPatron);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, b); };
    play(P1, patron);
    ASSERT_EQ(a1_.choices.size(), 1u);
    EXPECT_FALSE(offersStop(a1_.choices[0])) << "a mandatory cost can't be declined";
    EXPECT_TRUE(offersObject(a1_.choices[0], a));
    EXPECT_TRUE(onBoard(a));
    EXPECT_FALSE(onBoard(b)) << "killed as the cost";
    EXPECT_TRUE(onBoard(patron));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_TRUE(s().chain.items.empty()) << "no play trigger: the kill is a cost";
}

// ── 231 Commander Ledros: any number of kills, [Y] less each ──
TEST_F(GapAddCostsTest, Ledros_KillsAnyNumberForPower) {
    pool(P1, 6, Domain::Order, 2);
    auto ledros = handCard(P1, kLedros);
    auto a = unit(P1);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, ledros)) << "needs 2 kills: only 1 unit";
    auto b = unit(P1), c = unit(P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, ledros));
    std::vector<GameObjectId> order = {a, b};
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        for (auto o : order) if (auto i = chooseObject(l, o)) return i;
        return chooseStop(l);
    };
    play(P1, ledros);
    ASSERT_EQ(a1_.choices.size(), 3u);
    EXPECT_FALSE(offersStop(a1_.choices[0])) << "4 [Y] isn't payable yet";
    EXPECT_FALSE(offersStop(a1_.choices[1]));
    EXPECT_TRUE(offersStop(a1_.choices[2])) << "two kills cover it";
    EXPECT_FALSE(onBoard(a));
    EXPECT_FALSE(onBoard(b));
    EXPECT_TRUE(onBoard(c));
    EXPECT_TRUE(onBoard(ledros));
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)], 0);
}

TEST_F(GapAddCostsTest, Ledros_MayDeclineWhenPayable) {
    pool(P1, 6, Domain::Order, 4);
    auto a = unit(P1);
    auto ledros = handCard(P1, kLedros);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseStop(l); };
    play(P1, ledros);
    ASSERT_EQ(a1_.choices.size(), 1u);
    EXPECT_TRUE(offersObject(a1_.choices[0], a));
    EXPECT_TRUE(onBoard(a));
    EXPECT_TRUE(onBoard(ledros));
}

// ── 150 Kraken Hunter: spend any number of buffs, [O] less each ──
TEST_F(GapAddCostsTest, KrakenHunter_SpendsBuffsForPower) {
    pool(P1, 3);
    auto kraken = handCard(P1, kKrakenHunter);
    auto x = buffed(P1);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, kraken)) << "2 [O] owed, one buff";
    auto y = buffed(P1);
    unit(P1);   // no buff: not a candidate
    EXPECT_TRUE(engine_->testHook_canAfford(P1, kraken));
    play(P1, kraken, BaseLocation{P1});
    ASSERT_EQ(a1_.choices.size(), 2u);
    EXPECT_EQ(a1_.choices[0].size(), 2u) << "only the buffed units, no stop";
    EXPECT_EQ(s().getObject(x).buff_count, 0);
    EXPECT_EQ(s().getObject(y).buff_count, 0);
    EXPECT_EQ(s().getObject(x).current_might, 2);
    EXPECT_TRUE(onBoard(kraken));
}

// ── 146 Wallop: spend a buff to ignore the spell's cost ──
TEST_F(GapAddCostsTest, Wallop_SpendBuffIgnoresCost) {
    auto target = unit(P1);
    s().getObject(target).is_exhausted = true;
    auto wallop = handCard(P1, kWallop);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, wallop)) << "no Energy, no buff";
    auto b = buffed(P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, wallop)) << "the buff pays for it";
    play(P1, wallop, std::nullopt, {target});
    ASSERT_FALSE(a1_.choices.empty());
    EXPECT_FALSE(offersStop(a1_.choices[0])) << "the printed cost isn't payable";
    EXPECT_EQ(s().getObject(b).buff_count, 0);
    EXPECT_FALSE(s().getObject(target).is_exhausted) << "Wallop resolved";
}

// ── 207 Call to Glory: the buff spend is optional when the cost is payable ──
TEST_F(GapAddCostsTest, CallToGlory_OptionalBuffSpend) {
    pool(P1, 3);
    auto target = unit(P1);
    auto b = buffed(P1);
    auto ctg = handCard(P1, kCallToGlory);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, b); };
    play(P1, ctg, std::nullopt, {target});
    ASSERT_FALSE(a1_.choices.empty());
    EXPECT_TRUE(offersStop(a1_.choices[0]));
    EXPECT_EQ(s().getObject(b).buff_count, 0);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 3) << "cost ignored";

    // Declined: the printed cost is paid and the buff stays.
    auto ctg2 = handCard(P1, kCallToGlory);
    auto b2 = buffed(P1);
    a1_.choices.clear();
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseStop(l); };
    play(P1, ctg2, std::nullopt, {target});
    EXPECT_EQ(s().getObject(b2).buff_count, 1);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
}

// ── 1041 Ekko, Ingenious: return a friendly unit as he's played ──
TEST_F(GapAddCostsTest, Ekko_ReturnsUnitAsAdditionalCost) {
    pool(P1, 3);
    auto ekko = handCard(P1, kEkko);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, ekko));
    auto other = unit(P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, ekko));
    play(P1, ekko);
    ASSERT_EQ(a1_.choices.size(), 1u);
    EXPECT_FALSE(offersStop(a1_.choices[0]));
    EXPECT_TRUE(inHandOf(P1, other));
    EXPECT_TRUE(onBoard(ekko));
}

// ── 728 Stalking Wolf: kill prey as the cost; Ambushed "to its battlefield" ──
TEST_F(GapAddCostsTest, StalkingWolf_KillsPreyAtAmbushBattlefield) {
    pool(P1, 4, Domain::Order, 1);
    auto wolf = handCard(P1, kStalkingWolf);
    unit(P1);   // not prey
    EXPECT_FALSE(engine_->testHook_canAfford(P1, wolf)) << "no Bird/Cat/Dog/Poro";
    auto cat_base = unit(P1, kInvalidId, -1, {"Cat"});
    auto cat_here = unit(P1, kInvalidId, 0, {"Cat"});
    EXPECT_TRUE(engine_->testHook_canAfford(P1, wolf));
    play(P1, wolf, BattlefieldLocation{0});
    ASSERT_EQ(a1_.choices.size(), 1u);
    EXPECT_TRUE(offersObject(a1_.choices[0], cat_here));
    EXPECT_FALSE(offersObject(a1_.choices[0], cat_base)) << "Ambushed here thanks to its battlefield";
    EXPECT_FALSE(onBoard(cat_here));
    EXPECT_TRUE(onBoard(cat_base));
    ASSERT_TRUE(s().getObject(wolf).battlefieldId().has_value());
    EXPECT_EQ(*s().getObject(wolf).battlefieldId(), 0u);
}

}  // namespace
}  // namespace riftbound::test
