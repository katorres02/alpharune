#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When you conquer here, you may discard 1 and channel 1 rune exhausted."
// The "you may" opens the effect, so it is decided as the ability is
// finalized (CR 383.3.a). Discarding is part of the effect, not a cost: the
// card is chosen on resolution, and with an empty hand the rune is still
// channeled (CR 055: do as much as you can).
class RuinedMonastery : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const int go = confirmOptional(ctx, "Ruined Monastery: discard 1 and channel 1 rune exhausted?",
                                       []() { return true; });
        if (go <= 0) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        auto& ps = ctx.state.player(ctx.controller);
        GameObjectId discard = kInvalidId;
        if (!stepChooseObject(ctx, ctx.controller, ps.hand, "Ruined Monastery: discard 1",
                              20, 10, discard))
            return;
        if (discard != kInvalidId) ctx.executor.applyDiscard(ctx.controller, discard);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("RUINED MONASTERY: discard 1, channel 1 rune exhausted");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1123;
        d.def_id = R"RB(rad-166-167)RB";
        d.name = R"RB(Ruined Monastery)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-166/167)RB";
        d.collector_number = 166;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may discard 1 and channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2d756c5198dafb151f51cceca4f11824575960cd-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1123(CardRegistry& r) {
    r.registerCard(1123, std::make_unique<RuinedMonastery>());
}

}  // namespace riftbound
