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

class TheZeroDrive : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Equip [1][B] ──
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return payEquipAndAttach(ctx, ctx.source, unit, /*energy=*/1, Domain::Mind);
    }

    // ── "[Deathknell] — Banish me." (effect text given to the bearer) ──
    // A death trigger, not a replacement: the bearer dies (its own Deathknell
    // and "when a unit dies" abilities fire), then it is banished from the
    // trash and recorded for the replay ability.
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIDie; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                           const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.objectExists(unit)) return;
        const auto& u = ctx.state.getObject(unit);
        if (u.zone != ZoneType::Trash) return;   // already gone elsewhere
        ctx.events.logTrace("THE ZERO DRIVE: Deathknell — banish " + u.name);
        ctx.executor.banishObject(unit);
        if (ctx.state.objectExists(unit) && ctx.state.getObject(unit).zone == ZoneType::Banishment)
            ctx.state.getObject(ctx.source).tracked_objects.push_back(unit);
    }

    // ── Replay ability: [3][B], Banish this — only if unattached ──
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // [3][B]; "Banish this" is performed in onActivate (no banish-self cost flag).
        return {.exhaust = false, .energy = 3, .power = 1, .power_domain = Domain::Mind};
    }
    // "(Use only if unattached.)" — and only with something to replay.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!Card::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        if (!state.objectExists(source)) return false;
        const auto& g = state.getObject(source);
        return !g.attached_to.has_value() && !g.tracked_objects.empty();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& gear = ctx.state.getObject(ctx.source);
        if (gear.attached_to.has_value()) return;  // guard: only if unattached

        // Snapshot the recorded units; play each ignoring costs, each to the
        // location I choose (CR 355.2). The locations are all chosen first,
        // before anything moves (the ability re-enters while the agent decides).
        std::vector<GameObjectId> to_play = gear.tracked_objects;
        std::vector<LocationId> where(to_play.size(), LocationId{BaseLocation{ctx.controller}});
        std::vector<bool> playable(to_play.size(), false);
        for (size_t i = 0; i < to_play.size(); ++i) {
            if (!ctx.state.objectExists(to_play[i])) continue;
            const int r = pickPlayLocation(ctx, to_play[i], where[i], PlayerId::None,
                                           "The Zero Drive: play " +
                                               ctx.state.getObject(to_play[i]).name + " where?");
            if (r == -1) return;   // waiting for the agent
            playable[i] = r == 1;
        }
        ctx.state.getObject(ctx.source).tracked_objects.clear();
        for (size_t i = 0; i < to_play.size(); ++i) {
            const GameObjectId uid = to_play[i];
            if (!playable[i] || !ctx.state.objectExists(uid)) continue;
            auto& u = ctx.state.getObject(uid);
            // Remove the duplicate banishment-vector entry before re-zoning,
            // mirroring Dazzling Aurora's banish→play handoff.
            auto& bz = ctx.state.player(u.owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), uid), bz.end());
            ctx.executor.playIgnoringCost(ctx.controller, uid, where[i]);
            ctx.events.logTrace("THE ZERO DRIVE: replayed banished unit " + u.name);
        }

        // "Banish this" cost — route the gear itself to banishment.
        ctx.executor.banishObject(ctx.source);
    }

private:
    // Pay [energy] ready runes + recycle one matching-domain rune for power,
    // then attach. Mirrors equip_cards.cpp::standardEquip with an upfront
    // affordability pre-check so an unaffordable equip never half-pays.
    static bool payEquipAndAttach(CardContext& ctx, GameObjectId gear_id,
                                   GameObjectId unit_id, int energy_cost,
                                   Domain domain) {
        auto& state = ctx.state;
        auto player = ctx.controller;
        auto& ps = state.player(player);
        auto base_loc = BaseLocation{player};

        int ready_count = 0;
        GameObjectId domain_rune = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            if (!obj.is_exhausted) ready_count++;
            if (domain_rune == kInvalidId) {
                for (auto d : obj.domains) {
                    if (d == domain) { domain_rune = id; break; }
                }
            }
        }
        if (ready_count < energy_cost) return false;
        if (domain_rune == kInvalidId) return false;

        if (energy_cost > 0) {
            int remaining = energy_cost;
            for (auto& [id, obj] : state.objects) {
                if (remaining <= 0) break;
                if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
                if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                obj.is_exhausted = true;
                remaining--;
            }
        }
        {
            auto& dr = state.getObject(domain_rune);
            dr.location = std::nullopt;
            dr.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), domain_rune);
        }

        auto& gear = state.getObject(gear_id);
        auto& unit = state.getObject(unit_id);
        gear.attached_to = unit_id;
        unit.attachments.push_back(gear_id);
        gear.location = unit.location;
        gear.zone = unit.zone;
        unit.attachment_might_bonus += gear.might_bonus;
        unit.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 412;
        d.def_id = R"RB(sfd-090-221)RB";
        d.name = R"RB(The Zero Drive)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-090/221)RB";
        d.collector_number = 90;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][B] ([1][B]: Attach this to a unit you control.)
[3][B], Banish this: Play all units banished with this, ignoring their costs. (Use only if unattached.))RB";
        d.effect_text = R"RB([Deathknell] — Banish me. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cc80cbfa1a64ad22dc5f7df870e9206f6938f7f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_412(CardRegistry& r) {
    r.registerCard(412, std::make_unique<TheZeroDrive>());
}

} // namespace riftbound
