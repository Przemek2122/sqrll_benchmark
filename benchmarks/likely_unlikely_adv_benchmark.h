#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

#include "../benchmark.h"

namespace lu_adv {

// Something an entity carries around. The hot path reads it through a pointer.
class Inventory {
public:
    uint64_t gold = 0;
    uint64_t gems = 0;

    uint64_t value() const { return gold + gems * 10; }
};

// A game-like entity. Rarely has no inventory (nullptr) or is cursed.
class Entity {
public:
    uint64_t   id        = 0;
    uint32_t   type      = 0;
    bool       cursed    = false;    // rare (~1%)
    Inventory* inventory = nullptr;  // rarely nullptr (~1%)
};

// Slow lookups, used only on the rare paths.
class LootTable {
public:
    std::unordered_map<uint64_t, uint64_t> fallbackGold;  // entity id   -> gold
    std::map<uint32_t, uint64_t>           cursePenalty;  // entity type -> penalty

    uint64_t fallback_for(uint64_t id) const
    {
        auto it = fallbackGold.find(id);
        return it != fallbackGold.end() ? it->second : 0;
    }

    uint64_t penalty_for(uint32_t type) const
    {
        auto it = cursePenalty.find(type);
        return it != cursePenalty.end() ? it->second : 0;
    }
};

} // namespace lu_adv

class LikelyUnlikelyAdvancedBenchmark : public bench::BenchmarkGroup {
private:
    using Inventory = lu_adv::Inventory;
    using Entity    = lu_adv::Entity;
    using LootTable = lu_adv::LootTable;

    static constexpr int      kEntities = 16384;  // fits in L2, too big to memorize the pattern
    static constexpr uint32_t kTypes    = 16;

    std::vector<Inventory> inventories;
    std::vector<Entity>    entities;
    std::vector<Entity*>   entityPtrs;  // what the tests iterate over
    LootTable              loot;
    int rarePercent = 0;

    // Rare paths written FIRST, no hints -> bad layout by default.
    uint64_t score_neutral(const Entity* e) const
    {
        uint64_t score = 0;

        if (e->inventory == nullptr) {
            score += loot.fallback_for(e->id);      // rare
        } else {
            score += e->inventory->value();         // hot
        }

        if (e->cursed) {
            score ^= loot.penalty_for(e->type);     // rare
        } else {
            score += e->type;                       // hot
        }

        return score;
    }

    // Same order, correct hints on the rare paths.
    uint64_t score_hinted(const Entity* e) const
    {
        uint64_t score = 0;

        if (e->inventory == nullptr) [[unlikely]] {
            score += loot.fallback_for(e->id);
        } else {
            score += e->inventory->value();
        }

        if (e->cursed) [[unlikely]] {
            score ^= loot.penalty_for(e->type);
        } else {
            score += e->type;
        }

        return score;
    }

    // Same order, WRONG hints: rare paths marked as likely.
    uint64_t score_wrong(const Entity* e) const
    {
        uint64_t score = 0;

        if (e->inventory == nullptr) [[likely]] {
            score += loot.fallback_for(e->id);
        } else {
            score += e->inventory->value();
        }

        if (e->cursed) [[likely]] {
            score ^= loot.penalty_for(e->type);
        } else {
            score += e->type;
        }

        return score;
    }

    // Hot paths written first, no hints -> good layout by default.
    uint64_t score_reference(const Entity* e) const
    {
        uint64_t score = 0;

        if (e->inventory != nullptr) {
            score += e->inventory->value();
        } else {
            score += loot.fallback_for(e->id);
        }

        if (!e->cursed) {
            score += e->type;
        } else {
            score ^= loot.penalty_for(e->type);
        }

        return score;
    }

public:
    void set_up() override
    {
        rarePercent = 1;  // try also 5 and 20

        std::mt19937_64 gen{12345};  // fixed seed = same data every run
        std::uniform_int_distribution<int>      roll(0, 99);
        std::uniform_int_distribution<uint64_t> goldDist(0, 1000);
        std::uniform_int_distribution<uint32_t> typeDist(0, kTypes - 1);

        // Size everything up front so pointers into these vectors stay valid.
        inventories.assign(kEntities, Inventory{});
        entities.assign(kEntities, Entity{});
        entityPtrs.clear();
        loot.fallbackGold.clear();
        loot.cursePenalty.clear();

        for (uint32_t t = 0; t < kTypes; t++)
        {
            loot.cursePenalty[t] = goldDist(gen);
        }

        for (int i = 0; i < kEntities; i++)
        {
            inventories[i].gold = goldDist(gen);
            inventories[i].gems = goldDist(gen);

            Entity& e = entities[i];
            e.id     = i;
            e.type   = typeDist(gen);
            e.cursed = roll(gen) < rarePercent;

            if (roll(gen) < rarePercent) {
                e.inventory = nullptr;
                loot.fallbackGold[e.id] = goldDist(gen);
            } else {
                e.inventory = &inventories[i];
            }

            entityPtrs.push_back(&e);
        }
    }

    LikelyUnlikelyAdvancedBenchmark() : bench::BenchmarkGroup("Likely & Unlikely advanced benchmarks") {

        add_test("neutral (rare paths first, no hints)", [this]() {
            uint64_t res = 0;
            for (const Entity* e : entityPtrs) {
                res += score_neutral(e);
            }
            bench::do_not_optimize(res);
        });

        add_test("[[unlikely]] on rare paths", [this]() {
            uint64_t res = 0;
            for (const Entity* e : entityPtrs) {
                res += score_hinted(e);
            }
            bench::do_not_optimize(res);
        });

        add_test("WRONG: [[likely]] on rare paths", [this]() {
            uint64_t res = 0;
            for (const Entity* e : entityPtrs) {
                res += score_wrong(e);
            }
            bench::do_not_optimize(res);
        });

        add_test("reference (hot paths first, no hints)", [this]() {
            uint64_t res = 0;
            for (const Entity* e : entityPtrs) {
                res += score_reference(e);
            }
            bench::do_not_optimize(res);
        });
    }
};

// Automatic self-registration of example benchmark classes
REGISTER_BENCHMARK_CLASS(LikelyUnlikelyAdvancedBenchmark)