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

class AspiringEngineer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, return a gear from your trash to your hand." The
    // player picks the gear as the ability resolves (resume_point 20).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> gears;
        for (auto id : ps.trash)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).isGear()) gears.push_back(id);
        if (gears.empty()) return;
        GameObjectId picked = gears.front();
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point < 20) {
                std::vector<Intent> opts;
                for (auto id : gears) opts.push_back(objChoice(ctx.controller, id));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Aspiring Engineer: return a gear from trash to hand");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point != 20) return;
            ri.resume_point = 21;
            auto t = takenObject(ctx);
            if (std::find(gears.begin(), gears.end(), t) != gears.end()) picked = t;
        }
        ps.trash.erase(std::find(ps.trash.begin(), ps.trash.end(), picked));
        auto& g = ctx.state.getObject(picked);
        g.zone = ZoneType::Hand;
        g.location = std::nullopt;
        ps.hand.push_back(picked);
        ctx.events.logTrace("ASPIRING ENGINEER: returned " + g.name + " from trash to hand");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 384;
        d.def_id = R"RB(sfd-061-221)RB";
        d.name = R"RB(Aspiring Engineer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-061/221)RB";
        d.collector_number = 61;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.ability_text = R"RB(When you play me, return a gear from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5d61995ae384bcee56838beb0eb6d83bf5faefad-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_384(CardRegistry& r) {
    r.registerCard(384, std::make_unique<AspiringEngineer>());
}

} // namespace riftbound
