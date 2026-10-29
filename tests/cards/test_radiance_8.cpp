/// @file test_radiance_8.cpp
/// Radiance batch 8: Dockside Lock-Up, Mordekaiser Dominating, Seraphine Not
/// Alone, and the battlefields 1116-1123.

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
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

constexpr CardDefId kLockUp = 1112, kMordekaiser = 1113, kSeraphine = 1114, kBandle = 1116,
                    kBlackMarket = 1117, kBlackRose = 1118, kCosmicVista = 1119,
                    kDurand = 1120, kFirelights = 1121, kAmphitheater = 1122,
                    kRuinedMonastery = 1123;

Intent pickObj(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
    ADD_FAILURE() << "object " << id << " not offered";
    return legal.front();
}
bool offered(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return true;
    return false;
}
bool isYesNo(const std::vector<Intent>& legal) {
    return legal.size() == 2 && legal[1].chosen_value.has_value() && *legal[1].chosen_value == 1 &&
           legal[0].chosen_objects.empty();
}

class LambdaAgent : public AgentInterface {
public:
    std::function<std::optional<Intent>(const std::vector<Intent>&)> pick;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (pick) if (auto i = pick(legal)) return *i;
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

class Radiance8 : public CardTestFixture {
protected:
    EffectExecutor exec{state, events, card_db, &card_registry};

    GameObjectId addGear(PlayerId owner, CardDefId def, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_def_id = def;
        g.card_type = CardType::Gear;
        g.name = def != kInvalidId ? card_db.get(def).name : "TestGear";
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    // Make battlefield `bf` the given battlefield card.
    GameObjectId setBattlefieldCard(GameState& st, BattlefieldId bf, CardDefId def) {
        auto id = st.createObject();
        auto& o = st.getObject(id);
        o.card_def_id = def;
        o.card_type = CardType::Battlefield;
        o.name = card_db.get(def).name;
        o.zone = ZoneType::BattlefieldZone;
        o.owner = o.controller = P1;
        st.battlefields[bf].card_object_id = id;
        return id;
    }
    int chainItemsFrom(GameObjectId source, TriggerType t) {
        int n = 0;
        for (const auto& item : state.chain.items)
            if (item.source == source && item.fired_trigger == t) ++n;
        return n;
    }
    int unitsNamed(PlayerId p, const std::string& name) {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == p && o.location.has_value() && o.name == name) ++n;
        return n;
    }
    GameObjectId addRuneToDeck(PlayerId p, Domain d) {
        auto id = state.createObject();
        auto& r = state.getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::RuneDeck;
        state.player(p).rune_deck.push_back(id);
        return id;
    }
};

// ── 1112 Dockside Lock-Up ───────────────────────────────────────────────────
TEST_F(Radiance8, DocksideLockUpBanishesAUnitHereThenPlaysItOnHold) {
    const Card* c = card_registry.get(kLockUp);
    ASSERT_TRUE(c->hasDeploy());
    auto gear = addGear(P1, kLockUp, 0);
    auto foe = addUnit(P2, kMordekaiser, 0, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 3, 1);
    driveResumableTrigger(kLockUp, P1, gear, [&](const std::vector<Intent>& l) {
        EXPECT_FALSE(offered(l, elsewhere)) << "only units here";
        return pickObj(l, foe);
    }, exec, TriggerType::WhenYouPlayThis);
    ASSERT_EQ(state.getObject(foe).zone, ZoneType::Banishment);
    auto& ban = state.player(P2).banishment;
    ASSERT_NE(std::find(ban.begin(), ban.end(), foe), ban.end());

    // The hold ability triggers once, for its controller only.
    {
        ChainManager cm(state, events, card_db);
        TriggerManager tm(state, events, card_db, cm, card_registry);
        tm.subscribe();
        events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 1});
        EXPECT_EQ(chainItemsFrom(gear, TriggerType::WhenYouHoldHere), 1);
        state.chain.items.clear();
    }

    // Kill this to play the banished card: P1 now controls P2's Mordekaiser.
    driveResumableTrigger(kLockUp, P1, gear, [&](const std::vector<Intent>& l) {
        if (isYesNo(l)) return l[1];
        return l.front();
    }, exec, TriggerType::WhenYouHoldHere);
    EXPECT_FALSE(state.getObject(gear).location.has_value()) << "killed as the cost";
    const auto& m = state.getObject(foe);
    EXPECT_TRUE(m.location.has_value());
    EXPECT_EQ(m.controller, P1);
    EXPECT_EQ(std::find(ban.begin(), ban.end(), foe), ban.end());
}

