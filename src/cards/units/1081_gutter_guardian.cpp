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

class GutterGuardian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hidden] is engine-handled.
    // "[Show Off] a card with [Hidden]." (CR 832: engine asks as I'm played.)
    bool hasShowOff() const override { return true; }
    bool showOffMatches(const GameState& /*state*/, PlayerId /*player*/,
                        const GameObject& candidate) const override {
        if (candidate.isRune() || candidate.isBattlefield() || candidate.isLegend()) return false;
        return candidate.hasKeyword(Keyword::Hidden);
    }
    // "When you play me, if you showed off, [Add] [A]."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (shownOff(ctx.state, ctx.source) == kInvalidId) return;
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("GUTTER GUARDIAN: showed off -> [Add] [A]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1081;
        d.def_id = R"RB(rad-103-167)RB";
        d.name = R"RB(Gutter Guardian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-103/167)RB";
        d.collector_number = 103;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Zaun)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
[Show Off] a card with [Hidden]. (As you play me, you may reveal such a card from hand or pick another friendly one on the board.)
When you play me, if you showed off, [Add] [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/756827744317003e4992163ff9bf1dc991682de8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1081(CardRegistry& r) {
    r.registerCard(1081, std::make_unique<GutterGuardian>());
}

}  // namespace riftbound
