#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class NavoriFightingPit : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    // "When you hold here, buff a unit here." Any unit at this battlefield.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::optional<BattlefieldId> here;
        for (const auto& bf : ctx.state.battlefields)
            if (bf.card_object_id == ctx.source) here = bf.id;
        if (!here) return;
        auto legal = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& o) { return o.battlefieldId() == here; });
        GameObjectId t = pickTarget(ctx, "Navori Fighting Pit (buff a unit here)", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId && ctx.executor.hasPendingChoice()) return;  // suspended
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.buffUnit(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 278;
        d.def_id = R"RB(ogn-283-298)RB";
        d.name = R"RB(Navori Fighting Pit)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-283/298)RB";
        d.collector_number = 283;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, buff a unit here. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c03ed3ddf8b764963e4b0745e86a12a5ebcef2a3-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_278(CardRegistry& r) {
    r.registerCard(278, std::make_unique<NavoriFightingPit>());
}

} // namespace riftbound
