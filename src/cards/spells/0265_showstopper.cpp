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

class Showstopper : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Buff a friendly unit in your base, then move it to a battlefield."
    // The unit is the spell's target (friendly, in base); the battlefield is
    // chosen as it resolves (resume points 20/21/22, resume_data[15]).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == controller && obj.isAtBase())
                out.push_back(id);
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId unit = targets[0];
        const auto& u = ctx.state.getObject(unit);
        if (!u.isUnit() || u.controller != ctx.controller || !u.isAtBase()) return;  // mistarget
        if (ctx.state.battlefields.empty()) return;
        BattlefieldId dest = ctx.state.battlefields.front().id;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_data.size() < 16) ri.resume_data.resize(16, 0);
            if (ri.resume_point < 20) {
                std::vector<Intent> opts;
                for (const auto& bf : ctx.state.battlefields)
                    opts.push_back(valChoice(ctx.controller, 1 + static_cast<int>(bf.id),
                                             destLabel(1 + static_cast<int>(bf.id))));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Showstopper: move it to which battlefield");
                ri.resume_point = 21;
                return;
            }
            if (ri.resume_point == 21) {
                ri.resume_data[15] = takenValue(ctx, 1 + static_cast<int>(dest));
                ri.resume_point = 22;
            }
            dest = static_cast<BattlefieldId>(ri.resume_data[15] - 1);
        }
        ctx.executor.buffUnit(unit);  // "+1 [M] buff if it doesn't have one"
        ctx.executor.moveToBattlefield(unit, dest);
        ctx.events.logTrace("SHOWSTOPPER: buff base unit, move to BF#" + std::to_string(dest));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 265;
        d.def_id = R"RB(ogn-270-298)RB";
        d.name = R"RB(Showstopper)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-270/298)RB";
        d.collector_number = 270;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Sett)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Buff a friendly unit in your base, then move it to a battlefield. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c9b6f7a7cca1589fb53276f74ac8bc547b31e5ec-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_265(CardRegistry& r) {
    r.registerCard(265, std::make_unique<Showstopper>());
}

} // namespace riftbound
