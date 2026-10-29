#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When you defend here, choose a unit you control here. Give it [Tank] this
// combat." The defending player is the trigger's controller; the unit is
// chosen as the ability is finalized. [Tank] is granted until end of turn
// (giveTemporaryKeyword), which covers this combat.
class DurandMemorial : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouDefendHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> mine;
        if (auto bf = battlefieldOfCard(ctx.state, ctx.source)) {
            const BattlefieldId here = *bf;
            const PlayerId me = ctx.controller;
            mine = chooseableUnits(ctx.state, me, [here, me](const GameObject& o) {
                auto obf = o.battlefieldId();
                return o.controller == me && obf && *obf == here;
            });
        }
        const GameObjectId unit = pickTarget(ctx, "Durand Memorial: give [Tank] this combat", mine);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.executor.hasPendingChoice()) return;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        ctx.executor.giveTemporaryKeyword(unit, Keyword::Tank, 0);
        ctx.events.logTrace("DURAND MEMORIAL: " + ctx.state.getObject(unit).name + " gains [Tank]");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1120;
        d.def_id = R"RB(rad-161-167)RB";
        d.name = R"RB(Durand Memorial)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-161/167)RB";
        d.collector_number = 161;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you defend here, choose a unit you control here. Give it [Tank] this combat. (It must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b565fec4e11308f5f01416a1c914d50a1d17ca19-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1120(CardRegistry& r) {
    r.registerCard(1120, std::make_unique<DurandMemorial>());
}

}  // namespace riftbound
