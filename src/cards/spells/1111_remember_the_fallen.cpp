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

constexpr size_t kRtfPickSlot = 8;   // pickSeveral state: resume_data[8 .. 12]
constexpr int kRtfMax = 3;

// "Play up to 3 units and/or gear from your trash, each with Energy cost no
// more than [3] and Power cost no more than [A], ignoring their costs." As it
// resolves the controller picks the cards one at a time (or stops), then a
// play location for each (CR 355.2.a), then they are played in that order.
class RememberTheFallen : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        const CardDB& db = ctx.executor.cardDB();
        std::vector<int> options;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& o = ctx.state.getObject(cid);
            if (!o.isUnit() && !o.isGear()) continue;
            if (o.card_def_id == kInvalidId || !db.all().count(o.card_def_id)) continue;
            const auto& def = db.get(o.card_def_id);
            if (def.energy_cost > 3 || def.power_cost > 1) continue;
            options.push_back(static_cast<int>(cid));
        }
        auto picks = pickSeveral(ctx, ctx.controller,
                                 "Remember the Fallen: play a unit or gear from your trash",
                                 options, /*values=*/false, {}, /*min=*/0, kRtfMax, kRtfPickSlot);
        if (!picks.has_value()) return;   // waiting on the agent
        std::vector<std::pair<GameObjectId, LocationId>> plays;
        for (int p : *picks) {
            const auto cid = static_cast<GameObjectId>(p);
            if (!ctx.state.objectExists(cid) || ctx.state.getObject(cid).zone != ZoneType::Trash)
                continue;
            LocationId where;
            const int r = pickPlayLocation(ctx, cid, where, PlayerId::None,
                                           "Remember the Fallen: play " +
                                           ctx.state.getObject(cid).name + " where?");
            if (r < 0) return;   // waiting on the agent
            if (r == 0) continue;   // nowhere to play it: it stays in the trash
            plays.emplace_back(cid, where);
        }
        for (const auto& [cid, where] : plays) {
            if (!ctx.state.objectExists(cid)) continue;
            auto it = std::find(ps.trash.begin(), ps.trash.end(), cid);
            if (it == ps.trash.end()) continue;
            ps.trash.erase(it);
            ctx.events.logTrace("REMEMBER THE FALLEN: plays " + ctx.state.getObject(cid).name +
                                " from trash, ignoring its cost");
            ctx.executor.playIgnoringCost(ctx.controller, cid, where);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1111;
        d.def_id = R"RB(rad-135-167)RB";
        d.name = R"RB(Remember the Fallen)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-135/167)RB";
        d.collector_number = 135;
        d.artist = R"RB(Original Force)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 8;
        d.power_cost = 2;
        d.ability_text = R"RB(Play up to 3 units and/or gear from your trash, each with Energy cost no more than [3] and Power cost no more than [A], ignoring their costs.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0bfd45446d5ffb816438ae2668ad4e5b7f188ad3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1111(CardRegistry& r) {
    r.registerCard(1111, std::make_unique<RememberTheFallen>());
}

}  // namespace riftbound
