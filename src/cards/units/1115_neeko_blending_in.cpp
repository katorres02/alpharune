#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "If Neeko is in your deck, choose up to 3 different legends in addition to
//  your starting legend. When you play me, choose one of those Legends and add
//  it to your Legend Zone while I'm on the board." (errata)
//
// The deck-building choice is CardDef::additional_legend_choices: the deck
// list declares the legends ("Neeko Legends:"), DeckValidator checks them and
// setup stores them in PlayerState::additional_legend_pool. When the play
// trigger resolves the controller picks one (Radiance patch notes: chosen as it
// resolves). A legend already in the Legend Zone can't be chosen again ("at most
// one legend on the board per legend chosen during deckbuilding"). The legend
// enters ready and ceases to exist when Neeko leaves the board
// (EffectExecutor::removeUnboundLegends, CR 107.4.d.2).
class NeekoBlendingIn : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto& ps = ctx.state.player(ctx.controller);
        const auto pool = ps.additional_legend_pool;
        if (pool.empty()) return;
        const int n = static_cast<int>(std::min<size_t>(pool.size(), 32));
        uint32_t legal = 0;
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i) {
            const auto& name = ctx.executor.cardDB().get(pool[i]).name;
            labels.push_back(name);
            bool present = false;
            for (auto lid : ps.legends())
                if (ctx.state.objectExists(lid) && ctx.state.getObject(lid).name == name)
                    present = true;
            if (!present) legal |= (1u << i);
        }
        int mode = pickMode(ctx, "Neeko, Blending In: add a legend to your Legend Zone",
                            n, labels, legal);
        if (mode < 0) return;   // waiting on the agent, or no legend left to add
        // "While I'm on the board": once Neeko has left, the duration is over.
        if (!ctx.state.objectExists(ctx.source) ||
            !ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.addLegendToZone(ctx.controller, pool[mode], ctx.source);
        ctx.events.logTrace("NEEKO, BLENDING IN: added " + labels[mode] + " to the Legend Zone");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1115;
        d.def_id = R"RB(rad-167-167)RB";
        d.name = R"RB(Neeko, Blending In)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-167/167)RB";
        d.collector_number = 167;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {};   // Neutral
        d.tags = {R"RB(Neeko)RB", R"RB(Ixtal)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.additional_legend_choices = 3;
        d.ability_text = R"RB((Neutral cards can go in decks of any Domain.)
If Neeko is in your deck, choose up to 3 different legends in addition to your starting legend. (Different means none match names.)
When you play me, choose one of those Legends and add it to your Legend Zone while I'm on the board.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/607da6f534ce1b960ad02586ed4cd47c996e2c42-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1115(CardRegistry& r) {
    r.registerCard(1115, std::make_unique<NeekoBlendingIn>());
}

}  // namespace riftbound
