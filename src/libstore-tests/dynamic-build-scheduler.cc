#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "nix/store/build/worker.hh"

namespace nix {

TEST(DynamicBuildScheduler, disabledPreservesMaxJobsBehavior)
{
    DynamicBuildScheduler scheduler{DynamicBuildScheduler::Config{
        .enabled = false,
        .memoryBudgetBytes = 100,
    }};

    EXPECT_TRUE(scheduler.canStartBuild(0, 2, 90));
    EXPECT_TRUE(scheduler.canStartBuild(1, 2, 90));
    EXPECT_FALSE(scheduler.canStartBuild(2, 2, 1));
    EXPECT_FALSE(scheduler.canStartBuild(0, 0, std::nullopt));
}

TEST(DynamicBuildScheduler, knownMemoryReducesConcurrencyUnderBudget)
{
    DynamicBuildScheduler scheduler{DynamicBuildScheduler::Config{
        .enabled = true,
        .memoryBudgetBytes = 100,
    }};

    EXPECT_TRUE(scheduler.canStartBuild(0, 4, 70));
    scheduler.buildStarted(70);

    EXPECT_FALSE(scheduler.canStartBuild(1, 4, 40));
    EXPECT_TRUE(scheduler.canStartBuild(1, 4, 30));
}

TEST(DynamicBuildScheduler, unknownDerivationsUseNormalJobSlots)
{
    DynamicBuildScheduler scheduler{DynamicBuildScheduler::Config{
        .enabled = true,
        .memoryBudgetBytes = 100,
    }};

    scheduler.buildStarted(100);

    EXPECT_TRUE(scheduler.canStartBuild(1, 3, std::nullopt));
    EXPECT_TRUE(scheduler.canStartBuild(2, 3, std::nullopt));
    EXPECT_FALSE(scheduler.canStartBuild(3, 3, std::nullopt));
}

TEST(DynamicBuildScheduler, oversizedKnownDerivationRunsWhenAlone)
{
    DynamicBuildScheduler scheduler{DynamicBuildScheduler::Config{
        .enabled = true,
        .memoryBudgetBytes = 100,
    }};

    EXPECT_TRUE(scheduler.canStartBuild(0, 4, 200));
    scheduler.buildStarted(50);
    EXPECT_FALSE(scheduler.canStartBuild(1, 4, 200));
}

TEST(DynamicBuildScheduler, headroomIsApplied)
{
    EXPECT_EQ(DynamicBuildScheduler::applyHeadroom(1000, 20), 800);
    EXPECT_EQ(DynamicBuildScheduler::applyHeadroom(1000, 100), 0);
    EXPECT_EQ(DynamicBuildScheduler::applyHeadroom(1000, 101), 0);
}

TEST(DynamicBuildScheduler, laterFittingBuildCanBeSelected)
{
    DynamicBuildScheduler scheduler{DynamicBuildScheduler::Config{
        .enabled = true,
        .memoryBudgetBytes = 100,
    }};

    scheduler.buildStarted(80);

    std::vector<std::optional<uint64_t>> waiting{30, 20};
    auto selected = std::find_if(
        waiting.begin(), waiting.end(), [&](auto estimate) { return scheduler.canStartBuild(1, 4, estimate); });

    ASSERT_NE(selected, waiting.end());
    EXPECT_EQ(*selected, std::optional<uint64_t>{20});
}

} // namespace nix
