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

// "When you play me, ready a friendly legend.
//  Friendly legends have "[E]: Give a unit +3 [M] this turn.""
// The granted ability is this card's activated ability 0, handed to the
// controller's legend through the aura-granted-ability pipeline (the legend
// pays the [E]); this unit can't use it itself.
class HearteningSoul : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "A friendly legend": with several (Neeko), choose one as this resolves.
        std::vector<GameObjectId> legends;
        for (auto l : ctx.state.player(ctx.controller).legends())
            if (ctx.state.objectExists(l)) legends.push_back(l);
        if (legends.empty()) return;
        GameObjectId legend = legends.front();
        if (legends.size() > 1) {
            legend = pickTarget(ctx, "Heartening Soul: ready a friendly legend", legends);
            if (ctx.executor.hasPendingChoice()) return;
        }
        if (legend == kInvalidId || !ctx.state.objectExists(legend)) return;
        ctx.executor.readyObject(legend);
        ctx.events.logTrace("HEARTENING SOUL: ready a friendly legend");
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        a.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        a.needs_activation_time_target = true;
        return {a};
    }
    // Only a legend that was granted the ability uses it.
    bool canActivateAbilityAt(const GameState& state, PlayerId, GameObjectId source,
                              int) const override {
        return state.objectExists(source) && state.getObject(source).isLegend();
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state, PlayerId controller,
                                                    int /*ability_index*/) const override {
        return chooseableUnits(state, controller, [](const GameObject&) { return true; });
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId t = !targets.empty() ? targets[0]
            : pickTarget(ctx, "Heartening Soul (legend): give a unit +3 [M]",
                         enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (ctx.executor.hasPendingChoice()) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.giveTemporaryMight(t, 3);
        ctx.events.logTrace("HEARTENING SOUL (legend): +3 [M] to " + ctx.state.getObject(t).name);
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // "Friendly legends" — every legend in the zone (Neeko adds more).
        for (auto legend : state.player(controller).legends()) {
            if (!state.objectExists(legend)) continue;
            auto& granted = state.getObject(legend).granted_abilities;
            bool has = false;
            for (const auto& g : granted)
                if (g.source_def_id == cardDefId() && g.ability_index == 0) { has = true; break; }  // once
            if (!has) granted.push_back({cardDefId(), 0});
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1022;
        d.def_id = R"RB(rad-040-167)RB";
        d.name = R"RB(Heartening Soul)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-040/167)RB";
        d.collector_number = 40;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spirit)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.ability_text = R"RB(When you play me, ready a friendly legend.
Friendly legends have "[E]: Give a unit +3 [M] this turn.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0a5f9ab3216462356f6ddf8f172284dca6be21e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1022(CardRegistry& r) {
    r.registerCard(1022, std::make_unique<HearteningSoul>());
}

}  // namespace riftbound
