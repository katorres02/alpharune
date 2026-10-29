#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Explosimian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I hold, play three Bomb gear tokens to a single battlefield."
    // The battlefield is chosen as the ability resolves.
    static constexpr int kAwaitBf = 20;
    static constexpr int kDone = 21;
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    static void playBombs(CardContext& ctx, BattlefieldId bf) {
        for (int i = 0; i < 3; ++i) createBombToken(ctx, ctx.controller, bf);
        ctx.events.logTrace("EXPLOSIMIAN: hold -> 3 Bombs at " +
                            destLabel(1 + static_cast<int>(bf)));
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.battlefields.empty()) return;
        if (!ctx.state.chain.resuming.has_value()) {        // direct invocation (tests)
            playBombs(ctx, ctx.state.battlefields.front().id);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point < kAwaitBf) {
            std::vector<Intent> opts;
            for (const auto& bf : ctx.state.battlefields)
                opts.push_back(valChoice(ctx.controller, 1 + static_cast<int>(bf.id),
                                         destLabel(1 + static_cast<int>(bf.id))));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Explosimian: play three Bombs to which battlefield?");
            ri.resume_point = kAwaitBf;
            return;
        }
        ri.resume_point = kDone;
        const int d = takenValue(ctx, -1);
        for (const auto& bf : ctx.state.battlefields)
            if (1 + static_cast<int>(bf.id) == d) { playBombs(ctx, bf.id); return; }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1074;
        d.def_id = R"RB(rad-094-167)RB";
        d.name = R"RB(Explosimian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-094/167)RB";
        d.collector_number = 94;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Zaun)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 6;
        d.ability_text = R"RB(When I hold, play three Bomb gear tokens to a single battlefield. (They have [Deploy]. When each dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f2590617229f3efc078aa2429f65b85101303200-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1074(CardRegistry& r) {
    r.registerCard(1074, std::make_unique<Explosimian>());
}

}  // namespace riftbound
