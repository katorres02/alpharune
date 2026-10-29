/// @file test_audit_2026_10_errataB.cpp
/// Errata batch B of the 2026-10-28 card audit: cards with published errata,
/// each checked against its errata text (errata/errata_index.json).

#include "tests/cards/card_test_fixture.h"

#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using Picker = std::function<Intent(const std::vector<Intent>&)>;

// The option choosing object `id` (falls back to the first option).
Picker chooseObj(GameObjectId id) {
    return [id](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return i;
        return legal.front();
    };
}
// The option carrying int `v` (yes = 1 / no = 0 for confirmOptional).
Picker chooseVal(int v) {
    return [v](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
        return legal.front();
    };
}
// The "none" option: no object, no value.
Intent noneOf(const std::vector<Intent>& legal) {
    for (const auto& i : legal)
        if (i.chosen_objects.empty() && !i.chosen_value.has_value()) return i;
    ADD_FAILURE() << "no decline option offered";
    return legal.front();
}
// Answers in sequence; each entry is used for one prompt.
Picker inOrder(std::vector<Picker> steps) {
    auto idx = std::make_shared<size_t>(0);
    return [steps, idx](const std::vector<Intent>& legal) {
        if (*idx < steps.size()) return steps[(*idx)++](legal);
        return legal.front();
    };
}
const Picker kNone = [](const std::vector<Intent>& legal) { return noneOf(legal); };

class AuditErrataBTest : public CardTestFixture {
protected:
    GameObjectId addGearAt(PlayerId owner, CardDefId def_id, int at_bf = -1,
                           std::vector<std::string> tags = {}) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        obj.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestGear";
        obj.tags = def_id != kInvalidId ? card_db.get(def_id).tags : tags;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId installBattlefieldCard(BattlefieldId bf, CardDefId def_id) {
        auto src = state.createObject();
        auto& obj = state.getObject(src);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        obj.name = def_id != kInvalidId ? card_db.get(def_id).name : "BF";
        state.battlefields[bf].card_object_id = src;
        return src;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    const Card* card(CardDefId id) { return card_registry.get(id); }
};

constexpr CardDefId kDreamingTree = 287;
constexpr CardDefId kDraven = 343;
constexpr CardDefId kJanna = 376;
constexpr CardDefId kJax = 377;
constexpr CardDefId kKato = 434;
constexpr CardDefId kEdgeOfNight = 460;
constexpr CardDefId kFizz = 461;
constexpr CardDefId kGuards = 475;
constexpr CardDefId kRekSai = 491;
constexpr CardDefId kRelentlessPursuit = 503;
constexpr CardDefId kVoidRush = 505;
constexpr CardDefId kKarma548 = 548;
constexpr CardDefId kVoidBurrower = 553;
constexpr CardDefId kBoneSkewer = 701;
constexpr CardDefId kStalkingWolf = 728;
constexpr CardDefId kDeceiver = 756;
constexpr CardDefId kMirrorImage = 757;
constexpr CardDefId kResonatingStrike = 821;
constexpr CardDefId kGangplankNaval = 873;
constexpr CardDefId kSteelPaws = 830;   // 1-cost Calm Cat unit
constexpr CardDefId kCleave = 4;        // 1-cost Fury spell

// ── 287 The Dreaming Tree: only a SPELL choosing a friendly unit here ───────
TEST_F(AuditErrataBTest, DreamingTree_OnlySpellChoicesTrigger) {
    auto tree = installBattlefieldCard(0, kDreamingTree);
    auto u = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    const Card* c = card(kDreamingTree);
    pushSpellOnChain(P1, kInvalidId, {u});
    EXPECT_TRUE(c->triggerCondition(state, tree, P1, TriggerType::WhenAFriendlyUnitChosenHere,
                                    kInvalidId));
    // The same choice made by an ability does not count.
    state.chain.items.back().is_spell = false;
    state.chain.items.back().is_ability = true;
    EXPECT_FALSE(c->triggerCondition(state, tree, P1, TriggerType::WhenAFriendlyUnitChosenHere,
                                     kInvalidId));
    // A spell again, but this player already drew this turn.
    state.chain.items.back().is_spell = true;
    state.chain.items.back().is_ability = false;
    state.getObject(tree).card_counters["__dreaming_tree_drew_p" +
        std::to_string(static_cast<int>(P1))] = state.turn.turn_number + 1;
    EXPECT_FALSE(c->triggerCondition(state, tree, P1, TriggerType::WhenAFriendlyUnitChosenHere,
                                     kInvalidId));
}

// ── 343 Draven: [C] payable from an exhausted Fury rune ─────────────────────
TEST_F(AuditErrataBTest, Draven_PaysFromExhaustedFuryRune) {
    auto draven = addUnit(P1, kDraven, 0, 0);
    auto rune = addRune(P1, Domain::Fury, /*exhausted=*/true);
    EffectExecutor exec(state, events, card_db, &card_registry);
    const int before = state.getObject(draven).current_might;
    driveResumableTrigger(kDraven, P1, draven, chooseVal(1), exec,
                          TriggerType::WhenIAttackOrDefend);
    EXPECT_EQ(state.getObject(draven).current_might, before + 2);
    EXPECT_FALSE(state.getObject(rune).location.has_value());   // recycled for power
    EXPECT_NE(card_db.get(kDraven).ability_text.find("pay [C] to give me +2 [M] this turn"),
              std::string::npos);
}

// ── 376 Janna: "move UP TO one enemy unit" — declining is allowed ───────────
TEST_F(AuditErrataBTest, Janna_MayMoveNoEnemy) {
    auto janna = addUnit(P1, kJanna, 0, 0);
    auto friendly = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(friendly).damage_marked = 2;
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kJanna, P1, janna, chooseVal(0), exec);
    EXPECT_EQ(state.getObject(friendly).damage_marked, 0);   // healed
    EXPECT_TRUE(state.getObject(enemy).isAtBattlefield());   // not moved
}
TEST_F(AuditErrataBTest, Janna_MovesTheChosenEnemy) {
    auto janna = addUnit(P1, kJanna, 0, 0);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kJanna, P1, janna, inOrder({chooseVal(1), chooseObj(e2)}), exec);
    EXPECT_TRUE(state.getObject(e1).isAtBattlefield());
    EXPECT_TRUE(state.getObject(e2).isAtBase());
}

