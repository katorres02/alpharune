#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class OvertOperation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "For each friendly unit, you may spend its buff to ready it. Then buff
    // all friendly units." One yes/no per buffed friendly unit (resume point
    // 20, resume_data[6] = index into the friendly-unit list).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        auto spend_and_ready = [&](GameObjectId id) {
            auto& u = ctx.state.getObject(id);
            if (!ctx.executor.spendBuff(id)) return;   // removes its +1 [M]
            ctx.executor.readyObject(id);
            ctx.events.logTrace("OVERT OPERATION: spent buff to ready " + u.name);
        };
        const auto units = friendly_units();
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value()) {
            auto& ri = *ri_opt;
            while (ri.resume_data.size() < 7) ri.resume_data.push_back(0);
            size_t i = static_cast<size_t>(ri.resume_data[6]);
            if (ri.resume_point == 20) {          // answer for units[i]
                if (takenValue(ctx, 0) == 1 && i < units.size()) spend_and_ready(units[i]);
                ++i;
            }
            while (i < units.size() && ctx.state.getObject(units[i]).buff_count <= 0) ++i;
            ri.resume_data[6] = static_cast<int32_t>(i);
            if (i < units.size()) {
                const auto& u = ctx.state.getObject(units[i]);
                std::vector<Intent> opts = {valChoice(ctx.controller, 0, "Keep " + u.name + "'s buff"),
                                            valChoice(ctx.controller, 1, "Spend it to ready " + u.name)};
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Overt Operation: spend " + u.name + "'s buff to ready it?");
                ri.resume_point = 20;
                return;
            }
            ri.resume_point = 21;
        } else {
            for (auto id : units) spend_and_ready(id);   // direct invocation: yes to all
        }
        // "Then buff all friendly units." (buffUnit skips units that have one.)
        for (auto id : friendly_units()) ctx.executor.buffUnit(id);
        ctx.events.logTrace("OVERT OPERATION: buffed all friendly units");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 153;
        d.def_id = R"RB(ogn-153-298)RB";
        d.name = R"RB(Overt Operation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-153/298)RB";
        d.collector_number = 153;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
For each friendly unit, you may spend its buff to ready it. Then buff all friendly units. (Each one that doesn't have a buff gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/69dba13c930ba3962851346d2bc6cbeb4ca48455-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_153(CardRegistry& r) {
    r.registerCard(153, std::make_unique<OvertOperation>());
}

} // namespace riftbound
