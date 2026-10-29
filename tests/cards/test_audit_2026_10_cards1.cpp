/// @file test_audit_2026_10_cards1.cpp
/// 2026-10-28 card audit follow-up, batch "cards1" (Origins 182..297).

#include "tests/cards/card_test_fixture.h"

#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <vector>

namespace riftbound::test {
namespace {

constexpr CardDefId kScrapheap = 182, kTreasureTrove = 186, kZauniteBouncer = 188,
    kMaddenedMarauder = 191, kTwistedFate = 200, kBackToBack = 206, kCruelPatron = 208,
    kCullTheWeak = 209, kForge = 212, kImperialDecree = 221, kSpectralMatron = 226,
    kFiora = 232, kGrandStrategem = 233, kHarnessedDragon = 234, kLeona = 238,
    kDarius = 243, kDivineJudgment = 244, kViktor = 246, kStormbringer = 248,
    kLooseCannon = 249, kSmdr = 250, kGuillotine = 251, kNineTailedFox = 252,
    kFoxFire = 253, kLastBreath = 257, kZenithBlade = 258, kSwiftScout = 259,
    kGuerilla = 260, kBulletTime = 263, kShowstopper = 265, kHallowedTomb = 276,
    kNavori = 278, kReaversRow = 280, kDaughter = 294, kRelentlessStorm = 295,
    kRadiantDawn = 297;

using Picker = std::function<Intent(const std::vector<Intent>&)>;

Picker pickObj(GameObjectId id) {
    return [id](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return i;
        ADD_FAILURE() << "object " << id << " was not offered";
        return legal.front();
    };
}
Picker pickVal(int v) {
    return [v](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
        ADD_FAILURE() << "value " << v << " was not offered";
        return legal.front();
    };
}
/// Runs `steps` in order, one per published choice; records every offer.
struct Script {
    std::vector<Picker> steps;
    std::vector<std::vector<Intent>> offers;
    size_t n = 0;
    Picker fn() {
        return [this](const std::vector<Intent>& legal) {
            offers.push_back(legal);
            if (n < steps.size()) return steps[n++](legal);
            return legal.front();
        };
    }
};
bool offered(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return true;
    return false;
}

class AuditCards1Test : public CardTestFixture {
protected:
    GameObjectId addObject(PlayerId owner, CardDefId def_id, ZoneType zone) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        const auto& def = card_db.get(def_id);
        obj.name = def.name;
        obj.card_type = def.card_type;
        obj.super_type = def.super_type;
        obj.keywords = def.keywords;
        obj.domains = def.domains;
        obj.tags = def.tags;
        obj.base_might = obj.current_might = def.might;
        obj.zone = zone;
        if (zone == ZoneType::Trash) state.player(owner).trash.push_back(id);
        if (zone == ZoneType::Base) obj.location = BaseLocation{owner};
        if (zone == ZoneType::LegendZone) state.player(owner).legend_zone = id;
        return id;
    }

    /// Like driveResumable, but passes play-time `targets` to onResolve.
    void driveSpell(CardDefId def_id, PlayerId controller, GameObjectId source,
                    const std::vector<GameObjectId>& targets, Picker picker,
                    EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{state, events, exec, controller, source};
        card->onResolve(ctx, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }

    void driveActivate(CardDefId def_id, PlayerId controller, GameObjectId source,
                       Picker picker, EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
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

    const ChainItem* lastItemFrom(GameObjectId source) {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it)
            if (it->source == source) return &*it;
        return nullptr;
    }
};

// ── 182 Scrapheap: "discarded" draws too ──
TEST_F(AuditCards1Test, Scrapheap_DiscardTriggersFromTrash) {
    auto heap = addObject(P1, kScrapheap, ZoneType::Trash);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ObjectStateChangedEvent{heap, "discarded"});
    auto* item = lastItemFrom(heap);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->fired_trigger, TriggerType::WhenIAmDiscarded);
    addToDeck(P1, kScrapheap);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kScrapheap, P1, heap, TriggerType::WhenIAmDiscarded, exec);
    EXPECT_EQ(handSize(P1), 1);
}

