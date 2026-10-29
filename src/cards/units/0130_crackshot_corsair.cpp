#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class CrackshotCorsair : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // Triggers carry no targets: the enemy unit at my battlefield is chosen as
    // the ability is finalized.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::optional<BattlefieldId> here;
        if (ctx.state.objectExists(ctx.source))
            here = ctx.state.getObject(ctx.source).battlefieldId();
        std::vector<GameObjectId> legal;
        if (here)
            legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller != ctx.controller && u.battlefieldId() == here;
            });
        GameObjectId t = pickTarget(ctx, "Crackshot Corsair: deal 1", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        const int dmg = 1;
        ctx.executor.dealDamage(t, dmg, ctx.source);
        if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 130;
        d.def_id = R"RB(ogn-130-298)RB";
        d.name = R"RB(Crackshot Corsair)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-130/298)RB";
        d.collector_number = 130;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When I attack, deal 1 to an enemy unit here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9106ab084e02d7a489cb351e98112e5833b0bb11-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_130(CardRegistry& r) {
    r.registerCard(130, std::make_unique<CrackshotCorsair>());
}

} // namespace riftbound
