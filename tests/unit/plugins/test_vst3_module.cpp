#include <catch2/catch_test_macros.hpp>
#include <dc/plugins/VST3Module.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace
{

// ─── RAII helper: creates a temporary mock .vst3 bundle ──────────────

struct TempVST3Bundle
{
    fs::path base;
    fs::path bundle;

    TempVST3Bundle (const std::string& name,
                    bool withLinux = true,
                    bool withWin = false,
                    bool withMacOS = false)
    {
        auto tmpl = (fs::temp_directory_path() / "dc_test_vst3module_XXXXXX").string();
        REQUIRE(mkdtemp(tmpl.data()) != nullptr);
        base = tmpl;

        bundle = base / (name + ".vst3");

        if (withLinux)
            fs::create_directories (bundle / "Contents" / "x86_64-linux");
        if (withWin)
            fs::create_directories (bundle / "Contents" / "x86_64-win");
        if (withMacOS)
            fs::create_directories (bundle / "Contents" / "MacOS");
        if (! withLinux && ! withWin && ! withMacOS)
            fs::create_directories (bundle);
    }

    ~TempVST3Bundle()
    {
        std::error_code ec;
        fs::remove_all (base, ec);
    }

    // Non-copyable
    TempVST3Bundle (const TempVST3Bundle&) = delete;
    TempVST3Bundle& operator= (const TempVST3Bundle&) = delete;
};

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// isYabridgeBundle tests
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("isYabridgeBundle returns false for native Linux plugin", "[vst3_module]")
{
    TempVST3Bundle tmp ("NativePlugin", /*withLinux=*/true, /*withWin=*/false);
    REQUIRE (dc::VST3Module::isYabridgeBundle (tmp.bundle) == false);
}

TEST_CASE ("isYabridgeBundle returns true for yabridge plugin", "[vst3_module]")
{
    TempVST3Bundle tmp ("YabridgePlugin", /*withLinux=*/true, /*withWin=*/true);
    REQUIRE (dc::VST3Module::isYabridgeBundle (tmp.bundle) == true);
}

TEST_CASE ("isYabridgeBundle returns false for macOS-only plugin", "[vst3_module]")
{
    TempVST3Bundle tmp ("MacPlugin", /*withLinux=*/false, /*withWin=*/false, /*withMacOS=*/true);
    REQUIRE (dc::VST3Module::isYabridgeBundle (tmp.bundle) == false);
}

TEST_CASE ("isYabridgeBundle returns false for non-existent bundle", "[vst3_module]")
{
    REQUIRE (dc::VST3Module::isYabridgeBundle ("/nonexistent/path/fake.vst3") == false);
}

TEST_CASE ("isYabridgeBundle returns false for empty bundle", "[vst3_module]")
{
    TempVST3Bundle tmp ("EmptyPlugin", /*withLinux=*/false, /*withWin=*/false);
    REQUIRE (dc::VST3Module::isYabridgeBundle (tmp.bundle) == false);
}

TEST_CASE ("isYabridgeBundle returns true when only x86_64-win dir exists", "[vst3_module]")
{
    TempVST3Bundle tmp ("WinOnlyPlugin", /*withLinux=*/false, /*withWin=*/true);
    REQUIRE (dc::VST3Module::isYabridgeBundle (tmp.bundle) == true);
}

// ═══════════════════════════════════════════════════════════════════════
// Load / resolveLibraryPath error path tests (indirect via load())
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("load returns nullptr for non-existent bundle", "[vst3_module]")
{
    auto module = dc::VST3Module::load ("/nonexistent/path/fake.vst3");
    REQUIRE (module == nullptr);
}

TEST_CASE ("load returns nullptr when bundle exists but has no .so", "[vst3_module]")
{
    TempVST3Bundle tmp ("NoSoPlugin", /*withLinux=*/true, /*withWin=*/false);
    // Bundle directory exists with Contents/x86_64-linux/ but no .so file inside
    auto module = dc::VST3Module::load (tmp.bundle);
    REQUIRE (module == nullptr);
}

TEST_CASE ("load with skipProbe=true returns nullptr on empty bundle", "[vst3_module]")
{
    TempVST3Bundle tmp ("SkipProbePlugin", /*withLinux=*/true, /*withWin=*/false);
    // Even with skipProbe=true, load still fails because there is no .so
    auto module = dc::VST3Module::load (tmp.bundle, /*skipProbe=*/true);
    REQUIRE (module == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// probeModuleSafe tests
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("probeModuleSafe returns false for non-existent file", "[vst3_module]")
{
    REQUIRE (dc::VST3Module::probeModuleSafe ("/nonexistent/path/fake.vst3") == false);
}

TEST_CASE ("probeModuleSafe returns false for empty bundle", "[vst3_module]")
{
    TempVST3Bundle tmp ("ProbeEmptyPlugin", /*withLinux=*/true, /*withWin=*/false);
    // Bundle has Contents/x86_64-linux/ but no .so file
    REQUIRE (dc::VST3Module::probeModuleSafe (tmp.bundle) == false);
}
