/// @file test_radiance_1.cpp
/// Radiance batch 1: cards 988-1006 (RAD-003 .. RAD-021), plus the engine
/// support they use (play-from-banishment permission, Firebrand Raider's
/// turn flags, own-death replacement, per-battlefield Deathknell extra fires,
/// gear Deathknell / leaves-the-board triggers, triggers from the trash).

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <algorithm>
#include <memory>
#include <optional>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kBilgewaterTracker = 988;
constexpr CardDefId kBrawl = 989;
constexpr CardDefId kLingeringAnimus = 990;
constexpr CardDefId kScorchedEarth = 991;
constexpr CardDefId kRallyForWar = 992;
constexpr CardDefId kWarScout = 993;
constexpr CardDefId kAntagonisticApparatus = 994;
constexpr CardDefId kForbiddenTechnique = 995;
constexpr CardDefId kIncorrigibleConstruct = 996;
constexpr CardDefId kRunebladeAnvil = 997;
constexpr CardDefId kLostToTheSands = 998;
constexpr CardDefId kShimmeringSwimmer = 999;
constexpr CardDefId kAkaliBrash = 1000;
constexpr CardDefId kCoalescedEnergy = 1001;
constexpr CardDefId kDetonationAmplifier = 1002;
constexpr CardDefId kDrakehound = 1003;
constexpr CardDefId kFirebrandRaider = 1004;
constexpr CardDefId kMordekaiser = 1005;
constexpr CardDefId kEkko = 1006;
constexpr CardDefId kWatchfulSentry = 96;   // "[Deathknell] — Draw 1."
constexpr CardDefId kBomb = 985;

/// One scripted answer: an int choice (value) or an object choice.
struct Ans {
    std::optional<int> value;
    GameObjectId obj = kInvalidId;
};
Ans val(int v) { return Ans{v, kInvalidId}; }
Ans obj(GameObjectId o) { return Ans{std::nullopt, o}; }

/// Answers MakeChoice prompts from a script (in order), passes priority
/// otherwise; once the script runs out it answers 0 / the first option.
class ScriptAgent : public AgentInterface {
public:
    std::vector<Ans> script;
    size_t at = 0;
    int prompts = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        bool any_choice = false;
        for (const auto& i : legal) if (i.type == IntentType::MakeChoice) any_choice = true;
        if (!any_choice) {
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
        ++prompts;
        if (at < script.size()) {
            const Ans a = script[at];
            for (const auto& i : legal) {
                if (a.value && i.chosen_value == a.value) { ++at; return i; }
                if (a.obj != kInvalidId && !i.chosen_objects.empty() && i.chosen_objects[0] == a.obj) {
                    ++at; return i;
                }
            }
        }
        for (const auto& i : legal) if (i.chosen_value == 0) return i;
        return legal.front();
    }
};

using Picker = std::function<Intent(const std::vector<Intent>&)>;
/// A picker for the bare-executor drive helpers, answering from a script.
Picker scripted(std::vector<Ans> script) {
    auto agent = std::make_shared<ScriptAgent>();
    agent->script = std::move(script);
    return [agent](const std::vector<Intent>& legal) {
        GameState dummy;
        return agent->selectAction(dummy, legal);
    };
}

class Radiance1Test : public CardTestFixture {
protected:
    GameObjectId gear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = owner;
        o.card_def_id = def_id;
        o.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            o.name = card_db.get(def_id).name;
            o.tags = card_db.get(def_id).tags;
        } else {
            o.name = "TestGear";
        }
        if (at_bf >= 0) {
            o.zone = ZoneType::BattlefieldZone;
            o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            o.zone = ZoneType::Base;
            o.location = BaseLocation{owner};
        }
        return id;
    }
    /// The object of a spell P1 is resolving.
    GameObjectId spellObj(CardDefId def_id) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = P1;
        o.card_def_id = def_id;
        o.card_type = CardType::Spell;
        o.name = card_db.get(def_id).name;
        o.zone = ZoneType::Chain;
        return id;
    }

    /// Finalize a triggered ability (its choices), then resolve it.
    void runTrigger(CardDefId def_id, GameObjectId source, Picker picker, EffectExecutor& exec,
                    TriggerType firing = TriggerType::None, PlayerId controller = PlayerId::Player1) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        // Only cards that opt in are run once at finalization.
        const bool finalize = card->choicesAtFinalization() & Card::kFinalizeTrigger;
        ri.finalizing = finalize;
        state.chain.resuming = ri;
        CardContext ctx{state, events, exec, controller, source};
        ctx.firing_trigger = firing;
        ctx.registry = &card_registry;
        auto drive = [&] {
            card->onTrigger(ctx, {});
            while (exec.hasPendingChoice()) {
                auto pending = exec.consumePendingChoice();
                exec.recordChoice(picker(pending.legal));
                card->onTrigger(ctx, {});
            }
        };
        if (!finalize) { drive(); state.chain.resuming.reset(); return; }
        drive();
        ChainItem done = *state.chain.resuming;
        done.finalizing = false;
        done.choices_finalized = true;
        done.finalize_checked = 0;
        state.chain.resuming = done;
        drive();
        state.chain.resuming.reset();
    }
};

