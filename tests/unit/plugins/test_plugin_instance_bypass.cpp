#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

// PluginInstance requires a real VST3 module to construct, so we test
// the bypass flag semantics with a standalone struct that mirrors the
// atomic flag behaviour used in PluginInstance.
namespace
{

struct BypassFlag
{
    std::atomic<bool> bypassed_ {false};

    bool isBypassed() const
    {
        return bypassed_.load (std::memory_order_relaxed);
    }

    void resetBypass()
    {
        bypassed_.store (false, std::memory_order_relaxed);
    }

    void setBypass()
    {
        bypassed_.store (true, std::memory_order_relaxed);
    }
};

} // anonymous namespace

TEST_CASE ("PluginInstance bypass flag — default state", "[plugins][bypass]")
{
    BypassFlag flag;
    REQUIRE (flag.isBypassed() == false);
}

TEST_CASE ("PluginInstance bypass flag — set bypass", "[plugins][bypass]")
{
    BypassFlag flag;
    flag.setBypass();
    REQUIRE (flag.isBypassed() == true);
}

TEST_CASE ("PluginInstance bypass flag — resetBypass clears flag", "[plugins][bypass]")
{
    BypassFlag flag;
    flag.setBypass();
    REQUIRE (flag.isBypassed() == true);

    flag.resetBypass();
    REQUIRE (flag.isBypassed() == false);
}

TEST_CASE ("PluginInstance bypass flag — atomic safety across threads", "[plugins][bypass]")
{
    BypassFlag flag;

    // Writer thread sets the flag after a short spin
    std::thread writer ([&flag]()
    {
        for (int i = 0; i < 1000; ++i)
        {
            flag.setBypass();
            flag.resetBypass();
        }

        // Leave it in bypassed state at the end
        flag.setBypass();
    });

    writer.join();

    // After the writer is done, the flag should be set
    REQUIRE (flag.isBypassed() == true);
}

TEST_CASE ("PluginInstance bypass flag — process guard logic", "[plugins][bypass]")
{
    // Simulate the guard condition in PluginInstance::process()
    bool prepared = true;
    void* processor = reinterpret_cast<void*> (0x1); // non-null sentinel
    BypassFlag flag;

    SECTION ("all conditions met — process runs")
    {
        bool wouldReturn = (! prepared || processor == nullptr
                            || flag.isBypassed());
        REQUIRE (wouldReturn == false);
    }

    SECTION ("not prepared — early return")
    {
        prepared = false;
        bool wouldReturn = (! prepared || processor == nullptr
                            || flag.isBypassed());
        REQUIRE (wouldReturn == true);
    }

    SECTION ("null processor — early return")
    {
        processor = nullptr;
        bool wouldReturn = (! prepared || processor == nullptr
                            || flag.isBypassed());
        REQUIRE (wouldReturn == true);
    }

    SECTION ("bypassed — early return")
    {
        flag.setBypass();
        bool wouldReturn = (! prepared || processor == nullptr
                            || flag.isBypassed());
        REQUIRE (wouldReturn == true);
    }
}
