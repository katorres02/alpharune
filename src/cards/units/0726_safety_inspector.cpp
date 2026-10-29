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

// "You may spend 3 XP as an additional cost to play me.
//  When you play me, each player must kill one of their units. If you paid my
//  additional cost, you don't kill a unit this way."
//
// The 3 XP is an optional additional cost decided as I'm played
// (optionalAdditionalCost.spend_xp; the engine sets kPaidFlag). Each player
// chooses which of their own units dies: the controller first, then the
// opponent (stepChooseObject publishes the opponent's choice to them).

class SafetyInspector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    static constexpr const char* kPaidFlag = "__safety_inspector_xp_paid";
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.spend_xp = 3;
        c.paid_flag = kPaidFlag;
        return c;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        bool paid = false;
        if (ctx.state.objectExists(ctx.source)) {
            auto& me = ctx.state.getObject(ctx.source);
            auto it = me.card_counters.find(kPaidFlag);
            paid = it != me.card_counters.end() && it->second != 0;
        }
        auto own_units = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        GameObjectId mine = kInvalidId, theirs = kInvalidId;
        if (!paid &&
            !stepChooseObject(ctx, ctx.controller, own_units(ctx.controller),
                              "Safety Inspector: kill one of your units", 20, 10, mine))
            return;
        const PlayerId opp = opponent(ctx.controller);
        if (!stepChooseObject(ctx, opp, own_units(opp),
                              "Safety Inspector: kill one of your units", 22, 11, theirs))
            return;
        if (mine != kInvalidId && ctx.state.objectExists(mine) &&
            ctx.state.getObject(mine).location.has_value())
            ctx.executor.killObject(mine);
        if (theirs != kInvalidId && ctx.state.objectExists(theirs) &&
            ctx.state.getObject(theirs).location.has_value())
            ctx.executor.killObject(theirs);
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters.erase(kPaidFlag);
        ctx.events.logTrace(std::string("SAFETY INSPECTOR: each player kills a unit") +
                            (paid ? " (paid 3 XP: not the controller)" : ""));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 726;
        d.def_id = R"RB(unl-164-219)RB";
        d.name = R"RB(Safety Inspector)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-164/219)RB";
        d.collector_number = 164;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(You may spend 3 XP as an additional cost to play me.
When you play me, each player must kill one of their units. If you paid my additional cost, you don't kill a unit this way.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/499e65eb2d35e2bd774ea7fe2a70672234f71e90-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_726(CardRegistry& r) {
    r.registerCard(726, std::make_unique<SafetyInspector>());
}

} // namespace riftbound
