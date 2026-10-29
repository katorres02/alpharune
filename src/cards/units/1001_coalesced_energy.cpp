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

// "At the end of your turn, recycle all your runes, then channel 4 runes
// exhausted."
class CoalescedEnergy : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> runes;
        for (const auto& [id, o] : ctx.state.objects)
            if (o.isRune() && o.controller == ctx.controller && o.location.has_value())
                runes.push_back(id);
        if (!runes.empty()) ctx.executor.recycleCards(ctx.controller, runes);
        ctx.executor.channelRunes(ctx.controller, 4, /*enter_exhausted=*/true);
        ctx.events.logTrace("COALESCED ENERGY: recycled " + std::to_string(runes.size()) +
                            " runes, channelled 4 exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1001;
        d.def_id = R"RB(rad-016-167)RB";
        d.name = R"RB(Coalesced Energy)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-016/167)RB";
        d.collector_number = 16;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shurima)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 4;
        d.ability_text = R"RB(At the end of your turn, recycle all your runes, then channel 4 runes exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/59e551045ca75d932ff76c4820d873d4a2cde9e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1001(CardRegistry& r) {
    r.registerCard(1001, std::make_unique<CoalescedEnergy>());
}

}  // namespace riftbound
