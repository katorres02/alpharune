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

// "When you play a card with Energy cost [7] or more, ready me."
// The played card (the trigger's subject) is checked when it is played: a
// cheaper card doesn't trigger this at all.
class SanctumConservator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    bool triggerConditionOnDefs(const GameState& state, const CardDB& db, GameObjectId,
                                PlayerId, TriggerType, GameObjectId subject) const override {
        return printedEnergyCost(state, db, subject) >= 7;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("SANCTUM CONSERVATOR: a [7]+ card was played -> ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1014;
        d.def_id = R"RB(rad-030-167)RB";
        d.name = R"RB(Sanctum Conservator)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-030/167)RB";
        d.collector_number = 30;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 5;
        d.ability_text = R"RB(When you play a card with Energy cost [7] or more, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/776973101613a15a7d0a2fc90587d19098ecc894-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1014(CardRegistry& r) {
    r.registerCard(1014, std::make_unique<SanctumConservator>());
}

}  // namespace riftbound
