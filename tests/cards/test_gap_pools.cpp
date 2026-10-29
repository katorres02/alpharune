/// Gap round 2 — "pools":
///   1. Restricted Energy / Power (RunePool::restricted, SpendPurpose):
///      Iron Revenant (973), Fire Below the Mountain (506), and the cards that
///      already had a restriction (Daughter of the Void 294, Lux, Crownguard
///      312, Scorn of the Moon 786, Butcher of the Sands' unit-only Energy).
///   2. "Spend a buff" (EffectExecutor::spendBuff -> WhenYouSpendABuff):
///      Fae Dragon (423) and the cards that spend buffs.
///   3. Playing a unit from the trash (Card::trashPlayCost -> standing
///      TrashReplayGrant): Undying Legion (587).

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kHextechRay = 9;           // spell [1] + [R]
constexpr CardDefId kSunDisc = 21;             // gear [2] + [R]
constexpr CardDefId kSettBrawler = 164;        // "Spend my buff: +4 [M] this turn"
constexpr CardDefId kDaughterOfTheVoid = 294;
constexpr CardDefId kLuxCrownguard = 312;
constexpr CardDefId kLongSword = 345;          // Equip [R]
constexpr CardDefId kFaeDragon = 423;          // unit [7] + [O]
constexpr CardDefId kFireBelowMtn = 506;
constexpr CardDefId kUndyingLegion = 587;      // unit [3]; trash: [3][R] with Legion
constexpr CardDefId kIronRevenant = 973;
constexpr CardDefId kGoldToken = 326;

constexpr SpendPurpose kUnitsOrGear = SpendPurpose::PlayUnit | SpendPurpose::PlayGear;
constexpr SpendPurpose kGearOnly = SpendPurpose::PlayGear | SpendPurpose::GearAbility;

/// Answers MakeChoice prompts from a script of object ids / values (in order),
/// passes priority otherwise; when the script runs out it answers the first
/// option with chosen_value 0 / the first option.
class ScriptAgent : public AgentInterface {
public:
    struct Ans { std::optional<int> value; GameObjectId obj = kInvalidId; };
    std::vector<Ans> script;
    size_t at = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        bool any_choice = false;
        for (const auto& i : legal) if (i.type == IntentType::MakeChoice) any_choice = true;
        if (!any_choice) {
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
        if (at < script.size()) {
            const Ans a = script[at];
            for (const auto& i : legal) {
                if (a.value && i.chosen_value == a.value) { ++at; return i; }
                if (a.obj != kInvalidId && !i.chosen_objects.empty() &&
                    i.chosen_objects[0] == a.obj) { ++at; return i; }
            }
        }
        for (const auto& i : legal) if (i.chosen_value == 0) return i;
        return legal.front();
    }
};

class GapPoolsTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    ScriptAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    EffectExecutor& ex() { return engine_->testHook_executor(); }

    void SetUp() override {
        CardTestFixture::SetUp();
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
        for (PlayerId p : {P1, P2}) {
            auto lid = st.createObject();
            auto& lo = st.getObject(lid);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = lid;
        }
    }

    GameObjectId make(PlayerId p, CardDefId def_id, ZoneType zone) {
        auto id = s().createObject();
        auto& o = s().getObject(id);
        const auto& def = card_db.get(def_id);
        o.owner = o.controller = p;
        o.card_def_id = def_id;
        o.card_type = def.card_type;
        o.name = def.name;
        o.domains = def.domains;
        o.keywords = def.keywords;
        o.tags = def.tags;
        o.base_might = o.current_might = def.might;
        o.zone = zone;
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = make(p, def_id, ZoneType::Hand);
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId inTrash(PlayerId p, CardDefId def_id) {
        auto id = make(p, def_id, ZoneType::Trash);
        s().player(p).trash.push_back(id);
        return id;
    }
    GameObjectId onBoard(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = make(p, def_id, at_bf >= 0 ? ZoneType::BattlefieldZone : ZoneType::Base);
        auto& o = s().getObject(id);
        if (at_bf >= 0) o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        else o.location = BaseLocation{p};
        return id;
    }
    GameObjectId plainUnit(PlayerId p, int might = 2) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.name = "TestUnit";
        u.base_might = u.current_might = might;
        u.zone = ZoneType::Base;
        u.location = BaseLocation{p};
        return id;
    }
    GameObjectId rune(PlayerId p, Domain d, bool exhausted = false) {
        auto id = s().createObject();
        auto& r = s().getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        r.is_exhausted = exhausted;
        return id;
    }
    void activate(CardDefId def_id, GameObjectId source, PlayerId p = PlayerId::Player1) {
        CardContext ctx{s(), events, ex(), p, source};
        card_registry.get(def_id)->onActivate(ctx, {});
    }
    std::vector<Intent> playsOf(GameObjectId card) {
        std::vector<Intent> out;
        for (const auto& a : engine_->generateLegalActions())
            if (a.card == card && (a.type == IntentType::PlayCard ||
                                   a.type == IntentType::PlayActionCard ||
                                   a.type == IntentType::PlayReaction))
                out.push_back(a);
        return out;
    }
    int countOnBoard(PlayerId p, CardDefId def_id) {
        int n = 0;
        for (const auto& [id, o] : s().objects)
            if (o.controller == p && o.card_def_id == def_id && o.location.has_value()) ++n;
        return n;
    }
    /// Run `def_id`'s trigger as the engine would: once at finalization (when the
    /// card opts in), then at resolution, answering prompts with `agent`.
    void runTrigger(CardDefId def_id, GameObjectId source, TriggerType firing,
                    ScriptAgent& agent) {
        Card* card = card_registry.get(def_id);
        ChainItem ri;
        ri.id = s().chain.allocateId();
        ri.controller = P1;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        const bool finalize = card->choicesAtFinalization() & Card::kFinalizeTrigger;
        ri.finalizing = finalize;
        s().chain.resuming = ri;
        CardContext ctx{s(), events, ex(), P1, source};
        ctx.firing_trigger = firing;
        ctx.registry = &card_registry;
        auto drive = [&] {
            card->onTrigger(ctx, {});
            while (ex().hasPendingChoice()) {
                auto pending = ex().consumePendingChoice();
                ex().recordChoice(agent.selectAction(s(), pending.legal));
                card->onTrigger(ctx, {});
            }
        };
        drive();
        if (finalize) {
            ChainItem done = *s().chain.resuming;
            done.finalizing = false;
            done.choices_finalized = true;
            done.finalize_checked = 0;
            s().chain.resuming = done;
            drive();
        }
        s().chain.resuming.reset();
    }
};

// ═══ 1. Restricted pools ════════════════════════════════════════════════════

TEST_F(GapPoolsTest, RunePool_RestrictedEntriesMergeAndSpendNarrowestFirst) {
    RunePool pool;
    pool.addRestricted(true, 1, kUnitsOrGear);
    pool.addRestricted(true, 2, kUnitsOrGear);
    pool.addRestricted(true, 1, SpendPurpose::PlayUnit);
    EXPECT_EQ(pool.restricted.size(), 2u);
    EXPECT_EQ(pool.restrictedAvailable(true, SpendPurpose::PlayUnit, false), 4);
    EXPECT_EQ(pool.restrictedAvailable(true, SpendPurpose::PlayGear, false), 3);
    EXPECT_EQ(pool.restrictedAvailable(true, SpendPurpose::PlaySpell, false), 0);
    EXPECT_EQ(pool.restrictedAvailable(false, SpendPurpose::PlayUnit, false), 0)
        << "restricted Power is not Energy";
    // Paying 2 for a unit uses the unit-only [A] before the units-or-gear one.
    EXPECT_EQ(pool.spendRestricted(true, 2, SpendPurpose::PlayUnit, false), 2);
    EXPECT_EQ(pool.restrictedAmount(true, SpendPurpose::PlayUnit), 0);
    EXPECT_EQ(pool.restrictedAmount(true, kUnitsOrGear), 2);
    pool.clear();
    EXPECT_TRUE(pool.restricted.empty());
}

