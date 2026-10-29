/// @file test_gap_choices.cpp
/// Automatic picks replaced with real player choices, and the mechanisms
/// behind them:
///  - several triggered abilities of one card on one event go on the chain
///    separately, in the order their controller chooses
///    (Card::triggeredAbilityCount, ChainItem::trigger_index) — Akshan,
///    Mischievous (431);
///  - death replacements left on a unit (Card::applyGrantedDeathReplacement)
///    — Unlicensed Armory (23);
///  - an Equipment's copied effect text (GameObject::copied_effect_text_def)
///    — Svellsongur (382);
///  - split-damage targets chosen at finalization (splitDamageTargets /
///    splitDamageResolve) — Alpha Strike (751), Volibear, Furious (41);
///  - Card::equipCost — [Weaponmaster] prices an Equip without its text;
///  - last-known "here" information for units dying together — Lonely Poro
///    (359, 777);
///  - one name from a long list in int-coded steps (stepChooseFromList) —
///    The List (700);
///  - opponents' choices (stepChooseObject) — King's Edict (237), Call to
///    Battle (663), Chronobreak (970); the player's own picks — Disposal
///    Order (665), Azir, Ascendant (373); Double Trouble (594).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "cards/units/weaponmaster_base.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "openspiel/action_vocab.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <set>

namespace riftbound::test {
namespace {

constexpr CardDefId kArmory = 23, kVolibear = 41, kAhri = 66, kKingsEdict = 237,
    kLongSword = 345, kSkyfall = 353, kLonelyPoro = 359, kAzir = 373, kSvellsongur = 382,
    kDoransBlade = 417, kAkshan = 431, kSpinningAxe = 504, kDoubleTrouble = 594,
    kCallToBattle = 663, kDisposalOrder = 665, kTheList = 700, kHeirloom = 720,
    kGauntlets = 748, kAlphaStrike = 751, kLonelyPoro777 = 777, kChronobreak = 970;

static_assert(kIntChoiceSlots == openspiel::kNumIntChoices,
              "stepChooseFromList must fit the MakeChoice int slots");

// ── pickers ──
std::optional<Intent> objectOf(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
            i.chosen_objects.front() == id) return i;
    return std::nullopt;
}
std::optional<Intent> valueOf(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.chosen_objects.empty() &&
            i.chosen_value.value_or(-99) == v) return i;
    return std::nullopt;
}
std::optional<Intent> labelled(const std::vector<Intent>& legal, const std::string& l) {
    for (const auto& i : legal)
        if (i.type == IntentType::MakeChoice && i.choice_label == l) return i;
    return std::nullopt;
}
bool isDone(const Intent& i) {
    return i.chosen_objects.empty() && i.chosen_value.value_or(-1) == kPickSeveralDone;
}

/// Records each decision and the chain at each priority decision.
class RecAgent : public AgentInterface {
public:
    std::function<std::optional<Intent>(const std::vector<Intent>&)> pick;
    std::vector<std::vector<Intent>> choices;            // MakeChoice decisions
    std::vector<std::string> log;                        // "C" choice / "P" priority
    std::vector<std::vector<std::pair<CardDefId, int>>> chain_at_priority;
    Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
        bool prio = false;
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) prio = true;
        if (prio) {
            std::vector<std::pair<CardDefId, int>> v;
            for (const auto& ci : s.chain.items)
                if (ci.is_ability) v.emplace_back(ci.card_def_id, ci.trigger_index);
            chain_at_priority.push_back(v);
            log.push_back("P");
        }
        if (!legal.empty() && legal.front().type == IntentType::MakeChoice) {
            choices.push_back(legal);
            log.push_back("C");
        }
        if (pick) if (auto i = pick(legal)) return *i;
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

