#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// [Deflect]. "[E]: Choose an opponent. They must move a unit they control to
// my battlefield." 1v1: the opponent is chosen automatically. The opponent
// picks which of their units moves (any unit of theirs not already there).
// Nothing happens while I'm at my base (it is not a battlefield).
class AnglerWyrm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return ActivationCost{.exhaust = true}; }

    static std::vector<GameObjectId> movable(const GameState& state, PlayerId opp, BattlefieldId bf) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects) {
            if (!o.isUnit() || o.controller != opp || !o.location.has_value()) continue;
            auto obf = o.battlefieldId();
            if (obf && *obf == bf) continue;
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto here = ctx.state.getObject(ctx.source).battlefieldId();
        if (!here) return;
        const PlayerId opp = opponent(ctx.controller);
        auto units = movable(ctx.state, opp, *here);
        if (!ctx.state.chain.resuming.has_value()) {   // direct invocation
            if (!units.empty()) ctx.executor.moveToBattlefield(units.front(), *here);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == 0) {
            if (units.empty()) return;
            std::vector<Intent> opts;
            for (auto u : units) opts.push_back(objChoice(opp, u));
            ctx.executor.requestChoice(opp, std::move(opts),
                                       "Angler Wyrm: move one of your units to its battlefield");
            ri.resume_point = 1;
            return;
        }
        if (ri.resume_point == 1) {
            ri.resume_point = 2;
            GameObjectId u = takenObject(ctx);
            if (u == kInvalidId || std::find(units.begin(), units.end(), u) == units.end()) {
                if (units.empty()) return;
                u = units.front();
            }
            ctx.events.logTrace("ANGLER WYRM: opponent moves " + ctx.state.getObject(u).name +
                                " to battlefield #" + std::to_string(*here));
            ctx.executor.moveToBattlefield(u, *here);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1089;
        d.def_id = R"RB(rad-113-167)RB";
        d.name = R"RB(Angler Wyrm)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-113/167)RB";
        d.collector_number = 113;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB", R"RB(Dragon)RB"};   // as printed
        d.rarity = Rarity::Epic;
        d.energy_cost = 8;
        d.power_cost = 0;
        d.might = 8;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 1;
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[E]: Choose an opponent. They must move a unit they control to my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e805eba9add03fd3e1b1601b15c12a46fd403282-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1089(CardRegistry& r) {
    r.registerCard(1089, std::make_unique<AnglerWyrm>());
}

}  // namespace riftbound
