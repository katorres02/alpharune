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

class CullTheWeak : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Each player kills one of their units." Each player chooses one of
    // their own units (controller first, then opponent); both die together.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        auto own = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                    out.push_back(id);
            return out;
        };
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain, test path): first unit of each.
            std::vector<GameObjectId> victims;
            for (auto p : {ctx.controller, opp}) {
                auto units = own(p);
                if (!units.empty()) victims.push_back(units.front());
            }
            for (auto u : victims) ctx.executor.killObject(u);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 2) ri.resume_data.push_back(kInvalidId);
        auto ask = [&](PlayerId p, int next_point) -> bool {
            auto units = own(p);
            if (units.empty()) return false;
            std::vector<Intent> opts;
            for (auto u : units) opts.push_back(objChoice(p, u));
            ctx.executor.requestChoice(p, std::move(opts), "Cull the Weak: kill one of your units");
            ri.resume_point = next_point;
            return true;
        };
        switch (ri.resume_point) {
        case 0:
            if (ask(ctx.controller, 1)) return;
            [[fallthrough]];
        case 1:
            if (ri.resume_point == 1) ri.resume_data[0] = takenObject(ctx);
            if (ask(opp, 2)) return;
            break;
        case 2:
            ri.resume_data[1] = takenObject(ctx);
            break;
        default:
            break;
        }
        ri.resume_point = 3;
        for (int i = 0; i < 2; ++i) {
            const auto u = static_cast<GameObjectId>(ri.resume_data[i]);
            if (u != kInvalidId && ctx.state.objectExists(u) &&
                ctx.state.getObject(u).location.has_value())
                ctx.executor.killObject(u);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 209;
        d.def_id = R"RB(ogn-209-298)RB";
        d.name = R"RB(Cull the Weak)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-209/298)RB";
        d.collector_number = 209;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB(Each player kills one of their units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/49338a4b31727c6ef50e7dbc54e7004dcd2b6f4c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_209(CardRegistry& r) {
    r.registerCard(209, std::make_unique<CullTheWeak>());
}

} // namespace riftbound