/// Tests that need the engine (plays, cleanup, kill paths, agents).
class Radiance1EngineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    ScriptAgent a1_, a2_;
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
        for (PlayerId p : {P1, P2}) {
            auto lid = st.createObject();
            auto& lo = st.getObject(lid);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = lid;
        }
    }
    void fillObj(GameObject& o, PlayerId p, CardDefId def_id) {
        o.owner = o.controller = p;
        o.card_def_id = def_id;
        if (def_id == kInvalidId) { o.name = "TestUnit"; return; }
        const auto& d = card_db.get(def_id);
        o.name = d.name;
        o.card_type = d.card_type;
        o.super_type = d.super_type;
        o.domains = d.domains;
        o.tags = d.tags;
        o.keywords = d.keywords;
        o.base_might = d.might;
        o.current_might = d.might;
        o.assault_value = d.assault_value;
        o.might_bonus = d.might_bonus;
    }
    GameObjectId unit(PlayerId p, CardDefId def_id, int might, int at_bf = -1) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.card_type = CardType::Unit;
        fillObj(u, p, def_id);
        if (might > 0) u.base_might = u.current_might = might;
        if (at_bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId gearOn(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = unit(p, def_id, 0, at_bf);
        auto& g = s().getObject(id);
        g.card_type = CardType::Gear;
        g.base_might = g.current_might = 0;
        if (def_id == kInvalidId) g.name = "TestGear";
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        fillObj(s().getObject(id), p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId onDeck(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        fillObj(s().getObject(id), p, def_id);
        s().getObject(id).zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);
        return id;
    }
    GameObjectId inTrash(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        fillObj(s().getObject(id), p, def_id);
        s().getObject(id).zone = ZoneType::Trash;
        s().player(p).trash.push_back(id);
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
    void play(PlayerId p, GameObjectId card, std::optional<LocationId> loc = std::nullopt,
              Intent::PlaySource src = Intent::PlaySource::Hand) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = loc;
        i.play_source = src;
        engine_->testHook_executeIntent(i);
    }
    /// Resolve a triggered ability on the engine's state (no finalize pass).
    void driveTrigger(CardDefId def_id, PlayerId controller, GameObjectId source, Picker picker) {
        ChainItem ri;
        ri.id = s().chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        s().chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        CardContext ctx{s(), events, ex(), controller, source};
        ctx.registry = &card_registry;
        card->onTrigger(ctx, {});
        while (ex().hasPendingChoice()) {
            auto pending = ex().consumePendingChoice();
            ex().recordChoice(picker(pending.legal));
            card->onTrigger(ctx, {});
        }
        s().chain.resuming.reset();
    }
    int chainItemsFrom(GameObjectId source) {
        int n = 0;
        for (const auto& ci : s().chain.items) if (ci.source == source) ++n;
        return n;
    }
};

}  // namespace

// ── 988 Bilgewater Tracker ──────────────────────────────────────────────────
TEST_F(Radiance1Test, BilgewaterTracker_DealsOneToEachOfTwoChosenUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kBilgewaterTracker, 0, 0);
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 1);
    auto c = addUnit(P2, kInvalidId, 3, 1);
    runTrigger(kBilgewaterTracker, me, scripted({val(2), obj(a), obj(c)}), exec);
    EXPECT_EQ(state.getObject(a).damage_marked, 1);
    EXPECT_EQ(state.getObject(b).damage_marked, 0);
    EXPECT_EQ(state.getObject(c).damage_marked, 1);
}
TEST_F(Radiance1Test, BilgewaterTracker_UpToMeansZeroIsAllowed) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kBilgewaterTracker, 0, 0);
    auto a = addUnit(P2, kInvalidId, 3, 0);
    runTrigger(kBilgewaterTracker, me, scripted({val(0)}), exec);
    EXPECT_EQ(state.getObject(a).damage_marked, 0);
    EXPECT_EQ(state.getObject(me).damage_marked, 0);
}

