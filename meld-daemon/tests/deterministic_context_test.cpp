#include "meld/daemon/deterministic_context.hpp"
#include "meld/daemon/deterministic_handlers.hpp"
#include "meld/daemon/daemon.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace meld::daemon {
namespace {

// --- DeterministicTimeHandler ---

TEST(DeterministicTimeHandlerTest, ReturnsEpochAndAdvances) {
    DeterministicConfig cfg;
    cfg.seed = 0;
    cfg.epoch = 1000;  // 1000 seconds since Unix epoch
    cfg.time_increment_ms = 5;

    DeterministicContext ctx(cfg);
    DeterministicTimeHandler handler(ctx);

    // First call returns epoch * 1000 (converted to ms)
    uint64_t t0 = handler.now_ms();
    EXPECT_EQ(t0, 1000u * 1000u);

    // Second call advances by time_increment_ms
    uint64_t t1 = handler.now_ms();
    EXPECT_EQ(t1, 1000u * 1000u + 5u);

    // Third call advances again
    uint64_t t2 = handler.now_ms();
    EXPECT_EQ(t2, 1000u * 1000u + 10u);
}

TEST(DeterministicTimeHandlerTest, SleepAdvancesVirtualClock) {
    DeterministicConfig cfg;
    cfg.epoch = 100;
    cfg.time_increment_ms = 1;

    DeterministicContext ctx(cfg);
    DeterministicTimeHandler handler(ctx);

    uint64_t t0 = handler.now_ms();
    handler.sleep_ms(10);  // Should advance by 10 ticks
    uint64_t t1 = handler.now_ms();

    // t0 consumed 1 tick, sleep consumed 10 ticks, t1 is the 12th tick
    EXPECT_GT(t1, t0);
    EXPECT_EQ(t1 - t0, 11u);  // 10 from sleep + 1 from the first now_ms
}

TEST(DeterministicTimeHandlerTest, DefaultEpochUsed) {
    DeterministicConfig cfg;
    cfg.epoch = 0;  // Should use kDefaultEpoch
    cfg.time_increment_ms = 1;

    DeterministicContext ctx(cfg);
    uint64_t t = ctx.now_ms();
    EXPECT_EQ(t, DeterministicConfig::kDefaultEpoch * 1000u);
}

// --- DeterministicRandomHandler ---

TEST(DeterministicRandomHandlerTest, SameSeedProducesSameSequence) {
    DeterministicConfig cfg;
    cfg.seed = 42;

    DeterministicContext ctx1(cfg);
    DeterministicContext ctx2(cfg);
    DeterministicRandomHandler h1(ctx1);
    DeterministicRandomHandler h2(ctx2);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(h1.next(), h2.next()) << "Diverged at iteration " << i;
    }
}