TEST_F(Radiance8, DocksideLockUpBanishingATokenLeavesNothingToPlay) {
    auto gear = addGear(P1, kLockUp, 0);
    auto tok = addUnit(P2, kInvalidId, 1, 0);
    state.getObject(tok).is_token = true;
    driveResumableTrigger(kLockUp, P1, gear,
                          [&](const std::vector<Intent>& l) { return pickObj(l, tok); }, exec,
                          TriggerType::WhenYouPlayThis);
    bool asked = false;
    driveResumableTrigger(kLockUp, P1, gear, [&](const std::vector<Intent>& l) {
        asked = true;
        return l.front();
    }, exec, TriggerType::WhenYouHoldHere);
    EXPECT_FALSE(asked) << "no banished card: the cost isn't offered";
    EXPECT_TRUE(state.getObject(gear).location.has_value());
}

// ── 1113 Mordekaiser, Dominating ────────────────────────────────────────────
TEST_F(Radiance8, MordekaiserDiscardsOneToKillAUnitOrGear) {
    const auto& d = card_db.get(kMordekaiser);
    EXPECT_EQ(d.super_type, SuperType::Champion);
    EXPECT_EQ(d.might, 8);
    auto me = addUnit(P1, kMordekaiser, 0, 0);
    auto foe_gear = addGear(P2, kInvalidId, -1);
    auto junk = addToHand(P1, kSeraphine);
    driveResumableTrigger(kMordekaiser, P1, me, [&](const std::vector<Intent>& l) {
        if (isYesNo(l)) return l[1];
        if (offered(l, foe_gear)) return pickObj(l, foe_gear);
        return pickObj(l, junk);
    }, exec, TriggerType::WhenIAttack);
    EXPECT_TRUE(inTrash(P1, junk));
    EXPECT_FALSE(state.getObject(foe_gear).location.has_value()) << "gear killed";
}