// ── 989 Brawl ───────────────────────────────────────────────────────────────
TEST_F(Radiance1Test, Brawl_GivesAssaultEqualToMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u = addUnit(P1, kInvalidId, 4, 0);
    auto spell = spellObj(kBrawl);
    driveResumable(kBrawl, P1, spell, scripted({obj(u)}), exec);
    auto& o = state.getObject(u);
    EXPECT_TRUE(o.keywords.has(Keyword::Assault));
    EXPECT_EQ(o.assault_value, 4);
    EXPECT_EQ(o.temp_assault_value, 4) << "this turn only";
    o.combat_designation = CombatDesignation::Attacker;
    o.recomputeMight();
    EXPECT_EQ(o.current_might, 8);
    EXPECT_TRUE(card_db.get(kBrawl).keywords.has(Keyword::Action));
}

// ── 990 Lingering Animus ────────────────────────────────────────────────────
TEST_F(Radiance1Test, LingeringAnimus_EntersReadyOnlyIfAFriendlyUnitOrGearDied) {
    Card* c = card_registry.get(kLingeringAnimus);
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1));
    state.player(P2).unit_died_this_turn = true;
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1)) << "an enemy death doesn't count";
    state.player(P1).gear_died_this_turn = true;
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1));
    state.player(P1).gear_died_this_turn = false;
    state.player(P1).unit_died_this_turn = true;
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1));
}
TEST_F(Radiance1EngineTest, LingeringAnimus_KilledGearCountsAndPlayedReady) {
    makeEngine();
    auto g = gearOn(P1, kInvalidId);
    ex().killObject(g);
    EXPECT_TRUE(s().player(P1).gear_died_this_turn);
    s().player(P1).rune_pool.energy = 2;
    auto animus = inHand(P1, kLingeringAnimus);
    play(P1, animus, LocationId{BaseLocation{P1}});
    ASSERT_TRUE(s().getObject(animus).location.has_value());
    EXPECT_FALSE(s().getObject(animus).is_exhausted);
}

// ── 991 Scorched Earth ──────────────────────────────────────────────────────
TEST_F(Radiance1Test, ScorchedEarth_SplitsFiveAmongUnitsAtOneBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P2, kInvalidId, 9, 0);
    auto b = addUnit(P2, kInvalidId, 9, 0);
    auto far = addUnit(P2, kInvalidId, 9, 1);
    auto spell = spellObj(kScorchedEarth);
    std::vector<GameObjectId> offered;
    auto inner = scripted({val(0), obj(a), obj(a), obj(a), obj(b), obj(b)});
    driveResumable(kScorchedEarth, P1, spell, [&](const std::vector<Intent>& legal) {
        for (auto& i : legal) if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
        return inner(legal);
    }, exec);
    EXPECT_EQ(state.getObject(a).damage_marked, 3);
    EXPECT_EQ(state.getObject(b).damage_marked, 2);
    EXPECT_EQ(state.getObject(far).damage_marked, 0);
    EXPECT_EQ(std::count(offered.begin(), offered.end(), far), 0) << "a single battlefield";
    EXPECT_EQ(card_db.get(kScorchedEarth).domains, std::vector<Domain>{Domain::Fury});
}
TEST_F(Radiance1Test, ScorchedEarth_NeedsUnitsAtABattlefield) {
    Card* c = card_registry.get(kScorchedEarth);
    addUnit(P2, kInvalidId, 3);   // at base
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
    addUnit(P2, kInvalidId, 3, 1);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

// ── 992 Rally for War ───────────────────────────────────────────────────────
TEST_F(Radiance1Test, RallyForWar_ThreeRecruitsAtAnOccupiedEnemyBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kRallyForWar);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
    state.battlefields[1].controller = P2;
    addUnit(P2, kInvalidId, 2, 1);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
    auto spell = spellObj(kRallyForWar);
    driveResumable(kRallyForWar, P1, spell, scripted({val(1)}), exec);
    int recruits = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.controller == P1 && o.card_def_id == 954 && o.battlefieldId() &&
            *o.battlefieldId() == 1 && o.current_might == 1)
            ++recruits;
    EXPECT_EQ(recruits, 3);
}

