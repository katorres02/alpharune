/// @file test_gap_playloc.cpp
/// Play location of a permanent played by an EFFECT (CR 355.2): the player
/// playing it chooses base or a battlefield they control, plus whatever the
/// permanent's own permissions allow ([Deploy] gear only to battlefields,
/// CR 830; [Ambush], CR 822.1.c). Mechanism: legalPlayLocations /
/// EffectExecutor::playLocationsFor + Card::pickPlayLocation. Also the
/// printed-cost trigger condition (Card::triggerConditionOnDefs).

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kReinforce = 62, kPromisingFuture = 115, kDazzlingAurora = 160,
                    kNocturne = 194, kPerchedGrimwyrm = 338, kZeroDrive = 412,
                    kLastRites = 471, kRekSai = 491, kArcaneShift = 515,
                    kSoulspinner = 910, kSanctumConservator = 1014, kClocktower = 1047,
                    kAngler = 1089, kTranspos = 1091, kConcertPoster = 1092,
                    kHerald = 1094, kRestorer = 1096;

using Pending = EffectExecutor::PendingChoice;
using Picker = std::function<Intent(const Pending&)>;

bool isLocationPrompt(const Pending& p) { return p.label.rfind("location: ", 0) == 0; }

Intent pickVal(const std::vector<Intent>& legal, int v) {
    for (const auto& i : legal)
        if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
    ADD_FAILURE() << "value " << v << " not offered";
    return legal.front();
}
Intent pickObj(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
    ADD_FAILURE() << "object " << id << " not offered";
    return legal.front();
}
std::vector<int> values(const std::vector<Intent>& legal) {
    std::vector<int> out;
    for (const auto& i : legal) out.push_back(i.chosen_value.value_or(-99));
    return out;
}
bool hasBf(const std::vector<LocationId>& locs, BattlefieldId id) {
    for (const auto& l : locs)
        if (const auto* b = std::get_if<BattlefieldLocation>(&l); b && b->id == id) return true;
    return false;
}
bool hasBase(const std::vector<LocationId>& locs) {
    for (const auto& l : locs)
        if (std::holds_alternative<BaseLocation>(l)) return true;
    return false;
}

class GapPlayLocTest : public CardTestFixture {
protected:
    EffectExecutor exec{state, events, card_db, &card_registry};

    void control(BattlefieldId bf, PlayerId p) { state.battlefields[bf].controller = p; }

    GameObjectId addGear(PlayerId owner, CardDefId def) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_def_id = def;
        g.card_type = CardType::Gear;
        g.name = card_db.get(def).name;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def) {
        auto id = addToHand(owner, def);
        auto& h = state.player(owner).hand;
        h.erase(std::find(h.begin(), h.end(), id));
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    GameObjectId addBanished(PlayerId owner, CardDefId def) {
        auto id = addToHand(owner, def);
        auto& h = state.player(owner).hand;
        h.erase(std::find(h.begin(), h.end(), id));
        state.getObject(id).zone = ZoneType::Banishment;
        state.player(owner).banishment.push_back(id);
        return id;
    }
    bool atBf(GameObjectId id, BattlefieldId bf) {
        const auto& o = state.getObject(id);
        return o.battlefieldId().has_value() && *o.battlefieldId() == bf;
    }

    // Runs `call` like ChainManager::stepResolve: re-enters after each answer.
    // Records every prompt so tests can check who was asked what.
    std::vector<Pending> prompts;
    void run(ChainItem ri, const std::function<void(CardContext&)>& call, const Picker& pick) {
        state.chain.resuming = ri;
        CardContext ctx{state, events, exec, ri.controller, ri.source};
        ctx.firing_trigger = ri.fired_trigger;
        call(ctx);
        for (int guard = 0; exec.hasPendingChoice() && guard < 32; ++guard) {
            auto pending = exec.consumePendingChoice();
            prompts.push_back(pending);
            exec.recordChoice(pick(pending));
            call(ctx);
        }
        state.chain.resuming.reset();
    }
    ChainItem item(CardDefId def, PlayerId controller, GameObjectId source) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.source = source;
        ri.card_def_id = def;
        return ri;
    }
    void trigger(CardDefId def, PlayerId controller, GameObjectId source, TriggerType t,
                 const Picker& pick, GameObjectId subject = kInvalidId) {
        auto ri = item(def, controller, source);
        ri.is_ability = true;
        ri.fired_trigger = t;
        ri.triggering_subject = subject;
        Card* c = card_registry.get(def);
        run(ri, [&](CardContext& ctx) { c->onTrigger(ctx, {}); }, pick);
    }
    void resolve(CardDefId def, PlayerId controller, GameObjectId source,
                 const std::vector<GameObjectId>& targets, const Picker& pick) {
        auto ri = item(def, controller, source);
        ri.is_spell = true;
        ri.targets = targets;
        Card* c = card_registry.get(def);
        run(ri, [&](CardContext& ctx) { c->onResolve(ctx, targets); }, pick);
    }
    void activate(CardDefId def, PlayerId controller, GameObjectId source, const Picker& pick) {
        auto ri = item(def, controller, source);
        ri.is_ability = ri.is_activated_ability = true;
        Card* c = card_registry.get(def);
        run(ri, [&](CardContext& ctx) { c->onActivate(ctx, 0, {}); }, pick);
    }
    int locationPrompts() const {
        int n = 0;
        for (const auto& p : prompts) n += isLocationPrompt(p);
        return n;
    }
};

