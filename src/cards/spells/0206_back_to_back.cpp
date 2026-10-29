#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BackToBack : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !friendlyUnits(state, controller).empty();
    }
    // "Give two friendly units each +2 [M] this turn."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal_a = friendlyUnits(ctx.state, ctx.controller);
        auto [a, b] = pickTargetPair(ctx, "Back to Back: two friendly units", legal_a,
            [&](GameObjectId first) {
                auto v = friendlyUnits(ctx.state, ctx.controller);
                v.erase(std::remove(v.begin(), v.end(), first), v.end());
                return v;
            });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.executor.hasPendingChoice()) return;  // suspended
        for (auto u : {a, b})
            if (u != kInvalidId && ctx.state.objectExists(u))
                ctx.executor.giveTemporaryMight(u, 2);
    }
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                out.push_back(id);
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 206;
        d.def_id = R"RB(ogn-206-298)RB";
        d.name = R"RB(Back to Back)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-206/298)RB";
        d.collector_number = 206;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give two friendly units each +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7f5585dcd3e9c7866e3b99cb169a0404ea4c010f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_206(CardRegistry& r) {
    r.registerCard(206, std::make_unique<BackToBack>());
}

} // namespace riftbound
