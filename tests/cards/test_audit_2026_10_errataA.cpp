/// @file test_audit_2026_10_errataA.cpp
/// Cards with published errata (errata/errata_index.json), 2026-10-28 audit
/// follow-up, batch errataA: each card does exactly what its errata says.

#include "tests/cards/card_test_fixture.h"

#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kUnlicensedArmory = 23;
constexpr CardDefId kBlindFury = 25;
constexpr CardDefId kZhonya = 77;
constexpr CardDefId kAvaAchiever = 107;
constexpr CardDefId kTeemoStrategist = 121;   // a unit with [Hidden]
constexpr CardDefId kPackOfWonders = 181;
constexpr CardDefId kNocturne = 194;
constexpr CardDefId kTideturner = 199;
constexpr CardDefId kKarma = 235;
constexpr CardDefId kBaitedHook = 242;
constexpr CardDefId kIcathianRain = 247;
constexpr CardDefId kDragonsRage = 255;
constexpr CardDefId kUnforgiven = 256;
constexpr CardDefId kTheBoss = 264;
constexpr CardDefId kDisintegrate = 5;        // a spell

class ErrataATest : public CardTestFixture {
protected:
    GameObjectId addPermanent(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        const auto& def = card_db.get(def_id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = def.card_type;
        obj.name = def.name;
        obj.domains = def.domains;
        obj.keywords = def.keywords;
        if (def.card_type == CardType::Legend) {
            obj.zone = ZoneType::LegendZone;
            state.player(owner).legend_zone = id;
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    static Intent byValue(const std::vector<Intent>& legal, int v) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
        return legal.front();
    }
    static Intent byObject(const std::vector<Intent>& legal, GameObjectId id) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
        return legal.front();
    }

    // Resolve an activated ability through its resume loop.
    void driveActivate(CardDefId def_id, PlayerId controller, GameObjectId source,
                       const std::vector<GameObjectId>& targets,
                       std::function<Intent(const std::vector<Intent>&)> picker,
                       EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onActivate(ctx, 0, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onActivate(ctx, 0, targets);
        }
        state.chain.resuming.reset();
    }

    int runesOf(PlayerId p) {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.isRune() && o.controller == p && o.location.has_value()) ++n;
        return n;
    }
};

// ── Unlicensed Armory: "you may pay [C] to heal it, exhaust it, and recall it"
TEST_F(ErrataATest, UnlicensedArmoryPaysFuryToSave) {
    auto armory = addPermanent(P1, kUnlicensedArmory);
    auto u = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    addRune(P1, Domain::Fury);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int asked = 0;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { ++asked; return byValue(l, 1); });
    driveActivate(kUnlicensedArmory, P1, armory, {u}, nullptr, exec);
    exec.killObject(u);
    EXPECT_EQ(asked, 1);
    EXPECT_TRUE(state.getObject(u).isAtBase());
    EXPECT_TRUE(state.getObject(u).is_exhausted);
    EXPECT_EQ(runesOf(P1), 0);   // the Fury rune paid [C]
}

TEST_F(ErrataATest, UnlicensedArmoryDeclineOrNoPowerLetsItDie) {
    auto armory = addPermanent(P1, kUnlicensedArmory);
    auto u = addUnit(P1, kInvalidId, 3, 0);
    auto v = addUnit(P1, kInvalidId, 3, 0);
    addRune(P1, Domain::Fury);
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { return byValue(l, 0); });
    driveActivate(kUnlicensedArmory, P1, armory, {u}, nullptr, exec);
    exec.killObject(u);
    EXPECT_TRUE(inTrash(P1, u));
    EXPECT_EQ(runesOf(P1), 1);
    // No matching power: not offered, the unit dies.
    for (auto& [id, o] : state.objects) if (o.isRune()) o.domains = {Domain::Calm};
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { return byValue(l, 1); });
    driveActivate(kUnlicensedArmory, P1, armory, {v}, nullptr, exec);
    exec.killObject(v);
    EXPECT_TRUE(inTrash(P1, v));
}

