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

class MysticReversal : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Gain control of a spell. You may make new choices for it."
    // [Reaction] — only playable with a spell on the chain to take.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (const auto& it : state.chain.items)
            if (it.is_spell) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal;   // spell objects, top of the chain first
        for (auto it = ctx.state.chain.items.rbegin(); it != ctx.state.chain.items.rend(); ++it)
            if (it->is_spell) legal.push_back(it->source);
        GameObjectId pick = pickTarget(ctx, "Mystic Reversal: gain control of", legal);
        auto find_item = [&]() -> ChainItem* {
            for (auto& ci : ctx.state.chain.items)
                if (ci.is_spell && ci.source == pick) return &ci;
            return nullptr;
        };
        auto steal = [&](ChainItem& item) {
            item.controller = ctx.controller;
            if (ctx.state.objectExists(item.source))
                ctx.state.getObject(item.source).controller = ctx.controller;
            ctx.events.logTrace("MYSTIC REVERSAL: gained control of a spell");
        };
        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value()) {   // direct invocation: steal, keep choices
            if (pick == kInvalidId) return;
            if (auto* item = find_item()) steal(*item);
            return;
        }
        auto& ri = *ri_opt;
        if (ri.resume_point < 8) return;          // waiting for the pick
        if (ri.resume_point == 8) {
            ChainItem* item = pick == kInvalidId ? nullptr : find_item();
            if (!item) { ri.resume_point = 21; return; }
            steal(*item);
            std::vector<Intent> opts = {valChoice(ctx.controller, 0, "Keep its choices"),
                                        valChoice(ctx.controller, 1, "Make new choices")};
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Mystic Reversal: make new choices for it?");
            ri.resume_point = 20;
            return;
        }
        if (ri.resume_point != 20) return;
        ri.resume_point = 21;
        if (takenValue(ctx, 0) != 1) return;
        if (auto* item = find_item()) ctx.executor.makeNewChoices(item->id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 80;
        d.def_id = R"RB(ogn-080-298)RB";
        d.name = R"RB(Mystic Reversal)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-080/298)RB";
        d.collector_number = 80;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Gain control of a spell. You may make new choices for it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/298fe91f9d76086b7d77880e11016ed46389b61b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_80(CardRegistry& r) {
    r.registerCard(80, std::make_unique<MysticReversal>());
}

} // namespace riftbound
