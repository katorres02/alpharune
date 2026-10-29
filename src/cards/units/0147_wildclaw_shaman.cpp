#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class WildclawShaman : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you may spend a buff to buff me and ready me." The
    // "you may" and the cost (a buff of a friendly unit the player picks) are
    // decided and paid at finalization (CR 383.3.a-b). Resume points 20/21/22.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto buffed = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value() && obj.buff_count > 0)
                    out.push_back(id);
            return out;
        };
        auto spend = [&](GameObjectId u) {
            ctx.executor.spendBuff(u);
            ctx.events.logTrace("WILDCLAW SHAMAN: spent the buff of " +
                                ctx.state.getObject(u).name);
        };
        int conf = confirmOptional(ctx, "Wildclaw Shaman: spend a buff to buff & ready me?",
            [&]() { return costAlreadyPaid(ctx) || !buffed().empty(); });
        if (conf < 1) return;    // waiting, or declined / can't pay
        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value()) {          // direct invocation: first buffed unit
            auto b = buffed();
            if (b.empty()) return;
            spend(b.front());
        } else {
            auto& ri = *ri_opt;
            if (ri.resume_point < 20) {
                auto b = buffed();
                if (b.empty()) { ri.resume_point = 22; return; }
                std::vector<Intent> opts;
                for (auto u : b) opts.push_back(objChoice(ctx.controller, u));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Wildclaw Shaman: spend whose buff?");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point == 20) {
                GameObjectId u = takenObject(ctx);
                auto b = buffed();
                if (std::find(b.begin(), b.end(), u) == b.end())
                    u = b.empty() ? kInvalidId : b.front();
                if (u == kInvalidId) { ri.resume_point = 22; return; }
                if (costDueNow(ctx)) spend(u);
                ri.resume_point = 21;
            }
            if (ri.resume_point != 21) return;   // 22: the cost couldn't be paid
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.buffUnit(ctx.source);
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("WILDCLAW SHAMAN: buffed & readied me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 147;
        d.def_id = R"RB(ogn-147-298)RB";
        d.name = R"RB(Wildclaw Shaman)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-147/298)RB";
        d.collector_number = 147;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may spend a buff to buff me and ready me. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7abc938fee4ba397f52c8ea60d350857a7517b0c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_147(CardRegistry& r) {
    r.registerCard(147, std::make_unique<WildclawShaman>());
}

} // namespace riftbound
