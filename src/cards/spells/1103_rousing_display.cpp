#include "cards/card.h"
#include "cards/card_helpers.h"
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

// "[Show Off] a card with Energy cost [7] or more. Play two 1 [M] Recruit
// unit tokens to your base. If you showed off, play four to your base
// instead." The engine asks for the Show Off as the spell is played; the
// printed-cost condition is checked on the card table (showOffMatchesOnDefs).
class RousingDisplay : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasShowOff() const override { return true; }
    bool showOffMatches(const GameState&, PlayerId, const GameObject& c) const override {
        return c.card_def_id != kInvalidId && !c.isToken() && !c.isRune() &&
               !c.isBattlefield() && !c.isLegend();
    }
    bool showOffMatchesOnDefs(const GameState&, const CardDB& db, PlayerId,
                              const GameObject& c) const override {
        if (c.card_def_id == kInvalidId || !db.all().count(c.card_def_id)) return false;
        return db.get(c.card_def_id).energy_cost >= 7;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const bool showed = shownOff(ctx.state, ctx.source) != kInvalidId;
        const int n = showed ? 4 : 2;
        for (int i = 0; i < n; ++i) {
            auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                                {"Recruit"}, KeywordSet{},
                                                LocationId{BaseLocation{ctx.controller}}, false);
            bindTokenCard(ctx.state, tok, 954);
        }
        ctx.events.logTrace("ROUSING DISPLAY: " + std::to_string(n) + " Recruit tokens to base" +
                            (showed ? " (showed off)" : ""));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1103;
        d.def_id = R"RB(rad-127-167)RB";
        d.name = R"RB(Rousing Display)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-127/167)RB";
        d.collector_number = 127;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 5;
        d.power_cost = 0;
        d.ability_text = R"RB([Show Off] a card with Energy cost [7] or more. (As you play this, you may reveal such a card from hand or pick a friendly one.)
Play two 1 [M] Recruit unit tokens to your base. If you showed off, play four to your base instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/18aa7cab082fd0e293260bcf480a1a76d12b64d9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1103(CardRegistry& r) {
    r.registerCard(1103, std::make_unique<RousingDisplay>());
}

}  // namespace riftbound