// ── The mechanism ───────────────────────────────────────────────────────────

TEST_F(GapPlayLocTest, UnitMayGoToBaseOrABattlefieldItsPlayerControls) {
    control(0, P1);
    control(1, P2);
    auto u = addToHand(P1, kRestorer);
    auto locs = exec.playLocationsFor(P1, u);
    EXPECT_TRUE(hasBase(locs));
    EXPECT_TRUE(hasBf(locs, 0));
    EXPECT_FALSE(hasBf(locs, 1)) << "not a battlefield the opponent controls";
    EXPECT_EQ(locs.size(), 2u);
}

TEST_F(GapPlayLocTest, PlainGearGoesToBaseOnlyAndDeployGearOnlyToBattlefields) {
    control(0, P1);
    auto plain = addToHand(P1, kLastRites);
    auto locs = exec.playLocationsFor(P1, plain);
    ASSERT_EQ(locs.size(), 1u);
    EXPECT_TRUE(hasBase(locs));

    auto deploy = addToHand(P1, kConcertPoster);
    locs = exec.playLocationsFor(P1, deploy);
    EXPECT_FALSE(hasBase(locs)) << "[Deploy]: only to a battlefield (CR 830)";
    EXPECT_TRUE(hasBf(locs, 0));
    EXPECT_TRUE(hasBf(locs, 1));
}

TEST_F(GapPlayLocTest, AmbushAddsBattlefieldsWithMyUnitsForEffectPlays) {
    addUnit(P1, kInvalidId, 2, 1);   // my unit at an uncontrolled battlefield
    auto amb = addToHand(P1, kSoulspinner);
    ASSERT_TRUE(state.getObject(amb).hasKeyword(Keyword::Ambush));
    EXPECT_TRUE(hasBf(exec.playLocationsFor(P1, amb), 1));
    // The play-from-hand generator offers those only with Reaction timing.
    EXPECT_FALSE(hasBf(legalPlayLocations(state, card_db, &card_registry, P1, amb,
                                          /*include_ambush=*/false), 1));
}

TEST_F(GapPlayLocTest, StandingRestrictionsAndCardRestrictionsApply) {
    control(0, P1);
    auto u = addToHand(P1, kRestorer);
    state.player(P1).units_play_base_only = true;   // Mageseeker Warden
    auto locs = exec.playLocationsFor(P1, u);
    ASSERT_EQ(locs.size(), 1u);
    EXPECT_TRUE(hasBase(locs));
    state.player(P1).units_play_base_only = false;

    // Perched Grimwyrm lists its own locations (none: nothing conquered).
    auto grim = addToHand(P1, kPerchedGrimwyrm);
    EXPECT_EQ(exec.playLocationsFor(P1, grim),
              card_registry.get(kPerchedGrimwyrm)->getPlayLocations(state, P1));
}

TEST_F(GapPlayLocTest, SingleLegalLocationIsNotAsked) {
    auto unit = addToDeck(P1, kRestorer);
    auto aurora = addGear(P1, kDazzlingAurora);
    trigger(kDazzlingAurora, P1, aurora, TriggerType::AtEndOfTurn,
            [](const Pending& p) { return p.legal.front(); });
    EXPECT_EQ(locationPrompts(), 0);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
}

TEST_F(GapPlayLocTest, ChoiceSlotsAreStableBaseZeroBattlefieldIndexPlusOne) {
    control(1, P1);
    auto unit = addToDeck(P1, kRestorer);
    auto aurora = addGear(P1, kDazzlingAurora);
    trigger(kDazzlingAurora, P1, aurora, TriggerType::AtEndOfTurn,
            [](const Pending& p) { return pickVal(p.legal, 2); });
    ASSERT_EQ(locationPrompts(), 1);
    EXPECT_EQ(values(prompts[0].legal), (std::vector<int>{0, 2}));
    EXPECT_EQ(prompts[0].player, P1);
    EXPECT_TRUE(atBf(unit, 1));
}

// ── Cards ───────────────────────────────────────────────────────────────────

