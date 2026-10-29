#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

// "[Action] Move a friendly unit. You may attach up to one Equipment with the
//  same controller to it. This turn, that unit has "When I conquer, you may
//  move me to my base.""
//
// Resolution (resumable): the unit (pickTarget, chosen as the spell is
// played), then its destination (kAwaitDest: base or any battlefield it isn't
// at), then up to one Equipment its controller controls (kAwaitEquip, with a
// "none" option). The granted "When I conquer" is a DelayedAbility scoped to
// the unit, sourced from the unit itself and dispatched to this card's
// onTrigger; it lasts until end of turn and re-arms after each conquer.
class RelentlessPursuit : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    static constexpr int kAwaitDest = 20;
    static constexpr int kAwaitEquip = 21;
    static constexpr int kDone = 22;

    static std::vector<GameObjectId> equipmentFor(const GameState& state, PlayerId controller,
                                                  GameObjectId unit) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (obj.controller != controller || !obj.location.has_value()) continue;
            if (!isEquipment(obj)) continue;
            if (obj.attached_to.has_value() && *obj.attached_to == unit) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    void armConquerGrant(CardContext& ctx, GameObjectId unit) const {
        DelayedAbility da;
        da.source = unit;                 // "that unit has ..." — the unit's ability
        da.card_def_id = cardDefId();
        da.controller = ctx.state.getObject(unit).controller;
        da.trigger = TriggerType::WhenIConquer;
        da.target_filter = unit;
        da.expires_on_turn = ctx.state.turn.turn_number;   // "This turn"
        ctx.state.delayed_abilities.push_back(da);
    }
    void promptEquip(CardContext& ctx, ChainItem& ri, GameObjectId unit) {
        auto eq = equipmentFor(ctx.state, ctx.controller, unit);
        if (eq.empty()) { finish(ctx, ri, unit); return; }
        std::vector<Intent> opts;
        for (auto g : eq) opts.push_back(objChoice(ctx.controller, g));
        Intent none;                        // "up to one": attach nothing
        none.type = IntentType::MakeChoice;
        none.player = ctx.controller;
        opts.push_back(std::move(none));
        ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                   "Relentless Pursuit: attach an Equipment (or none)");
        ri.resume_point = kAwaitEquip;
    }
    void finish(CardContext& ctx, ChainItem& ri, GameObjectId unit) {
        if (ctx.state.objectExists(unit) && ctx.state.getObject(unit).location.has_value())
            armConquerGrant(ctx, unit);
        ri.resume_point = kDone;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId unit = pickTarget(ctx, "Relentless Pursuit: move which unit?", legal);
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;   // suspended / none
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain item): no agent to ask — first
            // destination, no Equipment.
            auto dests = moveDests(ctx.state, ctx.state.getObject(unit));
            if (!dests.empty()) moveTo(ctx, unit, dests.front());
            armConquerGrant(ctx, unit);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;

        if (ri.resume_point < kAwaitDest) {
            // "Move a friendly unit": to its base or another battlefield.
            auto dests = moveDests(ctx.state, ctx.state.getObject(unit));
            if (dests.empty()) { promptEquip(ctx, ri, unit); return; }
            std::vector<Intent> opts;
            for (int d : dests) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Relentless Pursuit: move it where?");
            ri.resume_point = kAwaitDest;
            return;
        }
        if (ri.resume_point == kAwaitDest) {
            const int d = takenValue(ctx, -1);
            auto dests = moveDests(ctx.state, ctx.state.getObject(unit));
            if (std::find(dests.begin(), dests.end(), d) != dests.end()) {
                moveTo(ctx, unit, d);
                ctx.events.logTrace("RELENTLESS PURSUIT: moved " +
                                    ctx.state.getObject(unit).name + " to " + destLabel(d));
            }
            promptEquip(ctx, ri, unit);
            return;
        }
        if (ri.resume_point == kAwaitEquip) {
            GameObjectId g = takenObject(ctx);
            auto eq = equipmentFor(ctx.state, ctx.controller, unit);
            if (g != kInvalidId && std::find(eq.begin(), eq.end(), g) != eq.end() &&
                ctx.state.getObject(unit).location.has_value()) {
                attachFree(ctx, g, unit);
                ctx.events.logTrace("RELENTLESS PURSUIT: attached " +
                                    ctx.state.getObject(g).name);
            }
            finish(ctx, ri, unit);
        }
    }

    // The granted "When I conquer, you may move me to my base." ctx.source is
    // the unit.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId me = ctx.source;
        auto can_move = [&]() {
            return ctx.state.objectExists(me) && ctx.state.getObject(me).isAtBattlefield();
        };
        const int go = confirmOptional(ctx, "Relentless Pursuit: move me to my base?", can_move);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // The ability lasts all turn: re-arm it for a later conquer.
        const bool first_pass = !ctx.state.chain.resuming.has_value() ||
                                ctx.state.chain.resuming->resume_point <= 2;
        if (first_pass && go != -1 && ctx.state.objectExists(me) &&
            ctx.state.getObject(me).location.has_value()) {
            bool armed = false;
            for (const auto& da : ctx.state.delayed_abilities)
                if (!da.fired && da.target_filter == me && da.card_def_id == cardDefId())
                    armed = true;
            if (!armed) armConquerGrant(ctx, me);
        }
        if (go != 1) return;
        ctx.executor.moveToBase(me);
        ctx.events.logTrace("RELENTLESS PURSUIT: conquered -> moved to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 503;
        d.def_id = R"RB(sfd-184-221)RB";
        d.name = R"RB(Relentless Pursuit)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-184/221)RB";
        d.collector_number = 184;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Lucian)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit. You may attach up to one Equipment with the same controller to it. This turn, that unit has "When I conquer, you may move me to my base.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/48754730486abefdb56221d0760b9c963aae7c09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_503(CardRegistry& r) {
    r.registerCard(503, std::make_unique<RelentlessPursuit>());
}

} // namespace riftbound