TEST_F(GapPoolsTest, RunePool_ShowdownOnlyEnergyNeedsAShowdown) {
    RunePool pool;
    pool.addRestricted(false, 1, SpendPurpose::Any, /*showdown_only=*/true);
    EXPECT_EQ(pool.restrictedAvailable(false, SpendPurpose::PlaySpell, false), 0);
    EXPECT_EQ(pool.restrictedAvailable(false, SpendPurpose::PlaySpell, true), 1);
    EXPECT_EQ(pool.spendRestricted(false, 3, SpendPurpose::PlaySpell, false), 0);
    EXPECT_EQ(pool.spendRestricted(false, 3, SpendPurpose::PlaySpell, true), 1);
}

TEST_F(GapPoolsTest, SpendPurposeOfPlaysAndAbilities) {
    auto spell = inHand(P1, kHextechRay);
    auto gear = inHand(P1, kSunDisc);
    auto unit = inHand(P1, kFaeDragon);
    EXPECT_EQ(playSpendPurpose(s().getObject(spell)), SpendPurpose::PlaySpell);
    EXPECT_EQ(playSpendPurpose(s().getObject(gear)), SpendPurpose::PlayGear);
    EXPECT_EQ(playSpendPurpose(s().getObject(unit)), SpendPurpose::PlayUnit);
    EXPECT_EQ(abilitySpendPurpose(s().getObject(gear)), SpendPurpose::GearAbility);
    EXPECT_EQ(abilitySpendPurpose(s().getObject(unit)), SpendPurpose::UnitAbility);
    EXPECT_EQ(abilitySpendPurpose(s().getObject(s().player(P1).legend_zone)),
              SpendPurpose::OtherAbility);
}

// ── 973 Iron Revenant: "Spend this Power only to play units or gear." ────────
TEST_F(GapPoolsTest, IronRevenant_PowerPaysForAUnitAndAGear) {
    // What Iron Revenant's ability adds (activation: test_radiance_0).
    s().player(P1).rune_pool.addRestricted(true, 1, kUnitsOrGear);

    s().player(P1).rune_pool.energy = 7;
    auto dragon = inHand(P1, kFaeDragon);             // [7][O]: no Body rune at all
    EXPECT_TRUE(engine_->testHook_canAfford(P1, dragon));
    ASSERT_TRUE(engine_->testHook_payCardCost(P1, dragon));
    EXPECT_TRUE(s().player(P1).rune_pool.restricted.empty()) << "the [A] paid the [O]";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);

    s().player(P1).rune_pool.addRestricted(true, 1, kUnitsOrGear);
    s().player(P1).rune_pool.energy = 2;
    auto disc = inHand(P1, kSunDisc);                 // gear [2][R]
    EXPECT_TRUE(engine_->testHook_canAfford(P1, disc));
}

TEST_F(GapPoolsTest, IronRevenant_PowerCannotPayForASpellOrAnAbility) {
    s().player(P1).rune_pool.addRestricted(true, 1, kUnitsOrGear);
    s().player(P1).rune_pool.energy = 1;
    auto ray = inHand(P1, kHextechRay);               // spell [1][R]
    EXPECT_FALSE(engine_->testHook_canAfford(P1, ray));
    EXPECT_FALSE(engine_->testHook_canPayAdditionalCost(P1, 0, 1, Domain::Fury))
        << "an ordinary (purpose-less) Power cost can't use it";
    // [Equip] is a gear ability, not a gear play.
    auto sword = onBoard(P1, kLongSword);
    auto bearer = plainUnit(P1);
    CardContext ctx{s(), events, ex(), P1, sword};
    EXPECT_FALSE(card_registry.get(kLongSword)->onEquip(ctx, bearer));
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(true, kUnitsOrGear), 1);
}

