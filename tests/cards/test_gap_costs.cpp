/// @file test_gap_costs.cpp
/// Costs of cards played by effects and costs other cards add to a play:
///  - "ignoring its Energy cost (you must still pay its Power cost)":
///    EffectExecutor::playPayingCost charges the Power; an unpayable play
///    doesn't happen and the card stays where it was (Kai'Sa, Evolutionary 112,
///    Soulgorger 196, The Harrowing 198, Here to Help 433).
///  - mandatory "exhaust a friendly unit" additional cost paid as it's played
///    (ObjectCost::ExhaustFriendlyUnit — Ki Burst 964).
///  - an immediate TrashReplayGrant: "Its controller may play this spell again
///    for [A]" (Dancing Grenade 582), played by either player.
///  - where a play came from (card_counters["__played_not_from_hand"] — Back
///    Off 604).
///  - a "play it from hand this turn ignoring its Energy cost" permission
///    (PlayerState::ignore_energy_play_grants — Jayce, Man of Progress 406).
///  - a granted [Repeat] equal to the spell's cost, Power included (Temporal
///    Portal 401).
///  - a spell played by an effect chooses its targets as it is finalized
///    (Drakehound Packmaster 1003).
///  - a play's place in the turn, fixed as it is played (Astral Heron 831).
///  - an additional cost another card grants spells (Card::
///    grantedSpellAdditionalCost — Starry-Eyed Songstress 979).

#include "tests/cards/card_test_fixture.h"

#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

namespace riftbound::test {
namespace {

constexpr CardDefId kKaiSa = 112, kSoulgorger = 196, kHarrowing = 198, kTemporalPortal = 401,
    kJayce = 406, kHereToHelp = 433, kDancingGrenade = 582, kBackOff = 604, kSunDisc = 21,
    kVengeance = 229, kAstralHeron = 831, kKiBurst = 964, kSongstress = 979,
    kDrakehound = 1003;

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
std::optional<Intent> chooseValue(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.chosen_objects.empty() &&
            i.chosen_value.value_or(-1) == v) return i;
    return std::nullopt;
}
bool offersObject(const std::vector<Intent>& legal, GameObjectId id) {
    return chooseObject(legal, id).has_value();
}
bool offersValue(const std::vector<Intent>& legal, int v) {
    return chooseValue(legal, v).has_value();
}

class GapCostsTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    ScriptAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    EffectExecutor& ex() { return engine_->testHook_executor(); }

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
        st.turn.turn_number = 1;
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
    GameObjectId cardObj(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& d = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = d.card_type;
        c.super_type = d.super_type;
        c.name = d.name; c.domains = d.domains; c.keywords = d.keywords; c.tags = d.tags;
        c.base_might = d.might;
        c.current_might = d.might;
        return id;
    }
    GameObjectId unit(PlayerId p, CardDefId def_id = kInvalidId, int at_bf = -1, int might = 3) {
        GameObjectId id;
        if (def_id != kInvalidId) {
            id = cardObj(p, def_id);
        } else {
            id = s().createObject();
            auto& u = s().getObject(id);
            u.owner = u.controller = p;
            u.card_type = CardType::Unit;
            u.name = "TestUnit";
            u.base_might = might;
            u.current_might = might;
        }
        auto& u = s().getObject(id);
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
        auto id = cardObj(p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId trashCard(PlayerId p, CardDefId def_id) {
        auto id = cardObj(p, def_id);
        s().getObject(id).zone = ZoneType::Trash;
        s().player(p).trash.push_back(id);
        return id;
    }
    GameObjectId deckCard(PlayerId p, CardDefId def_id) {
        auto id = cardObj(p, def_id);
        s().getObject(id).zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);
        return id;
    }
    void pool(PlayerId p, int energy, Domain d = Domain::Fury, int power = 0) {
        auto& rp = s().player(p).rune_pool;
        rp = RunePool{};
        rp.energy = energy;
        rp.power[static_cast<int>(d)] = power;
    }
    int power(PlayerId p, Domain d) { return s().player(p).rune_pool.power[static_cast<int>(d)]; }
    void play(PlayerId p, GameObjectId card, std::optional<LocationId> where = std::nullopt,
              std::vector<GameObjectId> targets = {},
              Intent::PlaySource source = Intent::PlaySource::Hand) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = where;
        i.targets = std::move(targets);
        i.play_source = source;
        engine_->testHook_executeIntent(i);
    }
    /// Resolve a triggered ability / spell effect through the engine's executor,
    /// answering its choices with `agent`.
    void drive(CardDefId def_id, PlayerId controller, GameObjectId source, ScriptAgent& agent,
               bool spell = false) {
        ChainItem ri;
        ri.id = s().chain.allocateId();
        ri.controller = controller;
        ri.is_ability = !spell;
        ri.is_spell = spell;
        ri.source = source;
        ri.card_def_id = def_id;
        s().chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{s(), events, ex(), controller, source};
        ctx.registry = &card_registry;
        auto step = [&]() { spell ? card->onResolve(ctx, {}) : card->onTrigger(ctx, {}); };
        step();
        for (int g = 0; g < 20 && ex().hasPendingChoice(); ++g) {
            auto pending = ex().consumePendingChoice();
            ex().recordChoice(agent.selectAction(s(), pending.legal));
            step();
        }
        s().chain.resuming.reset();
    }
    bool inList(const std::vector<GameObjectId>& v, GameObjectId id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    }
    bool onBoard(GameObjectId id) { return s().getObject(id).location.has_value(); }
};

