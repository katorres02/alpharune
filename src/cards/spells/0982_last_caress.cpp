#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class LastCaress : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "The next unit you play this turn, even from face down, can be played to
    // any battlefield. When you play it, [Buff] it."
    // PlayerState::next_unit_any_battlefield opens every battlefield to the
    // next unit played from hand (cleared when a unit is played); the buff is a
    // delayed "when you play a unit" ability whose subject is that unit.
    // GAP: a face-down card is still played at the battlefield it was hidden
    // at — the engine's face-down play has no location choice — though it does
    // count as the next unit and gets the buff.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).next_unit_any_battlefield = true;
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenYouPlayAUnit;
        da.expires_on_turn = ctx.state.turn.turn_number;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("LAST CARESS: next unit may be played to any battlefield");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& ri = ctx.state.chain.resuming;
        const GameObjectId unit = ri.has_value() ? ri->triggering_subject : kInvalidId;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        const auto& u = ctx.state.getObject(unit);
        if (!u.isUnit() || !u.location.has_value()) return;
        ctx.executor.buffUnit(unit);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 982;
        d.def_id = R"RB(rad-154-167)RB";
        d.name = R"RB(Last Caress)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-154/167)RB";
        d.collector_number = 154;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body,Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] The next unit you play this turn, even from face down, can be played to any battlefield. When you play it, [Buff] it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c34881b6fe2e63f6345845fe95cfe72eb9d4878c-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_982(CardRegistry& r) {
    r.registerCard(982, std::make_unique<LastCaress>());
}

}  // namespace riftbound
