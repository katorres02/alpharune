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

// "Give a unit [Assault] this turn equal to its Might." The Assault value is
// the unit's Might as the spell resolves.
class Brawl : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId t = pickTarget(ctx, "Brawl: give a unit [Assault] equal to its Might", legal);
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        const int might = ctx.state.getObject(t).current_might;
        if (might <= 0) return;
        ctx.executor.giveTemporaryKeyword(t, Keyword::Assault, might);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 989;
        d.def_id = R"RB(rad-004-167)RB";
        d.name = R"RB(Brawl)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-004/167)RB";
        d.collector_number = 4;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a unit [Assault] this turn equal to its Might. (+1 [M] for each instance of Assault while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/38fd13c7522bb47dcc5ce1596aeb6a638087ee09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_989(CardRegistry& r) {
    r.registerCard(989, std::make_unique<Brawl>());
}

}  // namespace riftbound
