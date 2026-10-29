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

class RecruitTheVanguard : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play four 1 [M] Recruit unit tokens. (They can be played to your base
    //  or to battlefields you control.)" Each token's location is the
    // player's choice (valChoice: 0 = base, 1 + i = battlefield i) when they
    // control a battlefield; otherwise it goes to base.
    // resume_point 20 = consume a location pick; resume_data[6] = tokens played.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto dests = [&]() {
            std::vector<int> out{0};
            for (const auto& bf : ctx.state.battlefields)
                if (bf.controller && *bf.controller == ctx.controller)
                    out.push_back(1 + static_cast<int>(bf.id));
            return out;
        };
        auto play = [&](int dest) {
            LocationId loc = dest <= 0
                ? LocationId{BaseLocation{ctx.controller}}
                : LocationId{BattlefieldLocation{static_cast<BattlefieldId>(dest - 1)}};
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                     1, {"Recruit"}, KeywordSet{}, loc,
                                     /*enter_ready=*/false);
        };
        if (!ctx.state.chain.resuming.has_value()) {
            for (int i = 0; i < 4; ++i) play(0);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 7) ri.resume_data.push_back(0);
        if (ri.resume_point == 20) {
            const int picked = takenValue(ctx, 0);
            auto d = dests();
            play(std::find(d.begin(), d.end(), picked) != d.end() ? picked : 0);
            ++ri.resume_data[6];
        }
        while (ri.resume_data[6] < 4) {
            auto d = dests();
            if (d.size() == 1) { play(0); ++ri.resume_data[6]; continue; }
            std::vector<Intent> opts;
            for (int v : d) opts.push_back(valChoice(ctx.controller, v, destLabel(v)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Recruit the Vanguard: where to play a Recruit");
            ri.resume_point = 20;
            return;
        }
        ri.resume_point = 21;
        ctx.events.logTrace("RECRUIT THE VANGUARD: played four 1[M] Recruit tokens");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 313;
        d.def_id = R"RB(ogs-015-024)RB";
        d.name = R"RB(Recruit the Vanguard)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-015/024)RB";
        d.collector_number = 15;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Play four 1 [M] Recruit unit tokens. (They can be played to your base or to battlefields you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/81d1c47459606f7b627778cce9b5f0e44d80f7fa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_313(CardRegistry& r) {
    r.registerCard(313, std::make_unique<RecruitTheVanguard>());
}

} // namespace riftbound