TEST_F(GapPlayLocTest, DazzlingAuroraPlaysToBase) {
    control(0, P1);
    auto unit = addToDeck(P1, kRestorer);
    auto aurora = addGear(P1, kDazzlingAurora);
    trigger(kDazzlingAurora, P1, aurora, TriggerType::AtEndOfTurn,
            [](const Pending& p) { return pickVal(p.legal, 0); });
    EXPECT_TRUE(state.getObject(unit).isAtBase());
}

TEST_F(GapPlayLocTest, LastRitesPlaysTheUnitWhereIChoose) {
    control(1, P1);
    auto rites = addGear(P1, kLastRites);
    auto unit = addToTrash(P1, kRestorer);   // [2], no Power
    addRune(P1, Domain::Order);
    addRune(P1, Domain::Order);
    trigger(kLastRites, P1, rites, TriggerType::WhenIConquerOrHold, [&](const Pending& p) {
        if (isLocationPrompt(p)) return pickVal(p.legal, 2);
        if (p.legal.front().chosen_value.has_value()) return pickVal(p.legal, 1);
        return pickObj(p.legal, unit);
    });
    EXPECT_EQ(locationPrompts(), 1);
    EXPECT_TRUE(atBf(unit, 1));
    EXPECT_FALSE(inTrash(P1, unit));
    EXPECT_EQ(readyRuneCount(P1), 0) << "it still pays its cost";
}

TEST_F(GapPlayLocTest, TranspositionistPlaysTheSmallerUnitWhereIChoose) {
    control(0, P1);
    auto tp = addUnit(P1, kTranspos);
    auto big = addUnit(P1, kAngler, 0, 0);
    auto small = addToHand(P1, kRestorer);
    int step = 0;
    activate(kTranspos, P1, tp, [&](const Pending& p) {
        if (isLocationPrompt(p)) return pickVal(p.legal, 1);
        return pickObj(p.legal, step++ == 0 ? big : small);
    });
    EXPECT_TRUE(inHand(P1, big));
    EXPECT_TRUE(atBf(small, 0));
}

TEST_F(GapPlayLocTest, MeticulousRestorationistPaysOnceAndPlaysWhereIChoose) {
    control(0, P1);
    auto resto = addToTrash(P1, kRestorer);
    auto big = addUnit(P1, kAngler);
    addRune(P1, Domain::Order);
    addRune(P1, Domain::Order);
    trigger(kRestorer, P1, resto, TriggerType::WhenYouPlayACard, [](const Pending& p) {
        return isLocationPrompt(p) ? pickVal(p.legal, 1) : pickVal(p.legal, 1);
    }, big);
    EXPECT_EQ(locationPrompts(), 1);
    EXPECT_TRUE(atBf(resto, 0));
    EXPECT_EQ(readyRuneCount(P1), 1) << "[Y] paid once, not again after the location answer";
}

TEST_F(GapPlayLocTest, PrintedCostConditionIsCheckedAsTheCardIsPlayed) {
    auto resto = addToTrash(P1, kRestorer);
    auto cons = addUnit(P1, kSanctumConservator);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto cheap = addUnit(P1, kHerald);   // [5]
    events.emit(CardPlayedEvent{cheap, P1, CardType::Unit, 1, 5});
    for (const auto& ci : state.chain.items) {
        EXPECT_NE(ci.source, resto) << "a cheaper card doesn't trigger Restorationist";
        EXPECT_NE(ci.source, cons) << "a cheaper card doesn't trigger Conservator";
    }
    auto big = addUnit(P1, kAngler);     // [8]
    events.emit(CardPlayedEvent{big, P1, CardType::Unit, 2, 8});
    int resto_items = 0, cons_items = 0;
    for (const auto& ci : state.chain.items) {
        resto_items += ci.source == resto;
        cons_items += ci.source == cons;
    }
    EXPECT_EQ(resto_items, 1);
    EXPECT_EQ(cons_items, 1);
}

TEST_F(GapPlayLocTest, ClocktowerGuardianPlaysTheBanishedCardWhereIChoose) {
    control(0, P1);
    auto guard = addUnit(P1, kClocktower);
    auto top = addToDeck(P1, kRestorer);
    trigger(kClocktower, P1, guard, TriggerType::WhenIDie, [](const Pending& p) {
        return isLocationPrompt(p) ? pickVal(p.legal, 1) : pickVal(p.legal, 1);
    });
    EXPECT_TRUE(atBf(top, 0));
}

