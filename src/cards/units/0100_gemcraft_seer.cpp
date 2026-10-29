#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class GemcraftSeer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Vision]" on me is engine-handled (TriggerManager::onCardPlayed).
    // "Other friendly units have [Vision]": the keyword is granted by aura, and
    // because the engine's play-time Vision look reads printed keywords only,
    // the look itself for a friendly unit played while I'm on the board is
    // this card's WhenYouPlayAUnit trigger (one Seer does it; a unit with
    // printed [Vision] already looks on its own).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject&) {
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;  // "other"
                if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                    continue;
                auraKeyword(u, sid, Keyword::Vision);
            }
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        if (subject == kInvalidId || subject == source || !state.objectExists(subject))
            return false;
        const auto& played = state.getObject(subject);
        if (!played.isUnit() || played.keywords.has(Keyword::Vision)) return false;
        // Only the lowest-id Seer its controller has on the board looks.
        for (const auto& [id, obj] : state.objects)
            if (id < source && obj.card_def_id == cardDefId() &&
                obj.controller == controller && obj.location.has_value())
                return false;
        return true;
    }
    // [Vision]: "look at the top card of your Main Deck. You may recycle it."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& deck = ctx.state.player(ctx.controller).main_deck;
        if (deck.empty()) return;
        const GameObjectId top = deck.back();
        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value() || ri_opt->resume_point < 20) {
            const auto& t = ctx.state.getObject(top);
            ctx.events.emit(CardRevealedEvent{top, t.card_def_id, t.owner,
                /*revealed_to_all=*/false, /*revealed_to=*/ctx.controller,
                ZoneType::MainDeck});
        }
        int recycle = 0;
        if (ri_opt.has_value()) {
            auto& ri = *ri_opt;
            if (ri.resume_point < 20) {
                std::vector<Intent> opts = {valChoice(ctx.controller, 0, "Keep it on top"),
                                            valChoice(ctx.controller, 1, "Recycle it")};
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Vision (Gemcraft Seer): recycle the top card?");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point != 20) return;
            ri.resume_point = 21;
            recycle = takenValue(ctx, 0);
        }
        if (recycle != 1 || deck.empty() || deck.back() != top) return;
        deck.pop_back();
        deck.insert(deck.begin(), top);
        ctx.events.logTrace("VISION (Gemcraft Seer): recycled the top card");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 100;
        d.def_id = R"RB(ogn-100-298)RB";
        d.name = R"RB(Gemcraft Seer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-100/298)RB";
        d.collector_number = 100;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
Other friendly units have [Vision].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4b6965bd592a5f602ab932094d677b6c85b2372b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_100(CardRegistry& r) {
    r.registerCard(100, std::make_unique<GemcraftSeer>());
}

} // namespace riftbound
