#pragma once
/// @file card_helpers.h
/// Cross-cutting card helpers used across multiple card types. Type-specific
/// bases live in their type dirs (gear/equip_base.h, units/weaponmaster_base.h).
#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/gear/equip_base.h"   // canonical standardEquip(+energy,Domain) + bases
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {

// The engine-known Gold gear token's card_def_id.
constexpr CardDefId kGoldGearCardDefId = 326;
// The Bomb gear token (Radiance, CR 187.12): [Deploy] + a Deathknell.
constexpr CardDefId kBombTokenCardDefId = 985;

// Power / Energy payment helpers (payOnePower, payEnergy, ...) live in
// gear/equip_base.h (included above) so the equip bases can use them.

// ── Battlefields and combat ──
/// The battlefield whose card object is `card_object` (a battlefield card's
/// own ctx.source, or a conquer / hold trigger's subject on a legend).
inline std::optional<BattlefieldId> battlefieldOfCard(const GameState& state,
                                                      GameObjectId card_object) {
    if (card_object == kInvalidId) return std::nullopt;
    for (const auto& bf : state.battlefields)
        if (bf.card_object_id == card_object) return bf.id;
    return std::nullopt;
}
/// Combat damage `player` assigned beyond lethal in their combat at `bf` this
/// turn — 0 if their most recent combat was somewhere else (or there was none).
inline int excessCombatDamageAt(const GameState& state, PlayerId player, BattlefieldId bf) {
    const auto& ps = state.player(player);
    return ps.last_combat_bf == bf ? ps.excess_combat_damage : 0;
}

inline bool standardEquip(CardContext& ctx, GameObjectId gear_id,
                          GameObjectId unit_id, std::optional<Domain> domain) {
    auto& state = ctx.state;
    if (!state.objectExists(gear_id) || !state.objectExists(unit_id)) return false;
    if (!payOnePower(ctx, ctx.controller, domain, SpendPurpose::GearAbility)) return false;
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit.name +
                        " (might_bonus=" + std::to_string(gear.might_bonus) + ")");
    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
    return true;
}

// Attach a gear directly (no cost) to a unit (e.g. Edge of Night's
// play-from-facedown → attach). Detaches from any prior bearer first.
inline void attachFree(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id) {
    auto& state = ctx.state;
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    if (gear.attached_to.has_value() && state.objectExists(*gear.attached_to)) {
        auto& old = state.getObject(*gear.attached_to);
        old.attachment_might_bonus -= gear.might_bonus;
        auto it = std::find(old.attachments.begin(), old.attachments.end(), gear_id);
        if (it != old.attachments.end()) old.attachments.erase(it);
        old.recomputeMight();
    }
    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    ctx.events.logTrace("ATTACH (free): " + gear.name + " -> " + unit.name);
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
}

// ── Discard-then-act resumable helpers ──
// discardOneThenAct: discard exactly one (agent-chosen), then post_fn(ctx, discarded).
template <typename PostFn>
inline void discardOneThenAct(CardContext& ctx, const std::string& label,
                              PostFn&& post_fn) {
    auto& ri = ctx.state.chain.resuming.value();
    auto& ps = ctx.state.player(ctx.controller);
    auto build_choices = [&]() {
        std::vector<Intent> out;
        for (auto cid : ps.hand) {
            Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
            c.chosen_objects = {cid}; out.push_back(std::move(c));
        }
        return out;
    };
    switch (ri.resume_point) {
    case 0:
        if (ps.hand.empty()) { post_fn(ctx, kInvalidId); return; }
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        ri.resume_point = 1;
        return;
    case 1: {
        GameObjectId chosen = kInvalidId;
        auto choice = ctx.executor.takeChoice();
        if (choice && !choice->chosen_objects.empty()) {
            chosen = choice->chosen_objects[0];
            ctx.executor.applyDiscard(ctx.controller, chosen);
        }
        post_fn(ctx, chosen);
        return;
    }
    }
}

// discardThenAct: discard up to `count` (agent-chosen), then post_fn(ctx).
template <typename PostFn>
inline void discardThenAct(CardContext& ctx, int count,
                           const std::string& label, PostFn&& post_fn) {
    auto& ri = ctx.state.chain.resuming.value();
    auto& ps = ctx.state.player(ctx.controller);
    auto build_choices = [&]() {
        std::vector<Intent> out;
        for (auto cid : ps.hand) {
            Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
            c.chosen_objects = {cid}; out.push_back(std::move(c));
        }
        return out;
    };
    switch (ri.resume_point) {
    case 0:
        if (ps.hand.empty() || count <= 0) { post_fn(ctx); return; }
        ri.resume_data = {count};
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        ri.resume_point = 1;
        return;
    case 1: {
        auto choice = ctx.executor.takeChoice();
        if (choice && !choice->chosen_objects.empty())
            ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
        if (!ri.resume_data.empty()) ri.resume_data[0]--;
        if (ri.resume_data.empty() || ri.resume_data[0] <= 0 || ps.hand.empty()) {
            post_fn(ctx); return;
        }
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        return;
    }
    }
}

