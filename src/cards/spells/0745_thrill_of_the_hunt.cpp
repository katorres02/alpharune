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

class ThrillOfTheHunt : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    // "Banish a friendly unit, then its owner plays it to any battlefield,
    // ignoring its cost." The unit is chosen as the spell is played; its owner
    // chooses the battlefield after it is banished (resume_point 20/21 —
    // pickTarget's 6..8 come first, so the two choices compose).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId unit_id = pickTarget(ctx, "Thrill of the Hunt", legal);
        if (unit_id == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting on the agent
        if (unit_id == kInvalidId || !ctx.state.objectExists(unit_id)) return;
        auto& ri_opt = ctx.state.chain.resuming;
        const bool banished_already = ri_opt.has_value() && ri_opt->resume_point >= 20;
        const PlayerId owner = ctx.state.getObject(unit_id).owner;

        if (!banished_already) {
            if (!ctx.state.getObject(unit_id).location.has_value()) return;
            const bool token = ctx.state.getObject(unit_id).isToken();
            ctx.executor.banishObject(unit_id);
            // playIgnoringCost re-zones it: drop the banishment entry.
            auto& bz = ctx.state.player(owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), unit_id), bz.end());
            // CR 183.1: a banished token ceases to exist — nothing to play.
            if (token) {
                if (ri_opt.has_value()) ri_opt->resume_point = 22;
                return;
            }
            // It comes back as a new object: no damage, buffs, statuses or roles.
            auto& u = ctx.state.getObject(unit_id);
            u.damage_marked = 0;
            u.buff_count = 0;
            u.temp_buff_count = 0;
            u.temp_might_bonus = 0;
            u.is_stunned = false;
            u.empowered = 0;
            u.empower_flip_eot = 0;
            u.combat_designation = CombatDesignation::None;
        }
        if (ri_opt.has_value() && ri_opt->resume_point >= 22) return;   // token: done

        std::vector<std::pair<int, std::string>> opts;
        for (const auto& bf : ctx.state.battlefields) {
            std::string bf_name = "BF#" + std::to_string(static_cast<int>(bf.id));
            if (ctx.state.objectExists(bf.card_object_id))
                bf_name = ctx.state.getObject(bf.card_object_id).name;
            opts.push_back({static_cast<int>(bf.id), bf_name});
        }
        int bf_choice = -1;
        if (!stepChooseValue(ctx, owner, opts, "Thrill of the Hunt: play it to which battlefield",
                             20, 10, -1, bf_choice))
            return;   // the owner is choosing
        if (ri_opt.has_value()) ri_opt->resume_point = 22;
        if (bf_choice < 0) {
            ctx.executor.playIgnoringCost(owner, unit_id, LocationId{BaseLocation{owner}});
            ctx.events.logTrace("THRILL: no battlefield, played to base");
            return;
        }
        ctx.executor.playIgnoringCost(owner, unit_id,
            LocationId{BattlefieldLocation{static_cast<BattlefieldId>(bf_choice)}});
        ctx.events.logTrace("THRILL: " + ctx.state.getObject(unit_id).name +
                            " played to BF#" + std::to_string(bf_choice) + ", ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 745;
        d.def_id = R"RB(unl-184-219)RB";
        d.name = R"RB(Thrill of the Hunt)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-184/219)RB";
        d.collector_number = 184;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Rengar)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Banish a friendly unit, then its owner plays it to any battlefield, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2753c2b3ea4fd55e5225a4451a29736b5c3434a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_745(CardRegistry& r) {
    r.registerCard(745, std::make_unique<ThrillOfTheHunt>());
}

} // namespace riftbound