TEST(DeterministicRandomHandlerTest, DifferentSeedProducesDifferentSequence) {
    DeterministicConfig cfg1;
    cfg1.seed = 1;
    DeterministicConfig cfg2;
    cfg2.seed = 2;

    DeterministicContext ctx1(cfg1);
    DeterministicContext ctx2(cfg2);
    DeterministicRandomHandler h1(ctx1);
    DeterministicRandomHandler h2(ctx2);

    bool any_different = false;
    for (int i = 0; i < 10; ++i) {
        if (h1.next() != h2.next()) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(DeterministicRandomHandlerTest, NextIntRespectsBound) {
    DeterministicConfig cfg;
    cfg.seed = 99;

    DeterministicContext ctx(cfg);
    DeterministicRandomHandler handler(ctx);

    for (int i = 0; i < 100; ++i) {
        int val = handler.next_int(10);
        EXPECT_GE(val, 0);
        EXPECT_LT(val, 10);
    }
}

TEST(DeterministicRandomHandlerTest, NextDoubleInRange) {
    DeterministicConfig cfg;
    cfg.seed = 7;

    DeterministicContext ctx(cfg);
    DeterministicRandomHandler handler(ctx);

    for (int i = 0; i < 100; ++i) {
        double val = handler.next_double();
        EXPECT_GE(val, 0.0);
        EXPECT_LT(val, 1.0);
    }
}

TEST(DeterministicRandomHandlerTest, SeedCallIsIgnored) {
    DeterministicConfig cfg;
    cfg.seed = 42;

    DeterministicContext ctx1(cfg);
    DeterministicContext ctx2(cfg);
    DeterministicRandomHandler h1(ctx1);
    DeterministicRandomHandler h2(ctx2);

    // Call seed on h1 — should be ignored
    h1.seed(999);

    // Both should still produce the same sequence
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(h1.next(), h2.next());
    }
}

// --- DeterministicScheduler ---

TEST(DeterministicSchedulerTest, TasksRunInSpawnOrder) {
    DeterministicConfig cfg;
    DeterministicContext ctx(cfg);
    DeterministicScheduler scheduler(ctx);

    std::vector<int> execution_order;

    scheduler.spawn([&] { execution_order.push_back(0); });
    scheduler.spawn([&] { execution_order.push_back(1); });
    scheduler.spawn([&] { execution_order.push_back(2); });

    EXPECT_EQ(scheduler.pending(), 3u);

    scheduler.drain();

    EXPECT_EQ(scheduler.pending(), 0u);
    ASSERT_EQ(execution_order.size(), 3u);
    EXPECT_EQ(execution_order[0], 0);
    EXPECT_EQ(execution_order[1], 1);
    EXPECT_EQ(execution_order[2], 2);
}

TEST(DeterministicSchedulerTest, DrainOnEmptyIsNoop) {
    DeterministicConfig cfg;
    DeterministicContext ctx(cfg);
    DeterministicScheduler scheduler(ctx);

    scheduler.drain();  // Should not crash
    EXPECT_EQ(scheduler.pending(), 0u);
}

// --- DeterministicContext ---

TEST(DeterministicContextTest, ResetRestoresInitialState) {
    DeterministicConfig cfg;
    cfg.seed = 42;
    cfg.epoch = 100;
    cfg.time_increment_ms = 1;

    DeterministicContext ctx(cfg);

    // Consume some state
    auto t0 = ctx.now_ms();
    auto r0 = ctx.next_random();
    ctx.next_spawn_index();

    // Reset
    ctx.reset();

    // Should produce same values as initial
    EXPECT_EQ(ctx.now_ms(), t0);
    EXPECT_EQ(ctx.next_random(), r0);
    EXPECT_EQ(ctx.next_spawn_index(), 0u);
}

TEST(DeterministicContextTest, AlwaysActive) {
    DeterministicContext ctx;
    EXPECT_TRUE(ctx.active());
}

// --- DeterministicHandlerSet ---

TEST(DeterministicHandlerSetTest, CreateBuildsAllHandlers) {
    DeterministicContext ctx;
    auto set = DeterministicHandlerSet::create(ctx);

    EXPECT_NE(set.time, nullptr);
    EXPECT_NE(set.random, nullptr);
    EXPECT_NE(set.scheduler, nullptr);
    EXPECT_TRUE(set.all_active());
}

TEST(DeterministicHandlerSetTest, EffectNames) {
    DeterministicContext ctx;
    auto set = DeterministicHandlerSet::create(ctx);

    EXPECT_EQ(set.time->effect_name(), "EffectTime");
    EXPECT_EQ(set.random->effect_name(), "EffectRandom");
    EXPECT_EQ(set.scheduler->effect_name(), "EffectScheduler");
}

// --- Daemon integration ---

TEST(DaemonDeterministicTest, NotActiveByDefault) {
    DaemonConfig cfg;
    cfg.workspace = "/tmp/test-workspace";
    MeldDaemon daemon(cfg);

    EXPECT_FALSE(daemon.is_deterministic());
    EXPECT_FALSE(daemon.deterministic_config().has_value());
}

TEST(DaemonDeterministicTest, ActivateAndDeactivate) {
    DaemonConfig cfg;
    cfg.workspace = "/tmp/test-workspace";
    MeldDaemon daemon(cfg);

    DeterministicConfig det_cfg;
    det_cfg.seed = 123;
    det_cfg.epoch = 500;

    daemon.activate_deterministic(det_cfg);
    EXPECT_TRUE(daemon.is_deterministic());

    auto echoed = daemon.deterministic_config();
    ASSERT_TRUE(echoed.has_value());
    EXPECT_EQ(echoed->seed, 123u);
    EXPECT_EQ(echoed->epoch, 500u);

    daemon.deactivate_deterministic();
    EXPECT_FALSE(daemon.is_deterministic());
    EXPECT_FALSE(daemon.deterministic_config().has_value());
}

}  // namespace
}  // namespace meld::daemon
