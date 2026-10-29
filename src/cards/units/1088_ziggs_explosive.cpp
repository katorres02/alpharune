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

// "[1], [E]: Play a Bomb gear token to a battlefield."
// "When I defend, you may kill a friendly gear to give me +2 [M] this turn."
class ZiggsExplosive : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger | kFinalizeActivate; }

    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1};
    }
    bool canActivateAbility(const GameState& state, PlayerId) const override {
        return !state.battlefields.empty();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const int n = static_cast<int>(ctx.state.battlefields.size());
        if (n == 0) return;
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i) labels.push_back("Bomb to battlefield #" + std::to_string(i));
        int bf = pickMode(ctx, "Ziggs, Explosive: Bomb to which battlefield?", n, labels);
        if (bf < 0) return;
        if (finalizingChoices(ctx)) return;
        if (bf >= static_cast<int>(ctx.state.battlefields.size())) return;
        createBombToken(ctx, ctx.controller, ctx.state.battlefields[bf].id);
        ctx.events.logTrace("ZIGGS, EXPLOSIVE: Bomb to battlefield #" + std::to_string(bf));
    }

    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto gear = [&]() {
            std::vector<GameObjectId> out;
            for (const auto& [id, o] : ctx.state.objects)
                if (o.isGear() && o.controller == me && o.location.has_value()) out.push_back(id);
            return out;
        };
        const bool paid = costAlreadyPaid(ctx);
        int go = confirmOptional(ctx, "Ziggs, Explosive: kill a friendly gear for +2 [M]?",
                                 [&]() { return paid || !gear().empty(); });
        if (go <= 0) return;
        GameObjectId g = pickTarget(ctx, "Ziggs, Explosive: gear to kill", gear());
        if (!paid) {
            if (g == kInvalidId) return;   // suspended, or nothing to kill
            if (costDueNow(ctx)) {
                if (!ctx.state.objectExists(g)) return;
                ctx.executor.killObject(g);
            }
        }
        if (finalizingChoices(ctx)) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.events.logTrace("ZIGGS, EXPLOSIVE: killed a gear -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1088;
        d.def_id = R"RB(rad-112-167)RB";
        d.name = R"RB(Ziggs, Explosive)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-112/167)RB";
        d.collector_number = 112;
        d.artist = R"RB(Yeon Art Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ziggs)RB", R"RB(Zaun)RB", R"RB(Yordle)RB"};   // as printed
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        d.ability_text = R"RB([1], [E]: Play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.)
When I defend, you may kill a friendly gear to give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0c1f1f3b69b31a69f9637ece841b5ee1029cdd3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1088(CardRegistry& r) {
    r.registerCard(1088, std::make_unique<ZiggsExplosive>());
}

}  // namespace riftbound
