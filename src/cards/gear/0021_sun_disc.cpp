#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class SunDisc : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[E]: [Legion] — The next unit you play this turn enters ready."
    // The activated ability ARMS a one-shot for this turn. While armed, the next
    // unit its controller plays enters ready (grantsEnterReadyTo, asked by
    // resolvePermanent); the WhenYouPlayAUnit trigger then disarms it.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool requiresLegion() const override { return true; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters[kArm] = ctx.state.turn.turn_number;
        ctx.events.logTrace("SUN DISC: armed next-unit-enters-ready for this turn");
    }

    bool grantsEnterReadyTo(const GameState& state, GameObjectId self,
                            const GameObject& played) const override {
        // A token entering the board doesn't fire WhenYouPlayAUnit (which
        // disarms me), so the one-shot is kept for a played card.
        return played.isUnit() && !played.is_token && armed(state, self);
    }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType, GameObjectId) const override {
        return armed(state, source);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters.erase(kArm);  // one-shot used
        ctx.events.logTrace("SUN DISC: next unit entered ready");
    }
private:
    static constexpr const char* kArm = "__sundisc_arm_turn";
    static bool armed(const GameState& state, GameObjectId self) {
        if (!state.objectExists(self)) return false;
        const auto& c = state.getObject(self).card_counters;
        auto it = c.find(kArm);
        return it != c.end() && it->second == state.turn.turn_number;
    }
public:
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 21;
        d.def_id = R"RB(ogn-021-298)RB";
        d.name = R"RB(Sun Disc)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-021/298)RB";
        d.collector_number = 21;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([E]: [Legion] — The next unit you play this turn enters ready. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/146d7514f15e6674f471f3aa9c3fadf22c0b634b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_21(CardRegistry& r) {
    r.registerCard(21, std::make_unique<SunDisc>());
}

} // namespace riftbound
