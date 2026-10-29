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

// "Your units have +2 [M]. When you play a unit, kill this."
class EphemeralWard : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject&) {
            for (auto& [id, o] : state.objects)
                if (o.isUnit() && o.controller == controller && o.location.has_value())
                    auraMight(o, sid, 2);
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location)
            return;
        ctx.executor.killObject(ctx.source);
        ctx.events.logTrace("EPHEMERAL WARD: a unit was played -> kill this");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1021;
        d.def_id = R"RB(rad-039-167)RB";
        d.name = R"RB(Ephemeral Ward)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-039/167)RB";
        d.collector_number = 39;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 2;
        d.ability_text = R"RB(Your units have +2 [M].
When you play a unit, kill this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d2e562a1b69a42ebd9c8f95a9420750f45aca5dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1021(CardRegistry& r) {
    r.registerCard(1021, std::make_unique<EphemeralWard>());
}

}  // namespace riftbound
