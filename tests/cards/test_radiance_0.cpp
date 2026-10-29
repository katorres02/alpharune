/// @file test_radiance_0.cpp
/// Radiance legends and their signature cards (batch 0): 969-984, 986, 987.

#include "tests/cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

#include <memory>

namespace riftbound::test {
namespace {

constexpr CardDefId kBoy = 969, kChronobreak = 970, kHexExpert = 971, kMinefield = 972,
                    kIronRevenant = 973, kRealmOfDeath = 974, kLady = 975, kTheBall = 976,
                    kNazumah = 977, kNtofo = 978, kSongstress = 979, kEncore = 980,
                    kAgony = 981, kLastCaress = 982, kExemplar = 983, kStandard = 984,
                    kDemolitionist = 986, kBanner = 987;
constexpr CardDefId kBlade = 958;   // Blade of the Exile: a 3-cost gear

using Pick = std::function<Intent(const std::vector<Intent>&)>;

/// Answers the published choices in order: a value or an object per step.
struct Script {
    struct Step { bool object; int v; };
    std::shared_ptr<std::vector<Step>> steps = std::make_shared<std::vector<Step>>();
    std::shared_ptr<size_t> at = std::make_shared<size_t>(0);
    Script& V(int v) { steps->push_back({false, v}); return *this; }
    Script& O(GameObjectId id) { steps->push_back({true, static_cast<int>(id)}); return *this; }
    Pick fn() const {
        auto s = steps; auto a = at;
        return [s, a](const std::vector<Intent>& legal) -> Intent {
            if (*a >= s->size()) { ADD_FAILURE() << "unexpected choice"; return legal.front(); }
            const Step st = (*s)[(*a)++];
            for (const auto& i : legal) {
                if (st.object && !i.chosen_objects.empty() &&
                    i.chosen_objects.front() == static_cast<GameObjectId>(st.v)) return i;
                if (!st.object && i.chosen_objects.empty() && i.chosen_value.has_value() &&
                    *i.chosen_value == st.v) return i;
            }
            ADD_FAILURE() << "option not offered (step " << (*a - 1) << ")";
            return legal.front();
        };
    }
    size_t used() const { return *at; }
};

class Radiance0Test : public CardTestFixture {
protected:
    EffectExecutor exec{state, events, card_db, &card_registry};

    GameObjectId addLegend(PlayerId p, CardDefId def) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = p;
        o.card_def_id = def;
        o.name = card_db.get(def).name;
        o.card_type = CardType::Legend;
        o.zone = ZoneType::LegendZone;
        state.player(p).legend_zone = id;
        return id;
    }
    GameObjectId addGear(PlayerId p, CardDefId def, int at_bf = -1) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = p;
        o.card_def_id = def;
        if (def != kInvalidId) {
            const auto& d = card_db.get(def);
            o.name = d.name;
            o.tags = d.tags;
            o.keywords = d.keywords;
        }
        o.card_type = CardType::Gear;
        if (at_bf >= 0) {
            o.zone = ZoneType::BattlefieldZone;
            o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            o.zone = ZoneType::Base;
            o.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId addBomb(PlayerId p, int bf) {
        auto id = addGear(p, kBombTokenCardDefId, bf);
        state.getObject(id).is_token = true;
        return id;
    }
    GameObjectId addToTrash(PlayerId p, CardType t) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = p;
        o.card_type = t;
        o.name = "Trash card";
        o.zone = ZoneType::Trash;
        state.player(p).trash.push_back(id);
        return id;
    }
    void driveActivate(CardDefId def, PlayerId c, GameObjectId src, Pick pick,
                       int idx = 0, std::vector<GameObjectId> targets = {}) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = c;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.source = src;
        ri.card_def_id = def;
        ri.ability_index = idx;
        ri.targets = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, c, src};
        card->onActivate(ctx, idx, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(pick(pending.legal));
            card->onActivate(ctx, idx, targets);
        }
        state.chain.resuming.reset();
    }
    int bombsAt(PlayerId p, int bf) {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.card_def_id == kBombTokenCardDefId && o.controller == p && o.location.has_value() &&
                o.battlefieldId() == std::optional<BattlefieldId>(static_cast<BattlefieldId>(bf)))
                ++n;
        return n;
    }
    int runesOnBoard(PlayerId p) {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isRune() && o.controller == p && o.location.has_value()) ++n;
        return n;
    }
    const ChainItem* chainItemFrom(GameObjectId src) {
        for (const auto& it : state.chain.items) if (it.source == src) return &it;
        return nullptr;
    }
};