// ── Countering (CR 426) ──
// CR 420.4.b: non-triggered checks of "cards you've played" count FINALIZED
// cards, and a countered card was finalized — so countering does not undo the
// played count. CR 420.4.a.1 / 426.1.b: abilities that trigger on a card being
// played simply never trigger, because those fire at resolution. Nothing is
// reverted here; the function remains as the single place that would change.
inline void revertCounteredPlay(CardContext& /*ctx*/, const ChainItem& /*top*/) {}

/// Put a card that was removed from the chain without resolving where it
/// belongs: `dest` (trash — CR 426.1.a.1 — or hand for "return it to hand"),
/// unless it carries a "then banish it" replacement (CR 391.3.a: a Flow spell
/// that would leave the chain other than by its own execution is banished).
inline void disposeCounteredCard(CardContext& ctx, GameObjectId id,
                                 ZoneType dest = ZoneType::Trash) {
    if (!ctx.state.objectExists(id)) return;
    auto& obj = ctx.state.getObject(id);
    obj.location = std::nullopt;
    if (obj.banish_on_leave_chain) {
        obj.banish_on_leave_chain = false;
        obj.zone = ZoneType::Banishment;
        ctx.state.player(obj.owner).banishment.push_back(id);
        ctx.state.player(obj.owner).cards_banished_this_turn++;
        ctx.events.logTrace("COUNTER: " + obj.name + " -> banished (played for Flow)");
        ctx.events.emit(ObjectStateChangedEvent{id, "banished_owned"});
        return;
    }
    obj.zone = dest;
    if (dest == ZoneType::Hand) ctx.state.player(obj.owner).hand.push_back(id);
    else ctx.state.player(obj.owner).trash.push_back(id);
}

// "This can't be countered" (the spell's own card) or "Your spells and
// abilities can't be countered" (its controller). Every counter effect checks
// this before removing a chain item.
inline bool isUncounterable(const CardContext& ctx, const ChainItem& item) {
    if (ctx.state.player(item.controller).spells_cant_be_countered) return true;
    if (ctx.registry && ctx.state.objectExists(item.source)) {
        const Card* c = ctx.registry->get(ctx.state.getObject(item.source).card_def_id);
        if (c && c->cantBeCountered()) return true;
    }
    return false;
}

inline void counterChainTop(CardContext& ctx) {
    if (ctx.state.chain.items.empty()) return;
    auto& top = ctx.state.chain.items.back();
    if (!top.is_spell) return;
    if (isUncounterable(ctx, top)) return;
    auto countered = top.source;
    revertCounteredPlay(ctx, top);  // CR 425.1.b
    ctx.state.chain.items.pop_back();
    if (ctx.state.objectExists(countered)) {
        ctx.events.logTrace("COUNTER: " + ctx.state.getObject(countered).name + " countered");
        disposeCounteredCard(ctx, countered);
    }
}

// ── Gold tokens ──
inline void createGoldToken(CardContext& ctx) {
    auto loc = BaseLocation{ctx.controller};
    auto tid = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                        0, {"Gold"}, KeywordSet{}, loc, false);
    if (ctx.state.objectExists(tid))
        ctx.state.getObject(tid).card_def_id = kGoldGearCardDefId;
}

inline GameObjectId createGoldExhausted(CardContext& ctx) {
    LocationId loc{BaseLocation{ctx.controller}};
    auto tok = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                        0, {}, {}, loc, /*enter_ready=*/false);
    if (ctx.state.objectExists(tok)) {
        auto& gold = ctx.state.getObject(tok);
        gold.is_exhausted = true;
        // Bind the Gold token card so its "[Reaction] Kill this, [E]: Add" ability exists.
        gold.card_def_id = kGoldGearCardDefId;
    }
    return tok;
}

// ── Bomb tokens (Radiance) ──
// "Play a Bomb gear token to a battlefield": Bombs have [Deploy], so they are
// only ever created at a battlefield (CR 830.1.b).
inline GameObjectId createBombToken(CardContext& ctx, PlayerId controller, BattlefieldId bf) {
    auto tok = ctx.executor.createToken(controller, CardType::Gear, "Bomb", 0, {"Bomb"},
                                        KeywordSet{}, LocationId{BattlefieldLocation{bf}},
                                        /*enter_ready=*/true);
    if (ctx.state.objectExists(tok)) {
        auto& b = ctx.state.getObject(tok);
        b.card_def_id = kBombTokenCardDefId;
        b.keywords.set(Keyword::Deathknell);
    }
    return tok;
}

// ── Misc ──
inline GameObjectId findMostRecentlyPlayedSpell(const GameState& state, PlayerId player) {
    const auto& ps = state.player(player);
    for (auto it = ps.trash.rbegin(); it != ps.trash.rend(); ++it) {
        if (!state.objectExists(*it)) continue;
        if (state.getObject(*it).isSpell()) return *it;
    }
    return kInvalidId;
}

inline void moveToLocation(EffectExecutor& exec, GameObjectId obj,
                           const std::optional<LocationId>& loc) {
    if (!loc) return;
    if (std::holds_alternative<BattlefieldLocation>(*loc))
        exec.moveToBattlefield(obj, std::get<BattlefieldLocation>(*loc).id);
    else
        exec.moveToBase(obj);
}

