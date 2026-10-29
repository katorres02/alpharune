#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// [Deploy]. "When you play this, banish a unit here." and "When you hold
// here, kill this to play the banished card, ignoring its cost. (Tokens
// aren't cards.)"
//
// The banished card is remembered on this gear (card_counters). "Kill this"
// is the hold ability's cost (CR 383.3.b), paid as it is finalized; its
// controller may decline to pay it (the ability then isn't finalized). With
// no banished card left to play (it was a token, or it has left Banishment)
// the cost isn't offered. The card is played by this gear's controller, who
// chooses its location (CR 355.2), and so controls it.
class DocksideLockUp : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenYouHoldHere};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenYouHoldHere) { onHold(ctx); return; }
        onPlayThis(ctx);
    }

private:
    static constexpr const char* kBanished = "__dlu_banished";
    static constexpr size_t kSlot = 12;   // resume_data slot: the card to play (+1)

    static std::vector<GameObjectId> unitsHere(const GameState& state, GameObjectId self,
                                               PlayerId me) {
        if (!state.objectExists(self)) return {};
        const auto here = state.getObject(self).battlefieldId();
        if (!here) return {};
        const BattlefieldId bf = *here;
        return chooseableUnits(state, me, [bf](const GameObject& o) {
            auto obf = o.battlefieldId();
            return obf && *obf == bf;
        });
    }

    void onPlayThis(CardContext& ctx) {
        const GameObjectId unit =
            pickTarget(ctx, "Dockside Lock-Up: banish a unit here",
                       unitsHere(ctx.state, ctx.source, ctx.controller));
        if (finalizingChoices(ctx)) return;
        if (ctx.executor.hasPendingChoice()) return;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        const bool is_token = ctx.state.getObject(unit).isToken();
        const std::string name = ctx.state.getObject(unit).name;
        ctx.executor.banishObject(unit);
        if (!is_token && ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters[kBanished] = static_cast<int>(unit) + 1;
        ctx.events.logTrace("DOCKSIDE LOCK-UP: banished " + name);
    }

    // The card this gear banished, if it is still in its owner's Banishment.
    static GameObjectId banishedCard(const GameState& state, GameObjectId self) {
        if (!state.objectExists(self)) return kInvalidId;
        const auto& c = state.getObject(self).card_counters;
        auto it = c.find(kBanished);
        if (it == c.end() || it->second <= 0) return kInvalidId;
        const GameObjectId card = static_cast<GameObjectId>(it->second - 1);
        if (!state.objectExists(card)) return kInvalidId;
        const auto& obj = state.getObject(card);
        if (obj.zone != ZoneType::Banishment || obj.isToken()) return kInvalidId;
        const auto& ban = state.player(obj.owner).banishment;
        if (std::find(ban.begin(), ban.end(), card) == ban.end()) return kInvalidId;
        return card;
    }

    void onHold(CardContext& ctx) {
        auto& ro = ctx.state.chain.resuming;
        // Remember the card before the cost kills this gear.
        if (ro.has_value() && !costAlreadyPaid(ctx)) {
            if (ro->resume_data.size() <= kSlot) ro->resume_data.resize(kSlot + 1, 0);
            if (ro->resume_data[kSlot] == 0)
                ro->resume_data[kSlot] =
                    static_cast<int32_t>(banishedCard(ctx.state, ctx.source)) + 1;
        }
        auto card_to_play = [&]() -> GameObjectId {
            if (ro.has_value() && ro->resume_data.size() > kSlot && ro->resume_data[kSlot] > 0)
                return static_cast<GameObjectId>(ro->resume_data[kSlot] - 1);
            return banishedCard(ctx.state, ctx.source);
        };
        const bool paid = costAlreadyPaid(ctx);
        const int go = confirmOptional(ctx, "Dockside Lock-Up: kill this to play the banished card?",
            [&]() {
                if (card_to_play() == kInvalidId) return false;
                return paid || (ctx.state.objectExists(ctx.source) &&
                                ctx.state.getObject(ctx.source).location.has_value());
            });
        if (go <= 0) return;
        if (costDueNow(ctx)) {
            if (!ctx.state.objectExists(ctx.source) ||
                !ctx.state.getObject(ctx.source).location.has_value()) return;
            ctx.executor.killObject(ctx.source);
            ctx.events.logTrace("DOCKSIDE LOCK-UP: killed this");
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)

        const GameObjectId card = card_to_play();
        if (card == kInvalidId || !ctx.state.objectExists(card)) return;
        if (ctx.state.getObject(card).zone != ZoneType::Banishment) return;   // left in response
        LocationId where;
        if (pickPlayLocation(ctx, card, where, PlayerId::None,
                             "Dockside Lock-Up: play the banished card where?") <= 0) return;
        auto& obj = ctx.state.getObject(card);
        auto& ban = ctx.state.player(obj.owner).banishment;
        ban.erase(std::remove(ban.begin(), ban.end(), card), ban.end());
        ctx.events.logTrace("DOCKSIDE LOCK-UP: play " + obj.name + " ignoring its cost");
        ctx.executor.playIgnoringCost(ctx.controller, card, where);
    }

    const CardDef def_ = [] {
        CardDef d;
        d.id = 1112;
        d.def_id = R"RB(rad-136-167)RB";
        d.name = R"RB(Dockside Lock-Up)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-136/167)RB";
        d.collector_number = 136;
        d.artist = R"RB(Yeon Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};   // gallery said "Device"; the printed card has no tag
        d.rarity = Rarity::Epic;
        d.energy_cost = 6;
        d.power_cost = 2;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, banish a unit here.
When you hold here, kill this to play the banished card, ignoring its cost. (Tokens aren't cards.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f477ccc474cb16e4e9d681b388f0673f77fae5b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1112(CardRegistry& r) {
    r.registerCard(1112, std::make_unique<DocksideLockUp>());
}

}  // namespace riftbound
