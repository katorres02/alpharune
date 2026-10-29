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

class ViPeacekeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, [Stun] an enemy unit here." The target is chosen as the
    // ability is put on the chain (CR 383.3), among enemy units at my
    // battlefield.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto here = ctx.state.getObject(ctx.source).battlefieldId();
        const PlayerId mine = ctx.controller;
        std::vector<GameObjectId> enemies;
        if (here) {
            enemies = chooseableUnits(ctx.state, mine, [&](const GameObject& o) {
                auto bf = o.battlefieldId();
                return o.controller != mine && bf && *bf == *here;
            });
            std::sort(enemies.begin(), enemies.end());
        }
        GameObjectId t = pickTarget(ctx, "Vi, Peacekeeper: stun an enemy unit here", enemies);
        if (t == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting on the agent
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.stunUnitBy(t, ctx.source);
        ctx.events.logTrace("VI: stun on attack");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 738;
        d.def_id = R"RB(unl-176-219)RB";
        d.name = R"RB(Vi, Peacekeeper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-176/219)RB";
        d.collector_number = 176;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Vi)RB", R"RB(Piltover)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When I attack, [Stun] an enemy unit here. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/51610bbdecd77b15f58b9a968611e536ebdf445e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_738(CardRegistry& r) {
    r.registerCard(738, std::make_unique<ViPeacekeeper>());
}

} // namespace riftbound
