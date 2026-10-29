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

class BlastCone : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play this, you may move an enemy unit."
    // "When you move an enemy unit, you may exhaust this to [Stun] it."
    // The second is a standing trigger (TriggerManager fires
    // WhenYouMoveAnEnemyUnit, subject = the moved unit), so it also follows
    // the first. Choices (and the exhaust cost) are made at finalization.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenYouMoveAnEnemyUnit};
    }
    static std::vector<GameObjectId> movableEnemies(const GameState& state, PlayerId controller) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller == controller) continue;
            if (!obj.location.has_value() || obj.isAtBase()) continue;  // somewhere to move from
            if (obj.untargetable_by_enemy || obj.cant_be_moved_by_enemy) continue;
            out.push_back(id);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenYouMoveAnEnemyUnit) {
            const GameObjectId moved = ctx.state.chain.resuming.has_value()
                ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
            auto still_legal = [&]() {
                if (!ctx.state.objectExists(moved)) return false;
                const auto& m = ctx.state.getObject(moved);
                if (!m.isUnit() || !m.location.has_value()) return false;
                return costAlreadyPaid(ctx) || !ctx.state.getObject(ctx.source).is_exhausted;
            };
            int conf = confirmOptional(ctx, "Blast Cone: exhaust this to stun the moved enemy?",
                                       still_legal);
            if (conf < 1) { if (finalizingChoices(ctx)) return; return; }
            if (costDueNow(ctx)) {
                if (ctx.state.getObject(ctx.source).is_exhausted) return;
                ctx.executor.exhaustObject(ctx.source);
            }
            if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
            if (!ctx.state.objectExists(moved) ||
                !ctx.state.getObject(moved).location.has_value()) return;
            ctx.executor.stunUnitBy(moved, ctx.source);
            ctx.events.logTrace("BLAST CONE: exhausted to stun the moved enemy");
            return;
        }

        // WhenYouPlayThis: "you may move an enemy unit" (to its base).
        int conf = confirmOptional(ctx, "Blast Cone: move an enemy unit?",
            [&]() { return !movableEnemies(ctx.state, ctx.controller).empty(); });
        if (conf < 1) { if (finalizingChoices(ctx)) return; return; }
        GameObjectId target = pickTarget(ctx, "Blast Cone: enemy unit to move",
                                         movableEnemies(ctx.state, ctx.controller));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (target == kInvalidId || !ctx.state.objectExists(target)) return;
        ctx.executor.moveToBase(target);
        ctx.events.logTrace("BLAST CONE: moved an enemy unit on play");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 695;
        d.def_id = R"RB(unl-133-219)RB";
        d.name = R"RB(Blast Cone)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-133/219)RB";
        d.collector_number = 133;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play this, you may move an enemy unit.
When you move an enemy unit, you may exhaust this to [Stun] it. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/093799f1ad34084f05011f3b326d4d32d96849e5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_695(CardRegistry& r) {
    r.registerCard(695, std::make_unique<BlastCone>());
}

} // namespace riftbound
