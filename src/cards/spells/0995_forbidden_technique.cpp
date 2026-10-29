#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// [1][C][C] for a Fury card: one Energy and two Fury power. A rune may be
// exhausted for the Energy and then recycled for power, so the check is
// "one Energy" and "two Fury power sources" independently.
bool forbiddenTechniqueCanPay(const GameState& state, PlayerId p) {
    if (payableEnergy(state, p) < 1) return false;
    const auto& pool = state.player(p).rune_pool;
    int power = pool.power[static_cast<int>(Domain::Fury)] + pool.universal_power;
    const LocationId base{BaseLocation{p}};
    for (const auto& [id, o] : state.objects)
        if (o.isRune() && o.controller == p && o.location.has_value() && *o.location == base &&
            runeMatchesDomain(o, Domain::Fury)) ++power;
    return power >= 2;
}

// "Ready a unit.
//  When you play a card with Energy cost [7] or more, you may play this from
//  your trash for [1][C][C]. Then banish this." (errata text)
// The second ability works from the trash (TriggerType::WhenYouPlayACardFromTrash).
// Playing it happens as the triggered ability resolves: it is paid, put on
// the chain (its target is chosen as it is finalized) and banished when it
// leaves the chain.
class ForbiddenTechnique : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId t = pickTarget(ctx, "Forbidden Technique: ready a unit", legal);
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.readyObject(t);
    }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACardFromTrash; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType, GameObjectId) const override {
        if (!state.objectExists(source)) return false;
        const auto& me = state.getObject(source);
        auto it = me.card_counters.find("__played_card_energy_cost");
        return it != me.card_counters.end() && it->second >= 7;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId self = ctx.source;
        auto inMyTrash = [&]() {
            if (!ctx.state.objectExists(self)) return false;
            const auto& o = ctx.state.getObject(self);
            if (o.zone != ZoneType::Trash) return false;
            const auto& tr = ctx.state.player(o.owner).trash;
            return std::find(tr.begin(), tr.end(), self) != tr.end();
        };
        int go = confirmOptional(ctx, "Forbidden Technique: play this from your trash for [1][R][R]",
            [&]() { return inMyTrash() && forbiddenTechniqueCanPay(ctx.state, ctx.controller); });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go == 0) return;
        if (!inMyTrash() || !forbiddenTechniqueCanPay(ctx.state, ctx.controller)) return;
        if (!payEnergy(ctx, ctx.controller, 1)) return;
        payOnePower(ctx, ctx.controller, Domain::Fury);
        payOnePower(ctx, ctx.controller, Domain::Fury);
        auto& o = ctx.state.getObject(self);
        auto& tr = ctx.state.player(o.owner).trash;
        tr.erase(std::remove(tr.begin(), tr.end(), self), tr.end());
        o.banish_on_leave_chain = true;   // "Then banish this."
        ctx.events.logTrace("FORBIDDEN TECHNIQUE: played from trash for [1][R][R]");
        ctx.executor.playIgnoringCost(ctx.controller, self);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 995;
        d.def_id = R"RB(rad-010-167)RB";
        d.name = R"RB(Forbidden Technique)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-010/167)RB";
        d.collector_number = 10;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.ability_text = R"RB(Ready a unit.
When you play a card with Energy cost [7] or more, you may play this from your trash for [1][C][C]. Then banish this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/293518dcf6c2f9e3f59a2e259826fd813436b3a5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_995(CardRegistry& r) {
    r.registerCard(995, std::make_unique<ForbiddenTechnique>());
}

}  // namespace riftbound
