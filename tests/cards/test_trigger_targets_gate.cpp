/// @file test_trigger_targets_gate.cpp
/// Regression gate for the 2026-10-28 card audit, engine-wide issue #1.
///
/// Triggered abilities go on the chain with NO targets: TriggerManager calls
/// ChainManager::addAbility(source, controller, def) and
/// GameEngine::chooseTargetsAtFinalize returns early for triggers, so
/// Card::getTargetRequirements() is never consulted for them. A card whose
/// only use of getTargetRequirements() is to feed a triggered ability's
/// `targets` therefore silently does nothing. Triggered abilities take their
/// target with Card::pickTarget (opting into kFinalizeTrigger so the choice
/// is made at finalization, CR 383.3).
///
/// getTargetRequirements() is still legitimate on a spell (onResolve) and on
/// a card with an activated ability (it feeds that ability's targets). This
/// test fails for any other card that declares target requirements.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"

#include <gtest/gtest.h>

#include <string>

namespace riftbound::test {
namespace {

using TriggerTargetsGate = CardTestFixture;

TEST_F(TriggerTargetsGate, TriggerOnlyCardsDoNotRelyOnTargetRequirements) {
    std::string offenders;
    for (const auto& [id, def] : card_db.all()) {
        const Card* c = card_registry.get(id);
        if (!c) continue;
        if (c->getTargetRequirements().count == 0) continue;
        if (def.card_type == CardType::Spell) continue;          // spell targets
        if (!c->activatedAbilities().empty()) continue;          // ability targets
        bool has_trigger = false;
        for (auto t : c->triggerTypes())
            if (t != TriggerType::None && t != TriggerType::Activated) has_trigger = true;
        if (!has_trigger) continue;
        offenders += "\n  " + std::to_string(id) + " " + def.name;
    }
    EXPECT_TRUE(offenders.empty())
        << "These cards declare getTargetRequirements() but only have triggered "
           "abilities, whose targets the engine never fills. Use pickTarget "
           "(+ kFinalizeTrigger) instead:" << offenders;
}

}  // namespace
}  // namespace riftbound::test
