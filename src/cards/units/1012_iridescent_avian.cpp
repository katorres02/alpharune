#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[E]: [Buff] a unit. If it's already buffed, give it +2 [M] this turn instead."
class IridescentAvian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        a.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        a.needs_activation_time_target = true;
        return {a};
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state, PlayerId controller,
                                                    int /*ability_index*/) const override {
        return chooseableUnits(state, controller, [](const GameObject&) { return true; });
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId t = !targets.empty() ? targets[0]
            : pickTarget(ctx, "Iridescent Avian: buff a unit",
                         enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (ctx.executor.hasPendingChoice()) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        if (ctx.state.getObject(t).buff_count > 0) {
            ctx.executor.giveTemporaryMight(t, 2);
            ctx.events.logTrace("IRIDESCENT AVIAN: already buffed -> +2 [M] this turn");
        } else {
            ctx.executor.buffUnit(t);
            ctx.events.logTrace("IRIDESCENT AVIAN: buff");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1012;
        d.def_id = R"RB(rad-027-167)RB";
        d.name = R"RB(Iridescent Avian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-027/167)RB";
        d.collector_number = 27;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ixtal)RB", R"RB(Bird)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB([E]: [Buff] a unit. If it's already buffed, give it +2 [M] this turn instead. (To buff a unit, give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/73fde9189b222313dcf8c2abe79b38082c76961e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1012(CardRegistry& r) {
    r.registerCard(1012, std::make_unique<IridescentAvian>());
}

}  // namespace riftbound