// ── 186 Treasure Trove: returning it to hand fires "leaves the board" ──
TEST_F(AuditCards1Test, TreasureTrove_BounceFiresLeaveTheBoard) {
    auto trove = addObject(P1, kTreasureTrove, ZoneType::Base);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db, &card_registry);
    exec.bounceToHand(trove);
    auto* item = lastItemFrom(trove);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->fired_trigger, TriggerType::WhenILeaveTheBoard);
}

// ── 188 Zaunite Bouncer: another unit at a battlefield, chosen ──
TEST_F(AuditCards1Test, ZauniteBouncer_ReturnsChosenUnitAtBattlefield) {
    auto self = addUnit(P1, kZauniteBouncer, 0, 0);
    auto in_base = addUnit(P2, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 1);
    EXPECT_EQ(card_registry.get(kZauniteBouncer)->getTargetRequirements().count, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(enemy)}};
    driveResumableTrigger(kZauniteBouncer, P1, self, s.fn(), exec, TriggerType::WhenYouPlayMe);
    ASSERT_EQ(s.offers.size(), 1u);
    EXPECT_FALSE(offered(s.offers[0], self));
    EXPECT_FALSE(offered(s.offers[0], in_base));
    EXPECT_TRUE(inHand(P2, enemy));
}

// ── 191 Maddened Marauder: only units at a battlefield ──
TEST_F(AuditCards1Test, MaddenedMarauder_OnlyUnitsAtBattlefields) {
    auto self = addUnit(P1, kMaddenedMarauder);           // in base
    auto at_bf = addUnit(P2, kInvalidId, 3, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(at_bf)}};
    driveResumableTrigger(kMaddenedMarauder, P1, self, s.fn(), exec, TriggerType::WhenYouPlayMe);
    ASSERT_EQ(s.offers.size(), 1u);
    EXPECT_FALSE(offered(s.offers[0], self));
    EXPECT_TRUE(state.getObject(at_bf).isAtBase());
}

// ── 200 Twisted Fate: one rune revealed even when a target is chosen ──
TEST_F(AuditCards1Test, TwistedFate_RevealsOnlyOneRuneAcrossTargetChoice) {
    auto tf = addUnit(P1, kTwistedFate, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto mind = addRune(P1, Domain::Mind);
    auto fury = addRune(P1, Domain::Fury);
    for (auto r : {mind, fury}) {
        state.getObject(r).location.reset();
        state.getObject(r).zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(r);   // top = back = fury
    }
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kTwistedFate, P1, tf, pickObj(enemy), exec, TriggerType::WhenIAttack);
    EXPECT_EQ(state.player(P1).rune_deck.back(), mind) << "a second rune was recycled";
    EXPECT_EQ(state.getObject(enemy).damage_marked, 2);
}

// ── 206 Back to Back: two friendly units each +2 ──
TEST_F(AuditCards1Test, BackToBack_BuffsTwoUnits) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 3, 0);
    auto src = pushSpellOnChain(P1, kBackToBack);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(a), pickObj(b)}};
    driveResumable(kBackToBack, P1, src, s.fn(), exec);
    EXPECT_EQ(state.getObject(a).current_might, 4);
    EXPECT_EQ(state.getObject(b).current_might, 5);
}

// ── 208 Cruel Patron: needs a friendly unit to kill (mandatory object cost) ──
TEST_F(AuditCards1Test, CruelPatron_NotPlayableWithoutFriendlyUnit) {
    const Card* c = card_registry.get(kCruelPatron);
    const auto oac = c->optionalAdditionalCost(state, P1);
    EXPECT_TRUE(oac.mandatory);
    EXPECT_EQ(oac.object_cost, Card::OptionalAdditionalCost::ObjectCost::KillFriendlyUnit);
    EXPECT_TRUE(c->objectCostCandidates(state, P1, kInvalidId, std::nullopt).empty());
    addUnit(P1, kInvalidId, 1);
    EXPECT_EQ(c->objectCostCandidates(state, P1, kInvalidId, std::nullopt).size(), 1u);
}

