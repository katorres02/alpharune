#pragma once
/// @file game_object.h
/// Runtime representation of a card/token on the board or in a zone.
///
/// CardDef is the static template (from registry.json).
/// GameObject is the mutable instance that tracks current state
/// (damage, exhaustion, temporary mods, location, attachments, etc).

#include "types.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace riftbound {

struct GameObject {
    GameObjectId id = kInvalidId;
    CardDefId card_def_id = kInvalidId;  // link to static card data
    PlayerId owner = PlayerId::None;
    PlayerId controller = PlayerId::None;

    // ── Identity (copied from CardDef, may be modified by layers) ──
    std::string name;
    CardType card_type = CardType::Unit;
    SuperType super_type = SuperType::None;
    std::vector<std::string> tags;
    std::vector<Domain> domains;

    // ── Unit stats ──
    int base_might = 0;
    int current_might = 0;  // after layers (assault, shield, buffs, auras)
    int damage_marked = 0;

    // ── State ──
    bool is_exhausted = false;
    bool is_stunned = false;
    bool is_hidden = false;             // facedown at a battlefield
    BattlefieldId hidden_at = kInvalidId; // which BF it's hidden at
    int hidden_on_turn = -1;            // turn it was hidden (gains Reaction next turn)

    // ── Empowered (Vendetta, CR 442/443/828) ──
    // Number of Empowered statuses. 0 = not Empowered. Normally binary (0/1);
    // objects allowed to be Empowered multiple times (Kayle, Justified) stack
    // up to Card::maxEmpower(). Cleared when the object leaves the board.
    int empowered = 0;
    // Deferred end-of-turn flip set by "Empower X. Disempower it at end of
    // turn" (-1) and "Disempower X. Empower it at end of turn" (+1). Applied
    // and cleared by the expiration step.
    int empower_flip_eot = 0;
    // Aura-computed reduction to THIS object's [Empower] energy cost (Risen
    // Altar). Reset each recalculateAuras.
    int empower_cost_reduction = 0;
    bool isEmpowered() const { return empowered > 0; }

    // ── Flow / "then banish this" (Vendetta) ──
    // When set on a spell, it is banished instead of going to trash when it
    // leaves the chain. Set when a spell is played for its Flow cost.
    bool banish_on_leave_chain = false;

    // ── Per-turn damage tracking ──
    // Total damage dealt to this object this turn, even if since healed
    // (Affectionate Poro: "if I haven't been dealt damage this turn").
    int damage_taken_this_turn = 0;
    // Recomputed each recalculateAuras: prevent all damage that ENEMY spells
    // and abilities would deal to me (Esteemed Hierophant).
    bool prevent_enemy_spell_ability_damage = false;
    // Recomputed each recalculateAuras: enemy spells and abilities can't move
    // me (Jagged Cutlass).
    bool cant_be_moved_by_enemy = false;
    // "It can't be chosen by enemy spells and abilities this turn" (Twilight
    // Shroud). ORed into untargetable_by_enemy by the aura pass; cleared at
    // end of turn.
    bool untargetable_by_enemy_this_turn = false;

    // ── Counted damage prevention (Ki Barrier: "prevent the next 7 damage") ──
    // Remaining damage to prevent this turn. dealDamage consumes it; expires at
    // end of turn.
    int prevent_damage_remaining = 0;

    // ── Intrinsic token-ness (CR 185.1) ──
    bool is_token = false;

    // ── A gear that is also a unit (CR 178 multiple types; Orianna, Lady of
    // Clockwork: "It becomes a unit gear"). isUnit() answers true while set;
    // isGear() still does. Maintained by the effect that grants it.
    bool also_unit = false;

    // ── Last-known information (CR 383.2.c.3.b / 429.1.a.1.b) ──
    // Snapshot taken just before this object leaves the board by dying, so
    // "When I die" abilities read the state from before the death. The kill
    // paths fill it; it stays valid while the death trigger resolves.
    struct LastKnown {
        bool valid = false;
        int might = 0;
        int empowered = 0;
        int buff_count = 0;
        CombatDesignation combat_designation = CombatDesignation::None;
        bool in_combat = false;                 // at a battlefield with combat in progress
        std::vector<GameObjectId> attachments;  // gear attached at death
        KeywordSet keywords;                    // printed + aura + "this turn"
    };
    LastKnown last_known;

    // ── Who marked the damage (CR: "your damage" = damage you marked) ──
    // damage_marked split by the player whose effect / units dealt it. Index
    // by playerIndex(). Cleared whenever damage_marked is healed to 0.
    int damage_marked_by[2] = {0, 0};

