#pragma once

#include "../UTIL/Utilities.h"

namespace GAME
{
    // tags & data
    struct ItemPickup {};                               // marks a drop in the world
    enum class PickupType : uint8_t { Health, Shield, Weapon };
    struct PickupData { PickupType type; };
    struct PickupVelocity { float zPerSecond; };

    // run-time tunables, read from GameConfig.ini
    struct ItemDropConfig
    {
		float  dropChance = 0.20f;                      // Chance of a drop spawning
		float  weights[3] = { 0.40f, 0.35f, 0.25f };    // Chance of each type Health, Shield, Weapon
		float  fallSpeed = 2.5f;                        // units / sec (-Z) speed of the drop
        float  despawnZ = -50.f;                        // kill the drop when below this
        static ItemDropConfig& Get();                   // singleton accessor
    };
}