// ── 993 War Scout ───────────────────────────────────────────────────────────
TEST_F(Radiance1Test, WarScout_AccelerateAndPlusTwoToAUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_db.get(kWarScout).keywords.has(Keyword::Accelerate));
    auto me = addUnit(P1, kWarScout, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    runTrigger(kWarScout, me, scripted({obj(ally)}), exec);
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(me).temp_might_bonus, 0);
}

// ── 994 Antagonistic Apparatus ──────────────────────────────────────────────
TEST_F(Radiance1Test, AntagonisticApparatus_EntersReadyAndMakesAReadyMechHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kAntagonisticApparatus)->entersReadyOnPlay());
    auto me = addUnit(P1, kAntagonisticApparatus, 0, 1);
    runTrigger(kAntagonisticApparatus, me, scripted({}), exec);
    int mechs = 0;
    for (auto& [id, o] : state.objects)
        if (o.isUnit() && o.name == "Mech" && hasTag(o, "Mech") && o.current_might == 3 &&
            !o.is_exhausted && o.battlefieldId() && *o.battlefieldId() == 1)
            ++mechs;
    EXPECT_EQ(mechs, 1);
}

// ── 995 Forbidden Technique ─────────────────────────────────────────────────
TEST_F(Radiance1Test, ForbiddenTechnique_ReadiesAUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(u).is_exhausted = true;
    auto spell = spellObj(kForbiddenTechnique);
    driveResumable(kForbiddenTechnique, P1, spell, scripted({obj(u)}), exec);
    EXPECT_FALSE(state.getObject(u).is_exhausted);
}
TEST_F(Radiance1Test, ForbiddenTechnique_TriggersOnlyForSevenOrMore) {
    Card* c = card_registry.get(kForbiddenTechnique);
    auto ft = state.createObject();
    auto& o = state.getObject(ft);
    o.card_counters["__played_card_energy_cost"] = 6;
    EXPECT_FALSE(c->triggerCondition(state, ft, P1, TriggerType::WhenYouPlayACardFromTrash, kInvalidId));
    o.card_counters["__played_card_energy_cost"] = 7;
    EXPECT_TRUE(c->triggerCondition(state, ft, P1, TriggerType::WhenYouPlayACardFromTrash, kInvalidId));
}
TEST_F(Radiance1EngineTest, ForbiddenTechnique_PlayedFromTrashForOneRRThenBanished) {
    makeEngine();
    auto ft = inTrash(P1, kForbiddenTechnique);
    auto tired = unit(P1, kInvalidId, 2);
    s().getObject(tired).is_exhausted = true;
    // Pay for Drakehound (9 + [R][R]) from the pool; then [1][R][R] for the
    // Technique from runes.
    s().player(P1).rune_pool.energy = 9;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    rune(P1, Domain::Fury);
    rune(P1, Domain::Fury);
    a1_.script = {val(1), obj(tired)};   // yes, play it; ready `tired`
    auto big = inHand(P1, kDrakehound);
    play(P1, big, LocationId{BaseLocation{P1}});
    EXPECT_EQ(s().getObject(ft).zone, ZoneType::Banishment) << "then banish this";
    EXPECT_FALSE(s().getObject(tired).is_exhausted);
    const auto& tr = s().player(P1).trash;
    EXPECT_EQ(std::count(tr.begin(), tr.end(), ft), 0);
    int runes = 0;
    for (auto& [id, o] : s().objects) if (o.isRune() && o.location.has_value()) ++runes;
    EXPECT_EQ(runes, 0) << "both Fury runes recycled for [R][R]";
}