    // ── Position ──
    ZoneType zone = ZoneType::MainDeck;
    std::optional<LocationId> location;  // only meaningful when on board
    // Last on-board location before the unit died / left play. Populated
    // by EffectExecutor::killObject just before clearing `location`.
    // Used by Deathknell triggers that need to know "where did I die"
    // (e.g. Loyal Poro: "didn't die alone" — check other friendlies at
    // this location). Stays valid across the trigger's chain item.
    std::optional<LocationId> last_location;

    // ── Combat ──
    CombatDesignation combat_designation = CombatDesignation::None;

    // ── Show Off (CR 832) ──
    // What this card showed off as it was played (a card revealed from hand or
    // a friendly object picked on the board); kInvalidId if it didn't. Set by
    // the engine when the card's chain item is finalized; read through
    // Card::shownOff.
    GameObjectId showed_off_object = kInvalidId;

    // ── Buffs ──
    int buff_count = 0;          // +1 might per buff
    int temp_buff_count = 0;     // temporary buffs applied "this turn" (expire at expiration step)
    int temp_assault_value = 0;  // temporary assault applied "this turn"
    int temp_shield_value = 0;   // temporary shield applied "this turn"
    int temp_disarm_value = 0;   // [Disarm X] given "this turn" (cleared at expiration)
    int temp_might_bonus = 0;    // temporary +N might applied "this turn"
    int moves_this_turn = 0;     // times this unit moved this turn (Kayn, Unleashed)
    bool immune_to_damage = false; // recomputed each recalculateAuras; set by a
                                   // card's applyPassiveAura when its condition
                                   // holds (Kayn, Unleashed: "if I have moved twice
                                   // this turn, I don't take damage"). dealDamage
                                   // no-ops against it.
    int spell_bonus_damage = 0;    // per-spell Bonus Damage rider bound to THIS
                                   // spell object at play time (Ravenborn Tome:
                                   // "the next spell you play deals N Bonus
                                   // Damage" — applies to every instance the spell
                                   // deals). Set from PlayerState::next_spell_bonus_damage.
    int spells_targeting_me_cost_reduction = 0; // Irelia, Graceful (462): "your
                                   // spells that choose me cost [1]/[A] less."
                                   // Recomputed each recalculateAuras; the spell
                                   // cost path reduces by this when I'm a target.

    // Aura-granted activated abilities (Forge of the Fluft 522 / Gardens of
    // Becoming 769 / Heimerdinger 111). A granting card's applyPassiveAura
    // appends a ref naming WHICH card (def id) and WHICH of its
    // activatedAbilities() this object may now use; the action generator emits
    // an activation per ref and dispatch routes to that card's onActivate with
    // THIS object as the source. Reset each recalculateAuras.
    struct GrantedAbilityRef {
        CardDefId source_def_id = 0;  // 0 = none; else the granting card
        int ability_index = 0;
        // ability_index value meaning "every [E] ability of the granting card"
        // ("I have all [E] abilities of ...", Heimerdinger, Inventor); the
        // action generator expands it to one ref per such ability.
        static constexpr int kAllExhaustAbilities = -1;
    };
    std::vector<GrantedAbilityRef> granted_abilities;
    // Abilities granted "this turn" by a resolved effect (Dominus: `give it
    // "[A][A]: Ready me."`). Merged into granted_abilities every aura pass and
    // cleared at end of turn.
    std::vector<GrantedAbilityRef> temp_granted_abilities;

    // ── Keywords (computed via layers, cached) ──
    KeywordSet keywords;

    // ── Parameterized keyword values ──
    int assault_value = 0;
    int shield_value = 0;
    int deflect_value = 0;

    // ── Attachment ──
    std::optional<GameObjectId> attached_to;          // card I'm attached to
    std::vector<GameObjectId> attachments;             // cards attached to me
    int might_bonus = 0;                               // this gear's might bonus (from CardDef)
    int attachment_might_bonus = 0;                    // total might bonus from attached gear
    bool is_rules_text_inactive = false;              // gear: rules text inactive while attached (CR 718.2)
    // [Deploy] (CR 830) — mirrors Card::hasDeploy(), refreshed each aura pass so
    // applyPassiveAura (which has no registry) can ask (Overloaded Traveler).
    bool has_deploy = false;

    // ── Targeting protection (CR 700.x — "can't be chosen by ...") ──
    // Set by the engine during cleanup / aura recalc by consulting the
    // Card's canBeChosenByEnemy() method. Card::enumerateLegalTargets
    // reads this flag to filter out untargetable enemies. Baron Nashor
    // ([709]) is the canonical example.
    bool untargetable_by_enemy = false;

    // ── Per-card persistent counters (legend state, etc.) ──
    // Generic int counters keyed by name so multiple card classes can have
    // their own state without ballooning this struct. Used by Virtuoso
    // (spells_banished_with_me). Survives across turns; only reset on
    // explicit Card subclass reset.
    std::unordered_map<std::string, int> card_counters;

