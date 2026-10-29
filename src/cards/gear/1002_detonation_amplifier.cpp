#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

std::vector<GameObjectId> detonationFodder(const GameState& state, PlayerId me, GameObjectId self) {
    std::vector<GameObjectId> out;
    for (const auto& [id, o] : state.objects) {
        if (id == self || o.controller != me || !o.location.has_value()) continue;
        if (o.isUnit() || o.isGear()) out.push_back(id);
    }
    return out;
}

// "[Deploy]
//  When you play this, you may kill another friendly unit or gear to draw 1.
//  Friendly [Deathknell] effects here trigger an additional time."
// The kill is a "[cost] to [effect]" payment, decided and paid when the
// ability is put on the chain (CR 383.3). The Deathknell clause feeds
// BattlefieldState::deathknell_extra, which the trigger manager adds to the
// Deathknell fires of a friendly object that dies at this battlefield.
class DetonationAmplifier : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto fodder = detonationFodder(ctx.state, ctx.controller, ctx.source);
        if (!costAlreadyPaid(ctx) && fodder.empty()) return;
        const int conf = confirmOptional(ctx, "Detonation Amplifier: kill another friendly unit or gear to draw 1",
            [&]() { return costAlreadyPaid(ctx) ||
                           !detonationFodder(ctx.state, ctx.controller, ctx.source).empty(); });
        if (conf < 1) return;
        if (costDueNow(ctx)) {
            GameObjectId victim = pickTarget(ctx, "Detonation Amplifier: kill a friendly unit or gear", fodder);
            if (ctx.executor.hasPendingChoice()) return;
            if (victim == kInvalidId || !ctx.state.objectExists(victim) ||
                !ctx.state.getObject(victim).location.has_value()) return;
            ctx.executor.killObject(victim);
        }
        if (finalizingChoices(ctx)) return;
        ctx.executor.drawCards(ctx.controller, 1);
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            auto bf = self.battlefieldId();
            if (!bf) return;
            for (auto& b : state.battlefields)
                if (b.id == *bf) b.deathknell_extra[playerIndex(controller)] += 1;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1002;
        d.def_id = R"RB(rad-017-167)RB";
        d.name = R"RB(Detonation Amplifier)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-017/167)RB";
        d.collector_number = 17;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, you may kill another friendly unit or gear to draw 1.
Friendly [Deathknell] effects here trigger an additional time.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0cd07e77e69a53b78486b07dd7610183c953d581-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1002(CardRegistry& r) {
    r.registerCard(1002, std::make_unique<DetonationAmplifier>());
}

}  // namespace riftbound
