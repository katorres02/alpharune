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

// "[E]: Play a Gold gear token exhausted.
//  Kill a friendly gear, [E]: Play a 1 [M] Recruit unit token.
//  Kill a friendly unit, [E]: Play a 3 [M] Mech unit token."
// The object killed as a cost is the ability's target: the engine kills it
// while paying the cost (ActivationCost::kill_friendly_gear / kill_friendly_unit).
class WhirlingTwirligig : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility gold;
        gold.cost.exhaust = true;
        ActivatedAbility recruit;
        recruit.cost.exhaust = true;
        recruit.cost.kill_friendly_gear = true;
        recruit.targets = TargetRequirements{.count = 1, .must_be_gear = true,
                                             .must_be_friendly = true};
        ActivatedAbility mech;
        mech.cost.exhaust = true;
        mech.cost.kill_friendly_unit = true;
        mech.targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                          .must_be_friendly = true};
        return {gold, recruit, mech};
    }
    // The friendly gear / unit that can be killed to pay ability 1 / 2. This
    // gear itself may pay ability 1's cost (both costs are paid together).
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state, PlayerId controller,
                                                    int ability_index) const override {
        std::vector<GameObjectId> out;
        if (ability_index != 1 && ability_index != 2) return out;
        for (const auto& [id, o] : state.objects) {
            if (o.controller != controller || !o.location.has_value()) continue;
            if (ability_index == 1 ? o.isGear() : o.isUnit()) out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        LocationId base{BaseLocation{ctx.controller}};
        switch (ability_index) {
        case 0:
            createGoldExhausted(ctx);
            ctx.events.logTrace("WHIRLING TWIRLIGIG: Gold token (exhausted)");
            break;
        case 1: {
            auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                                {"Recruit"}, KeywordSet{}, base, false);
            bindTokenCard(ctx.state, tok, 954);
            ctx.events.logTrace("WHIRLING TWIRLIGIG: killed a gear -> 1 [M] Recruit token");
            break;
        }
        case 2:
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3, {"Mech"},
                                     KeywordSet{}, base, false);
            ctx.events.logTrace("WHIRLING TWIRLIGIG: killed a unit -> 3 [M] Mech token");
            break;
        default:
            break;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1007;
        d.def_id = R"RB(rad-022-167)RB";
        d.name = R"RB(Whirling Twirligig)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-022/167)RB";
        d.collector_number = 22;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        // Gallery lists no domain (pre-release data); a [1] power cost needs
        // one, and #22 sits in the Fury block (Ekko #21, Ziggs #23).
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Epic;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.ability_text = R"RB([E]: Play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A].")
Kill a friendly gear, [E]: Play a 1 [M] Recruit unit token.
Kill a friendly unit, [E]: Play a 3 [M] Mech unit token.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a05ff95e5bb933664e09656973c72f4d39d8e701-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1007(CardRegistry& r) {
    r.registerCard(1007, std::make_unique<WhirlingTwirligig>());
}

}  // namespace riftbound
