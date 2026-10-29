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

// "[Assault] [Tank] When I attack, you may move a unit you control to my
// battlefield." The "you may" and the unit are chosen as the ability is
// finalized (CR 383.3); the move is an effect move (movement restrictions and
// applied move costs apply, EffectExecutor::moveToBattlefield).
class JarvanIVRoyalHeir : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        const PlayerId me = ctx.controller;
        const GameObjectId self = ctx.source;
        const BattlefieldId bf = *my_bf;
        auto movable = [&]() {
            return chooseableUnits(ctx.state, me, [&](const GameObject& o) {
                if (o.id == self || o.controller != me) return false;
                const auto at = o.battlefieldId();
                return !(at.has_value() && *at == bf);
            });
        };
        int go = confirmOptional(ctx, "Jarvan IV, Royal Heir: move a unit to my battlefield?",
                                 [&]() { return !movable().empty(); });
        if (go <= 0) return;
        GameObjectId t = pickTarget(ctx, "Jarvan IV, Royal Heir: move which unit", movable());
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        const auto& u = ctx.state.getObject(t);
        if (!u.location.has_value() || u.controller != me) return;
        const auto at = u.battlefieldId();
        if (at.has_value() && *at == bf) return;
        if (ctx.executor.moveToBattlefield(t, bf))
            ctx.events.logTrace("JARVAN IV, ROYAL HEIR: moved " + u.name + " to my battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1109;
        d.def_id = R"RB(rad-133-167)RB";
        d.name = R"RB(Jarvan IV, Royal Heir)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-133/167)RB";
        d.collector_number = 133;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Jarvan IV)RB", R"RB(Demacia)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Assault] (+1 [M] while I'm attacking.)
[Tank] (I must be assigned combat damage first.)
When I attack, you may move a unit you control to my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/967de7aa18d5833896825bfd7d78b234e088ae73-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1109(CardRegistry& r) {
    r.registerCard(1109, std::make_unique<JarvanIVRoyalHeir>());
}

}  // namespace riftbound