// ── Definitions ─────────────────────────────────────────────────────────────

TEST_F(Radiance0Test, DefinitionsAreRegistered) {
    struct Row { CardDefId id; const char* name; CardType type; };
    for (const Row& r : std::vector<Row>{
             {kBoy, "Boy Who Shattered Time", CardType::Legend},
             {kChronobreak, "Chronobreak", CardType::Spell},
             {kHexExpert, "Hexplosives Expert", CardType::Legend},
             {kMinefield, "Hexplosive Minefield", CardType::Spell},
             {kIronRevenant, "Iron Revenant", CardType::Legend},
             {kRealmOfDeath, "Realm of Death", CardType::Spell},
             {kLady, "Lady of Clockwork", CardType::Legend},
             {kTheBall, "The Ball", CardType::Gear},
             {kNazumah, "Pride of Nazumah", CardType::Legend},
             {kNtofo, "Ntofo Strikes", CardType::Spell},
             {kSongstress, "Starry-Eyed Songstress", CardType::Legend},
             {kEncore, "Encore", CardType::Spell},
             {kAgony, "Agony's Embrace", CardType::Legend},
             {kLastCaress, "Last Caress", CardType::Spell},
             {kExemplar, "Exemplar of Demacia", CardType::Legend},
             {kStandard, "Demacian Standard", CardType::Gear},
             {kDemolitionist, "Amateur Demolitionist", CardType::Unit},
             {kBanner, "Banner of Conquest", CardType::Gear}}) {
        ASSERT_NE(card_registry.get(r.id), nullptr) << r.name;
        const auto& d = card_db.get(r.id);
        EXPECT_EQ(d.name, r.name);
        EXPECT_EQ(d.card_type, r.type) << r.name;
        EXPECT_EQ(d.set_code, "RAD");
    }
    EXPECT_EQ(card_db.get(kChronobreak).super_type, SuperType::Signature);
    EXPECT_EQ(card_db.get(kChronobreak).energy_cost, 2);
    EXPECT_TRUE(card_db.get(kMinefield).keywords.has(Keyword::Action));
    EXPECT_TRUE(card_db.get(kLastCaress).keywords.has(Keyword::Reaction));
    EXPECT_TRUE(card_db.get(kTheBall).keywords.has(Keyword::Ganking));
    EXPECT_EQ(card_db.get(kDemolitionist).might, 1);
    for (CardDefId g : {kTheBall, kStandard, kBanner})
        EXPECT_TRUE(card_registry.get(g)->hasDeploy());
}

// ── 969 Boy Who Shattered Time ──────────────────────────────────────────────

TEST_F(Radiance0Test, BoyHasEmpowerAndDisempowerAbility) {
    auto abs = card_registry.get(kBoy)->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_TRUE(abs[0].cost.disempower_self);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_TRUE(abs[1].cost.exhaust);
    EXPECT_EQ(abs[1].cost.energy, 1);
}

TEST_F(Radiance0Test, BoyDiscountsNextUnitAndKillsItAtEndOfTurn) {
    auto boy = addLegend(P1, kBoy);
    driveActivate(kBoy, P1, boy, Script{}.fn());
    const auto& mods = state.player(P1).cost_modifiers;
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_TRUE(mods[0].next_unit_only);
    EXPECT_EQ(mods[0].energy_reduction, 2);
    EXPECT_EQ(mods[0].power_reduction, 1);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::WhenYouPlayAUnit);

    // The next unit is played: it gets "Kill me at end of turn".
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    state.delayed_abilities.clear();
    driveResumableTrigger(kBoy, P1, boy, nullptr, exec, TriggerType::WhenYouPlayAUnit, unit);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::AtEndOfTurn);
    EXPECT_TRUE(state.getObject(unit).location.has_value());
    driveResumableTrigger(kBoy, P1, boy, nullptr, exec, TriggerType::AtEndOfTurn);
    EXPECT_FALSE(state.getObject(unit).location.has_value()) << "killed at end of turn";
}

