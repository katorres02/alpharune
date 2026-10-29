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

    // Hand gear with Energy cost no more than [7] whose Power cost can be paid.
    std::vector<GameObjectId> eligibleHandGears(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto cid : ctx.state.player(ctx.controller).hand) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (!obj.isGear() || obj.card_def_id == kInvalidId) continue;
            if (ctx.executor.cardDB().get(obj.card_def_id).energy_cost > 7) continue;
            if (!ctx.executor.canPlayPayingCost(ctx.controller, cid, {.ignore_energy = true}))
                continue;
            out.push_back(cid);
        }
        return out;
    }

    // "When you play me, you may kill a friendly gear. If you do, you may play
    // a gear with Energy cost no more than [7] from hand this turn, ignoring
    // its Energy cost. (You must still pay its Power cost.)"
    // The "may" and the gear to kill are chosen at finalization; the kill
    // happens once as the ability resolves; then the player may pick a hand
    // gear (or "Don't") — resume_point 20 consumes that pick.
    // GAP: the play is offered right away, not as a permission that lasts
    // for the rest of the turn.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int conf = confirmOptional(ctx, "Jayce: kill a friendly gear?",
                                   [&]() { return !friendlyGears(ctx).empty(); });
        if (conf == -1) return;
        if (conf == 0) { finalizingChoices(ctx); return; }
        GameObjectId gear = pickTarget(ctx, "Jayce: kill a friendly gear", friendlyGears(ctx));
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)

        if (!ctx.state.chain.resuming.has_value()) {   // direct invocation
            if (gear == kInvalidId) return;
            ctx.executor.killObject(gear);
            auto e = eligibleHandGears(ctx);
            if (e.empty()) return;
            auto& ps = ctx.state.player(ctx.controller);
            ps.hand.erase(std::find(ps.hand.begin(), ps.hand.end(), e.front()));
            ctx.executor.playPayingCost(ctx.controller, e.front(), std::nullopt, {.ignore_energy = true});
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point < 20) {
            if (gear == kInvalidId || !ctx.state.objectExists(gear)) { ri.resume_point = 22; return; }
            ctx.executor.killObject(gear);
            ctx.events.logTrace("JAYCE: killed a friendly gear");
            auto eligible = eligibleHandGears(ctx);
            if (eligible.empty()) { ri.resume_point = 22; return; }
            std::vector<Intent> opts;
            for (auto id : eligible) opts.push_back(objChoice(ctx.controller, id));
            opts.push_back(valChoice(ctx.controller, 0, "Don't play a gear"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Jayce: play a gear from hand ignoring its Energy cost");
            ri.resume_point = 20;
            return;
        }
        if (ri.resume_point == 20) {
            ri.resume_point = 21;
            GameObjectId picked = takenObject(ctx);
            auto eligible = eligibleHandGears(ctx);
            if (std::find(eligible.begin(), eligible.end(), picked) == eligible.end()) {
                ri.resume_point = 22;
                return;
            }
            while (ri.resume_data.size() < 7) ri.resume_data.push_back(0);
            ri.resume_data[6] = static_cast<int32_t>(picked);
        }
        if (ri.resume_point != 21) return;
        const auto picked = static_cast<GameObjectId>(ri.resume_data[6]);
        // Base, or a battlefield for [Deploy] gear (CR 355.2, 830).
        LocationId where;
        const int r = pickPlayLocation(ctx, picked, where, PlayerId::None, "Jayce: play the gear where?");
        if (r == -1) return;   // waiting for the agent
        ri.resume_point = 22;
        if (r == 0) return;
        auto& ps = ctx.state.player(ctx.controller);
        ps.hand.erase(std::find(ps.hand.begin(), ps.hand.end(), picked));
        if (!ctx.executor.playPayingCost(ctx.controller, picked, where, {.ignore_energy = true})) {
            ps.hand.push_back(picked);
            return;
        }
        ctx.events.logTrace("JAYCE: played a gear from hand ignoring its Energy cost");
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
