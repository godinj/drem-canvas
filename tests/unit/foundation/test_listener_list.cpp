#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/listener_list.h>
#include <string>
#include <vector>

namespace {

struct TestListener
{
    virtual ~TestListener() = default;
    virtual void onEvent(int value) = 0;
};

struct CountingListener : TestListener
{
    int callCount = 0;
    int lastValue = 0;

    void onEvent(int value) override
    {
        ++callCount;
        lastValue = value;
    }
};

} // anonymous namespace

TEST_CASE("ListenerList add and call", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    CountingListener listener;

    list.add(&listener);
    list.call([](TestListener& l) { l.onEvent(42); });

    REQUIRE(listener.callCount == 1);
    REQUIRE(listener.lastValue == 42);
}

TEST_CASE("ListenerList remove stops callbacks", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    CountingListener listener;

    list.add(&listener);
    list.call([](TestListener& l) { l.onEvent(1); });
    REQUIRE(listener.callCount == 1);

    list.remove(&listener);
    list.call([](TestListener& l) { l.onEvent(2); });
    REQUIRE(listener.callCount == 1);  // not called again
}

TEST_CASE("ListenerList duplicate add is silently ignored", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    CountingListener listener;

    list.add(&listener);
    list.add(&listener);  // duplicate

    list.call([](TestListener& l) { l.onEvent(5); });
    REQUIRE(listener.callCount == 1);  // called only once
    REQUIRE(list.size() == 1);
}

TEST_CASE("ListenerList remove during callback is safe", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;

    struct SelfRemovingListener : TestListener
    {
        dc::ListenerList<TestListener>* parentList = nullptr;
        int callCount = 0;

        void onEvent(int) override
        {
            ++callCount;
            parentList->remove(this);
        }
    };

    SelfRemovingListener remover;
    remover.parentList = &list;
    CountingListener other;

    list.add(&remover);
    list.add(&other);

    // Both should be called because call() iterates a copy
    list.call([](TestListener& l) { l.onEvent(1); });
    REQUIRE(remover.callCount == 1);
    REQUIRE(other.callCount == 1);

    // Remover should no longer be in the list
    list.call([](TestListener& l) { l.onEvent(2); });
    REQUIRE(remover.callCount == 1);  // not called again
    REQUIRE(other.callCount == 2);
}

TEST_CASE("ListenerList add during callback", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;

    struct AddingListener : TestListener
    {
        dc::ListenerList<TestListener>* parentList = nullptr;
        CountingListener* toAdd = nullptr;
        int callCount = 0;

        void onEvent(int) override
        {
            ++callCount;
            if (toAdd != nullptr)
            {
                parentList->add(toAdd);
                toAdd = nullptr;  // only add once
            }
        }
    };

    CountingListener newListener;
    AddingListener adder;
    adder.parentList = &list;
    adder.toAdd = &newListener;

    list.add(&adder);

    // First call: adder adds newListener, but newListener should NOT
    // be called in the current iteration (iterating a copy)
    list.call([](TestListener& l) { l.onEvent(1); });
    REQUIRE(adder.callCount == 1);
    REQUIRE(newListener.callCount == 0);

    // Second call: both should now be called
    list.call([](TestListener& l) { l.onEvent(2); });
    REQUIRE(adder.callCount == 2);
    REQUIRE(newListener.callCount == 1);
}

TEST_CASE("ListenerList call on empty list is no-op", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    // Should not crash
    list.call([](TestListener& l) { l.onEvent(0); });
    REQUIRE(list.isEmpty());
}

TEST_CASE("ListenerList remove listener not in list is no-op", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    CountingListener listener;

    // Should not crash
    list.remove(&listener);
    REQUIRE(list.size() == 0);
}

TEST_CASE("ListenerList multiple listeners called in order", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;

    struct OrderListener : TestListener
    {
        std::string* order;
        std::string tag;

        OrderListener(std::string* o, std::string t) : order(o), tag(std::move(t)) {}
        void onEvent(int) override { *order += tag; }
    };

    std::string order;
    OrderListener a(&order, "A");
    OrderListener b(&order, "B");
    OrderListener c(&order, "C");

    list.add(&a);
    list.add(&b);
    list.add(&c);

    list.call([](TestListener& l) { l.onEvent(0); });
    REQUIRE(order == "ABC");
}

TEST_CASE("ListenerList size and isEmpty", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    REQUIRE(list.isEmpty());
    REQUIRE(list.size() == 0);

    CountingListener a, b;
    list.add(&a);
    REQUIRE(list.size() == 1);
    REQUIRE_FALSE(list.isEmpty());

    list.add(&b);
    REQUIRE(list.size() == 2);

    list.remove(&a);
    REQUIRE(list.size() == 1);

    list.remove(&b);
    REQUIRE(list.isEmpty());
}

TEST_CASE("ListenerList add nullptr is ignored", "[foundation][listener_list]")
{
    dc::ListenerList<TestListener> list;
    list.add(nullptr);
    REQUIRE(list.size() == 0);
}
