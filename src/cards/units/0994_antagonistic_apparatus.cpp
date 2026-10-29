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

// "I enter ready. When you play me, play a ready 3 [M] Mech unit token here."
class AntagonisticApparatus : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay() const override { return true; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& me = ctx.state.getObject(ctx.source);
        // "Here": where I am (or was, if I already left the board).
        std::optional<LocationId> here = me.location;
        if (!here) here = me.last_location;
        LocationId loc = here.value_or(LocationId{BaseLocation{ctx.controller}});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3, {"Mech"},
                                 KeywordSet{}, loc, /*enter_ready=*/true);
        ctx.events.logTrace("ANTAGONISTIC APPARATUS: ready 3 [M] Mech token here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 994;
        d.def_id = R"RB(rad-009-167)RB";
        d.name = R"RB(Antagonistic Apparatus)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-009/167)RB";
        d.collector_number = 9;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        // Gallery data lists no domain (pre-release); costs [6] + one power and
        // sits in the Fury block of the set, so it is Fury.
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Bandle City)RB", R"RB(Yordle)RB", R"RB(Mech)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 3;
        d.ability_text = R"RB(I enter ready.
When you play me, play a ready 3 [M] Mech unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5be775464c8b897501317c6d32fd60c8b962b5c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_994(CardRegistry& r) {
    r.registerCard(994, std::make_unique<AntagonisticApparatus>());
}

}  // namespace riftbound
