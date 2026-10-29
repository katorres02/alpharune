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

class Flash : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }

    // "Move up to 2 friendly units to base." Up to two sequential picks, each
    // with a "Done" option, made as the spell is finalized (CR 355).
    // resume_point 20 = publish pick, 21 = consume pick, 22 = done;
    // resume_data[6], [7] = the picked units (kInvalidId when not picked).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId controller = ctx.controller;
        auto movable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isUnit() && obj.controller == controller &&
                   obj.location.has_value() && !obj.isAtBase();
        };
        auto candidates = [&](GameObjectId exclude) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (id != exclude && movable(id)) out.push_back(id);
            return out;
        };
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain): move the first two.
            auto c = candidates(kInvalidId);
            for (size_t i = 0; i < c.size() && i < 2; ++i) ctx.executor.moveToBase(c[i]);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 8) ri.resume_data.push_back(0);
        auto prompt = [&]() {
            const auto first = static_cast<GameObjectId>(ri.resume_data[6]);
            auto c = candidates(first);
            if (c.empty()) { ri.resume_point = 22; return; }
            std::vector<Intent> opts;
            for (auto id : c) opts.push_back(objChoice(controller, id));
            opts.push_back(valChoice(controller, 0, "Done"));
            ctx.executor.requestChoice(controller, std::move(opts),
                                       "Flash: move a friendly unit to base");
            ri.resume_point = 21;
        };
        if (ri.resume_point < 20) {
            ri.resume_data[6] = static_cast<int32_t>(kInvalidId);
            ri.resume_data[7] = static_cast<int32_t>(kInvalidId);
            ri.resume_point = 20;
            prompt();
            if (ri.resume_point == 21) return;
        }
        if (ri.resume_point == 21) {
            GameObjectId picked = takenObject(ctx);
            const auto first = static_cast<GameObjectId>(ri.resume_data[6]);
            auto c = candidates(first);
            if (picked == kInvalidId || std::find(c.begin(), c.end(), picked) == c.end()) {
                ri.resume_point = 22;   // "Done"
            } else if (first == kInvalidId) {
                ri.resume_data[6] = static_cast<int32_t>(picked);
                prompt();
                if (ri.resume_point == 21) return;
            } else {
                ri.resume_data[7] = static_cast<int32_t>(picked);
                ri.resume_point = 22;
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ri.resume_point != 22) return;
        for (int slot : {6, 7}) {
            const auto u = static_cast<GameObjectId>(ri.resume_data[slot]);
            if (u == kInvalidId || !movable(u)) continue;   // no longer legal
            ctx.executor.moveToBase(u);
            ctx.events.logTrace("FLASH: moved " + ctx.state.getObject(u).name + " to base");
        }
        ri.resume_point = 23;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 309;
        d.def_id = R"RB(ogs-011-024)RB";
        d.name = R"RB(Flash)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-011/024)RB";
        d.collector_number = 11;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Move up to 2 friendly units to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d9cc1c13b75933e509e642213f13359350cd3f9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_309(CardRegistry& r) {
    r.registerCard(309, std::make_unique<Flash>());
}

} // namespace riftbound
