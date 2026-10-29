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

// "[Deathknell] — You may kill a unit or gear here." "Here" is where I died
// (last-known location, CR 383.2.c.3.b): my battlefield or my base. The "you
// may" and the choice of unit / gear are made as the ability is finalized
// (CR 383.3); a choice no longer legal at resolution does nothing.
class MournfulSpirit : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        const auto where = self.location.has_value() ? self.location : self.last_location;
        if (!where.has_value()) return;
        const LocationId here = *where;
        auto candidates = [&ctx, here]() {
            std::vector<GameObjectId> out;
            for (const auto& [id, o] : ctx.state.objects) {
                if (id == ctx.source || !o.location.has_value() || *o.location != here) continue;
                if (!o.isUnit() && !o.isGear()) continue;
                if (o.controller != ctx.controller && o.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            return out;
        };
        int go = confirmOptional(ctx, "Mournful Spirit: kill a unit or gear here?",
                                 [&]() { return !candidates().empty(); });
        if (go <= 0) return;
        GameObjectId t = pickTarget(ctx, "Mournful Spirit: kill which unit or gear", candidates());
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        const auto& o = ctx.state.getObject(t);
        if (!o.location.has_value() || *o.location != here) return;
        ctx.events.logTrace("MOURNFUL SPIRIT: deathknell -> kill " + o.name);
        ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1102;
        d.def_id = R"RB(rad-126-167)RB";
        d.name = R"RB(Mournful Spirit)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-126/167)RB";
        d.collector_number = 126;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spirit)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 1;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] You may kill a unit or gear here. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6d8500ac7efc5382985f566212d88ac8f41c135d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1102(CardRegistry& r) {
    r.registerCard(1102, std::make_unique<MournfulSpirit>());
}

}  // namespace riftbound
