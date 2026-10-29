#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// Resume slot for "which of my moves this turn triggered this" — Akali makes
// no choices, so the item's resume state is free to carry it.
constexpr int kAkaliOrdinalSet = 50;

// "The first time I move each turn, [Add] [1][A].
//  The second time I move each turn, banish the top card of your Main Deck.
//  Until your next Ending Phase, you may play that card. (You still pay its
//  costs.)"
// Every move triggers; the ability numbers itself (per turn, in the order the
// triggers are put on the chain) when it is finalized and only the 1st and
// 2nd do anything.
class AkaliBrash : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType, GameObjectId) const override {
        // moves_this_turn never over-counts (effect moves aren't counted), so
        // this only drops moves that are surely the third or later.
        return state.objectExists(source) && state.getObject(source).moves_this_turn <= 2;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int ordinal = 0;
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point == kAkaliOrdinalSet &&
            !ri_opt->resume_data.empty()) {
            ordinal = ri_opt->resume_data[0];
        } else {
            auto& me = ctx.state.getObject(ctx.source);
            const int turn = ctx.state.turn.turn_number;
            if (me.card_counters["__akali_brash_turn"] != turn) {
                me.card_counters["__akali_brash_turn"] = turn;
                me.card_counters["__akali_brash_moves"] = 0;
            }
            ordinal = ++me.card_counters["__akali_brash_moves"];
            if (ri_opt.has_value()) {
                ri_opt->resume_point = kAkaliOrdinalSet;
                ri_opt->resume_data = {ordinal};
            }
        }
        if (finalizingChoices(ctx)) return;
        if (ordinal == 1) {
            ctx.executor.addFloatingEnergy(ctx.controller, 1);
            ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
            ctx.events.logTrace("AKALI, BRASH: first move -> [Add] [1][A]");
        } else if (ordinal == 2) {
            auto& ps = ctx.state.player(ctx.controller);
            if (ps.main_deck.empty()) return;
            const GameObjectId top = ps.main_deck.back();
            ctx.executor.banishObject(top);
            if (!ctx.state.objectExists(top) ||
                ctx.state.getObject(top).zone != ZoneType::Banishment) return;
            ps.banished_playable.push_back(top);
            ctx.events.logTrace("AKALI, BRASH: second move -> banished " +
                                ctx.state.getObject(top).name +
                                ", playable until your next Ending Phase");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1000;
        d.def_id = R"RB(rad-015-167)RB";
        d.name = R"RB(Akali, Brash)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-015/167)RB";
        d.collector_number = 15;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ionia)RB", R"RB(Akali)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB(The first time I move each turn, [Add] [1][A].
The second time I move each turn, banish the top card of your Main Deck. Until your next Ending Phase, you may play that card. (You still pay its costs.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/20997ddaef2cd714c36e5473c156faf331b80b7b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1000(CardRegistry& r) {
    r.registerCard(1000, std::make_unique<AkaliBrash>());
}

}  // namespace riftbound
