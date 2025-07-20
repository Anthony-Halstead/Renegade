#pragma once

namespace GAME {

    /* ---------------- pick-up kinds ---------------- */
    enum class PickupType { Health, Shield, Ammo };

    /* ---------------- drop-configuration (read once from .ini) -------- */
    struct ItemDropConfig {
        float  dropChance = 0.25f;
        float  weights[3] = { 0.40f, 0.35f, 0.25f };   // health, shield, ammo
        float  fallSpeed = 2.5f;                      // units / second (-Z)
        float  despawnZ = -50.f;                     // kill when below this
    };

    /* ---------------- ECS components ---------------- */
    struct PickupManager { ItemDropConfig itemDropConfig; };            // 1× in scene
    struct ItemPickup { PickupType type; };                             // on every drop
    struct PickupVelocity { float zPerSecond; };                        // fall speed

    /* client-side animation state (spin + bob) */
    struct PickupAnim {
        float baseY;      // reference Y so bob is additive
        float spinRate;   // rad / second
        float yaw;        // accumulates every frame
        float bobPhase;   // initial phase offset for sine bob
    };

    /* ---------------- functions implemented in ItemDropSystem.cpp ---- */
    void ConstructConfig(entt::registry& reg, entt::entity ent);
    void HandlePickup(entt::registry& reg, GW::MATH::GVECTORF spawnPos);
    void UpdatePickups(entt::registry& reg, entt::entity);

} // namespace GAME
