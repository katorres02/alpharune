#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class ArmoredTusker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I move, you may deal damage equal to my [Shield] to a unit at a
    //  battlefield." My Shield = printed + granted Shield values, read as the
    //  ability resolves.
    static int myShield(const GameObject& me) {
        int s = me.shield_value;
        for (const auto& ae : me.aura_effects)
            if (ae.keyword == Keyword::Shield) s += ae.keyword_value > 0 ? ae.keyword_value : 1;
        return s;
    }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto atBf = chooseableUnits(ctx.state, ctx.controller,
            [](const GameObject& o) { return o.isAtBattlefield(); });
        int go = confirmOptional(ctx, "Armored Tusker: deal damage equal to my Shield?",
                                 [&] { return !atBf.empty(); });
        if (go == -1) return;
        if (go == 0) { finalizingChoices(ctx); return; }
        GameObjectId t = pickTarget(ctx, "Armored Tusker: a unit at a battlefield", atBf);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        int amount = 3;   // last known Shield if I've left the board
        if (ctx.state.objectExists(ctx.source)) amount = myShield(ctx.state.getObject(ctx.source));
        if (amount <= 0) return;
        ctx.executor.dealDamage(t, amount, ctx.source);
        if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1058;
        d.def_id = R"RB(rad-078-167)RB";
        d.name = R"RB(Armored Tusker)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-078/167)RB";
        d.collector_number = 78;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Noxus)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.shield_value = 3;
        d.ability_text = R"RB([Shield 3] (+3 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
When I move, you may deal damage equal to my [Shield] to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c479f2fda00506e71a57ac4e8cc297f7f3110a54-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1058(CardRegistry& r) {
    r.registerCard(1058, std::make_unique<ArmoredTusker>());
}

}  // namespace riftbound
