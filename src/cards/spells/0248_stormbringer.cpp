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

class Stormbringer : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Choose a friendly unit in your base. Deal damage equal to its Might to
    // all enemy units at a battlefield, then move your unit there."
    // The unit is chosen with pickTarget; the battlefield is the player's
    // choice among all battlefields (resume points 20/21/22, resume_data[15]).
    // The unit moves even if no enemy unit is there.
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> base_units;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.isAtBase()) continue;
            base_units.push_back(id);
        }
        std::sort(base_units.begin(), base_units.end());
        GameObjectId mover = pickTarget(ctx, "Stormbringer: friendly unit in base",
                                        base_units);
        if (mover == kInvalidId) return;   // suspended, or no unit in base
        if (!ctx.state.objectExists(mover) || ctx.state.battlefields.empty()) return;

        BattlefieldId dest = ctx.state.battlefields.front().id;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_data.size() < 16) ri.resume_data.resize(16, 0);
            if (ri.resume_point < 20) {
                std::vector<Intent> opts;
                for (const auto& bf : ctx.state.battlefields)
                    opts.push_back(valChoice(ctx.controller, 1 + static_cast<int>(bf.id),
                                             destLabel(1 + static_cast<int>(bf.id))));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Stormbringer: choose a battlefield");
                ri.resume_point = 21;
                return;
            }
            if (ri.resume_point == 21) {
                ri.resume_data[15] = takenValue(ctx, 1 + static_cast<int>(dest));
                ri.resume_point = 22;
            }
            dest = static_cast<BattlefieldId>(ri.resume_data[15] - 1);
        }
        int dmg = ctx.state.getObject(mover).current_might;

        // Deal damage to all enemy units there, collect-then-kill.
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> hit;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (obj.battlefieldId() != std::optional<BattlefieldId>(dest)) continue;
            hit.push_back(id);
        }
        for (auto id : hit) ctx.executor.dealDamage(id, dmg, ctx.source);
        for (auto id : hit)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).location.has_value() &&
                ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        // Then move your unit there.
        if (ctx.state.objectExists(mover) && ctx.state.getObject(mover).location.has_value())
            ctx.executor.moveToBattlefield(mover, dest);
        ctx.events.logTrace("STORMBRINGER: dealt " + std::to_string(dmg) +
                            " to " + std::to_string(hit.size()) +
                            " enemy units at BF#" + std::to_string(dest) +
                            ", then moved my unit there");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 248;
        d.def_id = R"RB(ogn-250-298)RB";
        d.name = R"RB(Stormbringer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-250/298)RB";
        d.collector_number = 250;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Volibear)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Choose a friendly unit in your base. Deal damage equal to its Might to all enemy units at a battlefield, then move your unit there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d86c4137cc7f77f103cd7d6228125df8cb9a54e1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_248(CardRegistry& r) {
    r.registerCard(248, std::make_unique<Stormbringer>());
}

} // namespace riftbound
