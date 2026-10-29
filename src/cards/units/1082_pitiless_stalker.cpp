#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// Gallery data has no domain for this card (pre-release record); the printed
// card is Chaos.
class PitilessStalker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[E]: [Stun] an enemy unit." The target is chosen as the ability is
    // activated (needs_activation_time_target).
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0, ctx.source);
        GameObjectId t = pickTarget(ctx, "Pitiless Stalker: stun an enemy unit", legal);
        if (ctx.executor.hasPendingChoice()) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.stunUnitBy(t, ctx.source);
        ctx.events.logTrace("PITILESS STALKER: stun " + ctx.state.getObject(t).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1082;
        d.def_id = R"RB(rad-104-167)RB";
        d.name = R"RB(Pitiless Stalker)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-104/167)RB";
        d.collector_number = 104;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Cat)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB([E]: [Stun] an enemy unit. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc61b244fb3ed01d61617c3c2633a71424604130-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1082(CardRegistry& r) {
    r.registerCard(1082, std::make_unique<PitilessStalker>());
}

}  // namespace riftbound