// ── 377 Jax: Quick-Draw/Reaction belong to the Equipment, not to Jax ────────
TEST_F(AuditErrataBTest, Jax_HasOnlyDeflect) {
    const auto& d = card_db.get(kJax);
    EXPECT_TRUE(d.keywords.has(Keyword::Deflect));
    EXPECT_FALSE(d.keywords.has(Keyword::Reaction));
    EXPECT_FALSE(d.keywords.has(Keyword::QuickDraw));
}

// ── 434 Kato: keywords keep their values ────────────────────────────────────
TEST_F(AuditErrataBTest, Kato_GrantsAssaultWithItsValue) {
    auto kato = addUnit(P1, kKato, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.giveTemporaryKeyword(kato, Keyword::Assault, 2);
    driveResumableTrigger(kKato, P1, kato, chooseObj(other), exec, TriggerType::WhenIMoveToFB);
    const auto& o = state.getObject(other);
    EXPECT_TRUE(o.hasKeyword(Keyword::Assault));
    EXPECT_EQ(o.assault_value, 2);
    EXPECT_TRUE(o.hasKeyword(Keyword::Deflect));
    EXPECT_EQ(o.temp_might_bonus, state.getObject(kato).current_might);
}

// ── 460 Edge of Night: the controller picks which unit here ─────────────────
TEST_F(AuditErrataBTest, EdgeOfNight_AttachesToChosenUnitHere) {
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    auto edge = addGearAt(P1, kEdgeOfNight, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kEdgeOfNight, P1, edge, chooseObj(u2), exec);
    ASSERT_TRUE(state.getObject(edge).attached_to.has_value());
    EXPECT_EQ(*state.getObject(edge).attached_to, u2);
    EXPECT_TRUE(state.getObject(u1).attachments.empty());
}

// ── 461 Fizz: the controller picks which trash spell to play ────────────────
TEST_F(AuditErrataBTest, Fizz_PlaysTheChosenSpell) {
    auto fizz = addUnit(P1, kFizz, 0, 0);
    auto older = addToTrash(P1, kCleave);
    auto newer = addToTrash(P1, kCleave);
    addRune(P1, Domain::Fury);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kFizz, P1, fizz, inOrder({chooseVal(1), chooseObj(older)}), exec);
    EXPECT_FALSE(inTrash(P1, older));
    EXPECT_TRUE(inTrash(P1, newer));
}

