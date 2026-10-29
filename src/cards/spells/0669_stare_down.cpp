#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class StareDown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit and a battlefield. Move all enemy units at that
    //  battlefield with less Might than the chosen unit to their base. Gain 1 XP."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    std::vector<GameObjectId> friendlyUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            out.push_back(id);
        }
        return out;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The battlefield (pickMode, resume 3/4/5) is asked before the
        // friendly unit's pick is read back (pickTarget, resume 6/7/8; the
        // unit itself was chosen at finalization), so the two compose.
        const int n = static_cast<int>(ctx.state.battlefields.size());
        if (n > 0) {
            std::vector<std::string> labels;
            for (const auto& bf : ctx.state.battlefields)
                labels.push_back("Battlefield " + std::to_string(bf.id));
            int m = pickMode(ctx, "Stare Down: choose a battlefield", n, labels);
            if (m == -1) return;   // waiting for agent
        }
        GameObjectId chosen = pickTarget(ctx, "Stare Down: choose a friendly unit",
                                         friendlyUnits(ctx));
        if (chosen == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for agent
        // Every choice is made: the effect runs exactly once from here.
        if (chosen != kInvalidId && ctx.state.objectExists(chosen) && n > 0) {
            int m = pickMode(ctx, "Stare Down: choose a battlefield", n);
            if (m < 0 || m >= n) m = 0;
            const BattlefieldId bf = ctx.state.battlefields[m].id;
            const int threshold = ctx.state.getObject(chosen).current_might;
            const PlayerId opp = opponent(ctx.controller);
            std::vector<GameObjectId> movers;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != opp) continue;
                auto obf = obj.battlefieldId();
                if (!obf || *obf != bf) continue;
                if (obj.current_might >= threshold) continue;
                movers.push_back(id);
            }
            for (auto id : movers)
                if (ctx.state.objectExists(id)) ctx.executor.moveToBase(id);
            ctx.events.logTrace("STARE DOWN: moved " + std::to_string(movers.size()) +
                                " enemy unit(s) (< " + std::to_string(threshold) +
                                "M) at BF" + std::to_string(bf) + " to base");
        }
        // "Gain 1 XP." (unconditional)
        ctx.state.player(ctx.controller).xp += 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 669;
        d.def_id = R"RB(unl-107-219)RB";
        d.name = R"RB(Stare Down)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-107/219)RB";
        d.collector_number = 107;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Choose a friendly unit and a battlefield. Move all enemy units at that battlefield with less Might than the chosen unit to their base. Gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/58d0353a966bb0ca292855a0a55de5769e28d155-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_669(CardRegistry& r) {
    r.registerCard(669, std::make_unique<StareDown>());
}

} // namespace riftbound