// ── Engine fixture (full play path: finalization, chain, agents) ──
class GapChoicesEngine : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    RecAgent a1_, a2_;
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
    GameObjectId unit(PlayerId p, CardDefId def_id = kInvalidId, int at_bf = -1, int might = 0) {
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
        if (might > 0) u.base_might = might;
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
    GameObjectId gear(PlayerId p, CardDefId def_id, bool equipment = true) {
        auto id = s().createObject();
        auto& g = s().getObject(id);
        g.owner = g.controller = p;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            g.name = d.name; g.tags = d.tags; g.keywords = d.keywords; g.domains = d.domains;
            g.might_bonus = d.might_bonus;
        } else {
            g.name = "TestGear";
            if (equipment) g.tags = {"Equipment"};
        }
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        return id;
    }
    void attach(GameObjectId g, GameObjectId u) {
        auto& go = s().getObject(g);
        auto& uo = s().getObject(u);
        go.attached_to = u;
        go.location = uo.location;
        go.zone = uo.zone;
        uo.attachments.push_back(g);
        uo.attachment_might_bonus += go.might_bonus;
        uo.recomputeMight();
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
    GameObjectId deckCard(PlayerId p) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        c.owner = c.controller = p;
        c.card_type = CardType::Spell;
        c.name = "Filler";
        c.zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);
        return id;
    }
    void pool(PlayerId p, int energy, Domain d = Domain::Fury, int power = 0) {
        s().player(p).rune_pool.energy = energy;
        s().player(p).rune_pool.power[static_cast<int>(d)] = power;
    }
    void play(PlayerId p, GameObjectId card, std::optional<LocationId> where = std::nullopt) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = where;
        engine_->testHook_executeIntent(i);
    }
    CardContext ctxFor(PlayerId p, GameObjectId src) {
        CardContext c{s(), events, engine_->testHook_executor(), p, src};
        c.registry = &card_registry;
        return c;
    }
};

// ═══ Mechanism: several triggered abilities on one event ═══════════════════

// 431 Akshan, Mischievous: [Weaponmaster] and the paid-cost steal are two
// chain items; the player orders them as they are finalized.
TEST_F(GapChoicesEngine, Akshan_TwoPlayAbilitiesAreSeparateItemsInTheChosenOrder) {
    pool(P1, 4, Domain::Body, 2);
    auto enemy_gear = gear(P2, kInvalidId, /*equipment=*/false);
    auto akshan = handCard(P1, kAkshan);
    bool ordered = false;
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = labelled(l, "Move an enemy gear to your base")) { ordered = true; return i; }
        if (auto i = objectOf(l, enemy_gear)) return i;
        if (auto i = valueOf(l, 1)) return i;   // pay the optional [O][O]
        return std::nullopt;
    };
    play(P1, akshan, LocationId{BaseLocation{P1}});
    EXPECT_TRUE(ordered) << "the player was asked to order the two abilities";
    ASSERT_FALSE(a1_.chain_at_priority.empty());
    const auto& chain = a1_.chain_at_priority.front();
    ASSERT_EQ(chain.size(), 2u) << "one chain item per ability";
    EXPECT_EQ(chain[0], std::make_pair(kAkshan, 1)) << "the steal went on the chain first";
    EXPECT_EQ(chain[1], std::make_pair(kAkshan, 0)) << "[Weaponmaster] above it, resolving first";
    EXPECT_EQ(s().getObject(enemy_gear).controller, P1) << "the steal resolved";
}

TEST_F(GapChoicesEngine, Akshan_DefaultOrderAndNoStealWithoutTheCost) {
    pool(P1, 4, Domain::Body, 2);
    gear(P2, kInvalidId, /*equipment=*/false);
    auto akshan = handCard(P1, kAkshan);
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        return valueOf(l, 0);   // decline the optional cost
    };
    play(P1, akshan, LocationId{BaseLocation{P1}});
    for (const auto& c : a1_.chain_at_priority)
        EXPECT_LE(c.size(), 1u) << "only [Weaponmaster] triggers";
}