// ═══ Power is still paid when an effect ignores the Energy cost ═══════════

// ── 112 Kai'Sa, Evolutionary ──
TEST_F(GapCostsTest, KaiSa_PaysThePowerOfTheSpellItPlays) {
    auto kaisa = unit(P1, kKaiSa, 0);
    s().player(P1).score = 3;
    auto grenade = trashCard(P1, kDancingGrenade);   // [2] + 1 Fury Power
    unit(P2, kInvalidId, 0, 9);
    pool(P1, 0, Domain::Fury, 1);
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = chooseObject(l, grenade)) return i;
        return chooseValue(l, 1);
    };
    drive(kKaiSa, P1, kaisa, a1_);
    EXPECT_EQ(power(P1, Domain::Fury), 0) << "its Power was paid";
    EXPECT_EQ(s().getObject(grenade).zone, ZoneType::Chain);
    EXPECT_FALSE(inList(s().player(P1).trash, grenade));
    EXPECT_EQ(s().getObject(grenade).card_counters["__recycle_on_leave_chain"], 1);
}

TEST_F(GapCostsTest, KaiSa_UnpayablePowerMeansNoPlay) {
    auto kaisa = unit(P1, kKaiSa, 0);
    s().player(P1).score = 3;
    auto grenade = trashCard(P1, kDancingGrenade);
    pool(P1, 5);   // Energy, but no Fury Power
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseValue(l, 1); };
    drive(kKaiSa, P1, kaisa, a1_);
    EXPECT_TRUE(a1_.choices.empty()) << "nothing playable: no prompt";
    EXPECT_TRUE(inList(s().player(P1).trash, grenade));
    EXPECT_EQ(s().getObject(grenade).zone, ZoneType::Trash);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 5);
}

// ── 196 Soulgorger ──
TEST_F(GapCostsTest, Soulgorger_PaysThePowerOfTheUnit) {
    auto sg = unit(P1, kSoulgorger);
    auto dead = trashCard(P1, kSoulgorger);   // [8] + 2 Chaos Power
    pool(P1, 0, Domain::Chaos, 2);
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = chooseObject(l, dead)) return i;
        return chooseValue(l, 1);
    };
    drive(kSoulgorger, P1, sg, a1_);
    EXPECT_TRUE(onBoard(dead));
    EXPECT_EQ(power(P1, Domain::Chaos), 0);
    EXPECT_FALSE(inList(s().player(P1).trash, dead));
}