// ── 970 Chronobreak ─────────────────────────────────────────────────────────

TEST_F(Radiance0Test, ChronobreakBanishesAndReplaysUnits) {
    auto at_bf = addUnit(P1, kInvalidId, 3, 0);
    auto at_base = addUnit(P1, kInvalidId, 2);
    addUnit(P2, kInvalidId, 4, 0);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Mind);
    auto spell = pushSpellOnChain(P1, kChronobreak);
    state.chain.items.clear();
    Script s;
    s.V(2).O(at_bf).O(at_base).V(0);   // X = 2; both units; BF unit back to its battlefield
    driveResumable(kChronobreak, P1, spell, s.fn(), exec);
    EXPECT_EQ(s.used(), 4u);
    EXPECT_EQ(runesOnBoard(P1), 0) << "paid [A][A]";
    const auto& a = state.getObject(at_bf);
    const auto& b = state.getObject(at_base);
    ASSERT_TRUE(a.location.has_value());
    ASSERT_TRUE(b.location.has_value());
    EXPECT_EQ(a.battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_TRUE(b.isAtBase());
    EXPECT_TRUE(a.is_exhausted) << "played units enter exhausted";
    EXPECT_TRUE(state.player(P1).banishment.empty());
}

TEST_F(Radiance0Test, ChronobreakXZeroDoesNothing) {
    auto u = addUnit(P1, kInvalidId, 3, 0);
    addRune(P1, Domain::Fury);
    auto spell = pushSpellOnChain(P1, kChronobreak);
    state.chain.items.clear();
    driveResumable(kChronobreak, P1, spell, Script{}.V(0).fn(), exec);
    EXPECT_EQ(runesOnBoard(P1), 1);
    EXPECT_EQ(state.getObject(u).battlefieldId(), std::optional<BattlefieldId>(0));
}

// ── 971 Hexplosives Expert / 986 Amateur Demolitionist ──────────────────────

TEST_F(Radiance0Test, HexplosivesExpertPlaysABombToAChosenBattlefield) {
    auto c = card_registry.get(kHexExpert)->getActivationCost();
    EXPECT_TRUE(c.exhaust);
    EXPECT_EQ(c.energy, 1);
    EXPECT_EQ(c.power, 1);
    EXPECT_TRUE(c.power_any_domain);
    auto legend = addLegend(P1, kHexExpert);
    driveActivate(kHexExpert, P1, legend, Script{}.V(1).fn());
    EXPECT_EQ(bombsAt(P1, 1), 1);
    EXPECT_EQ(bombsAt(P1, 0), 0);
}

TEST_F(Radiance0Test, AmateurDemolitionistPlaysABomb) {
    auto u = addUnit(P1, kDemolitionist, 0, -1);
    driveResumableTrigger(kDemolitionist, P1, u, Script{}.V(0).fn(), exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(bombsAt(P1, 0), 1);
    EXPECT_EQ(card_registry.get(kDemolitionist)->triggerType(), TriggerType::WhenYouPlayMe);
}

// ── 972 Hexplosive Minefield ────────────────────────────────────────────────

TEST_F(Radiance0Test, MinefieldKillsChosenBombsAndReplaysThemAtEndOfTurn) {
    auto b1 = addBomb(P1, 0);
    auto b2 = addBomb(P1, 1);
    addBomb(P2, 0);
    auto spell = pushSpellOnChain(P1, kMinefield);
    state.chain.items.clear();
    driveResumable(kMinefield, P1, spell, Script{}.O(b1).V(kPickSeveralDone).fn(), exec);
    EXPECT_FALSE(state.getObject(b1).location.has_value());
    EXPECT_TRUE(state.getObject(b2).location.has_value());
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::AtEndOfTurn);
    driveResumableTrigger(kMinefield, P1, spell, Script{}.V(0).fn(), exec, TriggerType::AtEndOfTurn);
    EXPECT_EQ(bombsAt(P1, 0), 1);
    EXPECT_EQ(bombsAt(P1, 1), 1);
}