inline bool isMighty(const GameObject& obj) { return obj.current_might >= 5; }

inline bool hasTag(const GameObject& obj, const std::string& tag) {
    for (const auto& t : obj.tags) if (t == tag) return true;
    return false;
}

inline bool isEquipment(const GameObject& obj) {
    return obj.isGear() && hasTag(obj, "Equipment");
}

// ── Tag presence (Ivern / Bird-Cat-Dog-Poro family) ──
struct TagPresence {
    bool bird = false, cat = false, dog = false, poro = false;
    int count() const { return (int)bird + (int)cat + (int)dog + (int)poro; }
    bool allFour() const { return bird && cat && dog && poro; }
};

inline TagPresence scanFriendlyTags(const GameState& state, PlayerId controller) {
    TagPresence out;
    for (const auto& [id, obj] : state.objects) {
        if (!obj.isUnit() || obj.controller != controller) continue;
        if (!obj.location.has_value()) continue;
        for (const auto& tag : obj.tags) {
            if      (tag == "Bird") out.bird = true;
            else if (tag == "Cat")  out.cat  = true;
            else if (tag == "Dog")  out.dog  = true;
            else if (tag == "Poro") out.poro = true;
        }
    }
    return out;
}

// ── Empower costs ───────────────────────────────────────────────────────────
// "[Empower] [N]" / "[Empower] [N][R]" / "[Empower] [N][A][A]".
inline Card::EmpowerSpec empowerCost(int energy, int power = 0,
                                     Domain power_domain = Domain::Fury,
                                     bool any_domain = false) {
    Card::EmpowerSpec s;
    s.valid = true;
    s.cost.energy = energy;
    s.cost.power = power;
    s.cost.power_domain = power_domain;
    s.cost.power_any_domain = any_domain;
    return s;
}

// ── Flow costs ──────────────────────────────────────────────────────────────
inline Card::FlowCost flow(int energy, int power = 0,
                           Domain power_domain = Domain::Fury,
                           bool any_domain = false) {
    Card::FlowCost f;
    f.valid = true;
    f.energy = energy;
    f.power = power;
    f.power_domain = power_domain;
    f.any_domain = any_domain;
    return f;
}

// ── Passive-aura plumbing ───────────────────────────────────────────────────
// Card::applyPassiveAura is invoked once per on-board instance of the card but
// is not told WHICH instance, so implementations loop over every instance. A
// marker effect makes each instance get its effect once per aura pass no
// matter how many copies are in play.
constexpr int kAuraOnceMarker = -777;

template <typename Fn>
void forEachSelfOnce(const Card& card, GameState& state, PlayerId controller, Fn&& fn) {
    for (auto& [sid, self] : state.objects) {
        if (self.card_def_id != card.cardDefId() || self.controller != controller) continue;
        if (!self.location.has_value() && self.zone != ZoneType::LegendZone) continue;
        bool seen = false;
        for (const auto& ae : self.aura_effects)
            if (ae.source == sid && ae.keyword == Keyword::Count &&
                ae.keyword_value == kAuraOnceMarker) { seen = true; break; }
        if (seen) continue;
        GameObject::AuraEffect marker;
        marker.source = sid;
        marker.keyword_value = kAuraOnceMarker;
        self.aura_effects.push_back(marker);
        fn(sid, self);
    }
}

/// "[Empowered][>] ..." — visit each Empowered instance once per aura pass.
template <typename Fn>
void forEachEmpoweredSelf(const Card& card, GameState& state, PlayerId controller, Fn&& fn) {
    forEachSelfOnce(card, state, controller, [&](GameObjectId sid, GameObject& self) {
        if (self.isEmpowered()) fn(sid, self);
    });
}

/// "+N [M]" from a continuous effect.
inline void auraMight(GameObject& obj, GameObjectId source, int amount) {
    GameObject::AuraEffect ae;
    ae.source = source;
    ae.might_bonus = amount;
    obj.aura_effects.push_back(ae);
}

/// A keyword from a continuous effect. [Assault N] / [Shield N] also carry
/// their conditional Might (while attacking / defending), as the engine's
/// aura fold only sums might_bonus.
inline void auraKeyword(GameObject& obj, GameObjectId source, Keyword kw, int value = 0) {
    GameObject::AuraEffect ae;
    ae.source = source;
    ae.keyword = kw;
    ae.keyword_value = value;
    if (kw == Keyword::Assault && obj.combat_designation == CombatDesignation::Attacker)
        ae.might_bonus = value > 0 ? value : 1;
    if (kw == Keyword::Shield && obj.combat_designation == CombatDesignation::Defender)
        ae.might_bonus = value > 0 ? value : 1;
    obj.aura_effects.push_back(ae);
}

// ── Board queries ───────────────────────────────────────────────────────────
/// Runes `player` controls (in base, ready or exhausted).
inline int runesControlled(const GameState& state, PlayerId player) {
    int n = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player && obj.location.has_value()) ++n;
    return n;
}