    // String-valued per-card state, keyed by purpose. For cards that "name"
    // something at play time and act on the named value later (The List 700:
    // "name a tag" → string_state["named_tag"]). Parallel to card_counters
    // (which only holds ints). Survives across turns; reset by the owning
    // Card subclass if needed.
    std::unordered_map<std::string, std::string> string_state;

    // ── Damage-modifying turn-scoped flags ──
    // Lotus Trap and similar cards set `damage_doubled_this_turn` on a
    // target unit; EffectExecutor::dealDamage consults the flag and
    // doubles the assignment. Cleared at expiration (end of turn).
    bool damage_doubled_this_turn = false;

    // ── Deferred one-shot death replacement (Tactical Retreat 737) ──
    // "The next time it would die this turn, heal it, exhaust it, and recall
    // it to base instead." killUnit consults this BEFORE the normal death
    // path: when set it heals + exhausts + recalls the unit to base and clears
    // the flag (one-shot). Expires at end of turn if unused.
    bool death_replacement_recall_pending = false;

    // "If it would die this turn, banish it instead." (Smite 568). Both kill
    // paths consult it after the recall replacement; expires at end of turn.
    bool banish_if_would_die_this_turn = false;

    // ── One-shot damage prevention (Counter Strike 510) ──
    // "The next time that unit would be dealt damage this turn, prevent it."
    // EffectExecutor::dealDamage consults this BEFORE applying damage: when set
    // it prevents the next instance of damage to this unit and clears the flag
    // (one-shot). Expires unused at end of turn.
    bool prevent_next_damage_this_turn = false;

    // ── Per-unit movement restriction (Determined Sentry 673) ──
    // "I can't move to base." Set by the card on play; consulted by the
    // main-phase move generator (parallel to BattlefieldState::blocks_move_to_base,
    // which is the per-battlefield variant).
    bool cant_move_to_base = false;
    // "I / it can't move this turn" (Entrenched Boar, Hunker Down, Vex,
    // Apathetic): the turn number on which this unit can't move — by its
    // standard move or by any effect (CR 458.3: recalls are not moves and are
    // unaffected); -1 = no restriction. `cant_move_only_by` narrows it to the
    // moves one player makes ("They can't move it this turn"); None = anyone.
    // Set through forbidMovesThisTurn; checked by moveForbidden (move_rules.h).
    int cant_move_turn = -1;
    PlayerId cant_move_only_by = PlayerId::None;
    void forbidMovesThisTurn(int turn, PlayerId only_by = PlayerId::None) {
        if (cant_move_turn == turn && cant_move_only_by != only_by)
            cant_move_only_by = PlayerId::None;   // two restrictions: union
        else
            cant_move_only_by = only_by;
        cant_move_turn = turn;
    }
    // Continuous "can't move to base" (Minotaur Reckoner: "Units can't move to
    // base."). Reset each recalculateAuras, re-set by the card's aura.
    bool aura_cant_move_to_base = false;
    // "I can't be readied" (Maduli the Gatekeeper). Recomputed each
    // recalculateAuras; the Awaken phase and EffectExecutor::readyObject skip it.
    bool cant_be_readied = false;

    // ── Take-control reversion (CR 416, Phase 6q+) ──
    // EffectExecutor::takeControl with `until_end_of_turn=true` snapshots
    // the original controller into `original_controller_eot` and flips
    // this flag. Expiration step (end-of-turn) restores `controller` from
    // the snapshot and clears the flag. Permanent control changes do NOT
    // set the flag.
    bool control_reverts_eot = false;
    PlayerId original_controller_eot = PlayerId::None;

    // ── Take-control reversion "until I leave the board" (Akshan 431) ──
    // When an effect takes control of this object only "until <source> leaves
    // the board", these record the controlling source and where control
    // returns. GameEngine::revertLapsedControl (run in cleanup) restores
    // `controller` and clears these once the source object is no longer in
    // play. Independent of the end-of-turn variant above.
    bool control_reverts_on_source_leave = false;
    GameObjectId control_source_obj = kInvalidId;
    PlayerId control_revert_to = PlayerId::None;

    // ── Card-specific tracked objects ──
    // Virtuoso [782] uses this to remember WHICH banished spells were
    // banished by ITS ability ("four spells banished with me"). When the
    // 4-stack payoff fires, only THESE return to trash — banished spells
    // from other sources stay in banishment. Generic vector so future
    // similar cards can re-use it.
    std::vector<GameObjectId> tracked_objects;

