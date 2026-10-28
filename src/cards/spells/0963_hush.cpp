#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
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

class Hush : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasLegalTargets(const GameState& state, PlayerId) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it)
            if (it->is_spell && it->card_def_id != kInvalidId) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.chain.items.empty()) return;
        auto& top = ctx.state.chain.items.back();
        if (!top.is_spell || isUncounterable(ctx, top)) return;
        int pc = top.card_def_id != kInvalidId ? ctx.executor.cardDB().get(top.card_def_id).power_cost : 0;
        if (pc < 2) { ctx.events.logTrace("HUSH: cannot counter (power cost < 2)"); return; }
        auto countered = top.source;
        revertCounteredPlay(ctx, top);
        ctx.state.chain.items.pop_back();
        if (ctx.state.objectExists(countered)) disposeCounteredCard(ctx, countered);
        ctx.events.logTrace("HUSH: countered");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 963;
        d.def_id = R"RB(rad-041-167)RB";
        d.name = R"RB(Hush)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-041/167)RB";
        d.collector_number = 41;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([Reaction] Counter a spell with Power cost [A][A] or more.)RB";
        d.energy_cost = 1;
        d.power_cost = 2;
        d.keywords = []{KeywordSet k; k.set(Keyword::Reaction); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_963(CardRegistry& r) {
    r.registerCard(963, std::make_unique<Hush>());
}

}  // namespace riftbound