/// Gear `player` controls on the board.
inline int gearControlled(const GameState& state, PlayerId player) {
    int n = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.controller == player && obj.location.has_value()) ++n;
    return n;
}

/// Cards in `player`'s trash with the given name.
inline int namedInTrash(const GameState& state, PlayerId player, const std::string& name) {
    int n = 0;
    for (auto cid : state.player(player).trash)
        if (state.objectExists(cid) && state.getObject(cid).name == name) ++n;
    return n;
}

/// On-board units matching `pred`, honoring "can't be chosen by enemy spells
/// and abilities" for units `chooser` doesn't control.
template <typename Pred>
std::vector<GameObjectId> chooseableUnits(const GameState& state, PlayerId chooser, Pred&& pred) {
    std::vector<GameObjectId> out;
    for (const auto& [id, obj] : state.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.controller != chooser && obj.untargetable_by_enemy) continue;
        if (pred(obj)) out.push_back(id);
    }
    return out;
}

/// Other units `controller` has at the same location as `self`.
inline int otherFriendlyUnitsHere(const GameState& state, const GameObject& self) {
    if (!self.location.has_value()) return 0;
    int n = 0;
    for (const auto& [id, obj] : state.objects) {
        if (id == self.id || !obj.isUnit() || obj.controller != self.controller) continue;
        if (obj.location.has_value() && *obj.location == *self.location) ++n;
    }
    return n;
}

/// "Its base Might becomes N this turn" (Lulu, Whimsical; Herald of the
/// Emperor). Modelled like Dragon Form: a this-turn Might delta of
/// (N - base Might), so buffs, gear and other modifiers still apply and it
/// expires with the other "this turn" Might. A later "becomes" this turn
/// replaces an earlier one made through this helper instead of stacking.
inline void setBaseMightThisTurn(GameState& state, GameObjectId unit, int n) {
    if (!state.objectExists(unit)) return;
    auto& u = state.getObject(unit);
    const int stamp = state.turn.turn_number + 1;   // +1 so turn 0 differs from "unset"
    auto it = u.card_counters.find("__base_set_turn");
    if (it != u.card_counters.end() && it->second == stamp)
        u.temp_might_bonus -= u.card_counters["__base_set_delta"];
    const int delta = n - u.base_might;
    u.card_counters["__base_set_turn"] = stamp;
    u.card_counters["__base_set_delta"] = delta;
    u.temp_might_bonus += delta;
    u.recomputeMight();
}

// ── Agent choices by hand ──
// For cards whose resolution asks a custom question (a unit from a computed
// list, then a destination, …) and so build their own MakeChoice options
// rather than going through Card::pickTarget.

/// A MakeChoice option that picks object `id`.
inline Intent objChoice(PlayerId p, GameObjectId id) {
    Intent c;
    c.type = IntentType::MakeChoice;
    c.player = p;
    c.chosen_objects = {id};
    return c;
}
/// The object the agent picked from objChoice options (kInvalidId if none).
inline GameObjectId takenObject(CardContext& ctx) {
    auto c = ctx.executor.takeChoice();
    return (c && !c->chosen_objects.empty()) ? c->chosen_objects[0] : kInvalidId;
}

/// A MakeChoice option carrying the int `v`, shown to the agent as `label`.
inline Intent valChoice(PlayerId p, int v, const std::string& label) {
    Intent c;
    c.type = IntentType::MakeChoice;
    c.player = p;
    c.chosen_value = v;
    c.choice_label = label;
    return c;
}
/// The int the agent picked from valChoice options (`fallback` if none).
inline int takenValue(CardContext& ctx, int fallback) {
    auto c = ctx.executor.takeChoice();
    return (c && c->chosen_value.has_value()) ? *c->chosen_value : fallback;
}

