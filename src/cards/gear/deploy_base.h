#pragma once
/// @file deploy_base.h
/// [Deploy] gear (CR 830): "This may be played or created to any battlefield",
/// "Play or create this only to a battlefield", "If this is not a unit, it is
/// not recalled in cleanups" and "When an opponent holds here, kill this."
/// The engine handles the cleanup exemption and the hold kill (it asks
/// Card::hasDeploy); this base supplies the play locations.
#include "cards/card.h"
#include "core/game_state.h"

#include <vector>

namespace riftbound {

class DeployGear : public GearCard {
public:
    bool hasDeploy() const override { return true; }
    bool restrictsPlayLocations() const override { return true; }
    std::vector<LocationId> getPlayLocations(const GameState& state,
                                             PlayerId /*player*/) const override {
        std::vector<LocationId> out;
        for (const auto& bf : state.battlefields) out.push_back(BattlefieldLocation{bf.id});
        return out;
    }
};

}  // namespace riftbound
