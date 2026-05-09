#include <gtest/gtest.h>

#include "nix/util/cgroup.hh"
#include "nix/util/file-system.hh"

namespace nix {

TEST(CgroupStats, readsCpuStat)
{
    auto tmpDir = createTempDir();
    AutoDelete delTmpDir(tmpDir, true);

    writeFile(tmpDir / "cpu.stat", "usage_usec 42\nuser_usec 123\nsystem_usec 456\n");

    auto stats = linux::getCgroupStats(tmpDir);

    ASSERT_TRUE(stats.cpuUser);
    ASSERT_TRUE(stats.cpuSystem);
    EXPECT_EQ(*stats.cpuUser, std::chrono::microseconds(123));
    EXPECT_EQ(*stats.cpuSystem, std::chrono::microseconds(456));
}

TEST(CgroupStats, readsMemoryPeak)
{
    auto tmpDir = createTempDir();
    AutoDelete delTmpDir(tmpDir, true);

    writeFile(tmpDir / "memory.peak", "789\n");

    auto stats = linux::getCgroupStats(tmpDir);

    ASSERT_TRUE(stats.peakMemoryBytes);
    EXPECT_EQ(*stats.peakMemoryBytes, 789);
}

TEST(CgroupStats, missingMemoryPeakIsNull)
{
    auto tmpDir = createTempDir();
    AutoDelete delTmpDir(tmpDir, true);

    writeFile(tmpDir / "cpu.stat", "user_usec 123\nsystem_usec 456\n");

    auto stats = linux::getCgroupStats(tmpDir);

    EXPECT_FALSE(stats.peakMemoryBytes);
}

} // namespace nix
