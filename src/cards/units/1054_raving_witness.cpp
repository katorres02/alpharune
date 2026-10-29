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

class RavingWitness : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I move, reveal the top card of your Main Deck. If it's [Mighty],
    //  draw it. Otherwise, recycle it." A card in the deck is Mighty if it is a
    //  unit with 5+ printed Might.
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.lookBeforeReveal(ctx.controller, ctx.controller);   // Void Hatchling
        auto& deck = ctx.state.player(ctx.controller).main_deck;
        if (deck.empty()) return;
        const GameObjectId top = deck.back();
        const auto& obj = ctx.state.getObject(top);
        ctx.events.emit(CardRevealedEvent{top, obj.card_def_id, obj.owner, true,
                                          PlayerId::None, ZoneType::MainDeck});
        int might = obj.base_might;
        if (obj.card_def_id != kInvalidId) might = ctx.executor.cardDB().get(obj.card_def_id).might;
        const bool mighty = obj.isUnit() && might >= 5;
        ctx.events.logTrace("RAVING WITNESS: revealed " + obj.name + (mighty ? " (Mighty) -> draw" : " -> recycle"));
        if (mighty) {
            ctx.executor.drawCards(ctx.controller, 1);
        } else {
            deck.pop_back();
            ctx.executor.recycleCards(ctx.controller, {top});
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1054;
        d.def_id = R"RB(rad-074-167)RB";
        d.name = R"RB(Raving Witness)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-074/167)RB";
        d.collector_number = 74;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(When I move, reveal the top card of your Main Deck. If it's [Mighty], draw it. Otherwise, recycle it. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/778e70b353d7f28eae9b54b1a37af3853238ba04-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1054(CardRegistry& r) {
    r.registerCard(1054, std::make_unique<RavingWitness>());
}

}  // namespace riftbound