// ── 209 Cull the Weak: each player picks their own unit ──
TEST_F(AuditCards1Test, CullTheWeak_EachPlayerChooses) {
    auto p1a = addUnit(P1, kInvalidId, 1, 0);
    auto p1b = addUnit(P1, kInvalidId, 1, 0);
    auto p2a = addUnit(P2, kInvalidId, 5, 0);
    auto p2b = addUnit(P2, kInvalidId, 1, 0);
    auto src = pushSpellOnChain(P1, kCullTheWeak);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(p1b), pickObj(p2b)}};
    driveResumable(kCullTheWeak, P1, src, s.fn(), exec);
    ASSERT_EQ(s.offers.size(), 2u);
    EXPECT_EQ(s.offers[1].front().player, P2);
    EXPECT_TRUE(inTrash(P1, p1b));
    EXPECT_TRUE(inTrash(P2, p2b));
    EXPECT_TRUE(state.getObject(p1a).location.has_value());
    EXPECT_TRUE(state.getObject(p2a).location.has_value());
}

// ── 212 Forge of the Future: Recruit token; kill this to recycle chosen cards ──
TEST_F(AuditCards1Test, ForgeOfTheFuture_RecruitAndRecycle) {
    auto forge = addObject(P1, kForge, ZoneType::Base);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kForge, P1, forge, TriggerType::WhenYouPlayThis, exec);
    int recruits = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.isToken() && o.isAtBase() && o.current_might == 1) ++recruits;
    EXPECT_EQ(recruits, 1);

    auto mine = addObject(P1, kScrapheap, ZoneType::Trash);
    auto theirs = addObject(P2, kScrapheap, ZoneType::Trash);
    Script s{{pickObj(theirs), pickVal(0)}};
    driveActivate(kForge, P1, forge, s.fn(), exec);
    EXPECT_TRUE(inTrash(P1, forge)) << "killing this is the cost";
    EXPECT_TRUE(inDeck(P2, theirs));
    EXPECT_TRUE(inTrash(P1, mine));
}

// ── 221 Imperial Decree: every damage this turn triggers a kill ──
TEST_F(AuditCards1Test, ImperialDecree_KillsUnitsThatTakeDamageThisTurn) {
    auto damaged_before = addUnit(P2, kInvalidId, 5, 0);
    state.getObject(damaged_before).damage_marked = 1;
    auto a = addUnit(P2, kInvalidId, 5, 0);
    auto b = addUnit(P1, kInvalidId, 5, 0);
    auto src = pushSpellOnChain(P1, kImperialDecree);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(src, kImperialDecree, P1, {}, exec);
    EXPECT_TRUE(state.getObject(damaged_before).location.has_value())
        << "already-damaged units are not killed";
    size_t before = state.chain.items.size();
    exec.dealDamage(a, 1, kInvalidId);
    exec.dealDamage(b, 1, kInvalidId);
    ASSERT_EQ(state.chain.items.size(), before + 2);
    EXPECT_EQ(state.chain.items.back().triggering_subject, b);
    driveResumableTrigger(kImperialDecree, P1, src, nullptr, exec,
                          TriggerType::WhenAUnitTakesDamage, a);
    EXPECT_TRUE(inTrash(P2, a));
}