// ═══ Mechanism: split damage targets at finalization ═══════════════════════

// 751 Alpha Strike: the friendly unit and the split's targets are chosen
// before anyone can respond; the division as it resolves.
TEST_F(GapChoicesEngine, AlphaStrike_TargetsAreChosenAsItIsPlayed) {
    pool(P1, 3, Domain::Calm, 1);
    auto striker = unit(P1, kInvalidId, 0, 3);
    auto e1 = unit(P2, kInvalidId, 0, 5);
    auto e2 = unit(P2, kInvalidId, 1, 5);
    auto spell = handCard(P1, kAlphaStrike);
    std::vector<GameObjectId> script = {striker, e1, e2, e2};
    size_t k = 0;
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (l.front().type != IntentType::MakeChoice) return std::nullopt;
        if (k < script.size()) if (auto i = objectOf(l, script[k])) { ++k; return i; }
        return std::nullopt;
    };
    play(P1, spell);
    // Choices: unit, target e1, target e2 — then priority — then the 1 extra point.
    ASSERT_GE(a1_.log.size(), 5u);
    EXPECT_EQ(a1_.log[0], "C");
    EXPECT_EQ(a1_.log[1], "C");
    EXPECT_EQ(a1_.log[2], "C");
    EXPECT_EQ(a1_.log[3], "P") << "split targets are chosen before priority (CR 355.14.b)";
    EXPECT_EQ(s().getObject(e1).damage_marked, 1);
    EXPECT_EQ(s().getObject(e2).damage_marked, 2) << "the division is decided at resolution";
}

// ═══ Mechanism: granted death replacement ══════════════════════════════════

// 23 Unlicensed Armory: the mark lives on the unit, so it works after the
// Armory has left the board; "the next time" uses it up.
TEST_F(GapChoicesEngine, UnlicensedArmory_ReplacementOutlivesTheArmory) {
    auto u = unit(P1, kInvalidId, 0, 3);
    auto armory = gear(P1, kArmory);
    auto c = ctxFor(P1, armory);
    card_registry.get(kArmory)->onActivate(c, 0, {u});
    ASSERT_EQ(s().getObject(u).death_replacement_grants.size(), 1u);
    auto& exec = engine_->testHook_executor();
    exec.killObject(armory);
    ASSERT_FALSE(s().getObject(armory).location.has_value());

    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    a1_.pick = [](const std::vector<Intent>& l) { return valueOf(l, 1); };   // pay [C]
    exec.killObject(u);
    ASSERT_TRUE(s().getObject(u).location.has_value()) << "saved: the Armory is gone, the mark isn't";
    EXPECT_TRUE(s().getObject(u).isAtBase()) << "recalled";
    EXPECT_TRUE(s().getObject(u).is_exhausted);
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0) << "paid [C]";
    EXPECT_TRUE(s().getObject(u).death_replacement_grants.empty()) << "used up";

    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    exec.killObject(u);
    EXPECT_FALSE(s().getObject(u).location.has_value()) << "only the next time";
}

TEST_F(GapChoicesEngine, UnlicensedArmory_LapsesAtEndOfTurnAndCanBeDeclined) {
    auto u = unit(P1, kInvalidId, 0, 3);
    auto v = unit(P1, kInvalidId, 0, 3);
    auto armory = gear(P1, kArmory);
    auto c = ctxFor(P1, armory);
    card_registry.get(kArmory)->onActivate(c, 0, {u});
    card_registry.get(kArmory)->onActivate(c, 0, {v});
    auto& exec = engine_->testHook_executor();
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    a1_.pick = [](const std::vector<Intent>& l) { return valueOf(l, 0); };   // decline
    exec.killObject(v);
    EXPECT_FALSE(s().getObject(v).location.has_value()) << "declined: it dies";
    s().turn.turn_number += 1;   // "this turn" is over
    a1_.pick = [](const std::vector<Intent>& l) { return valueOf(l, 1); };
    exec.killObject(u);
    EXPECT_FALSE(s().getObject(u).location.has_value());
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 2);
}

