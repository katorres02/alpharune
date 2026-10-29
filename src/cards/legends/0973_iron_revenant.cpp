#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class IronRevenant : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Reaction][>] Banish a unit or gear from your trash, [E]: [Add] [A].
    // Spend this Power only to play units or gear."
    // The banish cost is chosen and paid as the ability is played (CR 355).
    // The [A] is restricted to playing units or gear (RunePool::restricted).
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        return !banishable(state, controller).empty();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!costAlreadyPaid(ctx)) {
            GameObjectId card = pickTarget(ctx, "Iron Revenant: banish a unit or gear from your trash",
                                           banishable(ctx.state, ctx.controller));
            if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
            if (card == kInvalidId) return;                // the cost can't be paid
            if (costDueNow(ctx)) ctx.executor.banishObject(card);
        }
        if (finalizingChoices(ctx)) return;
        ctx.executor.addRestrictedResource(ctx.controller, /*power=*/true, 1,
                                           SpendPurpose::PlayUnit | SpendPurpose::PlayGear);
        ctx.events.logTrace("IRON REVENANT: [Add] [A] (units or gear only)");
    }
private:
    static std::vector<GameObjectId> banishable(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (auto id : state.player(player).trash) {
            if (!state.objectExists(id)) continue;
            const auto& o = state.getObject(id);
            if (o.isUnit() || o.isGear()) out.push_back(id);
        }
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 973;
        d.def_id = R"RB(rad-143-167)RB";
        d.name = R"RB(Iron Revenant)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-143/167)RB";
        d.collector_number = 143;
        d.artist = R"RB(TUTU)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury,Domain::Order};
        d.tags = {R"RB(Mordekaiser)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([Reaction][>] Banish a unit or gear from your trash, [E]: [Add] [A]. Spend this Power only to play units or gear. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/90ec3319518771820909592eee963d34dab037bc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_973(CardRegistry& r) {
    r.registerCard(973, std::make_unique<IronRevenant>());
}

}  // namespace riftbound