TEST_F(GapCostsTest, Soulgorger_UnitWhosePowerCantBePaidIsNotOffered) {
    auto sg = unit(P1, kSoulgorger);
    auto dead = trashCard(P1, kSoulgorger);
    pool(P1, 9, Domain::Chaos, 1);
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = chooseObject(l, dead)) return i;
        return chooseValue(l, 1);
    };
    drive(kSoulgorger, P1, sg, a1_);
    EXPECT_FALSE(onBoard(dead));
    EXPECT_TRUE(inList(s().player(P1).trash, dead));
    EXPECT_EQ(power(P1, Domain::Chaos), 1);
}

// ── 198 The Harrowing ──
TEST_F(GapCostsTest, Harrowing_PaysPowerAndOnlyOffersPayableUnits) {
    auto harrowing = cardObj(P1, kHarrowing);
    s().getObject(harrowing).zone = ZoneType::Chain;
    auto big = trashCard(P1, kSoulgorger);   // 2 Chaos Power
    auto kaisa = trashCard(P1, kKaiSa);      // 1 Mind Power
    pool(P1, 0, Domain::Mind, 1);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, kaisa); };
    drive(kHarrowing, P1, harrowing, a1_, /*spell=*/true);
    ASSERT_FALSE(a1_.choices.empty());
    EXPECT_FALSE(offersObject(a1_.choices[0], big)) << "its Power can't be paid";
    EXPECT_TRUE(onBoard(kaisa));
    EXPECT_EQ(power(P1, Domain::Mind), 0);
    EXPECT_TRUE(inList(s().player(P1).trash, big));
}

// ── 433 Here to Help ──
TEST_F(GapCostsTest, HereToHelp_ReducesByThreeAndChargesTheRest) {
    s().battlefields[1].controller = P1;
    auto hth = cardObj(P1, kHereToHelp);
    s().getObject(hth).zone = ZoneType::Chain;
    auto sg = handCard(P1, kSoulgorger);   // [8] + 2 Chaos -> [5] + 2 Chaos
    pool(P1, 5, Domain::Chaos, 2);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, sg); };
    drive(kHereToHelp, P1, hth, a1_, /*spell=*/true);
    EXPECT_TRUE(onBoard(sg));
    ASSERT_TRUE(s().getObject(sg).battlefieldId().has_value());
    EXPECT_EQ(*s().getObject(sg).battlefieldId(), 1u);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_EQ(power(P1, Domain::Chaos), 0);
}

TEST_F(GapCostsTest, HereToHelp_UnitItCantPayForIsNotOffered) {
    s().battlefields[1].controller = P1;
    auto hth = cardObj(P1, kHereToHelp);
    s().getObject(hth).zone = ZoneType::Chain;
    auto sg = handCard(P1, kSoulgorger);
    pool(P1, 4, Domain::Chaos, 2);   // one short of [5]
    drive(kHereToHelp, P1, hth, a1_, /*spell=*/true);
    EXPECT_TRUE(a1_.choices.empty());
    EXPECT_TRUE(inList(s().player(P1).hand, sg));
}

// ═══ 964 Ki Burst: "exhaust a friendly unit" is paid as it is played ══════

TEST_F(GapCostsTest, KiBurst_ExhaustIsAMandatoryCostPaidOnPlay) {
    pool(P1, 1);
    auto enemy = unit(P2, kInvalidId, 0);
    auto ki = handCard(P1, kKiBurst);
    auto tired = unit(P1);
    s().getObject(tired).is_exhausted = true;
    EXPECT_FALSE(engine_->testHook_canAfford(P1, ki)) << "no ready friendly unit";
    auto a = unit(P1), b = unit(P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, ki));
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, b); };
    play(P1, ki, std::nullopt, {enemy});
    ASSERT_FALSE(a1_.choices.empty());
    const auto& first = a1_.choices[0];
    EXPECT_TRUE(offersObject(first, a));
    EXPECT_TRUE(offersObject(first, b));
    EXPECT_FALSE(offersObject(first, tired)) << "an exhausted unit can't pay";
    EXPECT_FALSE(offersValue(first, 0)) << "mandatory: no declining";
    EXPECT_TRUE(s().getObject(b).is_exhausted);
    EXPECT_FALSE(s().getObject(a).is_exhausted);
    EXPECT_TRUE(s().getObject(enemy).is_stunned);
}

