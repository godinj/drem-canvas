#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    /// Case-insensitive check for .vst3 extension (mirrors PluginScanner.cpp)
    bool hasVst3Extension(const fs::path& p)
    {
        auto ext = p.extension().string();

        if (ext.size() != 5)  // ".vst3" is 5 characters including the dot
            return false;

        for (auto& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        return ext == ".vst3";
    }

    /// Recursive bundle finder — same algorithm as PluginScanner::findBundles()
    std::vector<fs::path> findBundles(const fs::path& searchDir)
    {
        std::vector<fs::path> bundles;
        std::error_code ec;

        auto options = fs::directory_options::follow_directory_symlink
                     | fs::directory_options::skip_permission_denied;

        for (auto it = fs::recursive_directory_iterator(searchDir, options, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (it->is_directory() && hasVst3Extension(it->path()))
            {
                bundles.push_back(it->path());
                it.disable_recursion_pending();
            }
        }

        std::sort(bundles.begin(), bundles.end());
        return bundles;
    }

    /// Create a minimal .vst3 bundle directory structure
    void createVst3Bundle(const fs::path& bundlePath)
    {
        fs::create_directories(bundlePath / "Contents");
    }
} // anonymous namespace

TEST_CASE("findBundles discovers direct child .vst3 bundles", "[plugins]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_scanner_direct";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    createVst3Bundle(tmp / "direct.vst3");

    auto bundles = findBundles(tmp);

    REQUIRE(bundles.size() == 1);
    REQUIRE(bundles[0].filename() == "direct.vst3");

    fs::remove_all(tmp);
}

TEST_CASE("findBundles discovers nested .vst3 bundles", "[plugins]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_scanner_nested";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    createVst3Bundle(tmp / "direct.vst3");
    createVst3Bundle(tmp / "subdir" / "nested.vst3");
    createVst3Bundle(tmp / "subdir" / "deep" / "deeper.vst3");
    fs::create_directories(tmp / "not-a-plugin");

    auto bundles = findBundles(tmp);

    REQUIRE(bundles.size() == 3);

    // Verify all three .vst3 bundles are found
    std::vector<std::string> names;

    for (const auto& b : bundles)
        names.push_back(b.filename().string());

    REQUIRE(std::find(names.begin(), names.end(), "direct.vst3") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "nested.vst3") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "deeper.vst3") != names.end());

    fs::remove_all(tmp);
}

TEST_CASE("findBundles excludes non-.vst3 directories", "[plugins]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_scanner_exclude";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    createVst3Bundle(tmp / "good.vst3");
    fs::create_directories(tmp / "not-a-plugin");
    fs::create_directories(tmp / "also-not-a-plugin" / "Contents");

    auto bundles = findBundles(tmp);

    REQUIRE(bundles.size() == 1);
    REQUIRE(bundles[0].filename() == "good.vst3");

    fs::remove_all(tmp);
}

TEST_CASE("findBundles returns sorted results", "[plugins]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_scanner_sorted";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    createVst3Bundle(tmp / "Zebra.vst3");
    createVst3Bundle(tmp / "Alpha.vst3");
    createVst3Bundle(tmp / "sub" / "Middle.vst3");

    auto bundles = findBundles(tmp);

    REQUIRE(bundles.size() == 3);
    REQUIRE(std::is_sorted(bundles.begin(), bundles.end()));

    fs::remove_all(tmp);
}

TEST_CASE("findBundles does not descend into .vst3 bundles", "[plugins]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_scanner_no_descend";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    // Create a .vst3 bundle that itself contains a nested .vst3 dir
    // (pathological case — should NOT be found)
    createVst3Bundle(tmp / "outer.vst3");
    createVst3Bundle(tmp / "outer.vst3" / "Contents" / "inner.vst3");

    auto bundles = findBundles(tmp);

    REQUIRE(bundles.size() == 1);
    REQUIRE(bundles[0].filename() == "outer.vst3");

    fs::remove_all(tmp);
}

TEST_CASE("findBundles handles empty directory", "[plugins]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_scanner_empty";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    auto bundles = findBundles(tmp);

    REQUIRE(bundles.empty());

    fs::remove_all(tmp);
}
