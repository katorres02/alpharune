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

constexpr CardDefId kRallyRecruitDef = 954;   // Recruit unit token

/// "An occupied enemy battlefield": controlled by an opponent, with units there.
bool rallyOccupiedEnemyBf(const GameState& state, PlayerId me, const BattlefieldState& bf) {
    if (!bf.controller.has_value() || *bf.controller == me) return false;
    for (const auto& [id, o] : state.objects)
        if (o.isUnit() && o.battlefieldId() && *o.battlefieldId() == bf.id) return true;
    return false;
}

// "Choose an occupied enemy battlefield. Play three 1 [M] Recruit unit tokens
// there." The battlefield is chosen when the spell is finalized (CR 355).
class RallyForWar : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (const auto& bf : state.battlefields)
            if (rallyOccupiedEnemyBf(state, controller, bf)) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const int nbf = static_cast<int>(ctx.state.battlefields.size());
        uint32_t legal = 0;
        std::vector<std::string> labels;
        for (int i = 0; i < nbf; ++i) {
            labels.push_back("Battlefield #" + std::to_string(i));
            if (i < 32 && rallyOccupiedEnemyBf(ctx.state, ctx.controller, ctx.state.battlefields[i]))
                legal |= (1u << i);
        }
        const int pick = pickMode(ctx, "Rally for War: choose an occupied enemy battlefield",
                                  nbf, labels, legal);
        if (pick == -1) return;
        if (finalizingChoices(ctx)) return;
        if (pick < 0 || pick >= nbf) return;
        // A chosen battlefield that is no longer an occupied enemy battlefield
        // is a mistarget (CR 359.3.e.6).
        if (!rallyOccupiedEnemyBf(ctx.state, ctx.controller, ctx.state.battlefields[pick])) {
            ctx.events.logTrace("RALLY FOR WAR: chosen battlefield no longer legal");
            return;
        }
        const BattlefieldId bf = ctx.state.battlefields[pick].id;
        for (int i = 0; i < 3; ++i) {
            auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                                {"Recruit"}, KeywordSet{},
                                                LocationId{BattlefieldLocation{bf}}, false);
            bindTokenCard(ctx.state, tok, kRallyRecruitDef);
        }
        ctx.events.logTrace("RALLY FOR WAR: three Recruits to BF#" + std::to_string(bf));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 992;
        d.def_id = R"RB(rad-007-167)RB";
        d.name = R"RB(Rally for War)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-007/167)RB";
        d.collector_number = 7;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.ability_text = R"RB(Choose an occupied enemy battlefield. Play three 1 [M] Recruit unit tokens there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/09c0fc35d8f4b72f58a95d1745b99c4133d02513-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_992(CardRegistry& r) {
    r.registerCard(992, std::make_unique<RallyForWar>());
}

}  // namespace riftbound