// ── 475 Guards!: the [C] payment is only offered when Order power exists ────
TEST_F(AuditErrataBTest, Guards_NoPromptWithoutOrderPower) {
    auto src = pushSpellOnChain(P1, kGuards);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    int prompts = 0;
    driveResumable(kGuards, P1, src, [&](const std::vector<Intent>& l) { ++prompts; return l.back(); },
                   exec);
    EXPECT_EQ(prompts, 0);
}

// ── 491 Rek'Sai: optional reveal; banished unit may be played here ──────────
TEST_F(AuditErrataBTest, RekSai_DeclineRevealLeavesDeck) {
    auto rek = addUnit(P1, kRekSai, 0, 0);
    addToDeck(P1, kSteelPaws);
    auto top = addToDeck(P1, kSteelPaws);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRekSai, P1, rek, chooseVal(0), exec, TriggerType::WhenIAttack);
    EXPECT_EQ(deckSize(P1), 2);
    EXPECT_EQ(state.player(P1).main_deck.back(), top);
}
TEST_F(AuditErrataBTest, RekSai_BanishAndPlayUnitHereRecycleRest) {
    auto rek = addUnit(P1, kRekSai, 0, 0);
    auto bottom = addToDeck(P1, kInvalidId);
    auto second = addToDeck(P1, kSteelPaws);
    auto top = addToDeck(P1, kSteelPaws);
    addRune(P1, Domain::Calm);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRekSai, P1, rek,
                          inOrder({chooseVal(1), chooseObj(second), chooseVal(1)}), exec,
                          TriggerType::WhenIAttack);
    EXPECT_TRUE(state.getObject(second).isAtBattlefield());   // played here
    // The other revealed card went to the bottom.
    ASSERT_EQ(deckSize(P1), 2);
    EXPECT_EQ(state.player(P1).main_deck.front(), top);
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom);
}

// ── 503 Relentless Pursuit: chosen move, optional attach, conquer grant ─────
TEST_F(AuditErrataBTest, RelentlessPursuit_MoveAttachAndGrant) {
    auto unit = addUnit(P1, kInvalidId, 3);              // at base
    auto eq = addGearAt(P1, kInvalidId, -1, {"Equipment"});
    auto src = pushSpellOnChain(P1, kRelentlessPursuit);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kRelentlessPursuit, P1, src,
                   inOrder({chooseObj(unit), chooseVal(1 + 1), chooseObj(eq)}), exec);
    ASSERT_TRUE(state.getObject(unit).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(unit).battlefieldId(), 1u);
    ASSERT_TRUE(state.getObject(eq).attached_to.has_value());
    EXPECT_EQ(*state.getObject(eq).attached_to, unit);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::WhenIConquer);
    EXPECT_EQ(state.delayed_abilities[0].target_filter, unit);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, state.turn.turn_number);

    // The granted ability: "you may move me to my base".
    state.delayed_abilities.clear();
    driveResumableTrigger(kRelentlessPursuit, P1, unit, chooseVal(1), exec,
                          TriggerType::WhenIConquer);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
}
TEST_F(AuditErrataBTest, RelentlessPursuit_AttachIsOptional) {
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto eq = addGearAt(P1, kInvalidId, -1, {"Equipment"});
    auto src = pushSpellOnChain(P1, kRelentlessPursuit);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kRelentlessPursuit, P1, src,
                   inOrder({chooseObj(unit), chooseVal(0), kNone}), exec);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
    EXPECT_FALSE(state.getObject(eq).attached_to.has_value());
}

// ── 505 Void Rush: the controller picks which card to banish (or none) ──────
TEST_F(AuditErrataBTest, VoidRush_BanishChosenDrawOther) {
    auto second = addToDeck(P1, kSteelPaws);
    auto top = addToDeck(P1, kSteelPaws);
    auto src = pushSpellOnChain(P1, kVoidRush);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kVoidRush, P1, src, chooseObj(second), exec);
    EXPECT_TRUE(state.getObject(second).location.has_value());   // played (cost 1 - 2 = free)
    EXPECT_TRUE(inHand(P1, top));
}
TEST_F(AuditErrataBTest, VoidRush_DeclineDrawsBoth) {
    auto second = addToDeck(P1, kSteelPaws);
    auto top = addToDeck(P1, kSteelPaws);
    auto src = pushSpellOnChain(P1, kVoidRush);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kVoidRush, P1, src, kNone, exec);
    EXPECT_TRUE(inHand(P1, top));
    EXPECT_TRUE(inHand(P1, second));
    EXPECT_TRUE(state.player(P1).banishment.empty());
}

