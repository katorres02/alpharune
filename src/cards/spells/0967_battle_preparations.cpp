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

class BattlePreparations : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // [Show Off] a gear is optional and costless, so it is taken whenever
    // possible: a gear in hand or a friendly gear on board (no reveal modeled).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        bool showed = false;
        for (auto& [id, o] : ctx.state.objects) {
            if (o.controller != ctx.controller && o.owner != ctx.controller) continue;
            if (o.card_type != CardType::Gear) continue;
            if (o.zone == ZoneType::Hand && o.owner == ctx.controller) { showed = true; break; }
            if (o.location.has_value() && o.controller == ctx.controller) { showed = true; break; }
        }
        if (!targets.empty() && ctx.state.objectExists(targets[0]))
            ctx.executor.giveTemporaryMight(targets[0], 2);
        if (showed) {
            int readied = 0;
            std::vector<GameObjectId> runes;
            for (auto& [rid, r] : ctx.state.objects)
                if (r.isRune() && r.controller == ctx.controller && r.location.has_value() && r.is_exhausted)
                    runes.push_back(rid);
            for (auto rid : runes) {
                if (readied >= 2) break;
                ctx.executor.readyObject(rid); ++readied;
            }
            ctx.events.logTrace("BATTLE PREPARATIONS: showed off, readied " + std::to_string(readied) + " runes");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 967;
        d.def_id = R"RB(rad-032-167)RB";
        d.name = R"RB(Battle Preparations)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-032/167)RB";
        d.collector_number = 32;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([Reaction] [Show Off] a gear. Give a unit +2 [M] this turn. If you showed off, ready up to 2 runes.)RB";
        d.energy_cost = 2;
        d.power_cost = 0;
        d.keywords = []{KeywordSet k; k.set(Keyword::Reaction); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_967(CardRegistry& r) {
    r.registerCard(967, std::make_unique<BattlePreparations>());
}

}  // namespace riftbound
