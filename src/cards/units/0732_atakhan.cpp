#include "cards/card.h"
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

class Atakhan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ([Ganking] engine-handled.)
    //
    // "You may kill a friendly unit as an additional cost to play me. If you
    //  do, I cost [1] less for each Energy it costs and [Y] less for each Power
    //  it costs." Decided by the engine before payment (an optional object
    //  cost whose discount is the killed unit's printed cost).
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.object_cost = OptionalAdditionalCost::ObjectCost::KillFriendlyUnit;
        c.reduce_by_object_cost = true;
        return c;
    }
    //
    // "When I attack, the defender must kill one of their units here." The
    // defender chooses which (stepChooseObject publishes the choice to them).
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        PlayerId defender = opponent(ctx.controller);
        std::vector<GameObjectId> theirs;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != defender) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *my_bf) theirs.push_back(id);   // "here"
        }
        std::sort(theirs.begin(), theirs.end());
        GameObjectId victim = kInvalidId;
        if (!stepChooseObject(ctx, defender, theirs,
                              "Atakhan: kill one of your units here", 20, 10, victim))
            return;
        if (victim != kInvalidId && ctx.state.objectExists(victim) &&
            ctx.state.getObject(victim).location.has_value()) {
            ctx.executor.killObject(victim);
            ctx.events.logTrace("ATAKHAN: the defender kills one of their units here");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 732;
        d.def_id = R"RB(unl-170-219)RB";
        d.name = R"RB(Atakhan)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-170/219)RB";
        d.collector_number = 170;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(Demon)RB"};
        d.energy_cost = 10;
        d.power_cost = 3;
        d.might = 7;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(You may kill a friendly unit as an additional cost to play me. If you do, I cost [1] less for each Energy it costs and [Y] less for each Power it costs.
[Ganking] (I can move from battlefield to battlefield.)
When I attack, the defender must kill one of their units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0b3994aad6b6c64ace14193c1363713e6b6ede2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_732(CardRegistry& r) {
    r.registerCard(732, std::make_unique<Atakhan>());
}

} // namespace riftbound