// ── 996 Incorrigible Construct ──────────────────────────────────────────────
TEST_F(Radiance1Test, IncorrigibleConstruct_ShowsOffMechsAndEntersReady) {
    Card* c = card_registry.get(kIncorrigibleConstruct);
    EXPECT_TRUE(c->hasShowOff());
    auto mech = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(mech).tags = {"Mech"};
    auto other = addUnit(P1, kInvalidId, 3, 0);
    EXPECT_TRUE(c->showOffMatches(state, P1, state.getObject(mech)));
    EXPECT_FALSE(c->showOffMatches(state, P1, state.getObject(other)));
    auto me = addUnit(P1, kIncorrigibleConstruct, 0, 0);
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1, state.getObject(me)));
    state.getObject(me).showed_off_object = mech;
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1, state.getObject(me)));
}
TEST_F(Radiance1EngineTest, IncorrigibleConstruct_PlayedWithAShowOffEntersReady) {
    makeEngine();
    auto mech = unit(P1, kInvalidId, 3);
    s().getObject(mech).tags = {"Mech"};
    s().player(P1).rune_pool.energy = 4;
    a1_.script = {obj(mech)};
    auto me = inHand(P1, kIncorrigibleConstruct);
    play(P1, me, LocationId{BaseLocation{P1}});
    ASSERT_TRUE(s().getObject(me).location.has_value());
    EXPECT_EQ(s().getObject(me).showed_off_object, mech);
    EXPECT_FALSE(s().getObject(me).is_exhausted);
}

// ── 997 Runeblade Anvil ─────────────────────────────────────────────────────
TEST_F(Radiance1EngineTest, RunebladeAnvil_BanishedTopCardIsPlayableUntilEndingPhase) {
    makeEngine();
    auto anvil = gearOn(P1, kRunebladeAnvil);
    auto top = onDeck(P1, kLingeringAnimus);
    CardContext ctx{s(), events, ex(), P1, anvil};
    card_registry.get(kRunebladeAnvil)->onTrigger(ctx, {});
    EXPECT_EQ(s().getObject(top).zone, ZoneType::Banishment);
    ASSERT_EQ(s().player(P1).banished_playable.size(), 1u);

    s().player(P1).rune_pool.energy = 2;
    const Intent* offered = nullptr;
    auto legal = engine_->generateLegalActions();
    for (const auto& a : legal)
        if (a.type == IntentType::PlayCard && a.card == top) { offered = &a; break; }
    ASSERT_NE(offered, nullptr) << "the banished card can be played";
    EXPECT_EQ(offered->play_source, Intent::PlaySource::Banishment);
    engine_->testHook_executeIntent(*offered);
    EXPECT_TRUE(s().getObject(top).location.has_value());
    const auto& ban = s().player(P1).banishment;
    EXPECT_EQ(std::count(ban.begin(), ban.end(), top), 0);
    EXPECT_TRUE(s().player(P1).banished_playable.empty());
}
TEST_F(Radiance1EngineTest, RunebladeAnvil_PermissionEndsAtYourNextEndingPhase) {
    makeEngine();
    auto anvil = gearOn(P1, kRunebladeAnvil);
    auto top = onDeck(P1, kLingeringAnimus);
    CardContext ctx{s(), events, ex(), P1, anvil};
    card_registry.get(kRunebladeAnvil)->onTrigger(ctx, {});
    engine_->testHook_endingStep();
    EXPECT_TRUE(s().player(P1).banished_playable.empty());
    s().player(P1).rune_pool.energy = 2;
    s().turn.phase = TurnPhase::MainPhase;
    for (const auto& a : engine_->generateLegalActions())
        EXPECT_FALSE(a.card == top && a.type == IntentType::PlayCard);
}
TEST_F(Radiance1EngineTest, RunebladeAnvil_KillingItTriggersLeavesTheBoard) {
    makeEngine();
    auto anvil = gearOn(P1, kRunebladeAnvil);
    Card* c = card_registry.get(kRunebladeAnvil);
    EXPECT_EQ(c->getActivationCost().energy, 2);
    EXPECT_TRUE(c->getActivationCost().exhaust);
    CardContext ctx{s(), events, ex(), P1, anvil};
    c->onActivate(ctx, {});
    EXPECT_FALSE(s().getObject(anvil).location.has_value());
    bool leave = false;
    for (const auto& ci : s().chain.items)
        if (ci.source == anvil && ci.fired_trigger == TriggerType::WhenILeaveTheBoard) leave = true;
    EXPECT_TRUE(leave);
}

// ── 998 Lost to the Sands ───────────────────────────────────────────────────
TEST_F(Radiance1Test, LostToTheSands_DealFourToAUnitAtABattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto at_base = addUnit(P2, kInvalidId, 5);
    auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    auto spell = spellObj(kLostToTheSands);
    std::vector<GameObjectId> offered;
    auto inner = scripted({val(0), obj(at_bf)});
    driveResumable(kLostToTheSands, P1, spell, [&](const std::vector<Intent>& legal) {
        for (auto& i : legal) if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
        return inner(legal);
    }, exec);
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 4);
    EXPECT_EQ(std::count(offered.begin(), offered.end(), at_base), 0);
}
TEST_F(Radiance1Test, LostToTheSands_KillAGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto g = gear(P2, kInvalidId);
    auto spell = spellObj(kLostToTheSands);
    driveResumable(kLostToTheSands, P1, spell, scripted({val(1), obj(g)}), exec);
    EXPECT_FALSE(state.getObject(g).location.has_value());
}