TEST_F(Radiance0Test, MinefieldKillingNoBombsSchedulesNothing) {
    addBomb(P1, 0);
    auto spell = pushSpellOnChain(P1, kMinefield);
    state.chain.items.clear();
    driveResumable(kMinefield, P1, spell, Script{}.V(kPickSeveralDone).fn(), exec);
    EXPECT_TRUE(state.delayed_abilities.empty());
    EXPECT_EQ(bombsAt(P1, 0), 1);
}

// ── 973 Iron Revenant ───────────────────────────────────────────────────────

TEST_F(Radiance0Test, IronRevenantBanishesFromTrashToAddPower) {
    auto legend = addLegend(P1, kIronRevenant);
    Card* c = card_registry.get(kIronRevenant);
    EXPECT_FALSE(c->canActivateAbility(state, P1)) << "nothing to banish";
    addToTrash(P1, CardType::Spell);
    EXPECT_FALSE(c->canActivateAbility(state, P1)) << "spells don't count";
    auto unit = addToTrash(P1, CardType::Unit);
    EXPECT_TRUE(c->canActivateAbility(state, P1));
    EXPECT_TRUE(c->isReactionAbility());
    driveActivate(kIronRevenant, P1, legend, Script{}.O(unit).fn());
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Banishment);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(state.player(P1).rune_pool.restrictedAmount(
                  true, SpendPurpose::PlayUnit | SpendPurpose::PlayGear), 1)
        << "the [A] may be spent only to play units or gear";
}

// ── 974 Realm of Death ──────────────────────────────────────────────────────

TEST_F(Radiance0Test, RealmOfDeathShiftsMightThenTheyTrade) {
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    auto spell = pushSpellOnChain(P1, kRealmOfDeath);
    state.chain.items.clear();
    driveResumable(kRealmOfDeath, P1, spell, Script{}.O(enemy).O(mine).fn(), exec);
    // enemy 5-2 = 3, friendly 3+2 = 5: the enemy takes 5 and dies, mine takes 3.
    EXPECT_FALSE(state.getObject(enemy).location.has_value());
    ASSERT_TRUE(state.getObject(mine).location.has_value());
    EXPECT_EQ(state.getObject(mine).damage_marked, 3);
    EXPECT_EQ(state.getObject(mine).current_might, 5);
}

// ── 975 Lady of Clockwork ───────────────────────────────────────────────────

TEST_F(Radiance0Test, LadyMakesAGearAUnitWithMightEqualToItsCost) {
    auto lady = addLegend(P1, kLady);
    auto blade = addGear(P1, kBlade);
    auto bomb = addBomb(P1, 0);
    addRune(P1, Domain::Calm);
    driveResumableTrigger(kLady, P1, lady, Script{}.V(1).O(blade).fn(), exec, TriggerType::AtStartOfMain);
    const auto& g = state.getObject(blade);
    EXPECT_TRUE(g.isUnit());
    EXPECT_TRUE(g.isGear());
    EXPECT_EQ(g.current_might, card_db.get(kBlade).energy_cost);
    EXPECT_EQ(readyRuneCount(P1), 0) << "paid [1]";
    auto units = state.allUnitsControlledBy(P1);
    EXPECT_NE(std::find(units.begin(), units.end(), blade), units.end());

    // Choosing another gear ends the first one's unit-ness.
    addRune(P1, Domain::Mind);
    driveResumableTrigger(kLady, P1, lady, Script{}.V(1).O(bomb).fn(), exec, TriggerType::AtStartOfMain);
    EXPECT_FALSE(state.getObject(blade).isUnit());
    EXPECT_TRUE(state.getObject(bomb).isUnit());
    EXPECT_EQ(state.getObject(bomb).current_might, 0);
}

