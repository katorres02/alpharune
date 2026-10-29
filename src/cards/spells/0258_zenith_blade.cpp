#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ZenithBlade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    // "Stun an enemy unit at a battlefield. You may move a friendly unit to
    // that enemy unit's battlefield." The move is one choice: "don't move" or
    // the friendly unit to move (resume points 20/21/22).
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto enemy_at_bf = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.controller != ctx.controller && o.isAtBattlefield();
        });
        std::sort(enemy_at_bf.begin(), enemy_at_bf.end());
        GameObjectId stunned = pickTarget(ctx, "Zenith Blade: stun enemy unit", enemy_at_bf);
        if (stunned == kInvalidId) return;  // suspended, or no legal target
        if (!ctx.state.objectExists(stunned)) return;
        auto& ri_opt = ctx.state.chain.resuming;
        const bool first_pass = !ri_opt.has_value() || ri_opt->resume_point < 20;
        if (first_pass) ctx.executor.stunUnitBy(stunned, ctx.source);
        if (!ri_opt.has_value()) return;   // direct invocation: no move choice
        auto& ri = *ri_opt;

        auto dest_bf = ctx.state.getObject(stunned).battlefieldId();
        auto movable = [&]() {
            std::vector<GameObjectId> out;
            if (!dest_bf) return out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.battlefieldId() == dest_bf) continue;  // already there
                out.push_back(id);
            }
            return out;
        };
        if (ri.resume_point < 20) {
            auto units = movable();
            if (units.empty()) { ri.resume_point = 22; return; }
            std::vector<Intent> opts;
            opts.push_back(valChoice(ctx.controller, 0, "Don't move a unit"));
            for (auto u : units) opts.push_back(objChoice(ctx.controller, u));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "may: Zenith Blade: move a friendly unit to that battlefield");
            ri.resume_point = 21;
            return;
        }
        if (ri.resume_point == 21) {
            ri.resume_point = 22;
            GameObjectId u = takenObject(ctx);
            auto units = movable();
            if (u == kInvalidId || !dest_bf ||
                std::find(units.begin(), units.end(), u) == units.end()) return;
            ctx.executor.moveToBattlefield(u, *dest_bf);
            ctx.events.logTrace("ZENITH BLADE: moved a friendly unit to the stunned unit's battlefield");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 258;
        d.def_id = R"RB(ogn-262-298)RB";
        d.name = R"RB(Zenith Blade)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-262/298)RB";
        d.collector_number = 262;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Leona)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Stun an enemy unit at a battlefield. You may move a friendly unit to that enemy unit's battlefield. (A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a572f0bc3ed1bd47c5759d36f5b951b15338e57e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_258(CardRegistry& r) {
    r.registerCard(258, std::make_unique<ZenithBlade>());
}

} // namespace riftbound
