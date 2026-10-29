#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When you play me, play a Bomb gear token to a battlefield.
//  Abilities of friendly gear deal 1 Bonus Damage."
class ZiggsShortFused : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // The battlefield is picked as the ability resolves (resume points 20/21).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.battlefields.empty()) return;
        if (!ctx.state.chain.resuming.has_value()) {   // direct invocation
            createBombToken(ctx, ctx.controller, ctx.state.battlefields.front().id);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point < 20) {
            std::vector<Intent> opts;
            for (const auto& bf : ctx.state.battlefields)
                opts.push_back(valChoice(ctx.controller, static_cast<int>(bf.id),
                                         "Battlefield #" + std::to_string(bf.id)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Ziggs, Short-Fused: play a Bomb to a battlefield");
            ri.resume_point = 21;
            return;
        }
        if (ri.resume_point != 21) return;
        ri.resume_point = 22;
        const int picked = takenValue(ctx, static_cast<int>(ctx.state.battlefields.front().id));
        BattlefieldId bf = ctx.state.battlefields.front().id;
        for (const auto& b : ctx.state.battlefields)
            if (static_cast<int>(b.id) == picked) bf = b.id;
        createBombToken(ctx, ctx.controller, bf);
        ctx.events.logTrace("ZIGGS, SHORT-FUSED: Bomb token to battlefield #" + std::to_string(bf));
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject&) {
            state.player(controller).gear_ability_bonus_damage += 1;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1008;
        d.def_id = R"RB(rad-023-167)RB";
        d.name = R"RB(Ziggs, Short-Fused)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-023/167)RB";
        d.collector_number = 23;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ziggs)RB", R"RB(Zaun)RB", R"RB(Yordle)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB(When you play me, play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.)
Abilities of friendly gear deal 1 Bonus Damage.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9df0d347bcfc0593cb724258ee07a1208c620848-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1008(CardRegistry& r) {
    r.registerCard(1008, std::make_unique<ZiggsShortFused>());
}

}  // namespace riftbound