TEST_F(Radiance0Test, LadyDeclinedChangesNothing) {
    auto lady = addLegend(P1, kLady);
    auto blade = addGear(P1, kBlade);
    addRune(P1, Domain::Calm);
    driveResumableTrigger(kLady, P1, lady, Script{}.V(0).fn(), exec, TriggerType::AtStartOfMain);
    EXPECT_FALSE(state.getObject(blade).isUnit());
    EXPECT_EQ(readyRuneCount(P1), 1);
}

TEST_F(Radiance0Test, LadyUnitGearStopsBeingAUnitWhenItLeavesTheBoard) {
    auto lady = addLegend(P1, kLady);
    auto blade = addGear(P1, kBlade);
    addRune(P1, Domain::Calm);
    driveResumableTrigger(kLady, P1, lady, Script{}.V(1).O(blade).fn(), exec, TriggerType::AtStartOfMain);
    ASSERT_TRUE(state.getObject(blade).isUnit());
    auto& g = state.getObject(blade);
    g.location.reset();
    g.zone = ZoneType::Trash;
    card_registry.get(kLady)->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.getObject(blade).isUnit());
    EXPECT_EQ(state.getObject(lady).card_counters.count("__lady_unit_gear"), 0u);
}

TEST_F(Radiance0Test, UnitGearAtABattlefieldIsNotRecalled) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase = TurnPhase::MainPhase;
    BattlefieldState b0; b0.id = 0; b0.controller = P1; s.battlefields.push_back(b0);
    BattlefieldState b1; b1.id = 1; b1.controller = P1; s.battlefields.push_back(b1);
    auto mk = [&](int bf) {
        auto id = s.createObject();
        auto& o = s.getObject(id);
        o.owner = o.controller = P1;
        o.card_def_id = kBlade;
        o.name = "Blade";
        o.card_type = CardType::Gear;
        o.zone = ZoneType::BattlefieldZone;
        o.location = BattlefieldLocation{static_cast<BattlefieldId>(bf)};
        return id;
    };
    auto unit_gear = mk(0);
    s.getObject(unit_gear).also_unit = true;
    s.getObject(unit_gear).base_might = 3;
    s.getObject(unit_gear).recomputeMight();
    auto plain_gear = mk(1);
    engine.testHook_cleanup();
    EXPECT_TRUE(s.getObject(unit_gear).isAtBattlefield()) << "a unit isn't recalled (CR 178.1.a.1.a)";
    EXPECT_TRUE(s.getObject(plain_gear).isAtBase()) << "non-unit gear is recalled (CR 323.6)";
}

// ── 976 The Ball ────────────────────────────────────────────────────────────

TEST_F(Radiance0Test, TheBallDealsTwoToEnemyUnitsHere) {
    auto ball = addGear(P1, kTheBall, 0);
    auto e1 = addUnit(P2, kInvalidId, 2, 0);
    auto e2 = addUnit(P2, kInvalidId, 3, 0);
    auto far = addUnit(P2, kInvalidId, 2, 1);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    driveResumableTrigger(kTheBall, P1, ball, Script{}.V(1).fn(), exec, TriggerType::WhenYouPlayThis);
    EXPECT_FALSE(state.getObject(e1).location.has_value());
    EXPECT_EQ(state.getObject(e2).damage_marked, 2);
    EXPECT_EQ(state.getObject(far).damage_marked, 0);
    EXPECT_EQ(state.getObject(mine).damage_marked, 0);
}

TEST_F(Radiance0Test, TheBallCanDrawOnMove) {
    auto ball = addGear(P1, kTheBall, 0);
    addToDeck(P1, kEncore);
    EXPECT_TRUE(card_registry.get(kTheBall)->firesOn(TriggerType::WhenIMove));
    driveResumableTrigger(kTheBall, P1, ball, Script{}.V(0).fn(), exec, TriggerType::WhenIMove);
    EXPECT_EQ(handSize(P1), 1);
}

// ── 977 Pride of Nazumah ────────────────────────────────────────────────────

