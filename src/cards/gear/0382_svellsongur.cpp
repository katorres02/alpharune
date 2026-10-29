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

class Svellsongur : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "As this is attached to a unit, copy that unit's text to this
    //  Equipment's effect text for as long as this is attached to it." Effect
    // text is appended to the bearer's rules text (CR 136.2.c), so the bearer
    // has its own abilities twice. Modeled by proxying the copied card:
    //  - the copy is taken after the attach (Vendetta rules FAQ: it doesn't
    //    see its own attach) at the next aura pass, and kept while attached to
    //    that unit (GameObject::copied_effect_text_def);
    //  - its triggered abilities trigger again for the bearer, each its own
    //    chain item (TriggerManager::fireTrigger reads the copy);
    //  - its activated abilities are granted to the bearer a second time
    //    (GrantedAbilityRef, as Heimerdinger).
    // APPROX: the copied card's static / passive abilities and keyword values
    // are not applied a second time.
    bool hasEquipAbility() const override { return true; }
    // [Weaponmaster] prices it through this (CR 821.1.c).
    EquipCostSpec equipCost(const GameState&, GameObjectId, GameObjectId) const override {
        return {true, 1, {{Domain::Calm}}};   // [Equip] [1][G]
    }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return standardEquip(ctx, ctx.source, unit,
                                         /*energy_cost=*/1, Domain::Calm);
    }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, g] : state.objects) {
            if (g.card_def_id != cardDefId() || g.controller != controller) continue;
            auto& latched = g.card_counters["__svellsongur_bearer"];   // bearer id + 1
            if (!g.location.has_value() || !g.attached_to.has_value() ||
                !state.objectExists(*g.attached_to)) {
                g.copied_effect_text_def = kInvalidId;
                latched = 0;
                continue;
            }
            const auto& bearer = state.getObject(*g.attached_to);
            if (latched != static_cast<int>(bearer.id) + 1) {
                latched = static_cast<int>(bearer.id) + 1;
                g.copied_effect_text_def = bearer.card_def_id;   // the copy is taken now
            }
        }
        // Grant the copied activated abilities (the action generator skips an
        // index the copied card doesn't have).
        for (auto& [id, g] : state.objects) {
            if (g.card_def_id != cardDefId() || g.controller != controller) continue;
            if (g.copied_effect_text_def == kInvalidId || !g.attached_to.has_value()) continue;
            if (!state.objectExists(*g.attached_to)) continue;
            auto& refs = state.getObject(*g.attached_to).granted_abilities;
            for (int k = 0; k < kMaxCopiedAbilities; ++k) {
                bool have = false;
                for (const auto& r : refs)
                    if (r.source_def_id == g.copied_effect_text_def && r.ability_index == k)
                        have = true;
                if (!have) refs.push_back({g.copied_effect_text_def, k});
            }
        }
    }

private:
    static constexpr int kMaxCopiedAbilities = 3;
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 382;
        d.def_id = R"RB(sfd-059-221)RB";
        d.name = R"RB(Svellsongur)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-059/221)RB";
        d.collector_number = 59;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][G] ([1][G]: Attach this to a unit you control.)
As this is attached to a unit, copy that unit's text to this Equipment's effect text for as long as this is attached to it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bc254398dfb5db217327b56862011a2fd6020789-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_382(CardRegistry& r) {
    r.registerCard(382, std::make_unique<Svellsongur>());
}

} // namespace riftbound