// ═══ 582 Dancing Grenade: its target's controller may play it again ═══════

TEST_F(GapCostsTest, DancingGrenade_OpponentPlaysItAgainForA) {
    pool(P1, 2, Domain::Fury, 1);
    s().player(P2).rune_pool.universal_power = 1;
    auto mine = unit(P1, kInvalidId, 0, 9);
    auto theirs = unit(P2, kInvalidId, 0, 9);
    auto grenade = handCard(P1, kDancingGrenade);
    a2_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = chooseObject(l, mine)) return i;   // the replay's target
        return chooseValue(l, 1);                       // yes, play it again
    };
    play(P1, grenade, std::nullopt, {theirs});
    EXPECT_EQ(s().getObject(theirs).damage_marked, 2);
    EXPECT_EQ(s().getObject(mine).damage_marked, 3) << "2 + 1 for the earlier hit this turn";
    EXPECT_EQ(s().player(P2).rune_pool.universal_power, 0) << "P2 paid [A]";
    EXPECT_TRUE(a1_.choices.empty()) << "P1 has no [A]: not asked to play it again";
    EXPECT_TRUE(inList(s().player(P1).trash, grenade)) << "back in its owner's trash";
    EXPECT_FALSE(inList(s().player(P2).trash, grenade));
    EXPECT_TRUE(s().player(P2).trash_replay_grants.empty());
}

TEST_F(GapCostsTest, DancingGrenade_DeclinedReplay) {
    pool(P1, 2, Domain::Fury, 1);
    s().player(P2).rune_pool.universal_power = 1;
    auto mine = unit(P1, kInvalidId, 0, 9);
    auto theirs = unit(P2, kInvalidId, 0, 9);
    auto grenade = handCard(P1, kDancingGrenade);
    a2_.pick = [&](const std::vector<Intent>& l) { return chooseValue(l, 0); };
    play(P1, grenade, std::nullopt, {theirs});
    ASSERT_EQ(a2_.choices.size(), 1u);
    EXPECT_TRUE(offersValue(a2_.choices[0], 1));
    EXPECT_EQ(s().getObject(theirs).damage_marked, 2);
    EXPECT_EQ(s().getObject(mine).damage_marked, 0);
    EXPECT_EQ(s().player(P2).rune_pool.universal_power, 1);
    EXPECT_TRUE(inList(s().player(P1).trash, grenade));
}

// ═══ 604 Back Off: a play from the trash isn't from hand ═════════════════

TEST_F(GapCostsTest, BackOff_PlayedFromTrashDoesNotDraw) {
    pool(P1, 6);
    auto enemy = unit(P2, kInvalidId, 0);
    deckCard(P1, kBackOff);
    deckCard(P1, kBackOff);
    auto bo = trashCard(P1, kBackOff);
    PlayerState::TrashReplayGrant g;
    g.card = bo;
    g.energy = 1;
    s().player(P1).trash_replay_grants.push_back(g);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, enemy); };
    play(P1, bo, std::nullopt, {}, Intent::PlaySource::Trash);
    EXPECT_TRUE(s().getObject(enemy).is_stunned);
    EXPECT_TRUE(s().player(P1).hand.empty()) << "not played from hand: no draw";
    EXPECT_EQ(s().getObject(bo).card_counters["__played_not_from_hand"], 1);

    // The same card from hand draws.
    auto& trash = s().player(P1).trash;
    trash.erase(std::remove(trash.begin(), trash.end(), bo), trash.end());
    s().getObject(bo).zone = ZoneType::Hand;
    s().player(P1).hand.push_back(bo);
    s().getObject(enemy).is_stunned = false;
    pool(P1, 3);
    play(P1, bo);
    EXPECT_EQ(s().getObject(bo).card_counters["__played_not_from_hand"], 0);
    EXPECT_EQ(s().player(P1).hand.size(), 1u) << "from hand: draw 1";
}

