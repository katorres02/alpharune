/// @file test_radiance_neeko.cpp
/// Radiance: Neeko, Blending In (1115) and the engine support it needs —
/// legends declared at deck building (CR 103.1, "Neeko Legends:" deck-list
/// section), several legends in the Legend Zone (CR 107.4.d.1), and a legend
/// that lasts only while Neeko is on the board (107.4.d.2).

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kNeeko = 1115;
constexpr CardDefId kLooseCannon = 249;    // legend: At start of your Beginning Phase, draw 1 if ≤1 card in hand
constexpr CardDefId kBlindMonk = 254;      // legend: [1], [E]: Buff a friendly unit
constexpr CardDefId kBountyHunter = 262;   // legend: [E]: Give a unit [Ganking] this turn
constexpr CardDefId kHearteningSoul = 1022;

std::string deckRoot() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return root ? std::string(root) : std::string(".");
}

/// Answers MakeChoice prompts with the scripted chosen_values in order (and
/// records what was offered); passes priority otherwise.
class NeekoAgent : public AgentInterface {
public:
    std::vector<int> script;
    size_t at = 0;
    std::vector<std::vector<int>> offered;   // chosen_values per prompt
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        std::vector<int> vals;
        for (const auto& i : legal)
            if (i.type == IntentType::MakeChoice && i.chosen_value) vals.push_back(*i.chosen_value);
        if (vals.empty()) {
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
        offered.push_back(vals);
        if (at < script.size()) {
            int want = script[at++];
            for (const auto& i : legal)
                if (i.type == IntentType::MakeChoice && i.chosen_value == want) return i;
        }
        return legal.front();
    }
};

class NeekoDeckTest : public CardTestFixture {
protected:
    DeckSubmission ahriDeck() {
        return DeckValidator::loadFromDeckList(deckRoot() + "/decks/ahri_test.txt", card_db);
    }
    /// The Ahri deck with one main-deck card swapped for Neeko.
    DeckSubmission neekoDeck() {
        auto d = ahriDeck();
        d.main_deck.back() = kNeeko;
        return d;
    }
    bool hasError(const ValidationResult& r, const std::string& needle) {
        for (const auto& e : r.errors) if (e.find(needle) != std::string::npos) return true;
        return false;
    }
};

class NeekoEngineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    NeekoAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    EffectExecutor& ex() { return engine_->testHook_executor(); }

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
        st.turn.turn_number = 3;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        for (PlayerId p : {P1, P2}) {   // blank Champion Legends
            auto lid = st.createObject();
            auto& lo = st.getObject(lid);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.name = "Champion Legend";
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = lid;
        }
        st.player(P1).additional_legend_pool = {kBountyHunter, kLooseCannon, kBlindMonk};
    }
    void fill(GameObject& o, PlayerId p, CardDefId def_id) {
        o.owner = o.controller = p;
        o.card_def_id = def_id;
        if (def_id == kInvalidId) { o.name = "TestUnit"; o.card_type = CardType::Unit; return; }
        const auto& d = card_db.get(def_id);
        o.name = d.name;
        o.card_type = d.card_type;
        o.super_type = d.super_type;
        o.domains = d.domains;
        o.tags = d.tags;
        o.keywords = d.keywords;
        o.base_might = o.current_might = d.might;
    }
    GameObjectId unit(PlayerId p, CardDefId def_id, int might = 0) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        fill(u, p, def_id);
        if (might > 0) u.base_might = u.current_might = might;
        u.zone = ZoneType::Base;
        u.location = BaseLocation{p};
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        fill(s().getObject(id), p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    void playNeeko(GameObjectId neeko) {
        s().player(P1).rune_pool.energy = 6;
        s().player(P1).rune_pool.universal_power = 1;
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = P1;
        i.card = neeko;
        i.play_location = LocationId{BaseLocation{P1}};
        engine_->testHook_executeIntent(i);
    }
};

}  // namespace

