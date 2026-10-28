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
#include <vector>

namespace riftbound {
namespace {

class AnOfferYouCantRefuse : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_gear = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        PlayerId owner_ctl = ctx.state.getObject(targets[0]).controller;
        ctx.executor.killObject(targets[0]);
        CardContext c2 = ctx; c2.controller = owner_ctl;
        createGoldExhausted(c2);
        ctx.events.logTrace("AN OFFER YOU CAN'T REFUSE: gear killed, controller gets Gold");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 965;
        d.def_id = R"RB(rad-101-167)RB";
        d.name = R"RB(An Offer You Can't Refuse)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-101/167)RB";
        d.collector_number = 101;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([Action] Kill a gear. Its controller plays a Gold gear token exhausted.)RB";
        d.energy_cost = 2;
        d.power_cost = 0;
        d.keywords = []{KeywordSet k; k.set(Keyword::Action); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_965(CardRegistry& r) {
    r.registerCard(965, std::make_unique<AnOfferYouCantRefuse>());
}

}  // namespace riftbound
