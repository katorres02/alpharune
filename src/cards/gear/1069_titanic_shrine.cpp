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

class TitanicShrine : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a unit with Might greater than each other unit on the
    //  board, exhaust me to draw 1." The Might comparison is the trigger
    // condition (checked as the unit is played); "exhaust me to" is an
    // optional cost decided at finalization (CR 383.3).
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto& played = state.getObject(subject);
        if (!played.isUnit()) return false;
        for (const auto& [id, o] : state.objects) {
            if (id == subject || !o.isUnit() || !o.location.has_value()) continue;
            if (o.zone == ZoneType::Chain) continue;
            if (o.current_might >= played.current_might) return false;
        }
        return true;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() {
            return costAlreadyPaid(ctx) ||
                   (ctx.state.objectExists(ctx.source) &&
                    !ctx.state.getObject(ctx.source).is_exhausted);
        };
        const int go = confirmOptional(ctx, "Titanic Shrine: exhaust me to draw 1?", still_legal);
        if (go <= 0) return;
        if (costDueNow(ctx)) {
            if (!still_legal()) return;
            ctx.executor.exhaustObject(ctx.source);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("TITANIC SHRINE: exhausted -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1069;
        d.def_id = R"RB(rad-089-167)RB";
        d.name = R"RB(Titanic Shrine)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-089/167)RB";
        d.collector_number = 89;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.ability_text = R"RB(When you play a unit with Might greater than each other unit on the board, exhaust me to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7d027f4e30df4f7141799709f9fb785835adefed-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1069(CardRegistry& r) {
    r.registerCard(1069, std::make_unique<TitanicShrine>());
}

}  // namespace riftbound