// ── 506 Fire Below the Mountain: "Use only to play gear or use gear abilities."
TEST_F(GapPoolsTest, FireBelow_PowerPaysForGearAndEquipButNotUnitsOrSpells) {
    auto legend = s().player(P1).legend_zone;
    activate(kFireBelowMtn, legend);
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(true, kGearOnly), 1);
    EXPECT_EQ(s().player(P1).rune_pool.universal_power, 0);

    s().player(P1).rune_pool.energy = 7;
    EXPECT_FALSE(engine_->testHook_canAfford(P1, inHand(P1, kFaeDragon))) << "not a unit";
    EXPECT_FALSE(engine_->testHook_canAfford(P1, inHand(P1, kHextechRay))) << "not a spell";
    EXPECT_TRUE(engine_->testHook_canAfford(P1, inHand(P1, kSunDisc))) << "a gear";

    // [Equip] [R] (Long Sword) paid with the gear-only [A], no rune needed.
    s().player(P1).rune_pool.energy = 0;
    auto sword = onBoard(P1, kLongSword);
    auto bearer = plainUnit(P1);
    CardContext ctx{s(), events, ex(), P1, sword};
    ASSERT_TRUE(card_registry.get(kLongSword)->onEquip(ctx, bearer));
    EXPECT_EQ(s().getObject(sword).attached_to, bearer);
    EXPECT_TRUE(s().player(P1).rune_pool.restricted.empty());
}

TEST_F(GapPoolsTest, FireBelow_PowerPaysAGearsActivatedAbilityPowerCost) {
    s().player(P1).rune_pool.addRestricted(true, 1, kGearOnly);
    auto gear = onBoard(P1, kSunDisc);
    auto unit = plainUnit(P1);
    ActivationCost c;
    c.power = 1;
    c.power_any_domain = true;
    EXPECT_TRUE(engine_->testHook_canPayActivationResources(P1, 0, c,
                                                   abilitySpendPurpose(s().getObject(gear))));
    EXPECT_FALSE(engine_->testHook_canPayActivationResources(P1, 0, c,
                                                    abilitySpendPurpose(s().getObject(unit))));
    ASSERT_TRUE(engine_->testHook_payActivationResources(P1, 0, c,
                                                abilitySpendPurpose(s().getObject(gear))));
    EXPECT_TRUE(s().player(P1).rune_pool.restricted.empty());
}

// ── 294 Daughter of the Void / 312 Lux, Crownguard: spell-only resources ────
TEST_F(GapPoolsTest, DaughterOfTheVoid_PowerStillOnlyPaysSpells) {
    activate(kDaughterOfTheVoid, s().player(P1).legend_zone);
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(true, SpendPurpose::PlaySpell), 1);
    s().player(P1).rune_pool.energy = 1;
    auto ray = inHand(P1, kHextechRay);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, ray));
    s().player(P1).rune_pool.energy = 2;
    EXPECT_FALSE(engine_->testHook_canAfford(P1, inHand(P1, kSunDisc)));
    ASSERT_TRUE(engine_->testHook_payCardCost(P1, ray));
    EXPECT_TRUE(s().player(P1).rune_pool.restricted.empty());
}

TEST_F(GapPoolsTest, LuxCrownguard_EnergyStillOnlyPaysSpells) {
    auto lux = onBoard(P1, kLuxCrownguard);
    activate(kLuxCrownguard, lux);
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(false, SpendPurpose::PlaySpell), 2);
    rune(P1, Domain::Fury);                            // the [R] for Hextech Ray
    auto ray = inHand(P1, kHextechRay);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, ray));
    EXPECT_FALSE(engine_->testHook_canAfford(P1, inHand(P1, kSunDisc)))
        << "the 2 spell-only Energy can't pay a gear's [2]";
    ASSERT_TRUE(engine_->testHook_payCardCost(P1, ray));
    EXPECT_EQ(s().player(P1).rune_pool.restrictedAmount(false, SpendPurpose::PlaySpell), 1);
}

// ═══ 2. Spending a buff ═════════════════════════════════════════════════════