// ═══ Mechanism: copied effect text ═════════════════════════════════════════

// 382 Svellsongur on Ahri, Alluring ("When I hold, you score 1 point."): the
// copied ability triggers too, as its own chain item.
TEST_F(GapChoicesEngine, Svellsongur_CopiedTriggerFiresAgain) {
    auto ahri = unit(P1, kAhri, 0);
    auto svell = gear(P1, kSvellsongur);
    attach(svell, ahri);
    engine_->testHook_cleanup();   // the copy is taken after the attach
    EXPECT_EQ(s().getObject(svell).copied_effect_text_def, kAhri);
    bool granted = false;
    for (const auto& r : s().getObject(ahri).granted_abilities)
        if (r.source_def_id == kAhri && r.ability_index == 0) granted = true;
    EXPECT_TRUE(granted) << "its activated abilities are granted to the bearer again";

    s().battlefields[0].controller = P1;
    engine_->testHook_scoringStep();
    EXPECT_EQ(s().player(P1).score, 3) << "hold 1 + Ahri's ability twice";
}

TEST_F(GapChoicesEngine, Svellsongur_CopyEndsWhenDetached) {
    auto ahri = unit(P1, kAhri, 0);
    auto svell = gear(P1, kSvellsongur);
    attach(svell, ahri);
    engine_->testHook_cleanup();
    engine_->testHook_executor().unattachGear(svell);
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(svell).copied_effect_text_def, kInvalidId);
    s().battlefields[0].controller = P1;
    engine_->testHook_scoringStep();
    EXPECT_EQ(s().player(P1).score, 2) << "hold 1 + Ahri once";
}

// ═══ Mechanism: last-known information of units dying together ═══════════

TEST_F(GapChoicesEngine, LonelyPoro_DyingTogetherIsNotAlone) {
    for (CardDefId poro_def : {kLonelyPoro, kLonelyPoro777}) {
        auto poro = unit(P1, poro_def, 0);
        auto friend_ = unit(P1, kInvalidId, 0);
        s().getObject(poro).damage_marked = 9;
        s().getObject(friend_).damage_marked = 9;
        engine_->testHook_processLethalDamage();
        ASSERT_FALSE(s().getObject(poro).location.has_value());
        EXPECT_EQ(s().getObject(poro).last_known.other_friendly_units_here, 1);
        deckCard(P1);
        const int hand = static_cast<int>(s().player(P1).hand.size());
        s().chain.items.clear();
        auto c = ctxFor(P1, poro);
        card_registry.get(poro_def)->onTrigger(c, {});
        EXPECT_EQ(static_cast<int>(s().player(P1).hand.size()), hand)
            << poro_def << ": a friendly unit was here as I died";
    }
}

TEST_F(GapChoicesEngine, LonelyPoro_AloneHereDraws) {
    for (CardDefId poro_def : {kLonelyPoro, kLonelyPoro777}) {
        auto poro = unit(P1, poro_def, 0);
        unit(P1, kInvalidId, 1);   // elsewhere
        unit(P2, kInvalidId, 0);   // an enemy here doesn't count
        s().getObject(poro).damage_marked = 9;
        engine_->testHook_processLethalDamage();
        deckCard(P1);
        const int hand = static_cast<int>(s().player(P1).hand.size());
        s().chain.items.clear();
        auto c = ctxFor(P1, poro);
        card_registry.get(poro_def)->onTrigger(c, {});
        EXPECT_EQ(static_cast<int>(s().player(P1).hand.size()), hand + 1) << poro_def;
    }
}

// ═══ Opponents' choices through the engine ═════════════════════════════════

