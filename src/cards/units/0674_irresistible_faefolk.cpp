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

class IrresistibleFaefolk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not at a BF — nothing to move to

        // Enemy units NOT already at my battlefield (at a base or elsewhere)
        // that may be chosen. The player picks which, at finalization.
        auto enemies_elsewhere = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value() || obj.untargetable_by_enemy) continue;
                auto obf = obj.battlefieldId();
                if (obf && *obf == *my_bf) continue;
                out.push_back(id);
            }
            return out;
        };
        int conf = confirmOptional(ctx,
            "Irresistible Faefolk: move an enemy unit to my battlefield?",
            [&]() { return !enemies_elsewhere().empty(); });
        if (conf == -1) return;
        if (conf == 0) { if (finalizingChoices(ctx)) return; return; }
        GameObjectId target = pickTarget(ctx, "Irresistible Faefolk: enemy unit to move",
                                         enemies_elsewhere());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (target == kInvalidId || !ctx.state.objectExists(target)) return;
        std::string name = ctx.state.getObject(target).name;
        ctx.executor.moveToBattlefield(target, *my_bf);
        ctx.events.logTrace("IRRESISTIBLE FAEFOLK: pulled " + name +
                             " to BF#" + std::to_string(static_cast<int>(*my_bf)));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 674;
        d.def_id = R"RB(unl-112-219)RB";
        d.name = R"RB(Irresistible Faefolk)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-112/219)RB";
        d.collector_number = 112;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I move to a battlefield, you may move an enemy unit to that battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6834e4b00a22dd6d7476d36884b43cf59b60c611-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_674(CardRegistry& r) {
    r.registerCard(674, std::make_unique<IrresistibleFaefolk>());
}

} // namespace riftbound
