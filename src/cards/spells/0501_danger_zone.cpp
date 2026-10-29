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

class DangerZone : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Give your Mechs +1 [M] this turn." No target: every friendly Mech on the
    // board. ([Repeat] is paid and re-runs this effect through the engine.)
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> mechs;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.controller != ctx.controller) continue;
            if (std::find(obj.tags.begin(), obj.tags.end(), "Mech") == obj.tags.end()) continue;
            mechs.push_back(id);
        }
        for (auto id : mechs) ctx.executor.giveTemporaryMight(id, 1);
        ctx.events.logTrace("DANGER ZONE: +1 [M] to " + std::to_string(mechs.size()) + " Mech(s)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 501;
        d.def_id = R"RB(sfd-182-221)RB";
        d.name = R"RB(Danger Zone)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-182/221)RB";
        d.collector_number = 182;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Rumble)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Repeat] [1][A] (You may pay the additional cost to repeat this spell's effect.)
Give your Mechs +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c4652150ba885dd346b8f3622fd0e4ada7cf767f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_501(CardRegistry& r) {
    r.registerCard(501, std::make_unique<DangerZone>());
}

} // namespace riftbound
