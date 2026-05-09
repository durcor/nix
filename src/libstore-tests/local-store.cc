#include <gtest/gtest.h>

#include "nix/store/local-store.hh"
#include "nix/store/store-open.hh"
#include "nix/store/tests/libstore.hh"
#include "nix/util/file-system.hh"
#include "nix/util/finally.hh"

#include "nix/store/sqlite.hh"

// Needed for template specialisations. This is not good! When we
// overhaul how store configs work, this should be fixed.
#include "nix/util/args.hh"
#include "nix/util/config-impl.hh"
#include "nix/util/abstract-setting-to-json.hh"

namespace nix {

static int64_t queryBuildResourceUsageRows(const std::filesystem::path & root)
{
    SQLite db(root / "nix/var/nix/db/db.sqlite", {.useWAL = settings.useSQLiteWAL});
    SQLiteStmt stmt;
    stmt.create(db, "select count(*) from BuildResourceUsage;");
    auto use(stmt.use());
    if (!use.next())
        throw Error("expected BuildResourceUsage count row");
    return use.getInt(0);
}

static bool hasBuildResourceUsageTable(const std::filesystem::path & root)
{
    SQLite db(root / "nix/var/nix/db/db.sqlite", {.useWAL = settings.useSQLiteWAL});
    SQLiteStmt stmt;
    stmt.create(db, "select 1 from sqlite_master where type = 'table' and name = 'BuildResourceUsage';");
    return stmt.use().next();
}

TEST(LocalStore, storeDir_absolutePath)
{
    std::filesystem::path storeDir =
#ifdef _WIN32
        "C:\\";
#else
        "/";
#endif
    storeDir /= "nix";
    storeDir /= "store";
    LocalStoreConfig config{"", {{"store", storeDir.string()}}};
    EXPECT_EQ(config.storeDir, storeDir.string());
}

TEST(LocalStore, storeDir_relativePath_rejected)
{
    EXPECT_THROW(LocalStoreConfig("", {{"store", (std::filesystem::path{"nix"} / "store").string()}}), UsageError);
}

TEST(LocalStore, storeDir_empty_rejected)
{
    EXPECT_THROW(LocalStoreConfig("", {{"store", ""}}), UsageError);
}

TEST(LocalStore, constructConfig_rootQueryParam)
{
#ifdef _WIN32
    constexpr std::string_view root = "C:\\foo\\bar";
#else
    constexpr std::string_view root = "/foo/bar";
#endif
    LocalStoreConfig config{
        "",
        {
            {
                "root",
                std::string{root},
            },
        },
    };

    EXPECT_EQ(config.rootDir.get(), std::optional<AbsolutePath>{std::string{root}});
}

TEST(LocalStore, constructConfig_rootPath)
{
#ifdef _WIN32
    constexpr std::string_view root = "C:\\foo\\bar";
#else
    constexpr std::string_view root = "/foo/bar";
#endif
    LocalStoreConfig config{std::string{root}, {}};

    EXPECT_EQ(config.rootDir.get(), std::optional<AbsolutePath>{std::string{root}});
}

TEST(LocalStore, constructConfig_to_string)
{
    LocalStoreConfig config{"", {}};
    EXPECT_EQ(config.getReference().to_string(), "local");
}

TEST(LocalStore, buildResourceUsageTableExistsOnFreshStore)
{
    auto tmpRoot = createTempDir();
    AutoDelete delTmpRoot(tmpRoot, true);

    auto store = openStore(fmt("local?root=%s", tmpRoot.string()));

    EXPECT_TRUE(hasBuildResourceUsageTable(tmpRoot));
}

TEST(LocalStore, buildResourceUsageMigrationCreatesTable)
{
    auto tmpRoot = createTempDir();
    AutoDelete delTmpRoot(tmpRoot, true);

    {
        auto store = openStore(fmt("local?root=%s", tmpRoot.string()));
    }

    {
        SQLite db(tmpRoot / "nix/var/nix/db/db.sqlite", {.useWAL = settings.useSQLiteWAL});
        SQLiteTxn txn(db);
        db.exec("drop table BuildResourceUsage;");
        db.exec("delete from SchemaMigrations where migration = '20260509-build-resource-usage';");
        txn.commit();
    }

    {
        auto store = openStore(fmt("local?root=%s", tmpRoot.string()));
    }

    EXPECT_TRUE(hasBuildResourceUsageTable(tmpRoot));

    {
        SQLite db(tmpRoot / "nix/var/nix/db/db.sqlite", {.useWAL = settings.useSQLiteWAL});
        db.exec("delete from SchemaMigrations where migration = '20260509-build-resource-usage';");
    }

    {
        auto store = openStore(fmt("local?root=%s", tmpRoot.string()));
    }

    EXPECT_TRUE(hasBuildResourceUsageTable(tmpRoot));
}

TEST(LocalStore, recordBuildResourceUsageDisabled)
{
    auto previousRecordBuildResourceUsage = settings.getLocalSettings().recordBuildResourceUsage.get();
    Finally restoreRecordBuildResourceUsage(
        [&] { settings.getLocalSettings().recordBuildResourceUsage.assign(previousRecordBuildResourceUsage); });
    settings.getLocalSettings().recordBuildResourceUsage.assign(false);

    auto tmpRoot = createTempDir();
    AutoDelete delTmpRoot(tmpRoot, true);

    auto store = openStore(fmt("local?root=%s", tmpRoot.string()));
    auto localStore = store.dynamic_pointer_cast<LocalStore>();
    ASSERT_NE(localStore, nullptr);

    localStore->recordBuildResourceUsage(
        StorePath::random("record-build-resource-usage-disabled.drv"),
        {{"out", UnkeyedRealisation{.outPath = StorePath::random("record-build-resource-usage-disabled")}}},
        BuildResourceUsage{
            .peakMemoryBytes = 1024,
            .cpuUser = std::chrono::microseconds(11),
            .cpuSystem = std::chrono::microseconds(22),
            .wallTime = 33,
            .sampleTime = 44,
        });

    EXPECT_EQ(queryBuildResourceUsageRows(tmpRoot), 0);
}

TEST(LocalStore, recordBuildResourceUsageEnabled)
{
    EnableExperimentalFeature feature{"build-resource-usage"};
    auto previousRecordBuildResourceUsage = settings.getLocalSettings().recordBuildResourceUsage.get();
    Finally restoreRecordBuildResourceUsage(
        [&] { settings.getLocalSettings().recordBuildResourceUsage.assign(previousRecordBuildResourceUsage); });
    settings.getLocalSettings().recordBuildResourceUsage.assign(true);

    auto tmpRoot = createTempDir();
    AutoDelete delTmpRoot(tmpRoot, true);

    auto store = openStore(fmt("local?root=%s", tmpRoot.string()));
    auto localStore = store.dynamic_pointer_cast<LocalStore>();
    ASSERT_NE(localStore, nullptr);

    localStore->recordBuildResourceUsage(
        StorePath::random("record-build-resource-usage-enabled.drv"),
        {{"out", UnkeyedRealisation{.outPath = StorePath::random("record-build-resource-usage-enabled")}}},
        BuildResourceUsage{
            .peakMemoryBytes = 1024,
            .cpuUser = std::chrono::microseconds(11),
            .cpuSystem = std::chrono::microseconds(22),
            .wallTime = 33,
            .sampleTime = 44,
        });

    EXPECT_EQ(queryBuildResourceUsageRows(tmpRoot), 1);
}

} // namespace nix
