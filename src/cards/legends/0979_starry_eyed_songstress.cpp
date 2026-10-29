#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class StarryEyedSongstress : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "As an additional cost to play a spell, you may exhaust me and three
    // friendly units to reduce the cost of that spell by [2]."
    // GAP: the engine has no hook for an additional cost granted to spells by
    // another card. Modelled as a [Reaction] ability ("[E], exhaust three
    // friendly units: the next spell you play this turn costs [2] less") that
    // the player uses just before playing the spell; it goes on the chain and
    // its discount can go unused.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        return readyUnits(state, controller).size() >= 3;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!costAlreadyPaid(ctx)) {
            auto picked = pickSeveral(ctx, ctx.controller, "Starry-Eyed Songstress: exhaust a friendly unit",
                                      readyUnits(ctx.state, ctx.controller), false, {}, 3, 3, 8);
            if (!picked) return;                   // waiting for the agent
            if (picked->size() < 3) return;        // the cost can't be paid
            if (costDueNow(ctx))
                for (int v : *picked) ctx.executor.exhaustObject(static_cast<GameObjectId>(v));
        }
        if (finalizingChoices(ctx)) return;
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_reduction = 2;
        m.next_spell_only = true;
        m.this_turn_only = true;
        ctx.state.player(ctx.controller).cost_modifiers.push_back(m);
        ctx.events.logTrace("STARRY-EYED SONGSTRESS: next spell costs [2] less");
    }
private:
    static std::vector<int> readyUnits(const GameState& state, PlayerId player) {
        std::vector<int> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == player && o.location.has_value() && !o.is_exhausted)
                out.push_back(static_cast<int>(id));
        std::sort(out.begin(), out.end());
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 979;
        d.def_id = R"RB(rad-151-167)RB";
        d.name = R"RB(Starry-Eyed Songstress)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-151/167)RB";
        d.collector_number = 151;
        d.artist = R"RB(Naifan Zhang)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind,Domain::Order};
        d.tags = {R"RB(Seraphine)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(As an additional cost to play a spell, you may exhaust me and three friendly units to reduce the cost of that spell by [2].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/399262205b15aa3b6306eb1d80a308791dcbe2ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_979(CardRegistry& r) {
    r.registerCard(979, std::make_unique<StarryEyedSongstress>());
}

}  // namespace riftbound