// 663 Call to Battle: the unit and its destination as it's played; the
// opponent picks their unit as it resolves.
TEST_F(GapChoicesEngine, CallToBattle_DestinationAtPlayOpponentPicksTheirUnit) {
    pool(P1, 3);
    s().battlefields[0].controller = P1;
    s().battlefields[1].controller = P1;
    auto mine = unit(P1);
    auto other = unit(P1, kInvalidId, 0);
    auto theirs_a = unit(P2);
    auto theirs_b = unit(P2);
    auto spell = handCard(P1, kCallToBattle);
    a1_.pick = [&](const std::vector<Intent>& l) -> std::optional<Intent> {
        if (auto i = objectOf(l, mine)) return i;
        return valueOf(l, 1);   // battlefield #1
    };
    a2_.pick = [&](const std::vector<Intent>& l) { return objectOf(l, theirs_b); };
    play(P1, spell);
    ASSERT_GE(a1_.log.size(), 3u);
    EXPECT_EQ(a1_.log[0], "C");
    EXPECT_EQ(a1_.log[1], "C") << "the destination is chosen as it is played (CR 355.4)";
    EXPECT_EQ(a1_.choices[1].size(), 2u) << "both battlefields you control";
    EXPECT_TRUE(s().getObject(other).battlefieldId().has_value());
    ASSERT_EQ(a2_.choices.size(), 1u) << "the opponent makes their own choice";
    EXPECT_TRUE(objectOf(a2_.choices[0], theirs_a).has_value());
    // (A combat may follow at the battlefield, so check the moves themselves.)
    EXPECT_EQ(s().getObject(mine).moves_this_turn, 1);
    EXPECT_EQ(s().getObject(theirs_b).moves_this_turn, 1) << "the opponent's pick moved";
    EXPECT_EQ(s().getObject(theirs_a).moves_this_turn, 0);
    EXPECT_TRUE(s().getObject(theirs_a).isAtBase());
}

// ═══ Direct-resolution tests (CardTestFixture) ═════════════════════════════

class GapChoicesTest : public CardTestFixture {
protected:
    GameObjectId gearAt(PlayerId p, CardDefId def_id, GameObjectId attached_to = kInvalidId) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = p;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& d = card_db.get(def_id);
            g.name = d.name; g.tags = d.tags; g.keywords = d.keywords;
            g.might_bonus = d.might_bonus;
        } else {
            g.name = "TestEquipment";
            g.tags = {"Equipment"};
        }
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        if (attached_to != kInvalidId) {
            auto& u = state.getObject(attached_to);
            g.attached_to = attached_to;
            g.location = u.location;
            g.zone = u.zone;
            u.attachments.push_back(id);
        }
        return id;
    }
    void trashCard(PlayerId p, GameObjectId& out) {
        out = state.createObject();
        auto& c = state.getObject(out);
        c.owner = c.controller = p;
        c.card_type = CardType::Spell;
        c.name = "Trashed";
        c.zone = ZoneType::Trash;
        state.player(p).trash.push_back(out);
    }
    /// Activate ability 0 of `def_id` through the resume loop.
    void driveActivate(CardDefId def_id, PlayerId controller, GameObjectId source,
                       std::function<Intent(const std::vector<Intent>&)> picker,
                       EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = ri.is_activated_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{state, events, exec, controller, source};
        card->onActivate(ctx, 0, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onActivate(ctx, 0, {});
        }
        state.chain.resuming.reset();
    }
};