    // ── Aura effects (Phase 5b — tagged effects) ──
    struct AuraEffect {
        GameObjectId source = kInvalidId;  // aura source object
        int might_bonus = 0;               // +/- M from this aura
        int might_minimum = 0;             // minimum M (0 = no min)
        Keyword keyword = Keyword::Count;  // granted keyword (Count = none)
        int keyword_value = 0;             // e.g. Assault 1, Shield 2
        bool suppress_combat_damage = false; // Vilemaw: "doesn't deal combat damage"
        int bonus_damage_taken = 0;        // +N to spell/ability damage dealt to me (Void Gate)
    };
    std::vector<AuraEffect> aura_effects;  // active auras affecting this object
    int aura_might_bonus = 0;              // cached sum of aura might bonuses
    KeywordSet aura_keywords;              // cached union of aura-granted keywords
    bool aura_no_combat_damage = false;    // cached: any aura suppresses combat dmg
    int aura_bonus_damage_taken = 0;       // cached: +N spell/ability damage taken (Void Gate)
    // [Disarm] granted by auras ("Your Mechs have [Disarm]", CR 831.2: values
    // sum). Reset each recalculateAuras; a card's applyPassiveAura adds to it.
    int aura_disarm_value = 0;

    /// This unit deals no combat damage (stunned, or an aura suppresses it —
    /// Vilemaw). Consulted by the combat-damage step.
    bool dealsNoCombatDamage() const { return is_stunned || aura_no_combat_damage; }

    // Keywords granted by "this turn" effects (Bounty Hunter's
    // `[E]: Give a unit [Ganking] this turn.`, Catalyst of Aeons-style
    // temp grants, etc.). Mirror-set into `keywords` when granted so
    // hasKeyword() short-circuits cheaply; the expiration step (CR
    // 317.2.c) walks this set and clears each bit from `keywords` if
    // the CardDef doesn't print it AND no active aura grants it,
    // then clears this field. Without this tracking, a "this turn"
    // grant becomes a permanent keyword — Bounty Hunter's Ganking
    // would stick forever, contradicting the card text.
    KeywordSet temp_keywords;

    // ── Convenience ──
    bool isReady() const { return !is_exhausted; }
    bool isUnit() const { return card_type == CardType::Unit || also_unit; }
    bool isGear() const { return card_type == CardType::Gear; }
    bool isSpell() const { return card_type == CardType::Spell; }
    bool isRune() const { return card_type == CardType::Rune; }
    bool isBattlefield() const { return card_type == CardType::Battlefield; }
    bool isLegend() const { return card_type == CardType::Legend; }
    bool isPermanent() const { return isUnit() || isGear(); }
    bool isChampion() const { return super_type == SuperType::Champion; }
    // CR 185.1: "Token" is an intrinsic category of game object, not a
    // supertype: a token stays a token however it is copied or altered, and a
    // card can never become one. is_token is that intrinsic bit; the supertype
    // check keeps token card defs (Gold, Recruit, ...) working.
    bool isToken() const { return is_token || super_type == SuperType::Token; }

    bool isAlive() const { return damage_marked < current_might; }
    bool hasLethalDamage() const {
        return damage_marked > 0 && damage_marked >= current_might;
    }

    bool isAtBase() const {
        return location.has_value() &&
               std::holds_alternative<BaseLocation>(*location);
    }

    bool isAtBattlefield() const {
        return location.has_value() &&
               std::holds_alternative<BattlefieldLocation>(*location);
    }

    std::optional<BattlefieldId> battlefieldId() const {
        if (location && std::holds_alternative<BattlefieldLocation>(*location)) {
            return std::get<BattlefieldLocation>(*location).id;
        }
        return std::nullopt;
    }

    /// Compute current might from base + buffs + assault/shield (Phase 1 simple version).
    /// Full layer system will replace this later.
    void recomputeMight() {
        // buff_count = permanent +1 buff counters (CR); temp_might_bonus =
        // this-turn temporary might (expires at the expiration step). Tracked
        // separately so "spend a buff" / "if it has a buff" gates read only the
        // permanent counters, not transient temp might.
        current_might = base_might + buff_count + temp_might_bonus +
                        attachment_might_bonus + aura_might_bonus;
        // Assault applies only while attacking (base + aura-granted)
        if (combat_designation == CombatDesignation::Attacker) {
            current_might += assault_value;
        }
        // Shield applies only while defending
        if (combat_designation == CombatDesignation::Defender) {
            current_might += shield_value;
        }
        // Apply aura minimums (e.g., Leona: -8M to minimum 1)
        for (auto& ae : aura_effects) {
            if (ae.might_minimum > 0 && current_might < ae.might_minimum) {
                current_might = ae.might_minimum;
            }
        }
        if (current_might < 0) current_might = 0;
    }

    /// Check if this object has a keyword (base + aura-granted).
    bool hasKeyword(Keyword kw) const {
        return keywords.has(kw) || aura_keywords.has(kw);
    }
};

} // namespace riftbound