TEST_F(GapCostsTest, EffectPlayFromTrashRecordsItsSource) {
    auto u = trashCard(P1, kSoulgorger);
    auto& trash = s().player(P1).trash;
    trash.erase(std::remove(trash.begin(), trash.end(), u), trash.end());
    const int before = s().player(P1).cards_played_not_from_hand_this_turn;
    ex().playIgnoringCost(P1, u);
    EXPECT_EQ(s().getObject(u).card_counters["__played_not_from_hand"], 1);
    EXPECT_EQ(s().player(P1).cards_played_not_from_hand_this_turn, before + 1);
    auto h = handCard(P1, kSoulgorger);
    s().player(P1).hand.clear();
    ex().playIgnoringCost(P1, h);
    EXPECT_EQ(s().getObject(h).card_counters["__played_not_from_hand"], 0);
    EXPECT_EQ(s().player(P1).cards_played_not_from_hand_this_turn, before + 1);
}

// ═══ 406 Jayce, Man of Progress: a gear play ignoring Energy, all turn ════

TEST_F(GapCostsTest, Jayce_PermissionLastsTheTurnAndPowerIsPaid) {
    auto jayce = unit(P1, kJayce, 0);
    auto old_gear = cardObj(P1, kSunDisc);
    s().getObject(old_gear).zone = ZoneType::Base;
    s().getObject(old_gear).location = BaseLocation{P1};
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = chooseObject(l, old_gear)) return i;
        return chooseValue(l, 1);
    };
    drive(kJayce, P1, jayce, a1_);
    EXPECT_FALSE(onBoard(old_gear));
    ASSERT_EQ(s().player(P1).ignore_energy_play_grants.size(), 1u);

    // Later in the turn: a [2] + 1 Fury gear with no Energy available.
    auto disc = handCard(P1, kSunDisc);
    pool(P1, 0, Domain::Fury, 1);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, disc));
    const auto legal = engine_->generateLegalActions();
    EXPECT_TRUE(std::any_of(legal.begin(), legal.end(), [&](const Intent& i) {
        return i.type == IntentType::PlayCard && i.card == disc;
    })) << "offered thanks to the permission";
    a1_.choices.clear();
    a1_.pick = nullptr;
    play(P1, disc, BaseLocation{P1});
    EXPECT_TRUE(a1_.choices.empty()) << "only payable with the permission: used without asking";
    EXPECT_TRUE(onBoard(disc));
    EXPECT_EQ(power(P1, Domain::Fury), 0) << "its Power is still paid";
    EXPECT_TRUE(s().player(P1).ignore_energy_play_grants.empty()) << "used up";
}

TEST_F(GapCostsTest, Jayce_PermissionMayBeKeptAndExpires) {
    PlayerState::IgnoreEnergyPlayGrant g;
    g.card_type = CardType::Gear;
    g.max_energy_cost = 7;
    s().player(P1).ignore_energy_play_grants.push_back(g);
    auto disc = handCard(P1, kSunDisc);
    pool(P1, 2, Domain::Fury, 1);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseValue(l, 0); };   // keep it
    play(P1, disc, BaseLocation{P1});
    ASSERT_EQ(a1_.choices.size(), 1u);
    EXPECT_TRUE(onBoard(disc));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "paid in full";
    EXPECT_EQ(s().player(P1).ignore_energy_play_grants.size(), 1u) << "kept for later this turn";
    // A unit doesn't match a gear permission.
    auto sg = handCard(P1, kSoulgorger);
    pool(P1, 0, Domain::Chaos, 2);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, sg));
    s().player(P1).resetTurnTracking();
    EXPECT_TRUE(s().player(P1).ignore_energy_play_grants.empty()) << "this turn only";
}