// 237 King's Edict: the opponent chooses which of their units dies.
TEST_F(GapChoicesTest, KingsEdict_OpponentChoosesTheUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto weak = addUnit(P2, kInvalidId, 1, 0);
    auto strong = addUnit(P2, kInvalidId, 7, 0);
    auto mine = addUnit(P1, kInvalidId, 1, 0);
    auto spell = addToHand(P1, kKingsEdict);
    std::vector<PlayerId> asked;
    std::vector<GameObjectId> offered;
    driveResumable(kKingsEdict, P1, spell, [&](const std::vector<Intent>& l) {
        asked.push_back(l.front().player);
        for (auto& i : l) offered.push_back(i.chosen_objects.front());
        return *objectOf(l, strong);   // the opponent's own call, not "the weakest"
    }, exec);
    ASSERT_EQ(asked.size(), 1u);
    EXPECT_EQ(asked[0], P2) << "each OTHER player chooses";
    EXPECT_EQ(std::count(offered.begin(), offered.end(), mine), 0) << "a unit you don't control";
    EXPECT_FALSE(state.getObject(strong).location.has_value());
    EXPECT_TRUE(state.getObject(weak).location.has_value());
}

// 665 Disposal Order: the player chooses which cards, up to 3.
TEST_F(GapChoicesTest, DisposalOrder_PlayerChoosesTheCards) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    GameObjectId oldest, mid, newest;
    trashCard(P2, oldest);
    trashCard(P2, mid);
    trashCard(P2, newest);
    auto spell = addToHand(P1, kDisposalOrder);
    int prompts = 0;
    driveResumable(kDisposalOrder, P1, spell, [&](const std::vector<Intent>& l) {
        ++prompts;
        if (prompts == 1) return *valueOf(l, 0);          // mode: recycle
        if (auto i = objectOf(l, oldest)) return *i;      // not the most recent
        for (auto& i : l) if (isDone(i)) return i;
        return l.front();
    }, exec);
    EXPECT_EQ(prompts, 3) << "mode, one card, Done";
    EXPECT_TRUE(inDeck(P2, oldest));
    EXPECT_TRUE(inTrash(P2, mid));
    EXPECT_TRUE(inTrash(P2, newest));
    EXPECT_FALSE(inTrash(P2, oldest)) << "it left the trash";
}

TEST_F(GapChoicesTest, DisposalOrder_AtMostThree) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<GameObjectId> cards(4);
    for (auto& c : cards) trashCard(P2, c);
    auto spell = addToHand(P1, kDisposalOrder);
    size_t max_offered = 0;
    driveResumable(kDisposalOrder, P1, spell, [&](const std::vector<Intent>& l) {
        if (auto i = valueOf(l, 0); i && l.size() == 2 && l.front().chosen_objects.empty()) return *i;
        max_offered = std::max(max_offered, l.size());
        for (auto& i : l) if (!i.chosen_objects.empty()) return i;
        return l.front();
    }, exec);
    EXPECT_EQ(deckSize(P2), 3) << "up to 3";
    EXPECT_EQ(trashSize(P2), 1);
}

// 594 Double Trouble: the rest go to the bottom in a random order (CR 417.5).
TEST_F(GapChoicesTest, DoubleTrouble_RestRecycledToTheBottom) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto keep = addToDeck(P1, kInvalidId);
    auto a = addToDeck(P1, kInvalidId);
    auto b = addToDeck(P1, kInvalidId);
    auto c = addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kDoubleTrouble);
    driveResumable(kDoubleTrouble, P1, spell, [](const std::vector<Intent>& l) { return l.front(); },
                   exec);
    const auto& deck = state.player(P1).main_deck;
    ASSERT_EQ(deck.size(), 4u);
    EXPECT_EQ(deck.back(), keep) << "the unlooked-at card is now on top";
    std::set<GameObjectId> bottom(deck.begin(), deck.begin() + 3);
    EXPECT_EQ(bottom, (std::set<GameObjectId>{a, b, c}));
}

