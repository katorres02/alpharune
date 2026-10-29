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

class EkkoRecurrent : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    // "[Deathknell] — Recycle me to ready your runes." "Recycle me" is the
    // ability's base cost (CR 383.3.b), paid as it is finalized; without it
    // (I'm no longer in the trash) the runes aren't readied.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        if (!costAlreadyPaid(ctx)) {
            if (me.zone != ZoneType::Trash) {
                if (finalizingChoices(ctx)) return;
                return;   // can't pay the cost
            }
            if (costDueNow(ctx)) {
                auto& trash = ctx.state.player(me.owner).trash;
                trash.erase(std::remove(trash.begin(), trash.end(), ctx.source), trash.end());
                ctx.executor.recycleCards(ctx.controller, {ctx.source});
                ctx.events.logTrace("EKKO: recycled me");
            }
        }
        if (finalizingChoices(ctx)) return;   // costs end here (CR 383.3)
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted || !obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        ctx.events.logTrace("EKKO: deathknell -> readied " +
                             std::to_string(readied) + " runes");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 110;
        d.def_id = R"RB(ogn-110-298)RB";
        d.name = R"RB(Ekko, Recurrent)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-110/298)RB";
        d.collector_number = 110;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ekko)RB", R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.)
[Deathknell] — Recycle me to ready your runes. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65da19325b6def53d33c07bc1aa8f91fd2f1e723-744x1039.png)RB";
        d.banned = true;  // tournament ban (2026-09-18)
        return d;
    }();
};

}  // anonymous namespace

void register_card_110(CardRegistry& r) {
    r.registerCard(110, std::make_unique<EkkoRecurrent>());
}

} // namespace riftbound