// ── Card definition ─────────────────────────────────────────────────────────
TEST_F(NeekoDeckTest, NeekoDefinition) {
    const auto& d = card_db.get(kNeeko);
    EXPECT_EQ(d.name, "Neeko, Blending In");
    EXPECT_EQ(d.card_type, CardType::Unit);
    EXPECT_EQ(d.super_type, SuperType::Champion);
    EXPECT_TRUE(d.domains.empty());
    EXPECT_EQ(d.energy_cost, 6);
    EXPECT_EQ(d.power_cost, 1);
    EXPECT_EQ(d.might, 4);
    EXPECT_EQ(d.additional_legend_choices, 3);
    EXPECT_EQ(d.def_id, "rad-167-167");
}

// ── Deck list parsing ───────────────────────────────────────────────────────
TEST_F(NeekoDeckTest, DeckListParsesNeekoLegendsSection) {
    std::ifstream in(deckRoot() + "/decks/ahri_test.txt");
    ASSERT_TRUE(in.is_open());
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    text += "\nNeeko Legends:\n1 Bounty Hunter\n1 Loose Cannon\n1 Blind Monk\n";
    auto path = std::filesystem::path(::testing::TempDir()) / "neeko_deck_parse.txt";
    { std::ofstream out(path); out << text; }
    auto deck = DeckValidator::loadFromDeckList(path.string(), card_db);
    ASSERT_EQ(deck.additional_legends.size(), 3u);
    EXPECT_EQ(deck.additional_legends[0], kBountyHunter);
    EXPECT_EQ(deck.additional_legends[1], kLooseCannon);
    EXPECT_EQ(deck.additional_legends[2], kBlindMonk);

    // A legend is listed once.
    { std::ofstream out(path); out << text << "2 Purifier\n"; }
    EXPECT_THROW(DeckValidator::loadFromDeckList(path.string(), card_db), std::runtime_error);
    std::filesystem::remove(path);
}

// ── Deck validation ─────────────────────────────────────────────────────────
TEST_F(NeekoDeckTest, NeekoDeckWithThreeDifferentLegendsIsLegal) {
    DeckValidator v(card_db);
    ASSERT_TRUE(v.validate(neekoDeck()).is_legal) << "baseline deck must be legal";
    auto d = neekoDeck();
    d.additional_legends = {kBountyHunter, kLooseCannon, kBlindMonk};
    auto r = v.validate(d);
    EXPECT_TRUE(r.is_legal) << (r.errors.empty() ? "" : r.errors.front());
    d.additional_legends = {kBountyHunter};   // "up to 3" (errata)
    EXPECT_TRUE(v.validate(d).is_legal);
}

TEST_F(NeekoDeckTest, NeekoInTheSideboardAlsoAllowsLegends) {
    DeckValidator v(card_db);
    auto d = ahriDeck();
    d.sideboard.push_back(kNeeko);
    d.additional_legends = {kBountyHunter, kLooseCannon};
    auto r = v.validate(d);
    EXPECT_TRUE(r.is_legal) << (r.errors.empty() ? "" : r.errors.front());
}

TEST_F(NeekoDeckTest, LegendsWithoutNeekoAreRejected) {
    DeckValidator v(card_db);
    auto d = ahriDeck();
    d.additional_legends = {kBountyHunter};
    auto r = v.validate(d);
    EXPECT_FALSE(r.is_legal);
    EXPECT_TRUE(hasError(r, "Neeko"));
}

TEST_F(NeekoDeckTest, MoreThanThreeLegendsAreRejected) {
    DeckValidator v(card_db);
    auto d = neekoDeck();
    d.additional_legends = {kBountyHunter, kLooseCannon, kBlindMonk, 502 /* Purifier */};
    auto r = v.validate(d);
    EXPECT_FALSE(r.is_legal);
    EXPECT_TRUE(hasError(r, "Too many additional legends"));
}

