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

class JayceManOfProgress : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    std::vector<GameObjectId> friendlyGears(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            out.push_back(id);
        }
        return out;
    }

    // "When you play me, you may kill a friendly gear. If you do, you may play
    // a gear with Energy cost no more than [7] from hand this turn, ignoring
    // its Energy cost. (You must still pay its Power cost.)"
    // The "may" and the gear to kill are chosen at finalization; the kill
    // happens as the ability resolves and leaves a permission that lasts the
    // turn (PlayerState::ignore_energy_play_grants): the next such gear played
    // from hand may use it, still paying its Power cost.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int conf = confirmOptional(ctx, "Jayce: kill a friendly gear?",
                                   [&]() { return !friendlyGears(ctx).empty(); });
        if (conf == -1) return;
        if (conf == 0) { finalizingChoices(ctx); return; }
        GameObjectId gear = pickTarget(ctx, "Jayce: kill a friendly gear", friendlyGears(ctx));
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        ctx.executor.killObject(gear);
        ctx.events.logTrace("JAYCE: killed a friendly gear");
        PlayerState::IgnoreEnergyPlayGrant g;
        g.source = ctx.source;
        g.card_type = CardType::Gear;
        g.max_energy_cost = 7;
        ctx.state.player(ctx.controller).ignore_energy_play_grants.push_back(g);
        ctx.events.logTrace("JAYCE: a gear (Energy cost <= 7) may be played from hand this turn "
                            "ignoring its Energy cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 406;
        d.def_id = R"RB(sfd-084-221)RB";
        d.name = R"RB(Jayce, Man of Progress)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-084/221)RB";
        d.collector_number = 84;
        d.artist = R"RB(TUTU)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Jayce)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may kill a friendly gear. If you do, you may play a gear with Energy cost no more than [7] from hand this turn, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b508fa4e52ebbd5f66e1fc7df28cfb70acdfe1f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_406(CardRegistry& r) {
    r.registerCard(406, std::make_unique<JayceManOfProgress>());
}

} // namespace riftbound