TEST_F(Radiance8, MordekaiserNeedsACardToDiscard) {
    auto me = addUnit(P1, kMordekaiser, 0, 0);
    auto foe = addUnit(P2, kInvalidId, 3, 0);
    bool asked = false;
    driveResumableTrigger(kMordekaiser, P1, me, [&](const std::vector<Intent>& l) {
        asked = true;
        return l.front();
    }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(asked);
    EXPECT_TRUE(state.getObject(foe).location.has_value());
}

// ── 1114 Seraphine, Not Alone ───────────────────────────────────────────────
TEST_F(Radiance8, SeraphineMakesARecruitAndGrowsWithExhaustedAllies) {
    auto sera = addUnit(P1, kSeraphine, 0, -1);
    fireTriggerAs(kSeraphine, P1, sera, TriggerType::WhenIBecomeExhausted, exec);
    EXPECT_EQ(unitsNamed(P1, "Recruit"), 1);
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto foe = addUnit(P2, kInvalidId, 2);
    state.getObject(a).is_exhausted = true;
    state.getObject(b).is_exhausted = true;
    state.getObject(foe).is_exhausted = true;
    state.getObject(sera).is_exhausted = true;   // "other" units only
    card_registry.get(kSeraphine)->applyPassiveAura(state, P1);
    int bonus = 0;
    for (const auto& ae : state.getObject(sera).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 3) << "two exhausted allies + the Recruit (tokens enter exhausted)";
}

// ── 1116 Bandle Scouts' Academy ─────────────────────────────────────────────
TEST_F(Radiance8, BandleScoutsAcademyGivesGoldWhenYouHideHere) {
    auto bf = setBattlefieldCard(state, 0, kBandle);
    auto hidden = addToHand(P2, kSeraphine);
    state.getObject(hidden).hidden_at = 0;
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(CardHiddenEvent{hidden, P2});
    ASSERT_EQ(chainItemsFrom(bf, TriggerType::WhenYouHideACard), 1);
    EXPECT_EQ(state.chain.items.back().controller, P2);
    fireTriggerAs(kBandle, P2, bf, TriggerType::WhenYouHideACard, exec);
    int gold = 0;
    for (const auto& [id, o] : state.objects)
        if (o.isGear() && o.controller == P2 && o.name == "Gold") {
            ++gold;
            EXPECT_TRUE(o.is_exhausted);
        }
    EXPECT_EQ(gold, 1);
}

// ── 1117 Black Market ───────────────────────────────────────────────────────
TEST_F(Radiance8, BlackMarketAnyPlayerOncePerTheirTurn) {
    auto engine = std::make_unique<GameEngine>(card_db, events, card_registry);
    LambdaAgent a1, a2;
    auto& st = engine->mutableState();
    st.mode = ModeOfPlay{};
    st.players[0].id = P1;
    st.players[1].id = P2;
    for (int i = 0; i < 2; ++i) {
        BattlefieldState b;
        b.id = i;
        st.battlefields.push_back(b);
    }
    engine->testHook_setAgents(&a1, &a2);
    st.turn.turn_player = P1;
    st.turn.phase = TurnPhase::MainPhase;
    st.turn.ns_state = NeutralShowdownState::Neutral;
    st.turn.oc_state = OpenClosedState::Open;
    auto bm = setBattlefieldCard(st, 0, kBlackMarket);
    st.battlefields[0].controller = P2;   // P1 doesn't control it
    for (int i = 0; i < 2; ++i) {
        auto r = st.createObject();
        auto& o = st.getObject(r);
        o.owner = o.controller = P1;
        o.card_type = CardType::Rune;
        o.domains = {Domain::Order};
        o.zone = ZoneType::Base;
        o.location = BaseLocation{P1};
    }
    for (int i = 0; i < 3; ++i) {
        auto c = st.createObject();
        auto& o = st.getObject(c);
        o.owner = o.controller = P1;
        o.card_type = CardType::Spell;
        o.zone = ZoneType::MainDeck;
        st.player(P1).main_deck.push_back(c);
    }
    auto find_activation = [&]() -> std::optional<Intent> {
        for (const auto& a : engine->generateLegalActions())
            if (a.type == IntentType::ActivateAbility && a.ability_source == bm) return a;
        return std::nullopt;
    };
    auto act = find_activation();
    ASSERT_TRUE(act.has_value()) << "offered to a player who doesn't control it";
    const size_t hand_before = st.player(P1).hand.size();
    engine->testHook_executeIntent(*act);
    EXPECT_EQ(st.player(P1).hand.size(), hand_before + 1);
    EXPECT_FALSE(find_activation().has_value()) << "once each turn";
    EXPECT_FALSE(card_registry.get(kBlackMarket)->canActivateAbility(st, P2))
        << "only on their own turn";
}

// ── 1118 Black Rose Sanctum ─────────────────────────────────────────────────
TEST_F(Radiance8, BlackRoseSanctumFirstNonTokenDeathHereEachTurn) {
    auto bf = setBattlefieldCard(state, 0, kBlackRose);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto tok = addUnit(P2, kInvalidId, 1, 0);
    state.getObject(tok).is_token = true;
    events.emit(UnitDiedEvent{tok, P2, BattlefieldLocation{0}, 1});
    EXPECT_EQ(chainItemsFrom(bf, TriggerType::WhenAFriendlyUnitDies), 0) << "tokens don't count";
    auto elsewhere = addUnit(P2, kInvalidId, 1, 1);
    events.emit(UnitDiedEvent{elsewhere, P2, BattlefieldLocation{1}, 1});
    EXPECT_EQ(chainItemsFrom(bf, TriggerType::WhenAFriendlyUnitDies), 0) << "not here";
    auto u = addUnit(P2, kInvalidId, 2, 0);
    events.emit(UnitDiedEvent{u, P2, BattlefieldLocation{0}, 2});
    ASSERT_EQ(chainItemsFrom(bf, TriggerType::WhenAFriendlyUnitDies), 1);
    EXPECT_EQ(state.chain.items.back().controller, P2);
    state.chain.items.clear();
    fireTriggerAs(kBlackRose, P2, bf, TriggerType::WhenAFriendlyUnitDies, exec);
    EXPECT_EQ(unitsNamed(P2, "Recruit"), 1);
    auto u2 = addUnit(P2, kInvalidId, 2, 0);
    events.emit(UnitDiedEvent{u2, P2, BattlefieldLocation{0}, 2});
    EXPECT_EQ(chainItemsFrom(bf, TriggerType::WhenAFriendlyUnitDies), 0) << "first time only";
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    events.emit(UnitDiedEvent{mine, P1, BattlefieldLocation{0}, 2});
    EXPECT_EQ(chainItemsFrom(bf, TriggerType::WhenAFriendlyUnitDies), 1) << "for each player";
}

// ── 1119 Cosmic Vista ───────────────────────────────────────────────────────
TEST_F(Radiance8, CosmicVistaScoresOnHoldWithAnEmptyRuneDeck) {
    auto bf = setBattlefieldCard(state, 0, kCosmicVista);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    addRuneToDeck(P1, Domain::Order);
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 1});
    EXPECT_EQ(chainItemsFrom(bf, TriggerType::WhenYouHoldHere), 0);
    state.player(P1).rune_deck.clear();
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 1});
    ASSERT_EQ(chainItemsFrom(bf, TriggerType::WhenYouHoldHere), 1);
    const int before = state.player(P1).score;
    fireTriggerAs(kCosmicVista, P1, bf, TriggerType::WhenYouHoldHere, exec);
    EXPECT_EQ(state.player(P1).score, before + 1);
}