TEST_F(NeekoDeckTest, LegendNamesMustDifferFromEachOtherAndTheStartingLegend) {
    DeckValidator v(card_db);
    auto d = neekoDeck();
    d.additional_legends = {kBountyHunter, kBountyHunter};
    auto r = v.validate(d);
    EXPECT_FALSE(r.is_legal);
    EXPECT_TRUE(hasError(r, "Duplicate additional legend"));

    d.additional_legends = {d.legend};
    r = v.validate(d);
    EXPECT_FALSE(r.is_legal);
    EXPECT_TRUE(hasError(r, "same name as the starting legend"));

    d.additional_legends = {kNeeko};   // not a legend
    r = v.validate(d);
    EXPECT_FALSE(r.is_legal);
    EXPECT_TRUE(hasError(r, "not a legend"));
}

TEST_F(NeekoDeckTest, SetupStoresTheDeclaredLegendsWithoutCreatingThem) {
    auto d1 = neekoDeck();
    d1.additional_legends = {kBountyHunter, kLooseCannon, kBlindMonk};
    auto d2 = ahriDeck();
    GameEngine engine(card_db, events, card_registry);
    engine.beginGame(d1, d2, 7);
    const auto& st = engine.state();
    EXPECT_EQ(st.player(PlayerId::Player1).additional_legend_pool, d1.additional_legends);
    EXPECT_TRUE(st.player(PlayerId::Player1).extra_legends.empty());
    EXPECT_TRUE(st.player(PlayerId::Player2).additional_legend_pool.empty());
    for (const auto& [id, o] : st.objects)
        EXPECT_NE(o.card_def_id, kBountyHunter) << "the legends don't exist until Neeko adds one";
}

// ── Playing Neeko ───────────────────────────────────────────────────────────
TEST_F(NeekoEngineTest, PlayingNeekoAddsTheChosenLegendReady) {
    makeEngine();
    auto neeko = inHand(P1, kNeeko);
    a1_.script = {1};   // Loose Cannon
    playNeeko(neeko);
    ASSERT_TRUE(s().getObject(neeko).location.has_value());
    ASSERT_EQ(a1_.offered.size(), 1u);
    EXPECT_EQ(a1_.offered[0], (std::vector<int>{0, 1, 2}));
    const auto& extras = s().player(P1).extra_legends;
    ASSERT_EQ(extras.size(), 1u);
    const auto& l = s().getObject(extras[0]);
    EXPECT_EQ(l.card_def_id, kLooseCannon);
    EXPECT_EQ(l.zone, ZoneType::LegendZone);
    EXPECT_EQ(l.controller, P1);
    EXPECT_TRUE(l.isLegend());
    EXPECT_FALSE(l.is_exhausted) << "the legends enter ready";
    EXPECT_EQ(s().player(P1).legends().size(), 2u);
    EXPECT_NE(s().player(P1).legend_zone, extras[0]) << "the Champion Legend keeps its slot";
}

TEST_F(NeekoEngineTest, ALegendAlreadyInTheZoneCantBeChosenAgain) {
    makeEngine();
    auto first = unit(P1, kNeeko);
    ex().addLegendToZone(P1, kBountyHunter, first);
    auto neeko = inHand(P1, kNeeko);
    a1_.script = {2};
    playNeeko(neeko);
    ASSERT_EQ(a1_.offered.size(), 1u);
    EXPECT_EQ(a1_.offered[0], (std::vector<int>{1, 2})) << "Bounty Hunter is already there";
    const auto& extras = s().player(P1).extra_legends;
    ASSERT_EQ(extras.size(), 2u);
    EXPECT_EQ(s().getObject(extras[1]).card_def_id, kBlindMonk);
}