// ── One step of a resumable resolution, answered by any player ──
// For choices Card's helpers don't cover: a choice made by a player other than
// the ability's controller ("each player kills one of their units", "its owner
// places it on the top or bottom"), or several choices in one resolution. `rp`
// is a resume_point the Card helpers don't use (20 and up); the step publishes
// while resume_point < rp, takes the answer at rp and is done above it. The
// answer is kept in resume_data[slot] (use 10 and up). Steps compose in
// increasing `rp` order. Returns false while the agent is deciding (the caller
// returns); otherwise `out` holds the answer (kInvalidId / the first option
// when there is no chain item, nothing to choose → kInvalidId / `fallback`).
inline bool stepChooseObject(CardContext& ctx, PlayerId chooser,
                             const std::vector<GameObjectId>& options,
                             const std::string& label, int rp, size_t slot,
                             GameObjectId& out) {
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) { out = options.empty() ? kInvalidId : options.front(); return true; }
    auto& ri = *ri_opt;
    while (ri.resume_data.size() <= slot) ri.resume_data.push_back(0);
    if (ri.resume_point < rp) {
        if (options.empty()) {
            ri.resume_data[slot] = static_cast<int32_t>(kInvalidId);
            ri.resume_point = rp + 1;
            out = kInvalidId;
            return true;
        }
        std::vector<Intent> opts;
        for (auto id : options) opts.push_back(objChoice(chooser, id));
        ctx.executor.requestChoice(chooser, std::move(opts), label);
        ri.resume_point = rp;
        return false;
    }
    if (ri.resume_point == rp) {
        GameObjectId t = takenObject(ctx);
        if (std::find(options.begin(), options.end(), t) == options.end())
            t = options.empty() ? kInvalidId : options.front();
        ri.resume_data[slot] = static_cast<int32_t>(t);
        ri.resume_point = rp + 1;
        out = t;
        return true;
    }
    out = static_cast<GameObjectId>(ri.resume_data[slot]);
    return true;
}
inline bool stepChooseValue(CardContext& ctx, PlayerId chooser,
                            const std::vector<std::pair<int, std::string>>& options,
                            const std::string& label, int rp, size_t slot,
                            int fallback, int& out) {
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) { out = options.empty() ? fallback : options.front().first; return true; }
    auto& ri = *ri_opt;
    while (ri.resume_data.size() <= slot) ri.resume_data.push_back(0);
    if (ri.resume_point < rp) {
        if (options.empty()) {
            ri.resume_data[slot] = static_cast<int32_t>(fallback);
            ri.resume_point = rp + 1;
            out = fallback;
            return true;
        }
        std::vector<Intent> opts;
        for (const auto& [v, l] : options) opts.push_back(valChoice(chooser, v, l));
        ctx.executor.requestChoice(chooser, std::move(opts), label);
        ri.resume_point = rp;
        return false;
    }
    if (ri.resume_point == rp) {
        int v = takenValue(ctx, options.empty() ? fallback : options.front().first);
        bool known = false;
        for (const auto& o : options) if (o.first == v) known = true;
        if (!known) v = options.empty() ? fallback : options.front().first;
        ri.resume_data[slot] = static_cast<int32_t>(v);
        ri.resume_point = rp + 1;
        out = v;
        return true;
    }
    out = static_cast<int>(ri.resume_data[slot]);
    return true;
}

/// Several picks in a row from one list ("Kill any number of friendly Bomb
/// tokens", "Choose two —", "a battlefield"): `chooser` picks one option at a
/// time, never the same one twice, up to `max`, and may stop with "Done" once
/// `min` are picked. `options` are object ids (offered with objChoice) or, with
/// `values`, ints 0..14 (offered with valChoice and `labels`). The options are
/// re-read on each call; picks already made stand.
/// State lives in the resolving chain item: resume_data[slot] (0 fresh,
/// 1 waiting, 2 done), [slot+1] the count, [slot+2 ...] the picks. Use slots
/// >= 8 (Card's choice helpers own 0..5) with room for max+2 entries, and call
/// Card::pickXAmount (which appends to resume_data) before this. Returns
/// nullopt while waiting for the agent (the caller returns). With no resolving
/// item (a direct call) it takes the first `max` options.
constexpr int kPickSeveralDone = 15;
inline std::optional<std::vector<int>> pickSeveral(
        CardContext& ctx, PlayerId chooser, const std::string& label,
        const std::vector<int>& options, bool values,
        const std::vector<std::string>& labels, int min, int max, size_t slot) {
    auto& ro = ctx.state.chain.resuming;
    if (!ro.has_value()) {
        std::vector<int> out(options.begin(),
                             options.begin() + std::min<size_t>(options.size(), std::max(0, max)));
        return out;
    }
    auto& rd = ro->resume_data;
    if (rd.size() < slot + 2) rd.resize(slot + 2, 0);
    auto picked = [&]() {
        std::vector<int> out;
        for (int i = 0; i < rd[slot + 1]; ++i) out.push_back(rd[slot + 2 + i]);
        return out;
    };
    if (rd[slot] == 2) return picked();
    if (rd[slot] == 1) {
        auto c = ctx.executor.takeChoice();
        bool stop = !c.has_value();
        int v = 0;
        if (c && !c->chosen_objects.empty()) v = static_cast<int>(c->chosen_objects[0]);
        else if (c && c->chosen_value.has_value()) {
            v = *c->chosen_value;
            if (!values || v == kPickSeveralDone) stop = true;
        } else stop = true;
        if (stop) { rd[slot] = 2; return picked(); }
        const int n = rd[slot + 1];
        if (rd.size() < slot + 3 + n) rd.resize(slot + 3 + n, 0);
        rd[slot + 2 + n] = v;
        rd[slot + 1] = n + 1;
        rd[slot] = 0;
    }
    std::vector<int> have = picked();
    std::vector<Intent> offer;
    std::string summary;
    for (size_t i = 0; i < options.size(); ++i) {
        const int o = options[i];
        if (std::find(have.begin(), have.end(), o) != have.end()) continue;
        if (values) {
            const std::string l = i < labels.size() ? labels[i] : "#" + std::to_string(o);
            offer.push_back(valChoice(chooser, o, l));
            summary += (summary.empty() ? "" : "/") + l;
        } else {
            offer.push_back(objChoice(chooser, static_cast<GameObjectId>(o)));
            summary += (summary.empty() ? "" : "/") +
                (ctx.state.objectExists(o) ? ctx.state.getObject(o).name : std::string("?"));
        }
    }
    if (static_cast<int>(have.size()) >= max || offer.empty()) { rd[slot] = 2; return have; }
    if (static_cast<int>(have.size()) >= min)
        offer.push_back(valChoice(chooser, kPickSeveralDone, "Done"));
    ctx.events.logTrace("PICK_PROMPT: " + label + " [" + summary + "] (" +
                        std::to_string(have.size()) + " picked)");
    ctx.executor.requestChoice(chooser, std::move(offer), label);
    rd[slot] = 1;
    return std::nullopt;
}