// ── 226 Spectral Matron: Power cost at most [A] (1) ──
TEST_F(AuditCards1Test, SpectralMatron_PowerCostAtMostOne) {
    CardDefId one = kInvalidId, two = kInvalidId;
    for (const auto& [id, def] : card_db.all()) {
        if (def.card_type != CardType::Unit || def.energy_cost > 3) continue;
        if (def.power_cost == 1 && one == kInvalidId) one = id;
        if (def.power_cost == 2 && two == kInvalidId) two = id;
    }
    ASSERT_NE(one, kInvalidId);
    ASSERT_NE(two, kInvalidId);
    auto self = addUnit(P1, kSpectralMatron);
    auto ok = addObject(P1, one, ZoneType::Trash);
    auto too_much = addObject(P1, two, ZoneType::Trash);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickVal(1), pickObj(ok)}};
    driveResumableTrigger(kSpectralMatron, P1, self, s.fn(), exec, TriggerType::WhenYouPlayMe);
    ASSERT_EQ(s.offers.size(), 2u);
    EXPECT_TRUE(offered(s.offers[1], ok));
    EXPECT_FALSE(offered(s.offers[1], too_much));
}

// ── 232 Fiora, Victorious: keywords only while Mighty ──
TEST_F(AuditCards1Test, FioraVictorious_KeywordsOnlyWhileMighty) {
    const auto& def = card_db.get(kFiora);
    EXPECT_FALSE(def.keywords.has(Keyword::Deflect));
    EXPECT_FALSE(def.keywords.has(Keyword::Ganking));
    EXPECT_FALSE(def.keywords.has(Keyword::Shield));
    auto f = addUnit(P1, kFiora, 0, 0);
    const Card* c = card_registry.get(kFiora);
    // The engine folds aura_effects into aura_keywords; read the grants directly.
    auto granted = [&](Keyword kw) {
        for (const auto& ae : state.getObject(f).aura_effects)
            if (ae.source == f && ae.keyword == kw) return true;
        return false;
    };
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(granted(Keyword::Ganking));
    EXPECT_FALSE(state.getObject(f).hasKeyword(Keyword::Ganking));
    state.getObject(f).aura_effects.clear();
    state.getObject(f).current_might = 5;
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(granted(Keyword::Deflect));
    EXPECT_TRUE(granted(Keyword::Ganking));
    EXPECT_TRUE(granted(Keyword::Shield));
}

// ── 233 Grand Strategem: all friendly units ──
TEST_F(AuditCards1Test, GrandStrategem_AllFriendlyUnits) {
    auto a = addUnit(P1, kInvalidId, 1, 0);
    auto b = addUnit(P1, kInvalidId, 2);
    auto e = addUnit(P2, kInvalidId, 2, 0);
    auto src = pushSpellOnChain(P1, kGrandStrategem);
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(src, kGrandStrategem, P1, {}, exec);
    EXPECT_EQ(state.getObject(a).current_might, 6);
    EXPECT_EQ(state.getObject(b).current_might, 7);
    EXPECT_EQ(state.getObject(e).current_might, 2);
}

// ── 234 Harnessed Dragon: chosen enemy unit dies ──
TEST_F(AuditCards1Test, HarnessedDragon_KillsChosenEnemy) {
    auto self = addUnit(P1, kHarnessedDragon);
    auto friendly = addUnit(P1, kInvalidId, 1);
    auto e1 = addUnit(P2, kInvalidId, 2, 0);
    auto e2 = addUnit(P2, kInvalidId, 2);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(e2)}};
    driveResumableTrigger(kHarnessedDragon, P1, self, s.fn(), exec, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(offered(s.offers[0], friendly));
    EXPECT_TRUE(inTrash(P2, e2));
    EXPECT_TRUE(state.getObject(e1).location.has_value());
}

// ── 238 Leona, Determined: only enemies here ──
TEST_F(AuditCards1Test, LeonaDetermined_StunsEnemyHereOnly) {
    auto leona = addUnit(P1, kLeona, 0, 0);
    auto here = addUnit(P2, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 2, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(here)}};
    driveResumableTrigger(kLeona, P1, leona, s.fn(), exec, TriggerType::WhenIAttack);
    ASSERT_EQ(s.offers.size(), 1u);
    EXPECT_FALSE(offered(s.offers[0], elsewhere));
    EXPECT_TRUE(state.getObject(here).is_stunned);
}