// ── 1120 Durand Memorial ────────────────────────────────────────────────────
TEST_F(Radiance8, DurandMemorialGivesTankToAUnitYouControlHere) {
    auto bf = setBattlefieldCard(state, 0, kDurand);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto other_bf = addUnit(P1, kInvalidId, 2, 1);
    auto foe = addUnit(P2, kInvalidId, 2, 0);
    driveResumableTrigger(kDurand, P1, bf, [&](const std::vector<Intent>& l) {
        EXPECT_FALSE(offered(l, other_bf));
        EXPECT_FALSE(offered(l, foe));
        return pickObj(l, mine);
    }, exec, TriggerType::WhenYouDefendHere);
    EXPECT_TRUE(state.getObject(mine).hasKeyword(Keyword::Tank));
}

// ── 1121 Firelights' Hideout ────────────────────────────────────────────────
TEST_F(Radiance8, FirelightsHideoutNextUnitHasAccelerate) {
    auto bf = setBattlefieldCard(state, 0, kFirelights);
    fireTriggerAs(kFirelights, P1, bf, TriggerType::WhenYouConquerHere, exec);
    EXPECT_TRUE(state.player(P1).next_unit_has_accelerate);
    EXPECT_FALSE(state.player(P2).next_unit_has_accelerate);

    // The engine grants Accelerate to the next unit's play...
    auto engine = std::make_unique<GameEngine>(card_db, events, card_registry);
    LambdaAgent a1, a2;
    engine->testHook_setAgents(&a1, &a2);
    auto& st = engine->mutableState();
    st.players[0].id = P1;
    st.players[1].id = P2;
    st.turn.turn_player = P1;
    st.player(P1).next_unit_has_accelerate = true;
    auto card = st.createObject();
    auto& o = st.getObject(card);
    o.owner = o.controller = P1;
    o.card_def_id = kSeraphine;
    o.card_type = CardType::Unit;
    o.domains = {Domain::Order};
    o.zone = ZoneType::Hand;
    st.player(P1).hand.push_back(card);
    for (int i = 0; i < 7; ++i) {
        auto r = st.createObject();
        auto& ro = st.getObject(r);
        ro.owner = ro.controller = P1;
        ro.card_type = CardType::Rune;
        ro.domains = {Domain::Order};
        ro.zone = ZoneType::Base;
        ro.location = BaseLocation{P1};
    }
    engine->testHook_payCardCost(P1, card);
    EXPECT_EQ(st.getObject(card).card_counters["__granted_accelerate"], 1);

    // ...and playing a unit uses it up.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto played = addUnit(P1, kInvalidId, 2);
    events.emit(CardPlayedEvent{played, P1, CardType::Unit, 1});
    EXPECT_FALSE(state.player(P1).next_unit_has_accelerate);
}