// 700 The List: any tag in the card pool, even one on no visible card,
// asked in steps of at most kNumIntChoices options.
TEST_F(GapChoicesTest, TheList_NamesATagOnNoVisibleCard) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::set<std::string> pool;
    for (const auto& [id, d] : card_db.all()) for (const auto& t : d.tags) pool.insert(t);
    ASSERT_GT(pool.size(), static_cast<size_t>(openspiel::kNumIntChoices))
        << "more tags than int slots: the two-step path is exercised";
    auto gearId = gearAt(P1, kTheList);
    const std::string want = "Yordle";   // no unit on the board or in hand
    ASSERT_TRUE(pool.count(want));
    int prompts = 0;
    driveResumableTrigger(kTheList, P1, gearId, [&](const std::vector<Intent>& l) -> Intent {
        ++prompts;
        EXPECT_LE(l.size(), static_cast<size_t>(openspiel::kNumIntChoices));
        std::set<int> values;
        for (auto& i : l) values.insert(i.chosen_value.value_or(-1));
        EXPECT_EQ(values.size(), l.size()) << "each option its own int slot";
        if (auto i = labelled(l, want)) return *i;
        for (auto& i : l) {
            const auto sep = i.choice_label.find(" ... ");
            if (sep != std::string::npos && i.choice_label.substr(0, sep) <= want &&
                want <= i.choice_label.substr(sep + 5)) return i;
        }
        ADD_FAILURE() << "no option leads to " << want;
        return l.front();
    }, exec, TriggerType::AsYouPlayMe);
    EXPECT_EQ(prompts, 2);
    EXPECT_EQ(state.getObject(gearId).string_state["named_tag"], want);
}

// 373 Azir, Ascendant: the Equipment move is "you may".
TEST_F(GapChoicesTest, Azir_MayDeclineOrChooseTheEquipment) {
    for (bool accept : {false, true}) {
        EffectExecutor exec(state, events, card_db, &card_registry);
        auto azir = addUnit(P1, kAzir, 0, 0);
        auto other = addUnit(P1, kInvalidId, 2, 1);
        auto e1 = gearAt(P1, kInvalidId, other);
        auto e2 = gearAt(P1, kInvalidId, other);
        int asked_yes_no = 0;
        driveActivate(kAzir, P1, azir, [&](const std::vector<Intent>& l) -> Intent {
            if (auto i = objectOf(l, other)) return *i;
            if (valueOf(l, 0) && valueOf(l, 1)) {
                ++asked_yes_no;
                return *valueOf(l, accept ? 1 : 0);
            }
            if (auto i = objectOf(l, e2)) return *i;
            return l.front();
        }, exec);
        EXPECT_EQ(asked_yes_no, 1);
        EXPECT_EQ(state.getObject(azir).battlefieldId(), std::optional<BattlefieldId>(1)) << "swapped";
        EXPECT_EQ(state.getObject(other).battlefieldId(), std::optional<BattlefieldId>(0));
        EXPECT_EQ(state.getObject(e1).attached_to, std::optional<GameObjectId>(other));
        EXPECT_EQ(state.getObject(e2).attached_to,
                  std::optional<GameObjectId>(accept ? azir : other));
    }
}

// 970 Chronobreak: the opponent orders the units they own.
TEST_F(GapChoicesTest, Chronobreak_OwnerOrdersTheirOwnUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.turn.turn_player = P1;
    auto u1 = addUnit(P2, kInvalidId, 2);   // owned by P2, controlled by P1
    auto u2 = addUnit(P2, kInvalidId, 2);
    for (auto u : {u1, u2}) {
        state.getObject(u).controller = P1;
        state.getObject(u).location = BaseLocation{P1};
    }
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Mind);
    auto spell = addToHand(P1, kChronobreak);
    std::vector<std::pair<PlayerId, size_t>> prompts;
    driveResumable(kChronobreak, P1, spell, [&](const std::vector<Intent>& l) -> Intent {
        prompts.emplace_back(l.front().player, l.size());
        if (prompts.size() == 1) return *valueOf(l, 2);   // X = 2
        if (prompts.size() == 2) return *objectOf(l, u1);
        if (prompts.size() == 3) return *objectOf(l, u2);
        return *objectOf(l, u2);   // the owner's order: u2 first
    }, exec);
    ASSERT_EQ(prompts.size(), 4u);
    EXPECT_EQ(prompts[3].first, P2) << "the owner chooses the order of their units";
    EXPECT_EQ(prompts[3].second, 2u);
    for (auto u : {u1, u2}) {
        EXPECT_TRUE(state.getObject(u).location.has_value());
        EXPECT_EQ(state.getObject(u).controller, P2) << "played back by its owner";
    }
}