// ── The Boss: "you may pay [C], exhaust me, and spend its buff"
TEST_F(ErrataATest, TheBossChargesPowerAndIsOptional) {
    auto boss = addPermanent(P1, kTheBoss);
    auto u = addUnit(P1, kInvalidId, 3, 0);
    addRune(P1, Domain::Order);
    EffectExecutor exec(state, events, card_db, &card_registry);
    ASSERT_TRUE(exec.buffUnit(u));
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { return byValue(l, 0); });
    exec.killObject(u);
    EXPECT_TRUE(inTrash(P1, u));             // declined
    EXPECT_FALSE(state.getObject(boss).is_exhausted);

    auto w = addUnit(P1, kInvalidId, 3, 0);
    ASSERT_TRUE(exec.buffUnit(w));
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { return byValue(l, 1); });
    exec.killObject(w);
    EXPECT_TRUE(state.getObject(w).isAtBase());
    EXPECT_TRUE(state.getObject(boss).is_exhausted);
    EXPECT_EQ(state.getObject(w).buff_count, 0);
    EXPECT_EQ(runesOf(P1), 0);               // [C] paid with the Order rune
}

TEST_F(ErrataATest, TheBossNotOfferedWithoutPower) {
    addPermanent(P1, kTheBoss);
    auto u = addUnit(P1, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    ASSERT_TRUE(exec.buffUnit(u));
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& l) { return byValue(l, 1); });
    exec.killObject(u);
    EXPECT_TRUE(inTrash(P1, u));
}

// ── Zhonya's Hourglass: no effect while facedown.
TEST_F(ErrataATest, ZhonyaOnlyWorksFaceUp) {
    auto z = addPermanent(P1, kZhonya);
    auto& zo = state.getObject(z);
    zo.location = std::nullopt;
    zo.zone = ZoneType::FacedownZone;
    zo.is_hidden = true;
    zo.hidden_at = 0;
    state.battlefields[0].facedown.push_back(z);
    auto u = addUnit(P1, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.killObject(u);
    EXPECT_TRUE(inTrash(P1, u));

    auto z2 = addPermanent(P1, kZhonya);
    auto v = addUnit(P1, kInvalidId, 3, 0);
    exec.killObject(v);
    EXPECT_TRUE(state.getObject(v).isAtBase());
    EXPECT_TRUE(inTrash(P1, z2));
}

// ── Blind Fury: a revealed spell is played too; a unit's location is chosen.
TEST_F(ErrataATest, BlindFuryPlaysARevealedSpell) {
    auto top = addToDeck(P2, kDisintegrate);
    auto bf = addToHand(P1, kBlindFury);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<GameObjectId> queued;
    exec.setQueueSpell([&](PlayerId p, GameObjectId s) { EXPECT_EQ(p, P1); queued.push_back(s); });
    driveResumable(kBlindFury, P1, bf, nullptr, exec);
    ASSERT_EQ(queued.size(), 1u);
    EXPECT_EQ(queued[0], top);
    EXPECT_EQ(state.getObject(top).controller, P1);
    EXPECT_TRUE(state.player(P2).banishment.empty());
}

TEST_F(ErrataATest, BlindFuryUnitLandsWhereChosen) {
    state.battlefields[1].controller = P1;
    auto top = addToDeck(P2, kTeemoStrategist);
    auto bf = addToHand(P1, kBlindFury);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kBlindFury, P1, bf,
                   [](const std::vector<Intent>& l) { return l.back(); }, exec);
    const auto& u = state.getObject(top);
    EXPECT_EQ(u.controller, P1);
    ASSERT_TRUE(u.battlefieldId().has_value());
    EXPECT_EQ(*u.battlefieldId(), 1u);
}

// ── Ava Achiever: no [Hidden] of her own; [C] buys a Hidden card played here.
TEST_F(ErrataATest, AvaAchieverHasNoHiddenAndPaysMind) {
    EXPECT_FALSE(card_db.get(kAvaAchiever).keywords.has(Keyword::Hidden));
    auto ava = addUnit(P1, kAvaAchiever, 0, 0);
    auto card = addToHand(P1, kTeemoStrategist);
    addRune(P1, Domain::Mind);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kAvaAchiever, P1, ava,
                          [](const std::vector<Intent>& l) { return l.back(); }, exec,
                          TriggerType::WhenIAttack);
    EXPECT_FALSE(inHand(P1, card));
    ASSERT_TRUE(state.getObject(card).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(card).battlefieldId(), 0u);
    EXPECT_EQ(runesOf(P1), 0);
}

