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

// "This costs [2] less if you choose a Bird, Cat, Dog, or Poro.
//  Play a unit with cost no more than [2] and no more than [A] from your
//  trash, ignoring its cost."
//
// The unit is chosen as the spell is played (needsPlayTimeTarget: the engine
// asks before paying, CR 355), so the discount follows the actual choice:
// selfCostReduction offers [2] off while a qualifying tagged unit is in the
// trash (so the play is offered when only the discounted price is payable) and
// targetCostAdjustment takes it back when the chosen unit isn't tagged. When
// only the discounted price is payable, only tagged units are offered.

// Printed costs of cards in a zone the GameState can't price (the trash): the
// card classes own their CardDefs, so ask a registry of them, built once.
const CardDef* printedDef_730(CardDefId id) {
    static const CardRegistry* reg = [] {
        auto* r = new CardRegistry();
        r->loadAll();
        return r;
    }();
    return id == kInvalidId ? nullptr : reg->classDef(id);
}

class UndyingLoyalty : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }

    static bool eligible(const GameObject& obj) {
        if (!obj.isUnit() || obj.isToken()) return false;
        const CardDef* d = printedDef_730(obj.card_def_id);
        return d && d->energy_cost <= 2 && d->power_cost <= 1;
    }
    static bool tagged(const GameObject& obj) {
        return hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
               hasTag(obj, "Dog") || hasTag(obj, "Poro");
    }
    static std::vector<GameObjectId> eligibleInTrash(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (auto cid : state.player(player).trash)
            if (state.objectExists(cid) && eligible(state.getObject(cid))) out.push_back(cid);
        return out;
    }
    static bool taggedAvailable(const GameState& state, PlayerId player) {
        for (auto cid : eligibleInTrash(state, player))
            if (tagged(state.getObject(cid))) return true;
        return false;
    }

    // Targets offered as the spell is played.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        auto out = eligibleInTrash(state, controller);
        // Undiscounted price not payable: only a discount-giving choice is.
        if (taggedAvailable(state, controller) &&
            payableEnergy(state, controller) < def_.energy_cost) {
            out.erase(std::remove_if(out.begin(), out.end(), [&](GameObjectId id) {
                return !tagged(state.getObject(id));
            }), out.end());
        }
        return out;
    }

    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return taggedAvailable(state, player) ? 2 : 0;
    }
    int targetCostAdjustment(const GameState& state, PlayerId player,
                             const std::vector<GameObjectId>& targets) const override {
        if (!taggedAvailable(state, player)) return 0;   // no discount was offered
        const bool chose_tagged = !targets.empty() && state.objectExists(targets[0]) &&
                                  tagged(state.getObject(targets[0]));
        return chose_tagged ? 0 : 2;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Legal at resolution: any eligible unit still in the trash (the cost
        // is already paid, so no affordability filter).
        auto legal = eligibleInTrash(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Undying Loyalty (unit from trash)", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        LocationId where;   // base or a battlefield I control (CR 355.2)
        if (pickPlayLocation(ctx, picked, where, PlayerId::None, "Undying Loyalty: play it where?") <= 0)
            return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked, where);
        ctx.events.logTrace("UNDYING LOYALTY: played a unit from trash, ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 730;
        d.def_id = R"RB(unl-168-219)RB";
        d.name = R"RB(Undying Loyalty)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-168/219)RB";
        d.collector_number = 168;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(This costs [2] less if you choose a Bird, Cat, Dog, or Poro.
Play a unit with cost no more than [2] and no more than [A] from your trash, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/703b85f8284ed13865012289d03602e2cd24f4b2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_730(CardRegistry& r) {
    r.registerCard(730, std::make_unique<UndyingLoyalty>());
}

} // namespace riftbound