// ── 999 Shimmering Swimmer ──────────────────────────────────────────────────
TEST_F(Radiance1EngineTest, ShimmeringSwimmer_PaidKillDealsTwo) {
    makeEngine();
    auto fodder = unit(P1, kInvalidId, 1);
    auto enemy = unit(P2, kInvalidId, 5, 0);
    s().player(P1).rune_pool.energy = 3;
    a1_.script = {val(1), obj(fodder), obj(enemy)};
    auto me = inHand(P1, kShimmeringSwimmer);
    play(P1, me, LocationId{BaseLocation{P1}});
    EXPECT_FALSE(s().getObject(fodder).location.has_value()) << "killed as the additional cost";
    EXPECT_EQ(s().getObject(enemy).damage_marked, 2);
}
TEST_F(Radiance1EngineTest, ShimmeringSwimmer_UnpaidDealsNothing) {
    makeEngine();
    auto fodder = unit(P1, kInvalidId, 1);
    auto enemy = unit(P2, kInvalidId, 5, 0);
    s().player(P1).rune_pool.energy = 3;
    a1_.script = {val(0)};
    auto me = inHand(P1, kShimmeringSwimmer);
    play(P1, me, LocationId{BaseLocation{P1}});
    EXPECT_TRUE(s().getObject(fodder).location.has_value());
    EXPECT_EQ(s().getObject(enemy).damage_marked, 0);
}

// ── 1000 Akali, Brash ───────────────────────────────────────────────────────
TEST_F(Radiance1Test, AkaliBrash_FirstMoveAddsSecondBanishesThirdNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.turn.turn_number = 4;
    auto akali = addUnit(P1, kAkaliBrash, 0, 0);
    auto top = addToDeck(P1, kLingeringAnimus);
    addToDeck(P1, kInvalidId);
    std::swap(state.player(P1).main_deck.front(), state.player(P1).main_deck.back());
    runTrigger(kAkaliBrash, akali, scripted({}), exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
    EXPECT_TRUE(state.player(P1).banished_playable.empty());
    runTrigger(kAkaliBrash, akali, scripted({}), exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.getObject(top).zone, ZoneType::Banishment);
    ASSERT_EQ(state.player(P1).banished_playable.size(), 1u);
    EXPECT_EQ(state.player(P1).banished_playable[0], top);
    runTrigger(kAkaliBrash, akali, scripted({}), exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).banished_playable.size(), 1u);
    EXPECT_EQ(deckSize(P1), 1);
    // A new turn starts the count again.
    state.turn.turn_number = 6;
    runTrigger(kAkaliBrash, akali, scripted({}), exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 2);
}

// ── 1001 Coalesced Energy ───────────────────────────────────────────────────
TEST_F(Radiance1Test, CoalescedEnergy_RecyclesAllRunesThenChannelsFourExhausted) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kCoalescedEnergy, 0, 0);
    std::vector<GameObjectId> old;
    for (int i = 0; i < 3; ++i) old.push_back(addRune(P1, Domain::Mind, i == 0));
    for (int i = 0; i < 5; ++i) {
        auto r = state.createObject();
        auto& o = state.getObject(r);
        o.owner = o.controller = P1;
        o.card_type = CardType::Rune;
        o.name = "Deck Rune";
        o.domains = {Domain::Fury};
        o.zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(r);
    }
    runTrigger(kCoalescedEnergy, me, scripted({}), exec, TriggerType::AtEndOfTurn);
    for (auto r : old) EXPECT_FALSE(state.getObject(r).location.has_value());
    EXPECT_EQ(readyRuneCount(P1), 0);
    EXPECT_EQ(exhaustedRuneCount(P1), 4);
    EXPECT_EQ(state.player(P1).rune_deck.size(), 4u);   // 5 - 4 + 3
}