// ── 1122 Packed Amphitheater ────────────────────────────────────────────────
TEST_F(Radiance8, PackedAmphitheaterBuffsChampionUnitsHere) {
    auto bf = setBattlefieldCard(state, 0, kAmphitheater);
    auto champ = addUnit(P2, kSeraphine, 0, 0);
    auto plain = addUnit(P1, kInvalidId, 2, 0);
    auto champ_away = addUnit(P1, kMordekaiser, 0, 1);
    card_registry.get(kAmphitheater)->applyPassiveAura(state, P1);
    auto bonus = [&](GameObjectId id) {
        int b = 0;
        for (const auto& ae : state.getObject(id).aura_effects)
            if (ae.source == bf) b += ae.might_bonus;
        return b;
    };
    EXPECT_EQ(bonus(champ), 1);
    EXPECT_EQ(bonus(plain), 0);
    EXPECT_EQ(bonus(champ_away), 0);
}

// ── 1123 Ruined Monastery ───────────────────────────────────────────────────
TEST_F(Radiance8, RuinedMonasteryDiscardsAndChannelsExhausted) {
    auto bf = setBattlefieldCard(state, 0, kRuinedMonastery);
    addRuneToDeck(P1, Domain::Order);
    auto junk = addToHand(P1, kSeraphine);
    auto keep = addToHand(P1, kMordekaiser);
    driveResumableTrigger(kRuinedMonastery, P1, bf, [&](const std::vector<Intent>& l) {
        if (isYesNo(l)) return l[1];
        return pickObj(l, junk);
    }, exec, TriggerType::WhenYouConquerHere);
    EXPECT_TRUE(inTrash(P1, junk));
    EXPECT_TRUE(inHand(P1, keep));
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
    EXPECT_EQ(readyRuneCount(P1), 0);
}

TEST_F(Radiance8, RuinedMonasteryDeclined) {
    auto bf = setBattlefieldCard(state, 0, kRuinedMonastery);
    addRuneToDeck(P1, Domain::Order);
    auto junk = addToHand(P1, kSeraphine);
    driveResumableTrigger(kRuinedMonastery, P1, bf,
                          [&](const std::vector<Intent>& l) { return l.front(); }, exec,
                          TriggerType::WhenYouConquerHere);
    EXPECT_TRUE(inHand(P1, junk));
    EXPECT_EQ(exhaustedRuneCount(P1), 0);
}

}  // namespace
}  // namespace riftbound::test