TEST_F(NeekoEngineTest, NoDeclaredLegendsAddsNothing) {
    makeEngine();
    s().player(P1).additional_legend_pool.clear();
    auto neeko = inHand(P1, kNeeko);
    playNeeko(neeko);
    ASSERT_TRUE(s().getObject(neeko).location.has_value());
    EXPECT_TRUE(s().player(P1).extra_legends.empty());
    EXPECT_TRUE(a1_.offered.empty());
}

TEST_F(NeekoEngineTest, TheLegendCeasesToExistWhenNeekoLeavesTheBoard) {
    makeEngine();
    auto neeko = inHand(P1, kNeeko);
    a1_.script = {0};
    playNeeko(neeko);
    ASSERT_EQ(s().player(P1).extra_legends.size(), 1u);
    auto legend = s().player(P1).extra_legends[0];
    ex().bounceToHand(neeko);
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().player(P1).extra_legends.empty());
    EXPECT_NE(s().getObject(legend).zone, ZoneType::LegendZone);
    const auto& ban = s().player(P1).banishment;
    EXPECT_EQ(std::count(ban.begin(), ban.end(), legend), 0) << "ceases to exist";
    EXPECT_NE(s().player(P1).legend_zone, kInvalidId) << "the Champion Legend stays";
    // Neeko's legends can be added again by the next Neeko.
    EXPECT_EQ(s().player(P1).additional_legend_pool.size(), 3u);
}

// ── An added legend works like a legend ─────────────────────────────────────
TEST_F(NeekoEngineTest, AddedLegendOffersItsExhaustAbilityAndReadiesInAwaken) {
    makeEngine();
    auto neeko = unit(P1, kNeeko);
    auto legend = ex().addLegendToZone(P1, kBountyHunter, neeko);
    auto ally = unit(P1, kInvalidId, 3);
    engine_->testHook_cleanup();

    std::optional<Intent> act;
    for (const auto& a : engine_->generateLegalActions())
        if (a.type == IntentType::ActivateAbility && a.ability_source == legend) act = a;
    ASSERT_TRUE(act.has_value()) << "the added legend's [E] ability is offered";

    act->targets = {ally};
    engine_->testHook_executeIntent(*act);
    EXPECT_TRUE(s().getObject(legend).is_exhausted);
    EXPECT_TRUE(s().getObject(ally).keywords.has(Keyword::Ganking));

    engine_->testHook_awakenPhase();
    EXPECT_FALSE(s().getObject(legend).is_exhausted) << "readied with the turn player's legends";
}

TEST_F(NeekoEngineTest, AddedLegendsTriggeredAbilityFires) {
    makeEngine();
    auto neeko = unit(P1, kNeeko);
    ex().addLegendToZone(P1, kLooseCannon, neeko);
    auto top = s().createObject();
    fill(s().getObject(top), P1, kInvalidId);
    s().getObject(top).zone = ZoneType::MainDeck;
    s().player(P1).main_deck.push_back(top);
    ASSERT_TRUE(s().player(P1).hand.empty());
    engine_->testHook_beginningStep();
    bool drew = std::count(s().player(P1).hand.begin(), s().player(P1).hand.end(), top) == 1;
    bool queued = false;
    for (const auto& ci : s().chain.items)
        if (ci.card_def_id == kLooseCannon) queued = true;
    EXPECT_TRUE(drew || queued) << "Loose Cannon's start-of-Beginning trigger fired";
}

TEST_F(NeekoEngineTest, FriendlyLegendGrantsReachAddedLegends) {
    makeEngine();
    auto neeko = unit(P1, kNeeko);
    auto legend = ex().addLegendToZone(P1, kBlindMonk, neeko);
    unit(P1, kHearteningSoul);
    engine_->testHook_cleanup();
    for (auto lid : s().player(P1).legends()) {
        bool granted = false;
        for (const auto& g : s().getObject(lid).granted_abilities)
            if (g.source_def_id == kHearteningSoul) granted = true;
        EXPECT_TRUE(granted) << "legend " << lid << (lid == legend ? " (added)" : " (champion)");
    }
}