// ── 243 Darius, Executioner: other friendly units here get +1 ──
TEST_F(AuditCards1Test, DariusExecutioner_AuraHere) {
    auto darius = addUnit(P1, kDarius, 0, 0);
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto away = addUnit(P1, kInvalidId, 2, 1);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    card_registry.get(kDarius)->applyPassiveAura(state, P1);
    auto bonus = [&](GameObjectId id) {
        int n = 0;
        for (const auto& ae : state.getObject(id).aura_effects) n += ae.might_bonus;
        return n;
    };
    EXPECT_EQ(bonus(here), 1);
    EXPECT_EQ(bonus(away), 0);
    EXPECT_EQ(bonus(enemy), 0);
    EXPECT_EQ(bonus(darius), 0);
}

// ── 244 Divine Judgment: tokens cease to exist; players choose ──
TEST_F(AuditCards1Test, DivineJudgment_TokensCeaseToExistAndChoicesAsked) {
    auto u1 = addUnit(P1, kInvalidId, 1);
    auto u2 = addUnit(P1, kInvalidId, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto tok = exec.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                BaseLocation{P1});
    auto src = pushSpellOnChain(P1, kDivineJudgment);
    Script s;   // first option each time: keeps the two lowest ids
    driveResumable(kDivineJudgment, P1, src, s.fn(), exec);
    EXPECT_EQ(s.offers.size(), 2u) << "two keep picks for P1's three units";
    EXPECT_TRUE(state.getObject(u1).location.has_value());
    EXPECT_TRUE(state.getObject(u2).location.has_value());
    EXPECT_EQ(state.getObject(tok).zone, ZoneType::Banishment);
    EXPECT_FALSE(inDeck(P1, tok));
}

// ── 246 Viktor, Leader: non-Recruit, other units only ──
TEST_F(AuditCards1Test, ViktorLeader_IgnoresRecruitsAndSelf) {
    auto viktor = addUnit(P1, kViktor, 0, 0);
    auto other = addUnit(P1, kInvalidId, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto recruit = exec.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                    BaseLocation{P1});
    const Card* c = card_registry.get(kViktor);
    EXPECT_TRUE(c->triggerCondition(state, viktor, P1, TriggerType::WhenAFriendlyUnitDies, other));
    EXPECT_FALSE(c->triggerCondition(state, viktor, P1, TriggerType::WhenAFriendlyUnitDies, recruit));
    EXPECT_FALSE(c->triggerCondition(state, viktor, P1, TriggerType::WhenAFriendlyUnitDies, viktor));
}

// ── 248 Stormbringer: the player picks the battlefield; the unit moves anyway ──
TEST_F(AuditCards1Test, Stormbringer_ChosenBattlefieldWithoutEnemies) {
    auto mover = addUnit(P1, kInvalidId, 4);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    auto src = pushSpellOnChain(P1, kStormbringer);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(mover), pickVal(1)}};   // battlefield #0
    driveResumable(kStormbringer, P1, src, s.fn(), exec);
    EXPECT_EQ(state.getObject(mover).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0);
}

// ── 249 Loose Cannon: only with one or fewer cards in hand ──
TEST_F(AuditCards1Test, LooseCannon_DrawsOnlyWithSmallHand) {
    auto legend = addObject(P1, kLooseCannon, ZoneType::LegendZone);
    addToDeck(P1, kScrapheap);
    addToDeck(P1, kScrapheap);
    addToHand(P1, kScrapheap);
    addToHand(P1, kScrapheap);
    EffectExecutor exec(state, events, card_db, &card_registry);
    fireTriggerAs(kLooseCannon, P1, legend, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(handSize(P1), 2);
    state.player(P1).hand.pop_back();
    fireTriggerAs(kLooseCannon, P1, legend, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(handSize(P1), 2);
}

// ── 250 Super Mega Death Rocket!: conquer returns it from the trash ──
TEST_F(AuditCards1Test, SuperMegaDeathRocket_ConquerReturnsFromTrash) {
    auto rocket = addObject(P1, kSmdr, ZoneType::Trash);
    auto fodder = addToHand(P1, kScrapheap);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Conquer, 1});
    auto* item = lastItemFrom(rocket);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->fired_trigger, TriggerType::WhenYouConquerFromTrash);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickVal(1), pickObj(fodder)}};
    driveResumableTrigger(kSmdr, P1, rocket, s.fn(), exec, TriggerType::WhenYouConquerFromTrash);
    EXPECT_TRUE(inHand(P1, rocket));
    EXPECT_TRUE(inTrash(P1, fodder));
}

