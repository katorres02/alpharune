#include "cards/card.h"
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

class GentleGemdragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me or another Dragon, ready up to 2 runes."
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouPlayAUnit};
    }
    // WhenYouPlayAUnit carries the played unit as its subject (it never fires
    // for my own play — WhenYouPlayMe does): only another Dragon counts.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType trigger, GameObjectId subject) const override {
        if (trigger != TriggerType::WhenYouPlayAUnit) return true;
        if (subject == kInvalidId || subject == source || !state.objectExists(subject)) return false;
        const auto& tags = state.getObject(subject).tags;
        return std::find(tags.begin(), tags.end(), "Dragon") != tags.end();
    }
    static std::vector<GameObjectId> exhaustedRunes(const GameState& state, PlayerId controller) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects)
            if (obj.isRune() && obj.controller == controller && obj.is_exhausted &&
                obj.location.has_value())
                out.push_back(id);
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // The player picks which runes (up to 2: readying is never worse, so
        // as many as there are, up to 2).
        auto [a, b] = pickTargetPair(ctx, "Gentle Gemdragon: ready runes",
            exhaustedRunes(ctx.state, ctx.controller),
            [&](GameObjectId first) {
                auto v = exhaustedRunes(ctx.state, ctx.controller);
                v.erase(std::remove(v.begin(), v.end(), first), v.end());
                return v;
            });
        auto& ri = ctx.state.chain.resuming;
        if (ri.has_value() && ri->resume_point < 13) return;   // waiting for agent
        int readied = 0;
        for (auto r : {a, b}) {
            if (r == kInvalidId || !ctx.state.objectExists(r)) continue;
            if (!ctx.state.getObject(r).is_exhausted) continue;
            ctx.executor.readyObject(r);
            ++readied;
        }
        if (readied > 0)
            ctx.events.logTrace("GENTLE GEMDRAGON: readied " + std::to_string(readied) + " rune(s)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 666;
        d.def_id = R"RB(unl-104-219)RB";
        d.name = R"RB(Gentle Gemdragon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-104/219)RB";
        d.collector_number = 104;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 8;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me or another Dragon, ready up to 2 runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65fcaff267b7f27cbda09ad23f2449188195d28e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_666(CardRegistry& r) {
    r.registerCard(666, std::make_unique<GentleGemdragon>());
}

} // namespace riftbound