/// A battlefield, int-coded by BattlefieldId, for pickSeveral(values=true).
inline std::vector<int> battlefieldOptions(const GameState& state,
                                           std::vector<std::string>* labels) {
    std::vector<int> out;
    for (const auto& bf : state.battlefields) {
        out.push_back(static_cast<int>(bf.id));
        if (labels)
            labels->push_back(state.objectExists(bf.card_object_id)
                ? state.getObject(bf.card_object_id).name
                : "Battlefield #" + std::to_string(bf.id));
    }
    return out;
}

/// Destinations a card effect may move `u` to, int-coded for valChoice:
/// 0 = its controller's base, 1 + i = battlefield i. Excludes where it is now.
inline std::vector<int> moveDests(const GameState& state, const GameObject& u) {
    std::vector<int> out;
    if (!u.location.has_value()) return out;
    if (!u.isAtBase()) out.push_back(0);
    for (const auto& bf : state.battlefields) {
        auto here = u.battlefieldId();
        if (here.has_value() && *here == bf.id) continue;
        out.push_back(1 + static_cast<int>(bf.id));
    }
    return out;
}
inline std::string destLabel(int d) {
    return d <= 0 ? std::string("Base") : "Battlefield #" + std::to_string(d - 1);
}
/// Move `u` to a moveDests destination.
inline void moveTo(CardContext& ctx, GameObjectId u, int dest) {
    if (dest <= 0) ctx.executor.moveToBase(u);
    else ctx.executor.moveToBattlefield(u, static_cast<BattlefieldId>(dest - 1));
}

// ── One name from a long list, in int-coded steps ──
/// MakeChoice int answers have kIntChoiceSlots action slots
/// (openspiel::kNumIntChoices), so a choice among more names than that ("name
/// a tag": every tag in the card pool) is asked in steps: first a range of
/// the (sorted) list, then a name within it — each step at most
/// kIntChoiceSlots options, each option a distinct slot. Steps use
/// stepChooseValue with resume_points rp, rp+2, ... and resume_data slots
/// slot, slot+1, ... (two steps cover 256 names, three 4096). Returns false
/// while waiting; then `out` is the chosen index (-1 if `names` is empty).
constexpr int kIntChoiceSlots = 16;
inline bool stepChooseFromList(CardContext& ctx, PlayerId chooser,
                               const std::vector<std::string>& names,
                               const std::string& label, int rp, size_t slot, int& out) {
    if (names.empty()) { out = -1; return true; }
    size_t lo = 0, hi = names.size();
    for (int level = 0;; ++level) {
        const size_t n = hi - lo;
        std::vector<std::pair<int, std::string>> opts;
        size_t chunk = 1;
        if (n <= static_cast<size_t>(kIntChoiceSlots)) {
            for (size_t i = lo; i < hi; ++i) opts.emplace_back(static_cast<int>(i - lo), names[i]);
        } else {
            chunk = (n + kIntChoiceSlots - 1) / kIntChoiceSlots;
            for (size_t b = 0; lo + b * chunk < hi; ++b) {
                const size_t first = lo + b * chunk;
                const size_t last = std::min(first + chunk, hi) - 1;
                opts.emplace_back(static_cast<int>(b), names[first] + " ... " + names[last]);
            }
        }
        int v = 0;
        if (!stepChooseValue(ctx, chooser, opts,
                             label + (chunk > 1 ? " (choose a range)" : ""),
                             rp + 2 * level, slot + level, 0, v))
            return false;
        if (chunk == 1) { out = static_cast<int>(lo) + v; return true; }
        lo += static_cast<size_t>(v) * chunk;
        hi = std::min(lo + chunk, hi);
    }
}

// ── Death replacements left on a unit (Card::applyGrantedDeathReplacement) ──
/// Record that `card_def`'s death replacement applies to `unit` (made by
/// `by`, with a card-defined `stamp` such as turn_number + 1). One per card.
inline void grantDeathReplacement(GameState& state, GameObjectId unit, CardDefId card_def,
                                  PlayerId by, int stamp) {
    if (!state.objectExists(unit)) return;
    auto& grants = state.getObject(unit).death_replacement_grants;
    for (auto& g : grants)
        if (g.source_def == card_def) { g.granted_by = by; g.stamp = stamp; return; }
    grants.push_back({card_def, by, stamp});
}
/// Remove `card_def`'s grant from `unit` ("the next time": used up), returning it.
inline std::optional<GameObject::DeathReplacementGrant> takeDeathReplacement(
        GameState& state, GameObjectId unit, CardDefId card_def) {
    if (!state.objectExists(unit)) return std::nullopt;
    auto& grants = state.getObject(unit).death_replacement_grants;
    for (auto it = grants.begin(); it != grants.end(); ++it)
        if (it->source_def == card_def) {
            auto g = *it;
            grants.erase(it);
            return g;
        }
    return std::nullopt;
}

