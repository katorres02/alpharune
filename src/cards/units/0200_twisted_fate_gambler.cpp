#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class TwistedFateGambler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, reveal the top rune of your rune deck, then recycle it.
    // Do one of the following based on its domain:
    //  [R] Deal 2 to an enemy unit here and 1 to all other enemy units here.
    //  [B] Draw 1.  [Y] Stun an enemy unit."
    // The reveal is part of the effect, so the [R]/[Y] target is chosen at
    // resolution once the domain is known. The revealed branch is stashed in
    // resume_data[kBranchSlot] so re-entering after the target choice does not
    // reveal and recycle a second rune.
    static constexpr size_t kBranchSlot = 6;
    enum Branch : int { kNone = 4, kR = 1, kB = 2, kY = 3 };
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri_opt = ctx.state.chain.resuming;
        int branch = 0;
        if (ri_opt.has_value() && ri_opt->resume_data.size() > kBranchSlot)
            branch = ri_opt->resume_data[kBranchSlot];
        if (branch == 0) {
            branch = revealAndRecycle(ctx);
            if (ri_opt.has_value()) {
                if (ri_opt->resume_data.size() <= kBranchSlot)
                    ri_opt->resume_data.resize(kBranchSlot + 1, 0);
                ri_opt->resume_data[kBranchSlot] = branch;
            }
        }
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        auto enemies_here = [&]() {
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller && my_bf && o.battlefieldId() == my_bf;
            });
        };

        if (branch == kR) {
            GameObjectId focus = pickTarget(ctx, "Twisted Fate [R] (deal 2 here)", enemies_here());
            if (focus == kInvalidId && ctx.executor.hasPendingChoice()) return;  // suspended
            // All enemy units here: 2 to the chosen one, 1 to the others.
            std::vector<GameObjectId> hit;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!my_bf || obj.battlefieldId() != my_bf) continue;
                hit.push_back(id);
            }
            if (focus == kInvalidId) hit.clear();  // no enemy unit to choose: nothing happens
            for (auto id : hit) ctx.executor.dealDamage(id, id == focus ? 2 : 1, ctx.source);
            for (auto id : hit)
                if (ctx.state.objectExists(id) && ctx.state.getObject(id).location.has_value() &&
                    ctx.state.getObject(id).hasLethalDamage())
                    ctx.executor.killObject(id);
        } else if (branch == kB) {
            ctx.executor.drawCards(ctx.controller, 1);
        } else if (branch == kY) {
            auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller;
            });
            GameObjectId t = pickTarget(ctx, "Twisted Fate [Y] (stun)", enemies);
            if (t == kInvalidId && ctx.executor.hasPendingChoice()) return;  // suspended
            if (t != kInvalidId && ctx.state.objectExists(t))
                ctx.executor.stunUnitBy(t, ctx.source);
        }
        ctx.events.logTrace("TWISTED FATE: revealed rune -> domain payoff");
    }

private:
    // Reveal the top rune of the rune deck, recycle it, and return its branch.
    int revealAndRecycle(CardContext& ctx) const {
        ctx.executor.lookBeforeReveal(ctx.controller, ctx.controller, /*rune_deck=*/true);   // Void Hatchling
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.rune_deck.empty()) return kNone;
        GameObjectId rune = ps.rune_deck.back();
        if (!ctx.state.objectExists(rune)) return kNone;
        const auto& robj = ctx.state.getObject(rune);
        auto has = [&](Domain d) {
            for (auto x : robj.domains) if (x == d) return true;
            return false;
        };
        int branch = has(Domain::Fury) ? kR : has(Domain::Mind) ? kB
                   : has(Domain::Order) ? kY : kNone;
        ctx.events.emit(CardRevealedEvent{
            /*card=*/rune, /*card_def_id=*/robj.card_def_id, /*owner=*/robj.owner,
            /*revealed_to_all=*/true, /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::RuneDeck});
        ps.rune_deck.pop_back();
        ctx.executor.recycleCards(ctx.controller, {rune});
        return branch;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 200;
        d.def_id = R"RB(ogn-200-298)RB";
        d.name = R"RB(Twisted Fate, Gambler)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-200/298)RB";
        d.collector_number = 200;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Twisted Fate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, reveal the top rune of your rune deck, then recycle it. Do one of the following based on its domain:
[R] — Deal 2 to an enemy unit here and 1 to all other enemy units here.[B] — Draw 1.[Y] — Stun an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9eff1a038528c4d633196f13259670d84b74c2a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_200(CardRegistry& r) {
    r.registerCard(200, std::make_unique<TwistedFateGambler>());
}

} // namespace riftbound
