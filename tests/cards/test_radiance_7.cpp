/// @file test_radiance_7.cpp
/// Radiance batch 7 (ids 1101-1111, RAD-125 .. RAD-135), plus the engine
/// support they use: Show Off on printed data (Card::showOffMatchesOnDefs),
/// "When a friendly gear dies" (TriggerType::WhenAFriendlyGearDies) and unit
/// token replacements (Card::unitTokenReplacement in createToken).

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/trigger_manager.h"

#include <algorithm>
#include <memory>
#include <optional>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kDuneDevourer = 1101, kMournfulSpirit = 1102, kRousingDisplay = 1103,
                    kSteadfastOutrider = 1104, kTrifarianCaptain = 1105,
                    kBattlefieldArmorer = 1106, kIlluminators = 1107, kHeimerdinger = 1108,
                    kJarvan = 1109, kRaucousAnthem = 1110, kRememberTheFallen = 1111;

struct Ans {
    std::optional<int> value;
    GameObjectId obj = kInvalidId;
};
Ans val(int v) { return Ans{v, kInvalidId}; }
Ans obj(GameObjectId o) { return Ans{std::nullopt, o}; }

/// Answers MakeChoice prompts from a script (in order), passes priority
/// otherwise; once the script runs out it answers 0 / the first option.
class ScriptAgent7 : public AgentInterface {
public:
    std::vector<Ans> script;
    size_t at = 0;
    std::vector<std::vector<Intent>> seen;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        bool any_choice = false;
        for (const auto& i : legal) if (i.type == IntentType::MakeChoice) any_choice = true;
        if (!any_choice) {
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
        seen.push_back(legal);
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
struct Scripted {
    std::shared_ptr<ScriptAgent7> agent = std::make_shared<ScriptAgent7>();
    Picker picker() {
        auto a = agent;
        return [a](const std::vector<Intent>& legal) {
            GameState dummy;
            return a->selectAction(dummy, legal);
        };
    }
};
Scripted scripted(std::vector<Ans> script) {
    Scripted s;
    s.agent->script = std::move(script);
    return s;
}
bool offeredObj(const std::vector<std::vector<Intent>>& prompts, GameObjectId id) {
    for (const auto& legal : prompts)
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return true;
    return false;
}

class Radiance7 : public CardTestFixture {
protected:
    EffectExecutor exec{state, events, card_db, &card_registry};

    GameObjectId gear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = owner;
        o.card_def_id = def_id;
        o.card_type = CardType::Gear;
        o.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestGear";
        if (at_bf >= 0) {
            o.zone = ZoneType::BattlefieldZone;
            o.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            o.zone = ZoneType::Base;
            o.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId toTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::find(h.begin(), h.end(), id));
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
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
    void runTrigger(CardDefId def_id, GameObjectId source, Picker picker,
                    TriggerType firing = TriggerType::None, GameObjectId subject = kInvalidId) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = P1;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        ri.triggering_subject = subject;
        const bool finalize = card->choicesAtFinalization() & Card::kFinalizeTrigger;
        ri.finalizing = finalize;
        state.chain.resuming = ri;
        CardContext ctx{state, events, exec, P1, source};
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
    int chainItemsFrom(GameObjectId source, TriggerType t) {
        int n = 0;
        for (const auto& ci : state.chain.items)
            if (ci.source == source && ci.fired_trigger == t) ++n;
        return n;
    }
};

class Radiance7Engine : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    ScriptAgent7 a1_, a2_;
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
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        fillObj(s().getObject(id), p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    void play(PlayerId p, GameObjectId card, std::optional<LocationId> loc = std::nullopt) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = loc;
        i.play_source = Intent::PlaySource::Hand;
        engine_->testHook_executeIntent(i);
    }
    int recruitsInBase(PlayerId p) {
        int n = 0;
        for (const auto& [id, o] : s().objects)
            if (o.isToken() && o.isUnit() && o.name == "Recruit" && o.location.has_value() &&
                std::holds_alternative<BaseLocation>(*o.location) && o.controller == p) ++n;
        return n;
    }
};

// ── def() sanity (printed data) ─────────────────────────────────────────────
TEST_F(Radiance7, DefsMatchThePrintedCards) {
    const auto& dd = card_db.get(kDuneDevourer);
    EXPECT_EQ(dd.energy_cost, 7);
    EXPECT_EQ(dd.power_cost, 1);
    EXPECT_EQ(dd.might, 7);
    EXPECT_TRUE(dd.keywords.has(Keyword::Deflect));
    EXPECT_EQ(card_db.get(kRaucousAnthem).domains, std::vector<Domain>{Domain::Order})
        << "gallery listed no domain; the printed card is Order";
    const auto& jv = card_db.get(kJarvan);
    EXPECT_EQ(jv.super_type, SuperType::Champion);
    EXPECT_EQ(jv.assault_value, 1);
    EXPECT_TRUE(jv.keywords.has(Keyword::Tank));
    EXPECT_EQ(card_db.get(kRememberTheFallen).power_cost, 2);
    for (CardDefId id = 1101; id <= 1111; ++id) EXPECT_EQ(card_db.get(id).set_code, "RAD");
}

// ── 1101 Dune Devourer ──────────────────────────────────────────────────────
TEST_F(Radiance7, DuneDevourer_OptionalKillCostsTwoLess) {
    auto c = card_registry.get(kDuneDevourer)->optionalAdditionalCost();
    EXPECT_TRUE(c.valid);
    EXPECT_FALSE(c.mandatory);
    EXPECT_EQ(c.object_cost, Card::OptionalAdditionalCost::ObjectCost::KillFriendlyUnit);
    EXPECT_EQ(c.max_objects, 1);
    EXPECT_EQ(c.reduce_energy_each, 2);
}
TEST_F(Radiance7Engine, DuneDevourer_PlayedKillingAFriendlyUnitForTwoLess) {
    makeEngine();
    auto victim = unit(P1, kInvalidId, 2);
    s().player(P1).rune_pool.energy = 5;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    a1_.script = {obj(victim)};
    auto dd = inHand(P1, kDuneDevourer);
    play(P1, dd, LocationId{BaseLocation{P1}});
    EXPECT_TRUE(s().getObject(dd).location.has_value()) << "affordable only with the kill";
    EXPECT_FALSE(s().getObject(victim).location.has_value());
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
}

// ── 1102 Mournful Spirit ────────────────────────────────────────────────────
TEST_F(Radiance7, MournfulSpirit_DeathknellKillsAUnitOrGearWhereItDied) {
    auto spirit = addUnit(P1, kMournfulSpirit, 0, 0);
    auto enemy_gear = gear(P2, kInvalidId, 0);
    auto enemy_elsewhere = addUnit(P2, kInvalidId, 3, 1);
    exec.killObject(spirit);
    ASSERT_EQ(state.getObject(spirit).zone, ZoneType::Trash);
    auto sc = scripted({val(1), obj(enemy_gear)});
    runTrigger(kMournfulSpirit, spirit, sc.picker(), TriggerType::WhenIDie);
    EXPECT_FALSE(state.getObject(enemy_gear).location.has_value());
    EXPECT_TRUE(state.getObject(enemy_elsewhere).location.has_value());
    EXPECT_FALSE(offeredObj(sc.agent->seen, enemy_elsewhere)) << "only things here";
}
TEST_F(Radiance7, MournfulSpirit_MayDecline) {
    auto spirit = addUnit(P1, kMournfulSpirit, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    exec.killObject(spirit);
    runTrigger(kMournfulSpirit, spirit, scripted({val(0)}).picker(), TriggerType::WhenIDie);
    EXPECT_TRUE(state.getObject(enemy).location.has_value());
}

// ── 1103 Rousing Display ────────────────────────────────────────────────────
TEST_F(Radiance7, RousingDisplay_ShowOffNeedsEnergyCostSevenOrMore) {
    const Card* c = card_registry.get(kRousingDisplay);
    ASSERT_TRUE(c->hasShowOff());
    auto big = addToHand(P1, kDuneDevourer);          // [7]
    auto small = addToHand(P1, kTrifarianCaptain);    // [2]
    const auto& b = state.getObject(big);
    const auto& sm = state.getObject(small);
    EXPECT_TRUE(c->showOffMatches(state, P1, b) && c->showOffMatchesOnDefs(state, card_db, P1, b));
    EXPECT_FALSE(c->showOffMatches(state, P1, sm) && c->showOffMatchesOnDefs(state, card_db, P1, sm));
}
TEST_F(Radiance7, RousingDisplay_TwoRecruitsOrFourIfShownOff) {
    auto spell = spellObj(kRousingDisplay);
    invokeOnResolve(spell, kRousingDisplay, P1, {}, exec);
    auto count = [&] {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isToken() && o.name == "Recruit" && o.card_def_id == 954 &&
                o.location == LocationId{BaseLocation{P1}}) ++n;
        return n;
    };
    EXPECT_EQ(count(), 2);
    auto spell2 = spellObj(kRousingDisplay);
    state.getObject(spell2).showed_off_object = addToHand(P1, kDuneDevourer);
    invokeOnResolve(spell2, kRousingDisplay, P1, {}, exec);
    EXPECT_EQ(count(), 6);
}
TEST_F(Radiance7Engine, RousingDisplay_PlayedShowingOffFromHand) {
    makeEngine();
    auto big = inHand(P1, kDuneDevourer);
    inHand(P1, kTrifarianCaptain);
    s().player(P1).rune_pool.energy = 5;
    a1_.script = {obj(big)};
    auto spell = inHand(P1, kRousingDisplay);
    play(P1, spell);
    EXPECT_EQ(s().getObject(spell).showed_off_object, big);
    EXPECT_EQ(recruitsInBase(P1), 4);
}

// ── 1104 Steadfast Outrider ─────────────────────────────────────────────────
TEST_F(Radiance7, SteadfastOutrider_PlusTwoWithTwoOtherFriendlyUnitsOrGearThere) {
    auto me = addUnit(P1, kSteadfastOutrider, 0, 0);
    const Card* c = card_registry.get(kSteadfastOutrider);
    auto bonus = [&] {
        int m = 0;
        for (const auto& ae : state.getObject(me).aura_effects) m += ae.might_bonus;
        return m;
    };
    addUnit(P1, kInvalidId, 1, 0);
    addUnit(P2, kInvalidId, 1, 0);   // an enemy doesn't count
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(bonus(), 0);
    state.getObject(me).aura_effects.clear();
    gear(P1, kInvalidId, 0);
    c->applyPassiveAura(state, P1);
    c->applyPassiveAura(state, P1);   // once per instance per pass
    EXPECT_EQ(bonus(), 2);
    EXPECT_TRUE(card_db.get(kSteadfastOutrider).keywords.has(Keyword::Tank));
}

// ── 1105 Trifarian Captain ──────────────────────────────────────────────────
TEST_F(Radiance7, TrifarianCaptain_RevealsACheapUnitOrGearFromTopFourAndRecyclesTheRest) {
    auto bottom = addToDeck(P1, kTrifarianCaptain);   // 5th from top: not looked at
    auto big = addToDeck(P1, kDuneDevourer);          // [7]: not eligible
    auto cheap = addToDeck(P1, kTrifarianCaptain);    // [2]
    auto spell = addToDeck(P1, kRaucousAnthem);
    auto top = addToDeck(P1, kDuneDevourer);
    auto me = addUnit(P1, kTrifarianCaptain);
    // Looked-at order (top first): top, spell, cheap, big -> cheap is #3.
    driveResumableTrigger(kTrifarianCaptain, P1, me, scripted({val(3)}).picker(), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(inHand(P1, cheap));
    EXPECT_EQ(handSize(P1), 1);
    auto& deck = state.player(P1).main_deck;
    ASSERT_EQ(deck.size(), 4u);
    EXPECT_EQ(deck.back(), bottom) << "the other three went to the bottom";
    for (auto id : {big, spell, top}) EXPECT_TRUE(inDeck(P1, id));
}
TEST_F(Radiance7, TrifarianCaptain_NothingEligibleRecyclesAll) {
    addToDeck(P1, kDuneDevourer);
    addToDeck(P1, kRaucousAnthem);
    auto me = addUnit(P1, kTrifarianCaptain);
    driveResumableTrigger(kTrifarianCaptain, P1, me, nullptr, exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(deckSize(P1), 2);
}

// ── 1106 Battlefield Armorer ────────────────────────────────────────────────
TEST_F(Radiance7, BattlefieldArmorer_TriggersForAFriendlyGearDyingAtABattlefield) {
    auto armorer = addUnit(P1, kBattlefieldArmorer);
    auto g_bf = gear(P1, kRememberTheFallen /* any non-token card */, 0);
    auto g_base = gear(P1, kRaucousAnthem);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    exec.killObject(g_base);
    EXPECT_EQ(chainItemsFrom(armorer, TriggerType::WhenAFriendlyGearDies), 0) << "not at a battlefield";
    exec.killObject(g_bf);
    EXPECT_EQ(chainItemsFrom(armorer, TriggerType::WhenAFriendlyGearDies), 1);
}
TEST_F(Radiance7, BattlefieldArmorer_PayOneToReturnTheGearToHand) {
    auto armorer = addUnit(P1, kBattlefieldArmorer);
    auto g = gear(P1, kRememberTheFallen, 0);
    exec.killObject(g);
    ASSERT_TRUE(inTrash(P1, g));
    state.player(P1).rune_pool.energy = 1;
    runTrigger(kBattlefieldArmorer, armorer, scripted({val(1)}).picker(),
               TriggerType::WhenAFriendlyGearDies, g);
    EXPECT_TRUE(inHand(P1, g));
    EXPECT_FALSE(inTrash(P1, g));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}
TEST_F(Radiance7, BattlefieldArmorer_CantPayLeavesItInTheTrash) {
    auto armorer = addUnit(P1, kBattlefieldArmorer);
    auto g = gear(P1, kRememberTheFallen, 0);
    exec.killObject(g);
    runTrigger(kBattlefieldArmorer, armorer, scripted({val(1)}).picker(),
               TriggerType::WhenAFriendlyGearDies, g);
    EXPECT_TRUE(inTrash(P1, g));
}

// ── 1107 Demacian Illuminators ──────────────────────────────────────────────
TEST_F(Radiance7, DemacianIlluminators_OneTriggerForUnitsDyingTogether) {
    auto me = addUnit(P1, kIlluminators);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    exec.killObject(a);
    exec.killObject(b);
    EXPECT_EQ(chainItemsFrom(me, TriggerType::WhenAFriendlyUnitDies), 1);
}
TEST_F(Radiance7, DemacianIlluminators_BuffsTheChosenFriendlyUnit) {
    auto me = addUnit(P1, kIlluminators);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto sc = scripted({obj(ally)});
    runTrigger(kIlluminators, me, sc.picker(), TriggerType::WhenAFriendlyUnitDies);
    EXPECT_EQ(state.getObject(ally).buff_count, 1);
    EXPECT_FALSE(offeredObj(sc.agent->seen, enemy));
}

// ── 1108 Heimerdinger, Academy Instructor ───────────────────────────────────
TEST_F(Radiance7, Heimerdinger_AtABattlefieldReplacesUnitTokensWithMechs) {
    auto heimer = addUnit(P1, kHeimerdinger, 0, 0);
    auto tok = exec.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                LocationId{BaseLocation{P1}});
    const auto& t = state.getObject(tok);
    EXPECT_EQ(t.name, "Mech");
    EXPECT_EQ(t.base_might, 3);
    EXPECT_EQ(t.tags, std::vector<std::string>{"Mech"});
    EXPECT_EQ(t.location, LocationId{BaseLocation{P1}}) << "same location";
    // Opponent's tokens are not affected; gear tokens are not unit tokens.
    auto enemy_tok = exec.createToken(P2, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                      LocationId{BaseLocation{P2}});
    EXPECT_EQ(state.getObject(enemy_tok).name, "Recruit");
    // In base: no replacement.
    state.getObject(heimer).location = BaseLocation{P1};
    state.getObject(heimer).zone = ZoneType::Base;
    auto tok2 = exec.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                 LocationId{BaseLocation{P1}});
    EXPECT_EQ(state.getObject(tok2).name, "Recruit");
}
TEST_F(Radiance7, Heimerdinger_ReplacementIsOptional) {
    addUnit(P1, kHeimerdinger, 0, 0);
    exec.setAgentQuery([](PlayerId, const std::vector<Intent>& o) { return o.front(); });   // "no"
    auto tok = exec.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                                LocationId{BaseLocation{P1}});
    EXPECT_EQ(state.getObject(tok).name, "Recruit");
}

