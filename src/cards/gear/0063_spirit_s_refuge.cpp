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

class SpiritSRefuge : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    // "When you play this, buff a friendly unit." Chosen at finalization.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller;
        });
        GameObjectId t = pickTarget(ctx, "Spirit's Refuge: buff", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.buffUnit(t);
    }
    // "Friendly buffed units have [Deflect] if they didn't already." The gear
    // itself doesn't have [Deflect].
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject&) {
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                    continue;
                if (u.buff_count <= 0) continue;
                if (u.keywords.has(Keyword::Deflect)) continue;  // "if they didn't already"
                bool granted = false;
                for (const auto& ae : u.aura_effects)
                    if (ae.keyword == Keyword::Deflect) { granted = true; break; }
                if (granted) continue;
                auraKeyword(u, sid, Keyword::Deflect, 1);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 63;
        d.def_id = R"RB(ogn-063-298)RB";
        d.name = R"RB(Spirit's Refuge)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-063/298)RB";
        d.collector_number = 63;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play this, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.)
Friendly buffed units have [Deflect] if they didn't already. (Opponents must pay [A] to choose those units with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/26d5e54580d1f17ed6c35a66ac6a90ce56c99ec8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_63(CardRegistry& r) {
    r.registerCard(63, std::make_unique<SpiritSRefuge>());
}

} // namespace riftbound