// ── 251 Noxian Guillotine: kill on its next damage this turn ──
TEST_F(AuditCards1Test, NoxianGuillotine_KillsOnNextDamage) {
    auto tgt = addUnit(P2, kInvalidId, 6, 0);
    auto other = addUnit(P2, kInvalidId, 6, 0);
    auto src = pushSpellOnChain(P1, kGuillotine);
    state.player(P1).cards_played_this_turn = 1;   // no Legion
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(src, kGuillotine, P1, {tgt}, exec);
    EXPECT_TRUE(state.getObject(tgt).location.has_value());
    size_t before = state.chain.items.size();
    exec.dealDamage(other, 1, kInvalidId);
    EXPECT_EQ(state.chain.items.size(), before);
    exec.dealDamage(tgt, 1, kInvalidId);
    ASSERT_EQ(state.chain.items.size(), before + 1);
    exec.dealDamage(tgt, 1, kInvalidId);
    EXPECT_EQ(state.chain.items.size(), before + 1) << "only the next time";
    driveResumableTrigger(kGuillotine, P1, src, nullptr, exec,
                          TriggerType::WhenAUnitTakesDamage, tgt);
    EXPECT_TRUE(inTrash(P2, tgt));
}

// ── 252 Nine-Tailed Fox: attacker at a battlefield you control gets -1 ──
TEST_F(AuditCards1Test, NineTailedFox_AttackerShrinks) {
    auto fox = addObject(P2, kNineTailedFox, ZoneType::LegendZone);
    auto attacker = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;
    state.battlefields[0].controller = P2;
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(CombatStartedEvent{0, P1, P2});
    auto* item = lastItemFrom(fox);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->triggering_subject, attacker);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kNineTailedFox, P2, fox, nullptr, exec,
                          TriggerType::WhenAnEnemyUnitAttacksYourBattlefield, attacker);
    EXPECT_EQ(state.getObject(attacker).current_might, 2);
}

// ── 253 Fox-Fire: the player chooses which units die ──
TEST_F(AuditCards1Test, FoxFire_PlayerChoosesUnits) {
    auto mine = addUnit(P1, kInvalidId, 1, 0);
    auto big = addUnit(P2, kInvalidId, 3, 0);
    auto small = addUnit(P2, kInvalidId, 2, 0);
    auto src = pushSpellOnChain(P1, kFoxFire);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(big), pickObj(big), pickVal(0)}};
    driveResumable(kFoxFire, P1, src, s.fn(), exec);
    ASSERT_EQ(s.offers.size(), 3u);
    EXPECT_FALSE(offered(s.offers[2], small)) << "over the remaining Might budget";
    EXPECT_TRUE(offered(s.offers[2], mine));
    EXPECT_TRUE(inTrash(P2, big));
    EXPECT_TRUE(state.getObject(small).location.has_value());
    EXPECT_TRUE(state.getObject(mine).location.has_value());
}

// ── 257 Last Breath: ready, then deal its Might to an enemy ──
TEST_F(AuditCards1Test, LastBreath_ReadiesAndDealsDamage) {
    auto f = addUnit(P1, kInvalidId, 4, 0);
    state.getObject(f).is_exhausted = true;
    auto e = addUnit(P2, kInvalidId, 3, 1);
    auto src = pushSpellOnChain(P1, kLastBreath);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(f), pickObj(e)}};
    driveResumable(kLastBreath, P1, src, s.fn(), exec);
    EXPECT_FALSE(state.getObject(f).is_exhausted);
    EXPECT_TRUE(inTrash(P2, e));
}

