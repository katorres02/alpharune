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

class StalwartGoatherd : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play me, you may kill a gear with Energy cost [4] or less."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> gears;
        for (const auto& [id, o] : ctx.state.objects) {
            if (!o.isGear() || !o.location.has_value()) continue;
            if (o.controller != ctx.controller && o.untargetable_by_enemy) continue;
            const int cost = o.card_def_id != kInvalidId
                ? ctx.executor.cardDB().get(o.card_def_id).energy_cost : 0;
            if (cost <= 4) gears.push_back(id);
        }
        std::sort(gears.begin(), gears.end());
        int go = confirmOptional(ctx, "Stalwart Goatherd: kill a gear?", [&] { return !gears.empty(); });
        if (go == -1) return;
        if (go == 0) { finalizingChoices(ctx); return; }
        GameObjectId t = pickTarget(ctx, "Stalwart Goatherd: a gear with cost 4 or less", gears);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1063;
        d.def_id = R"RB(rad-083-167)RB";
        d.name = R"RB(Stalwart Goatherd)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-083/167)RB";
        d.collector_number = 83;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Mount Targon)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 2;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 2;
        d.ability_text = R"RB([Shield 2] (+2 [M] while I'm a defender.)
When you play me, you may kill a gear with Energy cost [4] or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bff85727b58017cbd3921fd73e9b363f5a0205e4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1063(CardRegistry& r) {
    r.registerCard(1063, std::make_unique<StalwartGoatherd>());
}

}  // namespace riftbound