// ── 548 Karma, Channeler: Vision by keyword only; recycle -> buff ───────────
TEST_F(AuditErrataBTest, Karma548_RecycleBuffsAFriendlyUnit) {
    const Card* c = card(kKarma548);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouRecycle);
    EXPECT_TRUE(card_db.get(kKarma548).keywords.has(Keyword::Vision));
    auto karma = addUnit(P1, kKarma548, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kKarma548, P1, karma, chooseObj(other), exec);
    EXPECT_EQ(state.getObject(other).buff_count, 1);
}

// ── 553 Void Burrower: no trigger targets; exhaust is the cost ──────────────
TEST_F(AuditErrataBTest, VoidBurrower_ExhaustRevealRecycle) {
    EXPECT_EQ(card(kVoidBurrower)->getTargetRequirements().count, 0);
    auto legend = state.createObject();
    {
        auto& l = state.getObject(legend);
        l.card_def_id = kVoidBurrower;
        l.owner = l.controller = P1;
        l.card_type = CardType::Legend;
        l.zone = ZoneType::LegendZone;
    }
    state.player(P1).legend_zone = legend;
    auto a = addToDeck(P1, kSteelPaws);
    auto b = addToDeck(P1, kSteelPaws);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kVoidBurrower, P1, legend, inOrder({chooseVal(1), kNone}), exec,
                          TriggerType::WhenIConquer);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    EXPECT_TRUE(inDeck(P1, a));
    EXPECT_TRUE(inDeck(P1, b));
}

// ── 701 Bone Skewer: "You may choose a unit" — none is allowed ──────────────
TEST_F(AuditErrataBTest, BoneSkewer_MayChooseNoUnit) {
    auto bf_card = installBattlefieldCard(0, kInvalidId);
    installBattlefieldCard(1, kInvalidId);
    auto hand_unit = addToHand(P2, kSteelPaws);
    auto src = pushSpellOnChain(P1, kBoneSkewer);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kBoneSkewer, P1, src, inOrder({chooseObj(bf_card), kNone}), exec);
    EXPECT_TRUE(inHand(P2, hand_unit));
}
TEST_F(AuditErrataBTest, BoneSkewer_ChosenUnitPlayedAndStunned) {
    auto bf_card = installBattlefieldCard(0, kInvalidId);
    installBattlefieldCard(1, kInvalidId);
    auto hand_unit = addToHand(P2, kSteelPaws);
    auto src = pushSpellOnChain(P1, kBoneSkewer);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kBoneSkewer, P1, src, inOrder({chooseObj(bf_card), chooseObj(hand_unit)}),
                   exec);
    EXPECT_FALSE(inHand(P2, hand_unit));
    ASSERT_TRUE(state.getObject(hand_unit).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(hand_unit).battlefieldId(), 0u);
}

// ── 728 Stalking Wolf: unplayable without prey; Ambush "to its battlefield" ─
TEST_F(AuditErrataBTest, StalkingWolf_NeedsPreyToBePlayed) {
    const Card* c = card(kStalkingWolf);
    EXPECT_TRUE(c->optionalAdditionalCost(state, P1).mandatory);
    addUnit(P1, kInvalidId, 1);   // not a Bird/Cat/Dog/Poro
    EXPECT_TRUE(c->objectCostCandidates(state, P1, kInvalidId, std::nullopt).empty());
    addUnit(P1, kSteelPaws, 0, 0);   // a Cat
    EXPECT_EQ(c->objectCostCandidates(state, P1, kInvalidId, std::nullopt).size(), 1u);
}
TEST_F(AuditErrataBTest, StalkingWolf_AmbushedVictimMustBeThere) {
    const Card* c = card(kStalkingWolf);
    auto cat_base = addUnit(P1, kSteelPaws);        // a Cat at base
    auto cat_here = addUnit(P1, kSteelPaws, 0, 0);  // a Cat at battlefield 0
    const auto there = c->objectCostCandidates(state, P1, kInvalidId,
                                               LocationId{BattlefieldLocation{0}});
    ASSERT_EQ(there.size(), 1u);
    EXPECT_EQ(there.front(), cat_here);
    const auto at_base = c->objectCostCandidates(state, P1, kInvalidId,
                                                 LocationId{BaseLocation{P1}});
    EXPECT_EQ(at_base.size(), 2u);
    (void)cat_base;
}