// ── 1002 Detonation Amplifier ───────────────────────────────────────────────
TEST_F(Radiance1Test, DetonationAmplifier_KillAnotherFriendlyToDrawOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kDetonationAmplifier)->hasDeploy());
    auto amp = gear(P1, kDetonationAmplifier, 0);
    auto fodder = gear(P1, kInvalidId, 0);
    addToDeck(P1, kInvalidId);
    runTrigger(kDetonationAmplifier, amp, scripted({val(1), obj(fodder)}), exec);
    EXPECT_FALSE(state.getObject(fodder).location.has_value());
    EXPECT_EQ(handSize(P1), 1);
}
TEST_F(Radiance1Test, DetonationAmplifier_DecliningDoesNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto amp = gear(P1, kDetonationAmplifier, 0);
    auto fodder = addUnit(P1, kInvalidId, 2, 0);
    addToDeck(P1, kInvalidId);
    runTrigger(kDetonationAmplifier, amp, scripted({val(0)}), exec);
    EXPECT_TRUE(state.getObject(fodder).location.has_value());
    EXPECT_EQ(handSize(P1), 0);
}
TEST_F(Radiance1EngineTest, DetonationAmplifier_FriendlyDeathknellsHereFireTwice) {
    makeEngine();
    gearOn(P1, kDetonationAmplifier, 0);
    auto here = unit(P1, kWatchfulSentry, 1, 0);
    auto there = unit(P1, kWatchfulSentry, 1, 1);
    auto enemy_here = unit(P2, kWatchfulSentry, 1, 0);
    engine_->testHook_cleanup();
    EXPECT_EQ(s().battlefields[0].deathknell_extra[playerIndex(P1)], 1);
    s().chain.items.clear();
    ex().killObject(here);
    EXPECT_EQ(chainItemsFrom(here), 2);
    ex().killObject(there);
    EXPECT_EQ(chainItemsFrom(there), 1) << "here only";
    ex().killObject(enemy_here);
    EXPECT_EQ(chainItemsFrom(enemy_here), 1) << "friendly Deathknells only";
}
TEST_F(Radiance1EngineTest, DetonationAmplifier_BombDeathknellHereAlsoDoubles) {
    makeEngine();
    gearOn(P1, kDetonationAmplifier, 0);
    CardContext ctx{s(), events, ex(), P1, kInvalidId};
    auto bomb = createBombToken(ctx, P1, 0);
    engine_->testHook_cleanup();
    s().chain.items.clear();
    ex().killObject(bomb);
    EXPECT_EQ(chainItemsFrom(bomb), 2) << "a killed gear's Deathknell fires (+1 here)";
}

// ── 1003 Drakehound Packmaster ──────────────────────────────────────────────
TEST_F(Radiance1EngineTest, Drakehound_PlaysTheOpponentsTopCardForFree) {
    makeEngine();
    auto me = unit(P1, kDrakehound, 0);
    auto their_top = onDeck(P2, kLingeringAnimus);
    s().battlefields[1].controller = P1;
    // Pick Battlefield #1 (valChoice 2) as the place to play it.
    driveTrigger(kDrakehound, P1, me, scripted({val(2)}));
    auto& o = s().getObject(their_top);
    EXPECT_EQ(o.controller, P1);
    EXPECT_EQ(o.owner, P2);
    ASSERT_TRUE(o.battlefieldId().has_value());
    EXPECT_EQ(*o.battlefieldId(), 1u);
    EXPECT_TRUE(s().player(P2).main_deck.empty());
    EXPECT_TRUE(s().player(P2).banishment.empty());
}

