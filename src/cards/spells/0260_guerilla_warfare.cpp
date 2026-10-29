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

class GuerillaWarfare : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Return up to two cards with [Hidden] from your trash to your hand. You
    // can hide cards ignoring costs this turn." The player picks the cards one
    // at a time (or stops). Resume points 20/21/22; resume_data[15] = count,
    // [16..17] = picks.
    static constexpr size_t kCount = 15;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        const CardDB& db = ctx.executor.cardDB();
        std::vector<GameObjectId> picks;
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ps.trash) {
                if (!ctx.state.objectExists(cid)) continue;
                const auto& obj = ctx.state.getObject(cid);
                if (obj.card_def_id == kInvalidId) continue;
                if (!db.get(obj.card_def_id).keywords.has(Keyword::Hidden)) continue;
                if (std::find(picks.begin(), picks.end(), cid) != picks.end()) continue;
                out.push_back(cid);
            }
            return out;
        };
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (test path): the two most recently trashed.
            auto c = candidates();
            for (auto it = c.rbegin(); it != c.rend() && picks.size() < 2; ++it) picks.push_back(*it);
        } else {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_data.size() < kCount + 3) ri.resume_data.resize(kCount + 3, 0);
            if (ri.resume_point < 20) { ri.resume_point = 20; ri.resume_data[kCount] = 0; }
            for (int i = 0; i < ri.resume_data[kCount]; ++i)
                picks.push_back(static_cast<GameObjectId>(ri.resume_data[kCount + 1 + i]));
            if (ri.resume_point == 21) {
                GameObjectId got = takenObject(ctx);
                auto c = candidates();
                if (got == kInvalidId || std::find(c.begin(), c.end(), got) == c.end()) {
                    ri.resume_point = 22;
                } else {
                    ri.resume_data[kCount + 1 + picks.size()] = static_cast<int32_t>(got);
                    picks.push_back(got);
                    ri.resume_data[kCount] = static_cast<int32_t>(picks.size());
                    ri.resume_point = picks.size() >= 2 ? 22 : 20;
                }
            }
            if (ri.resume_point == 20) {
                auto c = candidates();
                if (!c.empty()) {
                    std::vector<Intent> opts;
                    opts.push_back(valChoice(ctx.controller, 0, "Done returning cards"));
                    for (auto cid : c) opts.push_back(objChoice(ctx.controller, cid));
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                        "Guerilla Warfare: return a [Hidden] card from your trash");
                    ri.resume_point = 21;
                    return;
                }
                ri.resume_point = 22;
            }
        }
        for (auto cid : picks) {
            auto it = std::find(ps.trash.begin(), ps.trash.end(), cid);
            if (it == ps.trash.end()) continue;
            ps.trash.erase(it);
            auto& obj = ctx.state.getObject(cid);
            obj.zone = ZoneType::Hand;
            obj.location = std::nullopt;
            ps.hand.push_back(cid);
            ctx.events.logTrace("GUERILLA WARFARE: returned " + obj.name +
                                 " from trash to hand");
        }
        ps.hide_ignoring_costs_turn = ctx.state.turn.turn_number;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 260;
        d.def_id = R"RB(ogn-264-298)RB";
        d.name = R"RB(Guerilla Warfare)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-264/298)RB";
        d.collector_number = 264;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Teemo)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(Return up to two cards with [Hidden] from your trash to your hand. You can hide cards ignoring costs this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3a1f70e49f1fdbac907a3395f517e96717e8afea-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_260(CardRegistry& r) {
    r.registerCard(260, std::make_unique<GuerillaWarfare>());
}

} // namespace riftbound