TEST_F(GapPoolsTest, SpendBuff_RemovesTheBuffAndEmitsTheEvent) {
    auto u = plainUnit(P1, 2);
    std::vector<std::string> seen;
    auto conn = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) { if (e.object == u) seen.push_back(e.what_changed); });
    EXPECT_FALSE(ex().spendBuff(u)) << "no buff to spend";
    EXPECT_TRUE(seen.empty());
    ex().buffUnit(u);
    EXPECT_EQ(s().getObject(u).current_might, 3);
    ASSERT_TRUE(ex().spendBuff(u));
    EXPECT_EQ(s().getObject(u).buff_count, 0);
    EXPECT_EQ(s().getObject(u).current_might, 2);
    EXPECT_NE(std::find(seen.begin(), seen.end(), "buff_spent"), seen.end());
    conn.disconnect();
}

TEST_F(GapPoolsTest, FaeDragon_SpendingABuffTriggersIt) {
    auto dragon = onBoard(P1, kFaeDragon);
    auto u = plainUnit(P1);
    s().getObject(u).buff_count = 1;
    s().chain.items.clear();
    ASSERT_TRUE(ex().spendBuff(u));
    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().source, dragon);
    EXPECT_EQ(s().chain.items.back().fired_trigger, TriggerType::WhenYouSpendABuff);
    s().chain.items.clear();
    // An opponent's buff spent doesn't trigger my Fae Dragon.
    auto theirs = plainUnit(P2);
    s().getObject(theirs).buff_count = 1;
    ex().spendBuff(theirs);
    EXPECT_TRUE(s().chain.items.empty());
}

TEST_F(GapPoolsTest, FaeDragon_SpendTriggerPlaysAnExhaustedGoldToken) {
    auto dragon = onBoard(P1, kFaeDragon);
    runTrigger(kFaeDragon, dragon, TriggerType::WhenYouSpendABuff, a1_);
    ASSERT_EQ(countOnBoard(P1, kGoldToken), 1);
    for (const auto& [id, o] : s().objects)
        if (o.card_def_id == kGoldToken) EXPECT_TRUE(o.is_exhausted);
}

TEST_F(GapPoolsTest, FaeDragon_PlayTriggerBuffsTheChosenUnitsUpToFour) {
    auto dragon = onBoard(P1, kFaeDragon);
    std::vector<GameObjectId> units;
    for (int i = 0; i < 5; ++i) units.push_back(plainUnit(P1));
    ScriptAgent agent;
    agent.script = {{std::nullopt, units[1]}, {std::nullopt, units[3]},
                    {kPickSeveralDone, kInvalidId}};
    runTrigger(kFaeDragon, dragon, TriggerType::WhenYouPlayMe, agent);
    EXPECT_EQ(s().getObject(units[1]).buff_count, 1);
    EXPECT_EQ(s().getObject(units[3]).buff_count, 1);
    EXPECT_EQ(s().getObject(units[0]).buff_count, 0);
    EXPECT_EQ(s().getObject(units[4]).buff_count, 0);
    EXPECT_EQ(s().getObject(dragon).buff_count, 0);

    // At most four, even without "Done".
    ScriptAgent greedy;
    greedy.script = {{std::nullopt, units[0]}, {std::nullopt, units[2]},
                     {std::nullopt, units[4]}, {std::nullopt, dragon}};
    runTrigger(kFaeDragon, dragon, TriggerType::WhenYouPlayMe, greedy);
    EXPECT_EQ(s().getObject(units[0]).buff_count, 1);
    EXPECT_EQ(s().getObject(units[4]).buff_count, 1);
    EXPECT_EQ(s().getObject(dragon).buff_count, 1);
}

TEST_F(GapPoolsTest, SettBrawler_SpendingItsBuffTriggersFaeDragon) {
    onBoard(P1, kFaeDragon);
    auto sett = onBoard(P1, kSettBrawler);
    s().getObject(sett).buff_count = 1;
    s().getObject(sett).recomputeMight();
    s().chain.items.clear();
    activate(kSettBrawler, sett);
    EXPECT_EQ(s().getObject(sett).buff_count, 0);
    EXPECT_EQ(s().getObject(sett).temp_might_bonus, 4);
    bool fired = false;
    for (const auto& ci : s().chain.items)
        if (ci.fired_trigger == TriggerType::WhenYouSpendABuff) fired = true;
    EXPECT_TRUE(fired);
}

// ═══ 3. Playing a unit from the trash ═══════════════════════════════════════