TEST_F(GapPlayLocTest, NocturnePaysAAndPlaysWhereItsOwnerChooses) {
    control(1, P1);
    auto n = addToDeck(P1, kNocturne);
    addRune(P1, Domain::Body);
    trigger(kNocturne, P1, n, TriggerType::WhenIRevealedFromTop, [](const Pending& p) {
        return isLocationPrompt(p) ? pickVal(p.legal, 2) : p.legal.back();
    });
    EXPECT_TRUE(atBf(n, 1));
    EXPECT_EQ(readyRuneCount(P1), 0);
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u) << "[A] paid by recycling the rune";
}

TEST_F(GapPlayLocTest, ArcaneShiftAsksTheOwner) {
    // A unit P1 stole from P2: its owner P2 plays it, choosing among P2's
    // locations.
    control(0, P1);
    control(1, P2);
    auto stolen = addUnit(P2, kRestorer);
    state.getObject(stolen).controller = P1;
    state.getObject(stolen).location = BaseLocation{P1};
    auto src = pushSpellOnChain(P1, kArcaneShift);
    state.chain.items.clear();
    resolve(kArcaneShift, P1, src, {stolen}, [](const Pending& p) {
        return isLocationPrompt(p) ? pickVal(p.legal, 2) : p.legal.front();
    });
    ASSERT_EQ(locationPrompts(), 1);
    for (const auto& p : prompts)
        if (isLocationPrompt(p)) {
            EXPECT_EQ(p.player, P2);
            EXPECT_EQ(values(p.legal), (std::vector<int>{0, 2}));
        }
    EXPECT_TRUE(atBf(stolen, 1));
    EXPECT_EQ(state.getObject(stolen).controller, P2);
}

TEST_F(GapPlayLocTest, ReinforcePlaysWhereIChoose) {
    control(0, P1);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kInvalidId);
    auto unit = addToDeck(P1, kRestorer);   // [2] - 5 = free
    auto src = pushSpellOnChain(P1, kReinforce);
    state.chain.items.clear();
    resolve(kReinforce, P1, src, {}, [&](const Pending& p) {
        return isLocationPrompt(p) ? pickVal(p.legal, 1) : pickObj(p.legal, unit);
    });
    EXPECT_TRUE(atBf(unit, 0));
}

TEST_F(GapPlayLocTest, RekSaiOffersHereBaseAndMyBattlefields) {
    control(0, P1);
    auto rek = addUnit(P1, kRekSai, 0, 1);   // attacking at battlefield 1
    addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kRestorer);
    addRune(P1, Domain::Order);
    addRune(P1, Domain::Order);
    trigger(kRekSai, P1, rek, TriggerType::WhenIAttack, [&](const Pending& p) {
        if (isLocationPrompt(p)) return pickVal(p.legal, 1);   // my other battlefield
        if (p.legal.front().chosen_value.has_value()) return pickVal(p.legal, 1);
        return pickObj(p.legal, top);
    });
    ASSERT_EQ(locationPrompts(), 1);
    for (const auto& p : prompts)
        if (isLocationPrompt(p)) EXPECT_EQ(values(p.legal), (std::vector<int>{0, 1, 2}));
    EXPECT_TRUE(atBf(top, 0));
}

TEST_F(GapPlayLocTest, ZeroDriveAsksForEachUnit) {
    control(1, P1);
    auto drive = addGear(P1, kZeroDrive);
    auto a = addBanished(P1, kRestorer);
    auto b = addBanished(P1, kHerald);
    state.getObject(drive).tracked_objects = {a, b};
    activate(kZeroDrive, P1, drive, [&](const Pending& p) {
        return pickVal(p.legal, prompts.size() == 1 ? 2 : 0);
    });
    EXPECT_EQ(locationPrompts(), 2);
    EXPECT_TRUE(atBf(a, 1));
    EXPECT_TRUE(state.getObject(b).isAtBase());
    EXPECT_EQ(state.getObject(drive).zone, ZoneType::Banishment);
}

TEST_F(GapPlayLocTest, PromisingFutureEachPlayerChoosesForTheirOwnCard) {
    control(0, P1);
    control(1, P2);
    auto mine = addToDeck(P1, kRestorer);
    auto theirs = addToDeck(P2, kRestorer);
    auto src = pushSpellOnChain(P1, kPromisingFuture);
    state.chain.items.clear();
    resolve(kPromisingFuture, P1, src, {}, [&](const Pending& p) {
        if (!isLocationPrompt(p)) return p.legal.front();
        return pickVal(p.legal, p.player == P1 ? 1 : 2);
    });
    ASSERT_EQ(locationPrompts(), 2);
    EXPECT_EQ(prompts[prompts.size() - 2].player, P2) << "the next player plays first";
    EXPECT_EQ(prompts.back().player, P1);
    EXPECT_TRUE(atBf(theirs, 1));
    EXPECT_TRUE(atBf(mine, 0));
}

}  // namespace
}  // namespace riftbound::test
