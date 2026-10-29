#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class JarvanIVMartialParagon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Your gear with [Deploy] have [Action]." — PlayerState flag read by the
    // showdown action generator (GameEngine::generateShowdownActions).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value() || self.zone == ZoneType::Chain) continue;
            state.player(controller).deploy_gear_have_action = true;
            return;
        }
    }
    // "When I conquer, if there is a friendly gear with [Deploy] here, channel
    //  1 rune exhausted." (Checked as it resolves.)
    static bool isDeployGear(const CardContext& ctx, const GameObject& o) {
        if (!o.isGear()) return false;
        if (o.card_def_id == kBombTokenCardDefId) return true;
        if (ctx.registry && o.card_def_id != kInvalidId)
            if (const Card* c = ctx.registry->get(o.card_def_id)) return c->hasDeploy();
        return false;
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        bool deploy_here = false;
        for (const auto& [id, o] : ctx.state.objects) {
            if (o.controller != ctx.controller || o.zone == ZoneType::Chain) continue;
            auto bf = o.battlefieldId();
            if (bf && *bf == *my_bf && isDeployGear(ctx, o)) { deploy_here = true; break; }
        }
        if (!deploy_here) return;
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("JARVAN IV, MARTIAL PARAGON: conquer with [Deploy] gear here -> "
                            "channel 1 rune exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1071;
        d.def_id = R"RB(rad-091-167)RB";
        d.name = R"RB(Jarvan IV, Martial Paragon)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-091/167)RB";
        d.collector_number = 91;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Demacia)RB",R"RB(Jarvan IV)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(Your gear with [Deploy] have [Action]. (They can be played on your turn or in showdowns.)
When I conquer, if there is a friendly gear with [Deploy] here, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8edacba51db193506e13e1afb6a8509256cdf55c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1071(CardRegistry& r) {
    r.registerCard(1071, std::make_unique<JarvanIVMartialParagon>());
}

}  // namespace riftbound