TEST_F(Radiance0Test, NazumahGivesAssaultEqualToShield) {
    auto legend = addLegend(P1, kNazumah);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(u).shield_value = 2;
    driveActivate(kNazumah, P1, legend, Script{}.fn(), 0, {u});
    auto& obj = state.getObject(u);
    obj.combat_designation = CombatDesignation::Attacker;
    obj.aura_effects.clear();
    card_registry.get(kNazumah)->applyPassiveAura(state, P1);
    int assault = 0, bonus = 0;
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == Keyword::Assault) { assault += ae.keyword_value; bonus += ae.might_bonus; }
    EXPECT_EQ(assault, 2);
    EXPECT_EQ(bonus, 2);
    // Next turn it's gone.
    state.turn.turn_number++;
    obj.aura_effects.clear();
    card_registry.get(kNazumah)->applyPassiveAura(state, P1);
    for (const auto& ae : obj.aura_effects) EXPECT_NE(ae.keyword, Keyword::Assault);
}

// ── 978 Ntofo Strikes ───────────────────────────────────────────────────────

TEST_F(Radiance0Test, NtofoStrikesDealsDamageEqualToShield) {
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(mine).shield_value = 1;   // printed [Shield]
    auto enemy = addUnit(P2, kInvalidId, 4, 0);
    auto spell = pushSpellOnChain(P1, kNtofo);
    state.chain.items.clear();
    driveResumable(kNtofo, P1, spell, Script{}.O(mine).O(enemy).fn(), exec);
    EXPECT_EQ(state.getObject(mine).shield_value, 3);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 3);
}

// 979 Starry-Eyed Songstress: tests/cards/test_gap_costs.cpp.

// ── 980 Encore ──────────────────────────────────────────────────────────────

TEST_F(Radiance0Test, EncoreRecruitsAndDrawsOneOfTheTopTwo) {
    auto bottom = addToDeck(P1, kEncore);
    auto second = addToDeck(P1, kEncore);
    auto top = addToDeck(P1, kEncore);
    auto spell = pushSpellOnChain(P1, kEncore);
    state.chain.items.clear();
    driveResumable(kEncore, P1, spell, Script{}.V(1).V(3).O(second).fn(), exec);
    EXPECT_TRUE(inHand(P1, second));
    const auto& deck = state.player(P1).main_deck;
    ASSERT_EQ(deck.size(), 2u);
    EXPECT_EQ(deck.front(), top) << "the other card is recycled to the bottom";
    EXPECT_EQ(deck.back(), bottom);
    int recruits = 0;
    for (const auto& [id, o] : state.objects)
        if (o.card_def_id == 954 && o.isUnit() && o.isAtBase() && o.current_might == 1) ++recruits;
    EXPECT_EQ(recruits, 2);
}

TEST_F(Radiance0Test, EncoreDamageAndPump) {
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto f1 = addUnit(P1, kInvalidId, 1, 0);
    auto f2 = addUnit(P1, kInvalidId, 1);
    auto spell = pushSpellOnChain(P1, kEncore);
    state.chain.items.clear();
    driveResumable(kEncore, P1, spell, Script{}.V(0).V(2).O(enemy).O(f1).O(f2).fn(), exec);
    EXPECT_FALSE(state.getObject(enemy).location.has_value());
    EXPECT_EQ(state.getObject(f1).current_might, 2);
    EXPECT_EQ(state.getObject(f2).current_might, 2);
}

// ── 981 Agony's Embrace ─────────────────────────────────────────────────────

TEST_F(Radiance0Test, AgonysEmbraceDrawsThenDiscardsOffTurnOncePerTurn) {
    auto legend = addLegend(P1, kAgony);
    state.turn.turn_player = P2;
    auto kept = addToHand(P1, kEncore);
    addToDeck(P1, kEncore);
    addToDeck(P1, kEncore);
    driveResumableTrigger(kAgony, P1, legend, Script{}.O(kept).fn(), exec,
                          TriggerType::WhenYouPlayFromFacedown);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(inTrash(P1, kept));
    Card* c = card_registry.get(kAgony);
    EXPECT_FALSE(c->triggerCondition(state, legend, P1, TriggerType::WhenYouPlayFromFacedown, kInvalidId));
    state.turn.turn_number++;
    EXPECT_TRUE(c->triggerCondition(state, legend, P1, TriggerType::WhenYouPlayFromFacedown, kInvalidId));
}