// ═══ 401 Temporal Portal: the granted [Repeat] includes the Power ═════════

TEST_F(GapCostsTest, TemporalPortal_RepeatTrancheChargesPower) {
    auto portal = cardObj(P1, kTemporalPortal);
    s().getObject(portal).zone = ZoneType::Base;
    s().getObject(portal).location = BaseLocation{P1};
    CardContext ctx{s(), events, ex(), P1, portal};
    card_registry.get(kTemporalPortal)->onActivate(ctx, {});
    ASSERT_TRUE(s().player(P1).grant_repeat_base_to_next_spell);
    unit(P2, kInvalidId, 0, 20);
    auto target = unit(P2, kInvalidId, 0, 20);
    auto grenade = handCard(P1, kDancingGrenade);   // [2] + 1 Fury
    pool(P1, 4, Domain::Fury, 1);   // the tranche's Power can't be paid
    play(P1, grenade, std::nullopt, {target});
    EXPECT_TRUE(a1_.choices.empty()) << "no Repeat offered without its Power";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 2);

    // With the Power the tranche is offered and charges [2] + 1 Fury.
    s().player(P1).grant_repeat_base_to_next_spell = true;
    auto grenade2 = handCard(P1, kDancingGrenade);
    pool(P1, 4, Domain::Fury, 2);
    a1_.choices.clear();
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseValue(l, 1); };
    play(P1, grenade2, std::nullopt, {target});
    ASSERT_FALSE(a1_.choices.empty());
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_EQ(power(P1, Domain::Fury), 0);
}

// ═══ 1003 Drakehound Packmaster: a spell it plays chooses its target ══════

TEST_F(GapCostsTest, Drakehound_PlayedSpellChoosesItsTarget) {
    auto victim = unit(P2, kInvalidId, 0, 4);
    auto other = unit(P2, kInvalidId, 0, 4);
    auto top = deckCard(P2, kVengeance);   // "Kill a unit."
    auto dh = handCard(P1, kDrakehound);
    pool(P1, 9, Domain::Fury, 2);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseObject(l, victim); };
    play(P1, dh, BaseLocation{P1});
    bool asked = false;
    for (const auto& c : a1_.choices) if (offersObject(c, victim) && offersObject(c, other)) asked = true;
    EXPECT_TRUE(asked) << "P1 chooses Vengeance's target as it is played";
    EXPECT_FALSE(onBoard(victim));
    EXPECT_TRUE(onBoard(other));
    EXPECT_TRUE(inList(s().player(P2).trash, top)) << "the spell lands in its owner's trash";
}

// ═══ 831 Astral Heron: "first card" is fixed as it is played ═════════════

TEST_F(GapCostsTest, AstralHeron_FirstCardReadsThePlayOrder) {
    auto heron = unit(P1, kAstralHeron, 0);
    const Card* c = card_registry.get(kAstralHeron);
    auto first = cardObj(P1, kDancingGrenade);
    auto second = cardObj(P1, kDancingGrenade);
    s().getObject(first).card_counters["__play_ordinal"] = 1;
    s().getObject(first).card_counters["__play_turn"] = 1;
    s().getObject(second).card_counters["__play_ordinal"] = 2;
    s().getObject(second).card_counters["__play_turn"] = 1;
    s().player(P1).cards_played_this_turn = 2;   // the later card resolved first
    EXPECT_TRUE(c->triggerCondition(s(), heron, P1, TriggerType::WhenYouPlayACard, first));
    EXPECT_FALSE(c->triggerCondition(s(), heron, P1, TriggerType::WhenYouPlayACard, second));
    s().turn.turn_number = 2;   // a stamp from an earlier turn doesn't count
    s().player(P1).cards_played_this_turn = 3;
    EXPECT_FALSE(c->triggerCondition(s(), heron, P1, TriggerType::WhenYouPlayACard, first));
}