// ── Pack of Wonders: a facedown card can be returned.
TEST_F(ErrataATest, PackOfWondersReturnsAFacedownCard) {
    auto pack = addPermanent(P1, kPackOfWonders);
    auto fd = addToHand(P1, kTeemoStrategist);
    state.player(P1).hand.clear();
    auto& o = state.getObject(fd);
    o.zone = ZoneType::FacedownZone;
    o.is_hidden = true;
    o.hidden_at = 0;
    state.battlefields[0].facedown.push_back(fd);
    const Card* card = card_registry.get(kPackOfWonders);
    auto legal = card->enumerateLegalTargets(state, P1, 0, pack);
    EXPECT_NE(std::find(legal.begin(), legal.end(), fd), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), pack), legal.end());
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveActivate(kPackOfWonders, P1, pack, {},
                  [&](const std::vector<Intent>& l) { return byObject(l, fd); }, exec);
    EXPECT_TRUE(inHand(P1, fd));
    EXPECT_FALSE(state.getObject(fd).is_hidden);
    EXPECT_TRUE(state.battlefields[0].facedown.empty());
}

// ── Nocturne: banish, then a separate "may" to play for [A] (paid).
TEST_F(ErrataATest, NocturnePaysAToPlay) {
    auto n = addToDeck(P1, kNocturne);
    addRune(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kNocturne, P1, n,
                          [](const std::vector<Intent>& l) { return l.back(); }, exec,
                          TriggerType::WhenIRevealedFromTop);
    EXPECT_TRUE(state.getObject(n).location.has_value());
    EXPECT_EQ(runesOf(P1), 0);
}

