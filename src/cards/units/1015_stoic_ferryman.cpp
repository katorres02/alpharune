#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Shield 3]. When you play me to a battlefield, you may move another unit
//  you control here to your base."
class StoicFerryman : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType, GameObjectId) const override {
        return state.objectExists(source) && state.getObject(source).isAtBattlefield();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto others = [&ctx]() {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(ctx.source)) return out;
            const auto& me = ctx.state.getObject(ctx.source);
            auto bf = me.battlefieldId();
            if (!bf) return out;
            for (const auto& [id, o] : ctx.state.objects) {
                if (id == ctx.source || !o.isUnit() || o.controller != ctx.controller) continue;
                auto obf = o.battlefieldId();
                if (obf && *obf == *bf) out.push_back(id);
            }
            return out;
        };
        int go = confirmOptional(ctx, "Stoic Ferryman: move another unit here to your base?",
                                 [&]() { return !others().empty(); });
        if (go == -1) return;
        if (go == 0) { finalizingChoices(ctx); return; }
        GameObjectId t = pickTarget(ctx, "Stoic Ferryman: move a unit to base", others());
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.moveToBase(t);
        ctx.events.logTrace("STOIC FERRYMAN: moved " + ctx.state.getObject(t).name + " to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1015;
        d.def_id = R"RB(rad-031-167)RB";
        d.name = R"RB(Stoic Ferryman)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-031/167)RB";
        d.collector_number = 31;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bilgewater)RB", R"RB(Sentinel)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 2;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 3;
        d.ability_text = R"RB([Shield 3] (+3 [M] while I'm a defender.)
When you play me to a battlefield, you may move another unit you control here to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8513fbe02a37f957b2a98bcee5badfdc56f41e82-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1015(CardRegistry& r) {
    r.registerCard(1015, std::make_unique<StoicFerryman>());
}

}  // namespace riftbound
