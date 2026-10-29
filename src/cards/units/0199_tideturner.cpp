#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class Tideturner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you may choose a unit you control at another
    //  location. Move me to its location and it to my original location."
    // The "may" and the unit are chosen as the trigger is finalized.
    static std::vector<GameObjectId> elsewhere(const GameState& state, PlayerId controller,
                                               GameObjectId self) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(self)) return out;
        const auto& me = state.getObject(self);
        if (!me.location.has_value()) return out;
        for (auto& [id, obj] : state.objects) {
            if (id == self || !obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value() || *obj.location == *me.location) continue;
            out.push_back(id);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&ctx, this]() {
            return !elsewhere(ctx.state, ctx.controller, ctx.source).empty();
        };
        int conf = confirmOptional(ctx, "Tideturner: swap places with a unit elsewhere?",
                                   still_legal);
        if (conf == -1) return;
        if (conf < 1) return;                 // declined / nothing elsewhere
        GameObjectId other = pickTarget(ctx, "Tideturner: unit at another location",
                                        elsewhere(ctx.state, ctx.controller, ctx.source));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (other == kInvalidId || !ctx.state.objectExists(other)) return;
        auto my_loc  = ctx.state.getObject(ctx.source).location;
        auto tgt_loc = ctx.state.getObject(other).location;
        if (!my_loc || !tgt_loc || *my_loc == *tgt_loc) return;
        moveToLocation(ctx.executor, ctx.source, tgt_loc);
        moveToLocation(ctx.executor, other,      my_loc);
        ctx.events.logTrace("TIDETURNER: swapped locations");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 199;
        d.def_id = R"RB(ogn-199-298)RB";
        d.name = R"RB(Tideturner)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-199/298)RB";
        d.collector_number = 199;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me, you may choose a unit you control at another location. Move me to its location and it to my original location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bad355dba8b32d1fba33dc3924cad6a34b61b5af-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_199(CardRegistry& r) {
    r.registerCard(199, std::make_unique<Tideturner>());
}

} // namespace riftbound