TEST_F(GapPoolsTest, UndyingLegion_NeedsLegionToBePlayedFromTheTrash) {
    auto legion = inTrash(P1, kUndyingLegion);
    s().player(P1).rune_pool.energy = 3;
    rune(P1, Domain::Fury, /*exhausted=*/true);
    engine_->testHook_cleanup();
    EXPECT_TRUE(playsOf(legion).empty()) << "no other card played this turn";
    Card* c = card_registry.get(kUndyingLegion);
    EXPECT_FALSE(c->trashPlayCost(s(), P1).valid);
    s().player(P1).cards_played_this_turn = 1;
    auto tp = c->trashPlayCost(s(), P1);
    ASSERT_TRUE(tp.valid);
    EXPECT_EQ(tp.energy, 3);
    EXPECT_EQ(tp.power, 1);
    EXPECT_EQ(tp.power_domain, Domain::Fury);
}

TEST_F(GapPoolsTest, UndyingLegion_PlayedFromTheTrashToBaseOrAControlledBattlefield) {
    auto legion = inTrash(P1, kUndyingLegion);
    s().player(P1).cards_played_this_turn = 1;
    s().battlefields[1].controller = P1;
    {   // a P1 unit holds battlefield 1
        auto holder = plainUnit(P1);
        s().getObject(holder).zone = ZoneType::BattlefieldZone;
        s().getObject(holder).location = BattlefieldLocation{1};
    }
    s().player(P1).rune_pool.energy = 3;
    auto fury = rune(P1, Domain::Fury, /*exhausted=*/true);
    engine_->testHook_cleanup();

    auto plays = playsOf(legion);
    ASSERT_EQ(plays.size(), 2u) << "base + the battlefield P1 controls";
    bool base = false, bf1 = false;
    for (const auto& p : plays) {
        EXPECT_EQ(p.play_source, Intent::PlaySource::Trash);
        ASSERT_TRUE(p.play_location.has_value());
        if (std::holds_alternative<BaseLocation>(*p.play_location)) base = true;
        if (auto* b = std::get_if<BattlefieldLocation>(&*p.play_location); b && b->id == 1)
            bf1 = true;
    }
    EXPECT_TRUE(base);
    EXPECT_TRUE(bf1);

    Intent at_bf;
    for (const auto& p : plays)
        if (std::holds_alternative<BattlefieldLocation>(*p.play_location)) at_bf = p;
    engine_->testHook_executeIntent(at_bf);
    const auto& u = s().getObject(legion);
    ASSERT_TRUE(u.location.has_value());
    EXPECT_EQ(u.battlefieldId(), std::optional<BattlefieldId>{1});
    EXPECT_EQ(std::count(s().player(P1).trash.begin(), s().player(P1).trash.end(), legion), 0);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0) << "paid [3]";
    EXPECT_FALSE(s().getObject(fury).location.has_value()) << "recycled the Fury rune for [R]";
}

TEST_F(GapPoolsTest, UndyingLegion_NotOfferedWithoutTheFuryPower) {
    auto legion = inTrash(P1, kUndyingLegion);
    s().player(P1).cards_played_this_turn = 1;
    s().player(P1).rune_pool.energy = 3;
    rune(P1, Domain::Calm, /*exhausted=*/true);
    engine_->testHook_cleanup();
    EXPECT_TRUE(playsOf(legion).empty());
    // Iron Revenant's units-or-gear [A] can pay the [R].
    s().player(P1).rune_pool.addRestricted(true, 1, kUnitsOrGear);
    EXPECT_FALSE(playsOf(legion).empty());
}

TEST_F(GapPoolsTest, UndyingLegion_InHandIsPlayedNormally) {
    auto legion = inHand(P1, kUndyingLegion);
    s().player(P1).rune_pool.energy = 3;
    engine_->testHook_cleanup();
    auto plays = playsOf(legion);
    ASSERT_FALSE(plays.empty()) << "[3] from hand, no [R] needed";
    for (const auto& p : plays) EXPECT_EQ(p.play_source, Intent::PlaySource::Hand);
}

}  // namespace