// ── Split damage (CR 355.14) ────────────────────────────────────────────────
// "Deal N damage split among <units>". Each unit in the split is a target
// chosen when the spell or ability is finalized (355.14.a-b), up to the
// initial amount of damage (355.14.c): splitDamageTargets, called before the
// card's finalizingChoices marker, asks for them one at a time (choosing an
// opponent's [Deflect] unit costs its Deflect then), with "Done" once
// `min_targets` are chosen. As it resolves, splitDamageResolve drops targets
// that are no longer legal; if more remain than there is damage, the
// controller picks which stop being targets until they match (355.14.h);
// every remaining target gets 1 (355.14.f-g) and the controller assigns the
// rest one point at a time (355.14.e). The damage is dealt once the division
// is known; `damaged` receives the units dealt damage (kill lethally damaged
// ones after). State lives in resume_data[kSplitBase .. kSplitEnd).
constexpr int kSplitMaxTargets = 32;
constexpr size_t kSplitBase = 16;
constexpr size_t kSplitTState = kSplitBase;                     // 0 ask, 1 waiting, 2 done
constexpr size_t kSplitTCount = kSplitBase + 1;
constexpr size_t kSplitTIds = kSplitBase + 2;
constexpr size_t kSplitRState = kSplitTIds + kSplitMaxTargets;   // 0 fresh, 1 drop, 2 assign, 3 done, 4 working
constexpr size_t kSplitRTotal = kSplitRState + 1;
constexpr size_t kSplitRCount = kSplitRState + 2;
constexpr size_t kSplitRIds = kSplitRState + 3;
constexpr size_t kSplitRExtra = kSplitRIds + kSplitMaxTargets;   // damage beyond the first point
constexpr size_t kSplitEnd = kSplitRExtra + kSplitMaxTargets;

/// Returns false while waiting for the controller. `initial_damage` is the
/// amount as the spell or ability is played.
inline bool splitDamageTargets(CardContext& ctx, const std::string& label, int initial_damage,
                               int min_targets, const std::vector<GameObjectId>& legal) {
    auto& ro = ctx.state.chain.resuming;
    if (!ro.has_value()) return true;   // direct invocation: splitDamageResolve spreads it
    auto& rd = ro->resume_data;
    if (rd.size() < kSplitEnd) rd.resize(kSplitEnd, 0);
    if (rd[kSplitTState] == 2) return true;
    auto chosen = [&rd]() {
        std::vector<GameObjectId> out;
        for (int i = 0; i < rd[kSplitTCount]; ++i)
            out.push_back(static_cast<GameObjectId>(rd[kSplitTIds + i]));
        return out;
    };
    auto offered = [&]() {
        const auto have = chosen();
        const int payable = ctx.executor.payableAnyPower(ctx.controller);
        std::vector<GameObjectId> out;
        for (auto t : legal)
            if (std::find(have.begin(), have.end(), t) == have.end() &&
                ctx.executor.deflectCostFor(ctx.controller, t) <= payable)
                out.push_back(t);
        return out;
    };
    const int cap = std::min(initial_damage, kSplitMaxTargets);
    if (rd[kSplitTState] == 1) {
        rd[kSplitTState] = 0;
        auto c = ctx.executor.takeChoice();
        GameObjectId t = (c && !c->chosen_objects.empty()) ? c->chosen_objects[0] : kInvalidId;
        const auto can = offered();
        if (std::find(can.begin(), can.end(), t) == can.end()) {
            const bool may_stop = rd[kSplitTCount] >= min_targets;
            if (may_stop || can.empty()) { rd[kSplitTState] = 2; return true; }
            t = can.front();
        }
        const int cost = ctx.executor.deflectCostFor(ctx.controller, t);
        if (cost > 0) {
            ctx.executor.payAnyPower(ctx.controller, cost);
            ctx.events.logTrace("DEFLECT: pays " + std::to_string(cost) + " [A] to choose " +
                                ctx.state.getObject(t).name);
        }
        ro->chosen_objects.push_back(t);
        noteEnemyChoice(ctx.state, ctx.controller, ctx.source, t);
        if (ctx.state.getObject(t).controller != ctx.controller)
            ctx.state.player(ctx.controller).chose_enemy_unit_this_turn = true;
        rd[kSplitTIds + rd[kSplitTCount]] = static_cast<int32_t>(t);
        rd[kSplitTCount] += 1;
        ctx.events.logTrace("SPLIT_TARGET: " + label + " -> " + ctx.state.getObject(t).name);
    }
    const auto can = offered();
    if (rd[kSplitTCount] >= cap || can.empty()) { rd[kSplitTState] = 2; return true; }
    std::vector<Intent> opts;
    for (auto t : can) opts.push_back(objChoice(ctx.controller, t));
    if (rd[kSplitTCount] >= min_targets)
        opts.push_back(valChoice(ctx.controller, kPickSeveralDone, "Done"));
    ctx.executor.requestChoice(ctx.controller, std::move(opts),
        "split damage: " + label + " — choose a target (" + std::to_string(rd[kSplitTCount]) +
        " of up to " + std::to_string(cap) + ")");
    rd[kSplitTState] = 1;
    return false;
}

