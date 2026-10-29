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

class ForgeOfTheFuture : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play this, play a 1 [M] Recruit unit token at your base."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        LocationId base{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                 {"Recruit"}, KeywordSet{}, base, false);
        ctx.events.logTrace("FORGE OF THE FUTURE: played -> 1 [M] Recruit at base");
    }

    // "Kill this: Recycle up to 4 cards from trashes." Killing this is the
    // cost (paid as the ability is activated); the cards are chosen one at a
    // time as it resolves, with a "done" option.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    static constexpr int kPublish = 20, kConsume = 21, kDone = 22;
    static constexpr size_t kCountSlot = 15;   // resume_data: count, then picks
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (costDueNow(ctx)) {
            if (ctx.state.objectExists(ctx.source) &&
                ctx.state.getObject(ctx.source).location.has_value())
                ctx.executor.killObject(ctx.source);
        }
        if (finalizingChoices(ctx)) return;   // the cost is paid; choices are made below

        std::vector<GameObjectId> picks;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_data.size() < kCountSlot + 5) ri.resume_data.resize(kCountSlot + 5, 0);
            if (ri.resume_point < kPublish) { ri.resume_point = kPublish; ri.resume_data[kCountSlot] = 0; }
            if (ri.resume_point == kConsume) {
                auto c = ctx.executor.takeChoice();
                GameObjectId got = (c && !c->chosen_objects.empty()) ? c->chosen_objects[0] : kInvalidId;
                int n = ri.resume_data[kCountSlot];
                if (got == kInvalidId) {
                    ri.resume_point = kDone;
                } else {
                    ri.resume_data[kCountSlot + 1 + n] = static_cast<int32_t>(got);
                    ri.resume_data[kCountSlot] = ++n;
                    ri.resume_point = n >= 4 ? kDone : kPublish;
                }
            }
            for (int i = 0; i < ri.resume_data[kCountSlot]; ++i)
                picks.push_back(static_cast<GameObjectId>(ri.resume_data[kCountSlot + 1 + i]));
            if (ri.resume_point == kPublish) {
                std::vector<Intent> opts;
                opts.push_back(valChoice(ctx.controller, 0, "Done recycling"));
                for (auto cid : trashCards(ctx.state))
                    if (std::find(picks.begin(), picks.end(), cid) == picks.end())
                        opts.push_back(objChoice(ctx.controller, cid));
                if (opts.size() > 1) {
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                               "Forge of the Future: recycle a card from a trash");
                    ri.resume_point = kConsume;
                    return;
                }
                ri.resume_point = kDone;
            }
        }
        for (auto cid : picks) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            auto& trash = ctx.state.player(obj.owner).trash;
            auto it = std::find(trash.begin(), trash.end(), cid);
            if (it == trash.end()) continue;   // no longer in a trash
            trash.erase(it);
            ctx.executor.recycleCards(ctx.controller, {cid});
        }
        ctx.events.logTrace("FORGE OF THE FUTURE: recycled " + std::to_string(picks.size()) +
                            " card(s) from trashes");
    }
    static std::vector<GameObjectId> trashCards(const GameState& state) {
        std::vector<GameObjectId> out;
        for (auto p : {PlayerId::Player1, PlayerId::Player2})
            for (auto cid : state.player(p).trash)
                if (state.objectExists(cid)) out.push_back(cid);
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 212;
        d.def_id = R"RB(ogn-212-298)RB";
        d.name = R"RB(Forge of the Future)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-212/298)RB";
        d.collector_number = 212;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.ability_text = R"RB(When you play this, play a 1 [M] Recruit unit token at your base.
Kill this: Recycle up to 4 cards from trashes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e6a76d28590bcd835b0a9a1e806fe0fa6883141-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_212(CardRegistry& r) {
    r.registerCard(212, std::make_unique<ForgeOfTheFuture>());
}

} // namespace riftbound
