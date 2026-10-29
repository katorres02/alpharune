#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// [Reaction]. "Choose a battlefield. Give friendly units there [Shield 2]
// this turn." The battlefield is chosen as the spell is played.
class ForDemacia : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const int n = static_cast<int>(ctx.state.battlefields.size());
        if (n == 0) return;
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i) labels.push_back("Battlefield #" + std::to_string(i));
        int pick = pickMode(ctx, "For Demacia!: choose a battlefield", n, labels);
        if (pick < 0) return;
        if (finalizingChoices(ctx)) return;
        if (pick >= static_cast<int>(ctx.state.battlefields.size())) return;
        const BattlefieldId bf = ctx.state.battlefields[pick].id;
        std::vector<GameObjectId> units;
        for (const auto& [id, o] : ctx.state.objects) {
            if (!o.isUnit() || o.controller != ctx.controller) continue;
            auto obf = o.battlefieldId();
            if (obf && *obf == bf) units.push_back(id);
        }
        for (auto u : units) ctx.executor.giveTemporaryKeyword(u, Keyword::Shield, 2);
        ctx.events.logTrace("FOR DEMACIA!: " + std::to_string(units.size()) +
                            " friendly unit(s) at battlefield #" + std::to_string(pick) +
                            " get [Shield 2] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1093;
        d.def_id = R"RB(rad-117-167)RB";
        d.name = R"RB(For Demacia!)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-117/167)RB";
        d.collector_number = 117;
        d.artist = R"RB(Original Force)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a battlefield. Give friendly units there [Shield 2] this turn. (+2 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5d119653fcba423c150815de3a412922c4e481e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1093(CardRegistry& r) {
    r.registerCard(1093, std::make_unique<ForDemacia>());
}

}  // namespace riftbound
