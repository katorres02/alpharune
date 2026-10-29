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

// "When you conquer here, you may ready a friendly gear. If it's an Equipment,
//  you may detach it."

class VeiledTemple : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    // The "you may" and the gear are chosen when the trigger is finalized
    // (CR 383.3); the detach "you may" depends on the gear and is asked as the
    // ability resolves (resume_points 20/21, after pickTarget's 6..8).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    static constexpr int kDetachAsked = 20;
    static constexpr int kDetachDone = 21;

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto gears = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isGear() && obj.location.has_value() && obj.controller == ctx.controller)
                    out.push_back(id);
            return out;
        };
        auto& ri = ctx.state.chain.resuming;
        if (!ri.has_value() || ri->resume_point < kDetachAsked) {
            int conf = confirmOptional(ctx, "Veiled Temple: ready a friendly gear?",
                                       [&]() { return !gears().empty(); });
            if (conf == -1) return;
            if (conf == 0) return;
        }
        GameObjectId gear = kInvalidId;
        if (!ri.has_value() || ri->resume_point < kDetachAsked) {
            gear = pickTarget(ctx, "Veiled Temple: ready which gear?", gears());
            if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
            if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
            ctx.executor.readyObject(gear);
            ctx.events.logTrace("VEILED TEMPLE: readied " + ctx.state.getObject(gear).name);
        } else if (ri->resume_data.size() >= 3) {
            gear = static_cast<GameObjectId>(ri->resume_data[2]);
        }
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        auto attachedEquipment = [&]() {
            const auto& g = ctx.state.getObject(gear);
            return isEquipment(g) && g.attached_to.has_value();
        };
        // "If it's an Equipment, you may detach it."
        if (!ri.has_value()) {   // direct invocation (tests): yes
            if (attachedEquipment()) ctx.executor.unattachGear(gear);
            return;
        }
        if (ri->resume_point < kDetachAsked) {
            if (!attachedEquipment()) return;
            Intent no; no.type = IntentType::MakeChoice; no.player = ctx.controller;
            no.chosen_value = 0; no.choice_label = "No — Veiled Temple: detach the Equipment";
            Intent yes = no; yes.chosen_value = 1;
            yes.choice_label = "Yes — Veiled Temple: detach the Equipment";
            ctx.executor.requestChoice(ctx.controller, {no, yes}, "may: Veiled Temple: detach");
            ri->resume_point = kDetachAsked;
            return;
        }
        if (ri->resume_point == kDetachAsked) {
            ri->resume_point = kDetachDone;
            auto picked = ctx.executor.takeChoice();
            if (picked && picked->chosen_value.value_or(0) == 1 && attachedEquipment()) {
                ctx.executor.unattachGear(gear);
                ctx.events.logTrace("VEILED TEMPLE: detached Equipment");
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 535;
        d.def_id = R"RB(sfd-221-221)RB";
        d.name = R"RB(Veiled Temple)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-221/221)RB";
        d.collector_number = 221;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may ready a friendly gear. If it's an Equipment, you may detach it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f2b38874a09b3e3df3fe584ea77e84aa5423e37-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_535(CardRegistry& r) {
    r.registerCard(535, std::make_unique<VeiledTemple>());
}

} // namespace riftbound
