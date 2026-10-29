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

class ArcaneShift : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        // 1. Banish a friendly unit, then its owner plays it, ignoring its cost.
        // Runs once: the owner chooses the play location first
        // (pickPlayLocation, which leaves resume_point alone) and the damage
        // target after it (pickTarget, 6..8), so a re-entry at resume_point
        // >= 6 is past this step.
        const bool step1_pending = !ri.has_value() || ri->resume_point < 6;
        if (step1_pending && !targets.empty() && ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).isUnit() &&
            ctx.state.getObject(targets[0]).location.has_value() &&
            ctx.state.getObject(targets[0]).controller == ctx.controller) {
            GameObjectId unit = targets[0];
            PlayerId owner = ctx.state.getObject(unit).owner;
            // CR 355.2: the player playing it — its owner — chooses its base
            // or a battlefield they control.
            LocationId where;
            const int r = pickPlayLocation(ctx, unit, where, owner,
                                           "Arcane Shift: play the unit where?");
            if (r == -1) return;  // waiting for the owner
            if (r == 1 && ctx.state.objectExists(unit)) {
                ctx.executor.banishObject(unit);
                auto& bz = ctx.state.player(owner).banishment;
                bz.erase(std::remove(bz.begin(), bz.end(), unit), bz.end());
                ctx.executor.playIgnoringCost(owner, unit, where);
                ctx.events.logTrace("ARCANE SHIFT: banished + replayed a friendly unit");
            }
        }

        // 2. Deal 3 to an enemy unit at a battlefield.
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.isAtBattlefield()) continue;
            if (obj.untargetable_by_enemy) continue;
            legal.push_back(id);
        }
        GameObjectId picked = pickTarget(ctx, "Arcane Shift: deal 3 to enemy unit", legal);
        if (picked == kInvalidId && ri.has_value() && ri->resume_point == 7) return;  // waiting
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.executor.dealDamage(picked, 3, ctx.source);
            ctx.events.logTrace("ARCANE SHIFT: dealt 3 to an enemy unit");
        }

        // 3. "Banish this."
        ctx.executor.banishObject(ctx.source);
        ctx.events.logTrace("ARCANE SHIFT: banished this");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 515;
        d.def_id = R"RB(sfd-200-221)RB";
        d.name = R"RB(Arcane Shift)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-200/221)RB";
        d.collector_number = 200;
        d.artist = R"RB(Original Force)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Ezreal)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Banish a friendly unit, then its owner plays it, ignoring its cost. Deal 3 to an enemy unit at a battlefield. Banish this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a5d1d26dee01db24f32a1432639478753d22aa92-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_515(CardRegistry& r) {
    r.registerCard(515, std::make_unique<ArcaneShift>());
}

} // namespace riftbound
