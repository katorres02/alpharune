#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class SpiritOfTheForge : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "You may return a friendly gear to its owner's hand as an additional cost
    // to play me." The engine asks which gear (or none) during the play and
    // records the gear's Energy cost in card_counters["__oac_gear_energy"].
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.return_friendly_gear = true;
        c.paid_flag = "__sotf_paid";
        return c;
    }
    // "When you play me, if you paid the additional cost, ready me and give me
    // +[M] this turn equal to the Energy cost of the gear you returned."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        if (me.card_counters["__sotf_paid"] != 1) return;
        const int x = me.card_counters["__oac_gear_energy"];
        me.card_counters["__sotf_paid"] = 0;
        if (!me.location.has_value()) return;
        ctx.executor.readyObject(ctx.source);
        if (x > 0) ctx.executor.giveTemporaryMight(ctx.source, x);
        ctx.events.logTrace("SPIRIT OF THE FORGE: ready me, +" + std::to_string(x) + " [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1046;
        d.def_id = R"RB(rad-066-167)RB";
        d.name = R"RB(Spirit of the Forge)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-066/167)RB";
        d.collector_number = 66;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Freljord)RB", R"RB(Spirit)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(You may return a friendly gear to its owner's hand as an additional cost to play me.
When you play me, if you paid the additional cost, ready me and give me +[M] this turn equal to the Energy cost of the gear you returned.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7a1f527e5910fbc18f7cb3cddf2aa59b483393f4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1046(CardRegistry& r) {
    r.registerCard(1046, std::make_unique<SpiritOfTheForge>());
}

}  // namespace riftbound
