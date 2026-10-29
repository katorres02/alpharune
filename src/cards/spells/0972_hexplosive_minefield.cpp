#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class HexplosiveMinefield : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill any number of friendly Bomb tokens. At end of turn, play that many
    // Bomb tokens to a single battlefield."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<int> bombs;
        for (const auto& [id, o] : ctx.state.objects)
            if (o.isGear() && o.controller == ctx.controller && o.location.has_value() &&
                o.card_def_id == kBombTokenCardDefId)
                bombs.push_back(static_cast<int>(id));
        std::sort(bombs.begin(), bombs.end());
        auto picked = pickSeveral(ctx, ctx.controller, "Hexplosive Minefield: kill a friendly Bomb",
                                  bombs, false, {}, 0, static_cast<int>(bombs.size()), 8);
        if (!picked) return;
        int killed = 0;
        for (int v : *picked) {
            const auto id = static_cast<GameObjectId>(v);
            if (!ctx.state.objectExists(id) || !ctx.state.getObject(id).location.has_value()) continue;
            ctx.executor.killObject(id);
            ++killed;
        }
        ctx.events.logTrace("HEXPLOSIVE MINEFIELD: killed " + std::to_string(killed) + " Bomb(s)");
        if (killed <= 0 || !ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters[kCount] += killed;
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::AtEndOfTurn;
        da.expires_on_turn = ctx.state.turn.turn_number;
        ctx.state.delayed_abilities.push_back(da);
    }
    // The delayed "At end of turn, play that many Bomb tokens to a single battlefield."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        const int n = me.card_counters[kCount];
        if (n <= 0) return;
        std::vector<std::string> labels;
        const auto bfs = battlefieldOptions(ctx.state, &labels);
        auto where = pickSeveral(ctx, ctx.controller, "Hexplosive Minefield: Bombs to which battlefield",
                                 bfs, true, labels, 1, 1, 8);
        if (!where) return;
        me.card_counters[kCount] = 0;
        if (where->empty()) return;
        for (int i = 0; i < n; ++i)
            createBombToken(ctx, ctx.controller, static_cast<BattlefieldId>((*where)[0]));
        ctx.events.logTrace("HEXPLOSIVE MINEFIELD: " + std::to_string(n) + " Bomb(s) played");
    }
private:
    static constexpr const char* kCount = "__minefield_bombs";
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 972;
        d.def_id = R"RB(rad-142-167)RB";
        d.name = R"RB(Hexplosive Minefield)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-142/167)RB";
        d.collector_number = 142;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury,Domain::Chaos};
        d.tags = {R"RB(Ziggs)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Kill any number of friendly Bomb tokens. At end of turn, play that many Bomb tokens to a single battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65b599befcc6ed873de0e7965bf497a805505327-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_972(CardRegistry& r) {
    r.registerCard(972, std::make_unique<HexplosiveMinefield>());
}

}  // namespace riftbound
