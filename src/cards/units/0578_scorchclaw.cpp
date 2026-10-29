#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Scorchclaw : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Hunt 2]: when I conquer or hold, gain 2 XP (no level gate).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("SCORCHCLAW: [Hunt 2] +2 XP");
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    // [Level 3][>] I have +1 [M] and enter ready.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 3) return;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.isUnit() || !self.location.has_value()) continue;
            bool already = false;
            for (const auto& ae : self.aura_effects) if (ae.source == sid) already = true;
            if (already) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = 1;
            self.aura_effects.push_back(ae);
        }
    }
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        return state.player(controller).xp >= 3;
    }
    bool requiresLevel() const override { return true; }
    int levelThreshold() const override { return 3; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 578;
        d.def_id = R"RB(unl-016-219)RB";
        d.name = R"RB(Scorchclaw)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-016/219)RB";
        d.collector_number = 16;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dog)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hunt);
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
[Level 3][>] I have +1 [M] and enter ready. (While you have 3+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f69940824f8ce62a479df28988dcbdf6ea6d3960-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_578(CardRegistry& r) {
    r.registerCard(578, std::make_unique<Scorchclaw>());
}

} // namespace riftbound
