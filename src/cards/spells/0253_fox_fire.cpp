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

class FoxFire : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill any number of units at a battlefield with total Might 4 or less."
    // The play-time target is a unit that names the battlefield. As the spell
    // resolves the player picks the units to kill there one at a time (any
    // controller, each fitting the remaining Might budget), or stops; the
    // chosen units then die together. Resume points 20/21/22; resume_data[15]
    // holds the count and [16..] the picks.
    bool needsPlayTimeTarget() const override { return true; }
    static constexpr size_t kCount = 15;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId anchor;
        if (!targets.empty()) {
            anchor = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            anchor = pickTarget(ctx, "Fox-Fire (a unit at a battlefield)", legal);
        }
        if (anchor == kInvalidId || !ctx.state.objectExists(anchor)) return;
        auto& ri_opt = ctx.state.chain.resuming;
        // The battlefield is fixed once the first pick is offered.
        std::optional<BattlefieldId> bf;
        if (ri_opt.has_value() && ri_opt->resume_point >= 20 && ri_opt->resume_data.size() > kCount)
            bf = static_cast<BattlefieldId>(ri_opt->resume_data[kCount - 1]);
        else
            bf = ctx.state.getObject(anchor).battlefieldId();
        if (!bf) return;

        std::vector<GameObjectId> picks;
        int budget = 4;
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.battlefieldId() != bf) continue;
                if (std::find(picks.begin(), picks.end(), id) != picks.end()) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                if (obj.current_might <= budget) out.push_back(id);
            }
            return out;
        };

        if (!ri_opt.has_value()) {
            // Direct invocation: lowest Might first (test path only).
            std::vector<std::pair<int, GameObjectId>> here;
            for (auto id : candidates()) here.emplace_back(ctx.state.getObject(id).current_might, id);
            std::sort(here.begin(), here.end());
            for (auto& [m, id] : here) if (m <= budget) { picks.push_back(id); budget -= m; }
        } else {
            auto& ri = *ri_opt;
            if (ri.resume_point < 20) {
                if (ri.resume_data.size() <= kCount) ri.resume_data.resize(kCount + 1, 0);
                ri.resume_data[kCount - 1] = static_cast<int32_t>(*bf);
                ri.resume_data[kCount] = 0;
                ri.resume_point = 20;
            }
            for (int i = 0; i < ri.resume_data[kCount]; ++i) {
                auto id = static_cast<GameObjectId>(ri.resume_data[kCount + 1 + i]);
                picks.push_back(id);
                if (ctx.state.objectExists(id)) budget -= ctx.state.getObject(id).current_might;
            }
            if (ri.resume_point == 21) {
                GameObjectId got = takenObject(ctx);
                auto c = candidates();
                if (got == kInvalidId || std::find(c.begin(), c.end(), got) == c.end()) {
                    ri.resume_point = 22;
                } else {
                    ri.resume_data.push_back(static_cast<int32_t>(got));
                    ri.resume_data[kCount] += 1;
                    picks.push_back(got);
                    budget -= ctx.state.getObject(got).current_might;
                    ri.resume_point = 20;
                }
            }
            if (ri.resume_point == 20) {
                auto c = candidates();
                if (!c.empty()) {
                    std::vector<Intent> opts;
                    opts.push_back(valChoice(ctx.controller, 0, "Done choosing units"));
                    for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                        "Fox-Fire: kill a unit (Might budget " + std::to_string(budget) + ")");
                    ri.resume_point = 21;
                    return;
                }
                ri.resume_point = 22;
            }
        }
        for (auto id : picks)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).location.has_value())
                ctx.executor.killObject(id);
        ctx.events.logTrace("FOX-FIRE: killed " + std::to_string(picks.size()) +
                            " units (total Might <= 4) at one BF");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true, .max_might = 4};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 253;
        d.def_id = R"RB(ogn-256-298)RB";
        d.name = R"RB(Fox-Fire)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-256/298)RB";
        d.collector_number = 256;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ahri)RB"};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Kill any number of units at a battlefield with total Might 4 or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7ad9d6a46a1c54080d54950a0044da3a82e32b45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_253(CardRegistry& r) {
    r.registerCard(253, std::make_unique<FoxFire>());
}

} // namespace riftbound