// ── 258 Zenith Blade: the player picks which unit moves ──
TEST_F(AuditCards1Test, ZenithBlade_PlayerPicksMover) {
    auto first = addUnit(P1, kInvalidId, 1);
    auto chosen = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 1);
    auto src = pushSpellOnChain(P1, kZenithBlade);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(enemy), pickObj(chosen)}};
    driveResumable(kZenithBlade, P1, src, s.fn(), exec);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_EQ(state.getObject(chosen).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_TRUE(state.getObject(first).isAtBase());
}

// ── 259 Swift Scout: [1] instead of [A] to hide ──
TEST_F(AuditCards1Test, SwiftScout_OffersEnergyHideCost) {
    EXPECT_EQ(card_registry.get(kSwiftScout)->hideEnergyCostInstead(), 1);
    EXPECT_EQ(card_registry.get(kScrapheap)->hideEnergyCostInstead(), -1);
}

// ── 260 Guerilla Warfare: up to two, chosen; hide free this turn ──
TEST_F(AuditCards1Test, GuerillaWarfare_ChosenCardsAndFreeHides) {
    auto h1 = addObject(P1, kFoxFire, ZoneType::Trash);
    auto h2 = addObject(P1, kFoxFire, ZoneType::Trash);
    auto src = pushSpellOnChain(P1, kGuerilla);
    state.turn.turn_number = 5;
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(h1), pickVal(0)}};
    driveResumable(kGuerilla, P1, src, s.fn(), exec);
    EXPECT_TRUE(inHand(P1, h1));
    EXPECT_TRUE(inTrash(P1, h2));
    EXPECT_EQ(state.player(P1).hide_ignoring_costs_turn, 5);
}

// ── 263 Bullet Time: ready runes pay [A] too ──
TEST_F(AuditCards1Test, BulletTime_ReadyRunesCountTowardX) {
    auto e = addUnit(P2, kInvalidId, 5, 0);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Body);
    auto src = pushSpellOnChain(P1, kBulletTime);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(e), pickVal(2)}};
    driveResumable(kBulletTime, P1, src, s.fn(), exec);
    ASSERT_EQ(s.offers.size(), 2u);
    EXPECT_EQ(s.offers[1].size(), 4u) << "X in 0..3";
    EXPECT_EQ(state.getObject(e).damage_marked, 2);
    EXPECT_EQ(readyRuneCount(P1), 1);
}

// ── 265 Showstopper: base units only; chosen battlefield ──
TEST_F(AuditCards1Test, Showstopper_BaseTargetAndChosenBattlefield) {
    auto base_unit = addUnit(P1, kInvalidId, 2);
    auto bf_unit = addUnit(P1, kInvalidId, 2, 0);
    const Card* c = card_registry.get(kShowstopper);
    auto legal = c->enumerateLegalTargets(state, P1);
    EXPECT_NE(std::find(legal.begin(), legal.end(), base_unit), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), bf_unit), legal.end());
    auto src = pushSpellOnChain(P1, kShowstopper);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveSpell(kShowstopper, P1, src, {base_unit}, pickVal(2), exec);   // battlefield #1
    EXPECT_EQ(state.getObject(base_unit).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_EQ(state.getObject(base_unit).buff_count, 1);
}