TEST_F(Radiance0Test, AgonysEmbraceOnYourTurnOnlyDraws) {
    auto legend = addLegend(P1, kAgony);
    state.turn.turn_player = P1;
    addToDeck(P1, kEncore);
    driveResumableTrigger(kAgony, P1, legend, Script{}.fn(), exec, TriggerType::WhenYouPlayFromFacedown);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Radiance0Test, LegendsTriggerOnFaceDownPlays) {
    auto legend = addLegend(P1, kAgony);
    auto card = addUnit(P1, kInvalidId, 2, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(PlayedFromFacedownEvent{card, P1});
    EXPECT_NE(chainItemFrom(legend), nullptr);
}

// ── 982 Last Caress ─────────────────────────────────────────────────────────

TEST_F(Radiance0Test, LastCaressBuffsTheNextUnitPlayed) {
    auto spell = pushSpellOnChain(P1, kLastCaress);
    state.chain.items.clear();
    driveResumable(kLastCaress, P1, spell, nullptr, exec);
    EXPECT_TRUE(state.player(P1).next_unit_any_battlefield);

    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    auto unit = addUnit(P1, kInvalidId, 2, 1);
    events.emit(CardPlayedEvent{unit, P1, CardType::Unit, 1});
    EXPECT_FALSE(state.player(P1).next_unit_any_battlefield) << "used by this unit";
    const ChainItem* item = chainItemFrom(spell);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->triggering_subject, unit);
    EXPECT_TRUE(state.delayed_abilities.empty());

    driveResumableTrigger(kLastCaress, P1, spell, nullptr, exec, TriggerType::WhenYouPlayAUnit, unit);
    EXPECT_EQ(state.getObject(unit).buff_count, 1);
}

TEST_F(Radiance0Test, LastCaressOpensEveryBattlefieldToTheNextUnit) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase = TurnPhase::MainPhase;
    s.turn.ns_state = NeutralShowdownState::Neutral;
    s.turn.oc_state = OpenClosedState::Open;
    BattlefieldState b0; b0.id = 0; s.battlefields.push_back(b0);
    BattlefieldState b1; b1.id = 1; b1.controller = P2; s.battlefields.push_back(b1);
    auto card = s.createObject();
    {
        auto& o = s.getObject(card);
        o.owner = o.controller = P1;
        o.card_def_id = kDemolitionist;
        o.name = "Amateur Demolitionist";
        o.card_type = CardType::Unit;
        o.zone = ZoneType::Hand;
        s.player(P1).hand.push_back(card);
    }
    for (int i = 0; i < 2; ++i) {
        auto r = s.createObject();
        auto& o = s.getObject(r);
        o.owner = o.controller = P1;
        o.card_type = CardType::Rune;
        o.domains = {Domain::Chaos};
        o.zone = ZoneType::Base;
        o.location = BaseLocation{P1};
    }
    auto playsTo = [&](BattlefieldId bf) {
        for (const auto& a : engine.generateLegalActions())
            if (a.type == IntentType::PlayCard && a.card == card && a.play_location.has_value() &&
                *a.play_location == LocationId{BattlefieldLocation{bf}})
                return true;
        return false;
    };
    EXPECT_FALSE(playsTo(0));
    EXPECT_FALSE(playsTo(1));
    s.player(P1).next_unit_any_battlefield = true;
    EXPECT_TRUE(playsTo(0));
    EXPECT_TRUE(playsTo(1));
}

// ── 983 Exemplar of Demacia ─────────────────────────────────────────────────