TEST_F(GapCostsTest, PlaysAreStampedWithTheirPlaceInTheTurn) {
    pool(P1, 4, Domain::Fury, 2);
    auto target = unit(P2, kInvalidId, 0, 20);
    auto g1 = handCard(P1, kDancingGrenade);
    auto g2 = handCard(P1, kDancingGrenade);
    play(P1, g1, std::nullopt, {target});
    play(P1, g2, std::nullopt, {target});
    EXPECT_EQ(s().getObject(g1).card_counters["__play_ordinal"], 1);
    EXPECT_EQ(s().getObject(g2).card_counters["__play_ordinal"], 2);
    EXPECT_EQ(s().getObject(g2).card_counters["__play_turn"], 1);
}

// ═══ 979 Starry-Eyed Songstress: an additional cost she grants spells ═════

class SongstressTest : public GapCostsTest {
protected:
    GameObjectId legend_ = kInvalidId;
    void SetUp() override {
        GapCostsTest::SetUp();
        legend_ = s().player(P1).legend_zone;
        auto& lo = s().getObject(legend_);
        lo.card_def_id = kSongstress;
        lo.name = card_db.get(kSongstress).name;
    }
};

TEST_F(SongstressTest, ForcedWhenOnlyTheDiscountMakesItPayable) {
    auto a = unit(P1), b = unit(P1), c = unit(P1), d = unit(P1);
    auto enemy = unit(P2, kInvalidId, 0);
    auto veng = handCard(P1, kVengeance);   // [4] + 2 Order
    pool(P1, 2, Domain::Order, 2);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, veng)) << "[2] less with the cost paid";
    std::vector<GameObjectId> order = {a, c, d};
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        for (auto o : order) if (auto i = chooseObject(l, o)) return i;
        return std::nullopt;
    };
    play(P1, veng, std::nullopt, {enemy});
    ASSERT_EQ(a1_.choices.size(), 3u) << "no yes/no: three unit picks";
    EXPECT_FALSE(offersValue(a1_.choices[0], 0));
    EXPECT_EQ(a1_.choices[0].size(), 4u);
    EXPECT_TRUE(s().getObject(legend_).is_exhausted);
    for (auto id : {a, c, d}) EXPECT_TRUE(s().getObject(id).is_exhausted);
    EXPECT_FALSE(s().getObject(b).is_exhausted);
    EXPECT_EQ(s().player(P1).last_exhaust_group_size, 3) << "exhausted at the same time";
    EXPECT_FALSE(onBoard(enemy)) << "Vengeance resolved";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty()) << "no leftover discount";
}

TEST_F(SongstressTest, OptionalWhenThePrintedCostIsPayable) {
    auto a = unit(P1), b = unit(P1), c = unit(P1);
    auto enemy = unit(P2, kInvalidId, 0);
    auto veng = handCard(P1, kVengeance);
    pool(P1, 4, Domain::Order, 2);
    a1_.pick = [&](const std::vector<Intent>& l) { return chooseValue(l, 0); };   // decline
    play(P1, veng, std::nullopt, {enemy});
    ASSERT_EQ(a1_.choices.size(), 1u);
    EXPECT_TRUE(offersValue(a1_.choices[0], 1));
    EXPECT_FALSE(s().getObject(legend_).is_exhausted);
    for (auto id : {a, b, c}) EXPECT_FALSE(s().getObject(id).is_exhausted);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "paid [4]";
}

TEST_F(SongstressTest, NeedsHerReadyAndThreeReadyUnits) {
    unit(P1); unit(P1);
    auto veng = handCard(P1, kVengeance);
    pool(P1, 2, Domain::Order, 2);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, veng)) << "only two friendly units";
    auto third = unit(P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, veng));
    s().getObject(legend_).is_exhausted = true;
    EXPECT_FALSE(engine_->testHook_canAfford(P1, veng)) << "she is exhausted";
    s().getObject(legend_).is_exhausted = false;
    s().getObject(third).is_exhausted = true;
    EXPECT_FALSE(engine_->testHook_canAfford(P1, veng)) << "an exhausted unit can't pay";
}

}  // namespace
}  // namespace riftbound::test