TEST_F(ErrataATest, NocturneCanStayBanished) {
    auto n = addToDeck(P1, kNocturne);
    addRune(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumableTrigger(kNocturne, P1, n,
                          [&](const std::vector<Intent>& l) {
                              return call++ == 0 ? l.back() : l.front(); }, exec,
                          TriggerType::WhenIRevealedFromTop);
    EXPECT_EQ(state.getObject(n).zone, ZoneType::Banishment);
    EXPECT_EQ(runesOf(P1), 1);
}

// ── Tideturner: no trigger targets; swaps with a unit elsewhere.
TEST_F(ErrataATest, TideturnerSwapsWithChosenUnit) {
    EXPECT_EQ(card_registry.get(kTideturner)->getTargetRequirements().count, 0);
    auto t = addUnit(P1, kTideturner, 0, 0);
    auto same = addUnit(P1, kInvalidId, 2, 0);
    auto far = addUnit(P1, kInvalidId, 2, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<GameObjectId> offered;
    driveResumableTrigger(kTideturner, P1, t,
                          [&](const std::vector<Intent>& l) {
                              for (auto& i : l)
                                  if (!i.chosen_objects.empty())
                                      offered.push_back(i.chosen_objects[0]);
                              return l.back(); }, exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_EQ(std::find(offered.begin(), offered.end(), same), offered.end());
    EXPECT_EQ(*state.getObject(t).battlefieldId(), 1u);
    EXPECT_EQ(*state.getObject(far).battlefieldId(), 0u);
}

// ── Karma, Channeler: buffing an already-buffed unit adds nothing.
TEST_F(ErrataATest, KarmaBuffDoesNotStack) {
    auto k = addUnit(P1, kKarma, 0, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 2; ++i)
        driveResumableTrigger(kKarma, P1, k,
                              [&](const std::vector<Intent>& l) { return byObject(l, k); }, exec,
                              TriggerType::WhenYouRecycle);
    EXPECT_EQ(state.getObject(k).buff_count, 1);
}

// ── Baited Hook: [1][C] cost (Order power, no self-recycle); the play is a "may".
TEST_F(ErrataATest, BaitedHookCostAndDecline) {
    auto cost = card_registry.get(kBaitedHook)->getActivationCost();
    EXPECT_FALSE(cost.recycle_self);
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Order);

    auto hook = addPermanent(P1, kBaitedHook);
    auto victim = addUnit(P1, kInvalidId, 3, 0);
    auto deck_unit = addToDeck(P1, kTeemoStrategist);   // Might 2 <= 4
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveActivate(kBaitedHook, P1, hook, {victim},
                  [](const std::vector<Intent>& l) { return byValue(l, 0); }, exec);
    EXPECT_TRUE(inTrash(P1, victim));
    EXPECT_TRUE(inDeck(P1, deck_unit));                 // recycled, not played
    EXPECT_FALSE(state.getObject(deck_unit).location.has_value());
}

TEST_F(ErrataATest, BaitedHookBanishesAndPlaysChosenUnit) {
    auto hook = addPermanent(P1, kBaitedHook);
    auto victim = addUnit(P1, kInvalidId, 3, 0);
    auto deck_unit = addToDeck(P1, kTeemoStrategist);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveActivate(kBaitedHook, P1, hook, {victim},
                  [](const std::vector<Intent>& l) { return l.back(); }, exec);
    EXPECT_TRUE(inTrash(P1, victim));
    EXPECT_FALSE(inDeck(P1, deck_unit));
    EXPECT_TRUE(state.getObject(deck_unit).location.has_value());
}

// ── Icathian Rain: each "Deal 2 to a unit" picks its own unit.
TEST_F(ErrataATest, IcathianRainSplitsDamage) {
    auto a = addUnit(P2, kInvalidId, 4, 0);
    auto b = addUnit(P2, kInvalidId, 10, 1);
    auto rain = addToHand(P1, kIcathianRain);
    EffectExecutor exec(state, events, card_db, &card_registry);
    int call = 0;
    driveResumable(kIcathianRain, P1, rain,
                   [&](const std::vector<Intent>& l) {
                       return byObject(l, call++ < 2 ? a : b); }, exec);
    EXPECT_EQ(call, 6);
    EXPECT_TRUE(inTrash(P2, a));                       // 4 damage on a 4-Might unit
    EXPECT_EQ(state.getObject(b).damage_marked, 8);    // the other four hits
}

// ── Dragon's Rage: caster picks the destination and the second enemy unit.
TEST_F(ErrataATest, DragonsRageChoosesDestinationAndPartner) {
    auto mover = addUnit(P2, kInvalidId, 3, 0);
    auto stay = addUnit(P2, kInvalidId, 2, 0);
    auto partner = addUnit(P2, kInvalidId, 5, 1);
    auto other = addUnit(P2, kInvalidId, 1, 1);
    auto rage = addToHand(P1, kDragonsRage);
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<std::vector<Intent>> seen;
    driveResumable(kDragonsRage, P1, rage,
                   [&](const std::vector<Intent>& l) {
                       seen.push_back(l);
                       if (seen.size() == 1) return byObject(l, mover);
                       if (seen.size() == 2) return byValue(l, 2);   // battlefield 1
                       return byObject(l, partner); }, exec);
    ASSERT_EQ(seen.size(), 3u);
    // Destination never offers where the unit already is (battlefield 0 = 1).
    for (auto& i : seen[1]) EXPECT_NE(i.chosen_value.value_or(-1), 1);
    EXPECT_TRUE(inTrash(P2, mover));                         // took 5
    EXPECT_EQ(state.getObject(partner).damage_marked, 3);
    EXPECT_EQ(state.getObject(other).damage_marked, 0);
    EXPECT_EQ(state.getObject(stay).damage_marked, 0);
}

// ── Unforgiven: also moves a unit FROM its base to a chosen battlefield.
TEST_F(ErrataATest, UnforgivenMovesFromBase) {
    auto leg = addPermanent(P1, kUnforgiven);
    auto u = addUnit(P1, kInvalidId, 2);   // at base
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveActivate(kUnforgiven, P1, leg, {u},
                  [](const std::vector<Intent>& l) { return byValue(l, 2); }, exec);
    ASSERT_TRUE(state.getObject(u).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(u).battlefieldId(), 1u);
    driveActivate(kUnforgiven, P1, leg, {u}, nullptr, exec);
    EXPECT_TRUE(state.getObject(u).isAtBase());
}

}  // namespace
}  // namespace riftbound::test