// ── 276 Hallowed Tomb: optional; only the Chosen Champion ──
TEST_F(AuditCards1Test, HallowedTomb_ReturnsChosenChampionOnly) {
    auto tomb = addObject(P1, kHallowedTomb, ZoneType::Base);
    auto other_champ = addObject(P1, kTwistedFate, ZoneType::Trash);
    auto chosen = addObject(P1, kFiora, ZoneType::Trash);
    state.getObject(chosen).card_counters["__chosen_champion"] = 1;
    state.player(P1).champion_zone = kInvalidId;
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kHallowedTomb, P1, tomb, pickVal(0), exec, TriggerType::WhenYouHoldHere);
    EXPECT_TRUE(inTrash(P1, chosen)) << "declined";
    driveResumableTrigger(kHallowedTomb, P1, tomb, pickVal(1), exec, TriggerType::WhenYouHoldHere);
    EXPECT_EQ(state.player(P1).champion_zone, chosen);
    EXPECT_TRUE(inTrash(P1, other_champ));
}

// ── 278 Navori Fighting Pit: a unit here is buffed ──
TEST_F(AuditCards1Test, NavoriFightingPit_BuffsChosenUnitHere) {
    auto pit = addObject(P1, kNavori, ZoneType::Base);
    state.getObject(pit).location.reset();
    state.battlefields[0].card_object_id = pit;
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto there = addUnit(P1, kInvalidId, 2, 1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickObj(here)}};
    driveResumableTrigger(kNavori, P1, pit, s.fn(), exec, TriggerType::WhenYouHoldHere);
    ASSERT_EQ(s.offers.size(), 1u);
    EXPECT_FALSE(offered(s.offers[0], there));
    EXPECT_EQ(state.getObject(here).buff_count, 1);
}

// ── 280 Reaver's Row: picked at resolution, no target requirements ──
TEST_F(AuditCards1Test, ReaversRow_MovesChosenUnitHereToBase) {
    auto row = addObject(P1, kReaversRow, ZoneType::Base);
    state.getObject(row).location.reset();
    state.battlefields[0].card_object_id = row;
    auto u = addUnit(P1, kInvalidId, 2, 0);
    EXPECT_EQ(card_registry.get(kReaversRow)->getTargetRequirements().count, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Script s{{pickVal(1), pickObj(u)}};
    driveResumableTrigger(kReaversRow, P1, row, s.fn(), exec, TriggerType::WhenYouDefendHere);
    EXPECT_TRUE(state.getObject(u).isAtBase());
}

// ── 294 Daughter of the Void: spell-only [A] ──
TEST_F(AuditCards1Test, DaughterOfTheVoid_AddsSpellOnlyPower) {
    auto legend = addObject(P1, kDaughter, ZoneType::LegendZone);
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kDaughter);
    CardContext ctx{state, events, exec, P1, legend};
    c->onActivate(ctx, {});
    EXPECT_EQ(state.player(P1).rune_pool.restrictedAmount(true, SpendPurpose::PlaySpell), 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

// ── 295 Relentless Storm: the played unit itself must be Mighty ──
TEST_F(AuditCards1Test, RelentlessStorm_PlayedUnitMustBeMighty) {
    auto legend = addObject(P1, kRelentlessStorm, ZoneType::LegendZone);
    addUnit(P1, kInvalidId, 6, 0);                       // a Mighty unit already in play
    auto small = addUnit(P1, kInvalidId, 1);
    auto big = addUnit(P1, kInvalidId, 5);
    const Card* c = card_registry.get(kRelentlessStorm);
    EXPECT_FALSE(c->triggerCondition(state, legend, P1, TriggerType::WhenYouPlayAUnit, small));
    EXPECT_TRUE(c->triggerCondition(state, legend, P1, TriggerType::WhenYouPlayAUnit, big));
}

// ── 297 Radiant Dawn: the chosen unit is buffed ──
TEST_F(AuditCards1Test, RadiantDawn_BuffsChosenUnit) {
    auto legend = addObject(P1, kRadiantDawn, ZoneType::LegendZone);
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    EffectExecutor exec(state, events, card_db, &card_registry);
    driveResumableTrigger(kRadiantDawn, P1, legend, pickObj(b), exec, TriggerType::WhenYouStun);
    EXPECT_EQ(state.getObject(a).buff_count, 0);
    EXPECT_EQ(state.getObject(b).buff_count, 1);
}

}  // namespace
}  // namespace riftbound::test