// ── 1109 Jarvan IV, Royal Heir ──────────────────────────────────────────────
TEST_F(Radiance7, JarvanIV_WhenIAttackMovesAUnitIControlToMyBattlefield) {
    auto jarvan = addUnit(P1, kJarvan, 0, 0);
    auto friend_base = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 1);
    auto sc = scripted({val(1), obj(friend_base)});
    runTrigger(kJarvan, jarvan, sc.picker(), TriggerType::WhenIAttack);
    ASSERT_TRUE(state.getObject(friend_base).location.has_value());
    EXPECT_EQ(*state.getObject(friend_base).location, LocationId{BattlefieldLocation{0}});
    EXPECT_FALSE(offeredObj(sc.agent->seen, enemy));
}
TEST_F(Radiance7, JarvanIV_MayDecline) {
    auto jarvan = addUnit(P1, kJarvan, 0, 0);
    auto friend_base = addUnit(P1, kInvalidId, 2);
    runTrigger(kJarvan, jarvan, scripted({val(0)}).picker(), TriggerType::WhenIAttack);
    EXPECT_EQ(*state.getObject(friend_base).location, LocationId{BaseLocation{P1}});
}

// ── 1110 Raucous Anthem ─────────────────────────────────────────────────────
TEST_F(Radiance7, RaucousAnthem_ReadiesYourUnitsAndGivesThemPlusThree) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 1);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    for (auto id : {a, b, enemy}) state.getObject(id).is_exhausted = true;
    invokeOnResolve(spellObj(kRaucousAnthem), kRaucousAnthem, P1, {}, exec);
    EXPECT_FALSE(state.getObject(a).is_exhausted);
    EXPECT_FALSE(state.getObject(b).is_exhausted);
    EXPECT_EQ(state.getObject(a).current_might, 5);
    EXPECT_EQ(state.getObject(b).current_might, 4);
    EXPECT_TRUE(state.getObject(enemy).is_exhausted);
    EXPECT_EQ(state.getObject(enemy).current_might, 2);
}