// ═══ Card::equipCost (CR 818 / 821.1.c) ════════════════════════════════════

// Every Equip cost the cards expose matches their printed "[Equip] ..." text;
// costs with non-rune parts are not offered to [Weaponmaster].
TEST_F(GapChoicesTest, EquipCost_MatchesThePrintedCostOfEveryEquipment) {
    int checked = 0;
    for (const auto& [id, d] : card_db.all()) {
        if (d.card_type != CardType::Gear) continue;
        const std::string& t = d.ability_text;
        const size_t at = t.find("[Equip]");
        if (at == std::string::npos) continue;
        const Card* card = card_registry.get(id);
        ASSERT_NE(card, nullptr);
        const auto spec = card->equipCost(state, kInvalidId, kInvalidId);
        size_t p = at + 7;
        while (p < t.size() && t[p] == ' ') ++p;
        if (t.compare(p, 3, "\xE2\x80\x94") == 0) {   // "[Equip] — <other costs>"
            EXPECT_FALSE(spec.payable_shape) << d.name;
            ++checked;
            continue;
        }
        int energy = 0;
        std::vector<std::vector<Domain>> power;
        while (p < t.size() && t[p] == '[') {
            const size_t q = t.find(']', p);
            const std::string tok = t.substr(p + 1, q - p - 1);
            if (std::isdigit(static_cast<unsigned char>(tok[0]))) energy += std::stoi(tok);
            else if (tok == "R") power.push_back({Domain::Fury});
            else if (tok == "G") power.push_back({Domain::Calm});
            else if (tok == "B") power.push_back({Domain::Mind});
            else if (tok == "O") power.push_back({Domain::Body});
            else if (tok == "P") power.push_back({Domain::Chaos});
            else if (tok == "Y") power.push_back({Domain::Order});
            else if (tok == "A") power.push_back({});
            else if (tok == "C") power.push_back(d.domains);
            p = q + 1;
        }
        EXPECT_TRUE(spec.payable_shape) << d.name;
        EXPECT_EQ(spec.energy, energy) << d.name;
        EXPECT_EQ(spec.power, power) << d.name;
        ++checked;
    }
    EXPECT_GE(checked, 40);
}

TEST_F(GapChoicesTest, Weaponmaster_PricesThroughTheHook) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto wm = addUnit(P1, kInvalidId, 2);
    CardContext ctx{state, events, exec, P1, wm};
    auto gauntlets = gearAt(P1, kGauntlets);
    auto c = weaponmaster::reducedEquipCost(ctx, state.getObject(gauntlets), wm);
    EXPECT_TRUE(c.payable_shape);
    EXPECT_EQ(c.energy, 1) << "[3] reduced by its Might 2";
    EXPECT_TRUE(c.power.empty()) << "[A] less";
    auto skyfall = gearAt(P1, kSkyfall);
    c = weaponmaster::reducedEquipCost(ctx, state.getObject(skyfall), wm);
    EXPECT_EQ(c.energy, 1);
    EXPECT_TRUE(c.power.empty()) << "[1][R] minus a Power";
    auto heirloom = gearAt(P1, kHeirloom);
    EXPECT_FALSE(weaponmaster::reducedEquipCost(ctx, state.getObject(heirloom), wm).payable_shape)
        << "\"[Equip] — Spend 1 XP\" isn't paid with runes";
    (void)kLongSword; (void)kDoransBlade; (void)kSpinningAxe; (void)kVolibear;
}

}  // namespace
}  // namespace riftbound::test