TEST_F(Radiance0Test, ExemplarMovesAUnitToTheDeployedGearsBattlefield) {
    auto legend = addLegend(P1, kExemplar);
    auto gear = addGear(P1, kBanner, 1);
    auto unit = addUnit(P1, kInvalidId, 2);
    addRune(P1, Domain::Body);
    Card* c = card_registry.get(kExemplar);
    EXPECT_TRUE(c->triggerCondition(state, legend, P1, TriggerType::WhenYouPlayACard, gear));
    EXPECT_FALSE(c->triggerCondition(state, legend, P1, TriggerType::WhenYouPlayACard, unit));
    driveResumableTrigger(kExemplar, P1, legend, Script{}.V(1).O(unit).fn(), exec,
                          TriggerType::WhenYouPlayACard, gear);
    EXPECT_EQ(state.getObject(unit).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    EXPECT_EQ(runesOnBoard(P1), 0) << "paid [A]";
}

TEST_F(Radiance0Test, ExemplarCannotBeUsedWithoutPower) {
    auto legend = addLegend(P1, kExemplar);
    auto gear = addGear(P1, kBanner, 1);
    auto unit = addUnit(P1, kInvalidId, 2);
    driveResumableTrigger(kExemplar, P1, legend, Script{}.fn(), exec, TriggerType::WhenYouPlayACard, gear);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
    EXPECT_FALSE(state.getObject(legend).is_exhausted);
}

// ── 984 Demacian Standard ───────────────────────────────────────────────────

TEST_F(Radiance0Test, DemacianStandardPumpsAnAttackerAndCanGoHome) {
    auto gear = addGear(P1, kStandard, 0);
    auto a1 = addUnit(P1, kInvalidId, 2, 0);
    auto a2 = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(a1).combat_designation = CombatDesignation::Attacker;
    state.getObject(a2).combat_designation = CombatDesignation::Attacker;
    driveResumableTrigger(kStandard, P1, gear, Script{}.O(a2).fn(), exec, TriggerType::WhenIAttack);
    EXPECT_EQ(state.getObject(a2).current_might, 4);
    EXPECT_EQ(state.getObject(a1).current_might, 2);

    driveResumableTrigger(kStandard, P1, gear, Script{}.V(1).fn(), exec, TriggerType::WhenYouHoldHere);
    EXPECT_TRUE(inHand(P1, gear));
}

TEST_F(Radiance0Test, GearAtABattlefieldTriggersOnAttackAndHoldThere) {
    auto gear = addGear(P1, kStandard, 0);
    addUnit(P1, kInvalidId, 2, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(CombatStartedEvent{1, P1, P2});
    EXPECT_EQ(chainItemFrom(gear), nullptr) << "not its battlefield";
    events.emit(CombatStartedEvent{0, P2, P1});
    EXPECT_EQ(chainItemFrom(gear), nullptr) << "its controller is defending";
    events.emit(CombatStartedEvent{0, P1, P2});
    const ChainItem* atk = chainItemFrom(gear);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(atk->fired_trigger, TriggerType::WhenIAttack);
    state.chain.items.clear();
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Hold, 1});
    const ChainItem* hold = chainItemFrom(gear);
    ASSERT_NE(hold, nullptr);
    EXPECT_EQ(hold->fired_trigger, TriggerType::WhenYouHoldHere);
}

// ── 987 Banner of Conquest ──────────────────────────────────────────────────

TEST_F(Radiance0Test, BannerGivesFriendlyUnitsHereAssault) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    BattlefieldState b0; b0.id = 0; s.battlefields.push_back(b0);
    BattlefieldState b1; b1.id = 1; s.battlefields.push_back(b1);
    auto mk = [&](PlayerId p, CardType t, int bf, CardDefId def) {
        auto id = s.createObject();
        auto& o = s.getObject(id);
        o.owner = o.controller = p;
        o.card_def_id = def;
        o.card_type = t;
        o.base_might = 2;
        o.zone = ZoneType::BattlefieldZone;
        o.location = BattlefieldLocation{static_cast<BattlefieldId>(bf)};
        return id;
    };
    mk(P1, CardType::Gear, 0, kBanner);
    auto friend_here = mk(P1, CardType::Unit, 0, kInvalidId);
    auto enemy_here = mk(P2, CardType::Unit, 0, kInvalidId);
    auto friend_away = mk(P1, CardType::Unit, 1, kInvalidId);
    engine.testHook_cleanup();
    EXPECT_TRUE(s.getObject(friend_here).hasKeyword(Keyword::Assault));
    EXPECT_FALSE(s.getObject(enemy_here).hasKeyword(Keyword::Assault));
    EXPECT_FALSE(s.getObject(friend_away).hasKeyword(Keyword::Assault));
}

}  // namespace
}  // namespace riftbound::test