// ── 1004 Firebrand Raider ───────────────────────────────────────────────────
TEST_F(Radiance1EngineTest, FirebrandRaider_UnitsStayExhaustedInAwakenButEnterReady) {
    makeEngine();
    auto raider = unit(P1, kFirebrandRaider, 0);
    EXPECT_TRUE(card_registry.get(kFirebrandRaider)->entersReadyOnPlay());
    auto tired = unit(P1, kInvalidId, 2);
    s().getObject(tired).is_exhausted = true;
    auto r = rune(P1, Domain::Fury, true);
    engine_->testHook_cleanup();
    EXPECT_TRUE(s().turn.units_dont_awaken);
    EXPECT_TRUE(s().turn.all_units_enter_ready);
    engine_->testHook_awakenPhase();
    EXPECT_TRUE(s().getObject(tired).is_exhausted) << "units don't ready";
    EXPECT_FALSE(s().getObject(r).is_exhausted) << "runes still do";
    // Other units (any player's) enter ready.
    s().turn.phase = TurnPhase::MainPhase;
    s().player(P1).rune_pool.energy = 2;
    auto animus = inHand(P1, kLingeringAnimus);
    play(P1, animus, LocationId{BaseLocation{P1}});
    EXPECT_FALSE(s().getObject(animus).is_exhausted);
    auto tok = ex().createToken(P2, CardType::Unit, "Recruit", 1, {}, {}, BaseLocation{P2}, false);
    EXPECT_FALSE(s().getObject(tok).is_exhausted);
    // Gone: the effects end.
    ex().killObject(raider);
    engine_->testHook_cleanup();
    EXPECT_FALSE(s().turn.units_dont_awaken);
    EXPECT_FALSE(s().turn.all_units_enter_ready);
}
TEST_F(Radiance1Test, FirebrandRaider_ReadiesAllEnemyUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto me = addUnit(P1, kFirebrandRaider, 0, 0);
    auto e1 = addUnit(P2, kInvalidId, 2, 0);
    auto e2 = addUnit(P2, kInvalidId, 2);
    auto mine = addUnit(P1, kInvalidId, 2);
    for (auto u : {e1, e2, mine}) state.getObject(u).is_exhausted = true;
    runTrigger(kFirebrandRaider, me, scripted({}), exec);
    EXPECT_FALSE(state.getObject(e1).is_exhausted);
    EXPECT_FALSE(state.getObject(e2).is_exhausted);
    EXPECT_TRUE(state.getObject(mine).is_exhausted);
}

// ── 1005 Mordekaiser, Twice Slain ───────────────────────────────────────────
TEST_F(Radiance1EngineTest, Mordekaiser_KillsAFriendInsteadOfDyingOncePerTurn) {
    makeEngine();
    const auto& d = card_db.get(kMordekaiser);
    EXPECT_TRUE(d.keywords.has(Keyword::Accelerate));
    EXPECT_EQ(d.assault_value, 3);
    auto mord = unit(P1, kMordekaiser, 0, 1);
    auto a = unit(P1, kInvalidId, 1);
    auto b = unit(P1, kInvalidId, 1);
    s().getObject(mord).damage_marked = 3;
    a1_.script = {val(1), obj(a)};
    ex().killObject(mord);
    auto& m = s().getObject(mord);
    EXPECT_TRUE(m.isAtBase());
    EXPECT_TRUE(m.is_exhausted);
    EXPECT_EQ(m.damage_marked, 0);
    EXPECT_FALSE(s().getObject(a).location.has_value());
    // Second time this turn: it dies.
    a1_.script = {val(1), obj(b)};
    a1_.at = 0;
    ex().killObject(mord);
    EXPECT_FALSE(s().getObject(mord).location.has_value());
    EXPECT_TRUE(s().getObject(b).location.has_value());
}
TEST_F(Radiance1EngineTest, Mordekaiser_DecliningDies) {
    makeEngine();
    auto mord = unit(P1, kMordekaiser, 0, 1);
    auto a = unit(P1, kInvalidId, 1);
    a1_.script = {val(0)};
    ex().killObject(mord);
    EXPECT_FALSE(s().getObject(mord).location.has_value());
    EXPECT_TRUE(s().getObject(a).location.has_value());
}

// ── 1006 Ekko, Revolutionary ────────────────────────────────────────────────
TEST_F(Radiance1EngineTest, Ekko_BanishesAndReplaysToBaseReady) {
    makeEngine();
    Card* c = card_registry.get(kEkko);
    EXPECT_TRUE(c->entersReadyOnPlay());
    EXPECT_TRUE(c->isActionAbility());
    const auto cost = c->getActivationCost();
    EXPECT_TRUE(cost.discard);
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 1);
    EXPECT_FALSE(cost.exhaust);
    auto ekko = unit(P1, kEkko, 0, 1);
    auto& e = s().getObject(ekko);
    e.damage_marked = 2;
    e.is_exhausted = true;
    CardContext ctx{s(), events, ex(), P1, ekko};
    c->onActivate(ctx, {});
    auto& after = s().getObject(ekko);
    EXPECT_TRUE(after.isAtBase());
    EXPECT_FALSE(after.is_exhausted);
    EXPECT_EQ(after.damage_marked, 0);
    EXPECT_TRUE(s().player(P1).banishment.empty());
    EXPECT_EQ(s().player(P1).cards_played_this_turn, 1) << "played again";
}