// ── 756 Deceiver: cost paid as chosen; no [Temporary] on the legend ─────────
TEST_F(AuditErrataBTest, Deceiver_PaysDiscardAndExhaust) {
    EXPECT_FALSE(card_db.get(kDeceiver).keywords.has(Keyword::Temporary));
    auto bf_card = installBattlefieldCard(0, kInvalidId);
    auto legend = state.createObject();
    {
        auto& l = state.getObject(legend);
        l.card_def_id = kDeceiver;
        l.owner = l.controller = P1;
        l.card_type = CardType::Legend;
        l.zone = ZoneType::LegendZone;
    }
    state.player(P1).legend_zone = legend;
    addToHand(P1, kSteelPaws);
    auto discard = addToHand(P1, kSteelPaws);
    auto model = addUnit(P2, kInvalidId, 4, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kDeceiver, P1, legend,
                          inOrder({chooseVal(1), chooseObj(discard), chooseObj(model)}), exec,
                          TriggerType::WhenIConquerOrHold, bf_card);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    EXPECT_TRUE(inTrash(P1, discard));
    int reflections = 0;
    for (const auto& [id, o] : state.objects)
        if (o.isUnit() && o.controller == P1 && o.battlefieldId() == std::optional<BattlefieldId>(0) &&
            o.hasKeyword(Keyword::Temporary))
            ++reflections;
    EXPECT_EQ(reflections, 1);
}

// ── 757 Mirror Image: [Temporary] is given to the token, not printed ────────
TEST_F(AuditErrataBTest, MirrorImage_TokenIsTemporarySpellIsNot) {
    EXPECT_FALSE(card_db.get(kMirrorImage).keywords.has(Keyword::Temporary));
    auto model = addUnit(P2, kInvalidId, 4, 0);
    auto src = pushSpellOnChain(P1, kMirrorImage);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(src, kMirrorImage, P1, {model}, exec);
    int tokens = 0;
    for (const auto& [id, o] : state.objects)
        if (o.isUnit() && o.controller == P1 && o.isAtBase() && o.hasKeyword(Keyword::Temporary))
            ++tokens;
    EXPECT_EQ(tokens, 1);
}

// ── 821 Resonating Strike / 873 Gangplank, Naval: Vendetta errata text ──────
TEST_F(AuditErrataBTest, ResonatingStrike_ErrataReactionAndThisTurnMight) {
    const auto& d = card_db.get(kResonatingStrike);
    EXPECT_NE(d.ability_text.find("[Reaction] (Play any time, even before spells and abilities resolve.)"),
              std::string::npos);
    EXPECT_TRUE(d.keywords.has(Keyword::Reaction));
    auto mover = addUnit(P1, kInvalidId, 2);           // at base
    addUnit(P1, kInvalidId, 1, 0);
    state.battlefields[0].controller = P1;
    auto src = pushSpellOnChain(P1, kResonatingStrike);
    state.chain.items.clear();
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumable(kResonatingStrike, P1, src,
                   inOrder({chooseVal(0), chooseObj(mover)}), exec);
    ASSERT_TRUE(state.getObject(mover).battlefieldId().has_value());
    EXPECT_EQ(state.getObject(mover).temp_might_bonus, 2);   // +2 [M] this turn
}
TEST_F(AuditErrataBTest, GangplankNaval_ReplacementGivesThreeMightThisTurn) {
    EXPECT_NE(card_db.get(kGangplankNaval).ability_text.find("+3 [M] this turn instead"),
              std::string::npos);
    auto gp = addUnit(P1, kGangplankNaval, 0, 0);
    state.getObject(gp).empowered = 1;
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gp};
    EXPECT_TRUE(card_registry.get(kGangplankNaval)
                    ->replaceHostileEffect(ctx, gp, Card::HostileEffect::Stun));
    EXPECT_EQ(state.getObject(gp).temp_might_bonus, 3);
}

}  // namespace
}  // namespace riftbound::test