/// Returns false while waiting for the controller; true once the damage is
/// dealt (or there is none to deal). `legal` are the units that are still
/// legal targets; `total` the damage to divide now.
inline bool splitDamageResolve(CardContext& ctx, const std::string& label, int total,
                               GameObjectId damage_source, const std::vector<GameObjectId>& legal,
                               std::vector<GameObjectId>& damaged) {
    auto& ro = ctx.state.chain.resuming;
    if (!ro.has_value()) {
        // Direct invocation (no chain item): no agent can be asked; an even
        // spread over the legal units, like the other helpers' first option.
        if (total <= 0 || legal.empty()) return true;
        std::vector<std::pair<GameObjectId, int>> alloc;
        for (int i = 0; i < total; ++i) {
            const auto id = legal[i % legal.size()];
            bool found = false;
            for (auto& a : alloc) if (a.first == id) { ++a.second; found = true; }
            if (!found) alloc.emplace_back(id, 1);
        }
        for (const auto& [id, n] : alloc) {
            ctx.executor.dealDamage(id, n, damage_source);
            damaged.push_back(id);
        }
        return true;
    }
    auto& ri = *ro;
    auto& rd = ri.resume_data;
    if (rd.size() < kSplitEnd) rd.resize(kSplitEnd, 0);
    if (rd[kSplitRState] == 3) return true;
    auto kept = [&rd]() {
        std::vector<GameObjectId> out;
        for (int i = 0; i < rd[kSplitRCount]; ++i)
            out.push_back(static_cast<GameObjectId>(rd[kSplitRIds + i]));
        return out;
    };
    if (rd[kSplitRState] == 0) {
        // The targets chosen at finalization that are still legal.
        int n = 0;
        for (int i = 0; i < rd[kSplitTCount]; ++i) {
            const auto t = static_cast<GameObjectId>(rd[kSplitTIds + i]);
            if (std::find(legal.begin(), legal.end(), t) == legal.end()) {
                ctx.events.logTrace("SPLIT: " + label + " — a target is no longer legal");
                continue;
            }
            rd[kSplitRIds + n] = static_cast<int32_t>(t);
            rd[kSplitRExtra + n] = 0;
            ++n;
        }
        rd[kSplitRCount] = n;
        rd[kSplitRTotal] = std::max(0, total);
        rd[kSplitRState] = 4;
    } else if (rd[kSplitRState] == 1 || rd[kSplitRState] == 2) {
        const auto ks = kept();
        auto c = ctx.executor.takeChoice();
        GameObjectId t = (c && !c->chosen_objects.empty()) ? c->chosen_objects[0] : kInvalidId;
        auto it = std::find(ks.begin(), ks.end(), t);
        if (it == ks.end()) it = ks.begin();
        const int i = static_cast<int>(it - ks.begin());
        if (rd[kSplitRState] == 1) {   // that target stops being a target
            for (int k = i; k + 1 < rd[kSplitRCount]; ++k) {
                rd[kSplitRIds + k] = rd[kSplitRIds + k + 1];
                rd[kSplitRExtra + k] = rd[kSplitRExtra + k + 1];
            }
            rd[kSplitRCount] -= 1;
        } else {
            rd[kSplitRExtra + i] += 1;
        }
        rd[kSplitRState] = 4;
    }

    const auto ks = kept();
    const int n = static_cast<int>(ks.size());
    const int t_total = rd[kSplitRTotal];
    if (n == 0 || t_total <= 0) { rd[kSplitRState] = 3; return true; }
    auto ask = [&](const std::string& what, int state) {
        std::vector<Intent> opts;
        for (auto t : ks) opts.push_back(objChoice(ctx.controller, t));
        ctx.executor.requestChoice(ctx.controller, std::move(opts), what);
        rd[kSplitRState] = state;
        return false;
    };
    if (n > t_total)
        return ask("split damage: " + label + " — " + std::to_string(t_total) +
                   " damage for " + std::to_string(n) + " targets: which stops being a target", 1);
    int extra = 0;
    for (int i = 0; i < n; ++i) extra += rd[kSplitRExtra + i];
    const int remaining = t_total - n - extra;
    if (remaining > 0 && n > 1)
        return ask("split damage: " + label + " — assign 1 more damage (" +
                   std::to_string(remaining) + " left)", 2);
    if (remaining > 0) rd[kSplitRExtra] += remaining;
    rd[kSplitRState] = 3;
    for (int i = 0; i < n; ++i) {
        const auto id = ks[i];
        if (!ctx.state.objectExists(id)) continue;
        const int amount = 1 + rd[kSplitRExtra + i];
        ctx.events.logTrace("SPLIT: " + label + " -> " + std::to_string(amount) + " to " +
                            ctx.state.getObject(id).name);
        ctx.executor.dealDamage(id, amount, damage_source);
        damaged.push_back(id);
    }
    return true;
}

} // namespace riftbound