// ── 1111 Remember the Fallen ────────────────────────────────────────────────
TEST_F(Radiance7, RememberTheFallen_PlaysUpToThreeCheapUnitsOrGearFromTrash) {
    auto u1 = toTrash(P1, kTrifarianCaptain);    // [2]
    auto u2 = toTrash(P1, kTrifarianCaptain);
    auto big = toTrash(P1, kDuneDevourer);       // [7]: not eligible
    auto spell = toTrash(P1, kRaucousAnthem);    // a spell: not eligible
    auto sc = scripted({obj(u1), obj(u2), val(kPickSeveralDone)});
    driveResumable(kRememberTheFallen, P1, spellObj(kRememberTheFallen), sc.picker(), exec);
    EXPECT_TRUE(state.getObject(u1).location.has_value());
    EXPECT_TRUE(state.getObject(u2).location.has_value());
    EXPECT_TRUE(inTrash(P1, big));
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_FALSE(offeredObj(sc.agent->seen, big));
    EXPECT_FALSE(offeredObj(sc.agent->seen, spell));
}
TEST_F(Radiance7, RememberTheFallen_MayPlayNone) {
    auto u1 = toTrash(P1, kTrifarianCaptain);
    driveResumable(kRememberTheFallen, P1, spellObj(kRememberTheFallen),
                   scripted({val(kPickSeveralDone)}).picker(), exec);
    EXPECT_TRUE(inTrash(P1, u1));
}

}  // namespace
