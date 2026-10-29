#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
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

class HighNote : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Give a unit -[M] this turn equal to the number of exhausted units you control."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        std::sort(legal.begin(), legal.end());
        GameObjectId t = pickTarget(ctx, "High Note: give a unit -[M]", legal);
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        int n = 0;
        for (const auto& [id, o] : ctx.state.objects)
            if (o.isUnit() && o.controller == ctx.controller && o.location.has_value() &&
                o.is_exhausted) ++n;
        if (n > 0) ctx.executor.giveTemporaryMight(t, -n);
        ctx.events.logTrace("HIGH NOTE: -" + std::to_string(n) + " [M] to " +
                            ctx.state.getObject(t).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1028;
        d.def_id = R"RB(rad-048-167)RB";
        d.name = R"RB(High Note)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-048/167)RB";
        d.collector_number = 48;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit –[M] this turn equal to the number of exhausted units you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/afba3669ea1063dafb0e4c2c766694e00540eb13-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1028(CardRegistry& r) {
    r.registerCard(1028, std::make_unique<HighNote>());
}

}  // namespace riftbound
