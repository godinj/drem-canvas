#include <catch2/catch_test_macros.hpp>
#include <dc/model/PropertyTree.h>
#include <dc/model/UndoManager.h>

#include <string>
#include <vector>

using dc::PropertyId;
using dc::PropertyTree;
using dc::UndoManager;
using dc::Variant;

// ═══════════════════════════════════════════════════════════════
// Construction and validity
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: default construction produces invalid tree", "[model][property_tree]")
{
    PropertyTree tree;
    REQUIRE_FALSE (tree.isValid());
}

TEST_CASE ("PropertyTree: construction with type produces valid tree", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Track"));
    REQUIRE (tree.isValid());
    REQUIRE (tree.getType() == PropertyId ("Track"));
}

// ═══════════════════════════════════════════════════════════════
// Property operations
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: setProperty / getProperty round-trip", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));

    SECTION ("Int property")
    {
        tree.setProperty (PropertyId ("volume"), Variant (100));
        REQUIRE (tree.getProperty (PropertyId ("volume")).toInt() == 100);
    }

    SECTION ("Double property")
    {
        tree.setProperty (PropertyId ("gain"), Variant (0.75));
        REQUIRE (tree.getProperty (PropertyId ("gain")).toDouble() == 0.75);
    }

    SECTION ("Bool property")
    {
        tree.setProperty (PropertyId ("muted"), Variant (true));
        REQUIRE (tree.getProperty (PropertyId ("muted")).toBool() == true);
    }

    SECTION ("String property")
    {
        tree.setProperty (PropertyId ("name"), Variant ("Track 1"));
        REQUIRE (tree.getProperty (PropertyId ("name")).toString() == "Track 1");
    }

    SECTION ("Binary property")
    {
        std::vector<uint8_t> data = { 0xAA, 0xBB };
        tree.setProperty (PropertyId ("blob"), Variant (data));
        REQUIRE (tree.getProperty (PropertyId ("blob")).toBinary() == data);
    }
}

TEST_CASE ("PropertyTree: getProperty on missing key returns Void", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    auto result = tree.getProperty (PropertyId ("nonexistent"));
    REQUIRE (result.isVoid());
}

TEST_CASE ("PropertyTree: getProperty with fallback returns fallback for missing key", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    auto result = tree.getProperty (PropertyId ("missing"), Variant (42));
    REQUIRE (result.toInt() == 42);
}

TEST_CASE ("PropertyTree: getProperty with fallback returns value when present", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    tree.setProperty (PropertyId ("x"), Variant (10));
    auto result = tree.getProperty (PropertyId ("x"), Variant (42));
    REQUIRE (result.toInt() == 10);
}

TEST_CASE ("PropertyTree: hasProperty", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));

    REQUIRE_FALSE (tree.hasProperty (PropertyId ("x")));

    tree.setProperty (PropertyId ("x"), Variant (1));
    REQUIRE (tree.hasProperty (PropertyId ("x")));
}

TEST_CASE ("PropertyTree: removeProperty", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    tree.setProperty (PropertyId ("x"), Variant (42));

    auto old = tree.removeProperty (PropertyId ("x"));
    REQUIRE (old.toInt() == 42);
    REQUIRE_FALSE (tree.hasProperty (PropertyId ("x")));
}

TEST_CASE ("PropertyTree: removeProperty on missing key returns Void", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    auto old = tree.removeProperty (PropertyId ("nonexistent"));
    REQUIRE (old.isVoid());
}

TEST_CASE ("PropertyTree: getNumProperties", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    REQUIRE (tree.getNumProperties() == 0);

    tree.setProperty (PropertyId ("a"), Variant (1));
    REQUIRE (tree.getNumProperties() == 1);

    tree.setProperty (PropertyId ("b"), Variant (2));
    REQUIRE (tree.getNumProperties() == 2);

    tree.removeProperty (PropertyId ("a"));
    REQUIRE (tree.getNumProperties() == 1);
}

TEST_CASE ("PropertyTree: getPropertyName returns correct PropertyId", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    tree.setProperty (PropertyId ("alpha"), Variant (1));
    tree.setProperty (PropertyId ("beta"), Variant (2));

    REQUIRE (tree.getPropertyName (0) == PropertyId ("alpha"));
    REQUIRE (tree.getPropertyName (1) == PropertyId ("beta"));
}

TEST_CASE ("PropertyTree: setting same value again does not notify", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    tree.setProperty (PropertyId ("x"), Variant (42));

    int callCount = 0;
    struct Counter : PropertyTree::Listener
    {
        int& count;
        Counter (int& c) : count (c) {}
        void propertyChanged (PropertyTree&, PropertyId) override { ++count; }
    } listener (callCount);

    tree.addListener (&listener);
    tree.setProperty (PropertyId ("x"), Variant (42));  // same value
    tree.removeListener (&listener);

    REQUIRE (callCount == 0);
}

TEST_CASE ("PropertyTree: overwriting property updates value", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    tree.setProperty (PropertyId ("x"), Variant (1));
    tree.setProperty (PropertyId ("x"), Variant (2));

    REQUIRE (tree.getProperty (PropertyId ("x")).toInt() == 2);
    REQUIRE (tree.getNumProperties() == 1);
}

// ═══════════════════════════════════════════════════════════════
// Child operations
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: addChild / getChild round-trip", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));

    parent.addChild (child, 0);

    REQUIRE (parent.getNumChildren() == 1);
    REQUIRE (parent.getChild (0) == child);
}

TEST_CASE ("PropertyTree: addChild at various positions", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));
    PropertyTree c (PropertyId ("C"));
    PropertyTree d (PropertyId ("D"));

    parent.addChild (a, 0);   // [A]
    parent.addChild (b, -1);  // [A, B]  (-1 means append)
    parent.addChild (c, 0);   // [C, A, B]  (insert at beginning)
    parent.addChild (d, 1);   // [C, D, A, B]  (insert at middle)

    REQUIRE (parent.getNumChildren() == 4);
    REQUIRE (parent.getChild (0) == c);
    REQUIRE (parent.getChild (1) == d);
    REQUIRE (parent.getChild (2) == a);
    REQUIRE (parent.getChild (3) == b);
}

TEST_CASE ("PropertyTree: addChild with index beyond bounds appends", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));

    parent.addChild (a, 100);  // beyond bounds
    parent.addChild (b, 100);

    REQUIRE (parent.getNumChildren() == 2);
    REQUIRE (parent.getChild (0) == a);
    REQUIRE (parent.getChild (1) == b);
}

TEST_CASE ("PropertyTree: removeChild by index", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));
    PropertyTree c (PropertyId ("C"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);
    parent.addChild (c, -1);

    parent.removeChild (1);  // remove B

    REQUIRE (parent.getNumChildren() == 2);
    REQUIRE (parent.getChild (0) == a);
    REQUIRE (parent.getChild (1) == c);
}

TEST_CASE ("PropertyTree: removeChild by reference", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);

    parent.removeChild (a);

    REQUIRE (parent.getNumChildren() == 1);
    REQUIRE (parent.getChild (0) == b);
}

TEST_CASE ("PropertyTree: removeAllChildren", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    parent.addChild (PropertyTree (PropertyId ("A")), -1);
    parent.addChild (PropertyTree (PropertyId ("B")), -1);
    parent.addChild (PropertyTree (PropertyId ("C")), -1);

    parent.removeAllChildren();

    REQUIRE (parent.getNumChildren() == 0);
}

TEST_CASE ("PropertyTree: moveChild", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));
    PropertyTree c (PropertyId ("C"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);
    parent.addChild (c, -1);  // [A, B, C]

    parent.moveChild (0, 2);  // move A to end: [B, C, A]

    REQUIRE (parent.getChild (0) == b);
    REQUIRE (parent.getChild (1) == c);
    REQUIRE (parent.getChild (2) == a);
}

TEST_CASE ("PropertyTree: moveChild to same position is no-op", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);

    int callCount = 0;
    struct Counter : PropertyTree::Listener
    {
        int& count;
        Counter (int& c) : count (c) {}
        void childOrderChanged (PropertyTree&, int, int) override { ++count; }
    } listener (callCount);

    parent.addListener (&listener);
    parent.moveChild (0, 0);  // same position
    parent.removeListener (&listener);

    REQUIRE (callCount == 0);
}

TEST_CASE ("PropertyTree: getChild out of bounds returns invalid tree", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    parent.addChild (PropertyTree (PropertyId ("A")), -1);

    REQUIRE_FALSE (parent.getChild (-1).isValid());
    REQUIRE_FALSE (parent.getChild (1).isValid());
    REQUIRE_FALSE (parent.getChild (100).isValid());
}

TEST_CASE ("PropertyTree: getChildWithType", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Root"));
    PropertyTree track (PropertyId ("Track"));
    PropertyTree clip (PropertyId ("Clip"));

    parent.addChild (track, -1);
    parent.addChild (clip, -1);

    auto found = parent.getChildWithType (PropertyId ("Clip"));
    REQUIRE (found == clip);
}

TEST_CASE ("PropertyTree: getChildWithType returns invalid when not found", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Root"));
    parent.addChild (PropertyTree (PropertyId ("Track")), -1);

    auto found = parent.getChildWithType (PropertyId ("Mixer"));
    REQUIRE_FALSE (found.isValid());
}

TEST_CASE ("PropertyTree: getChildWithProperty", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Root"));

    PropertyTree a (PropertyId ("Track"));
    a.setProperty (PropertyId ("name"), Variant ("Drums"));

    PropertyTree b (PropertyId ("Track"));
    b.setProperty (PropertyId ("name"), Variant ("Bass"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);

    auto found = parent.getChildWithProperty (PropertyId ("name"), Variant ("Bass"));
    REQUIRE (found == b);
}

TEST_CASE ("PropertyTree: getChildWithProperty returns invalid when not found", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Root"));
    PropertyTree child (PropertyId ("Track"));
    child.setProperty (PropertyId ("name"), Variant ("Drums"));
    parent.addChild (child, -1);

    auto found = parent.getChildWithProperty (PropertyId ("name"), Variant ("Guitar"));
    REQUIRE_FALSE (found.isValid());
}

TEST_CASE ("PropertyTree: indexOf", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Root"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));
    PropertyTree c (PropertyId ("C"));
    PropertyTree orphan (PropertyId ("Orphan"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);
    parent.addChild (c, -1);

    REQUIRE (parent.indexOf (a) == 0);
    REQUIRE (parent.indexOf (b) == 1);
    REQUIRE (parent.indexOf (c) == 2);
    REQUIRE (parent.indexOf (orphan) == -1);
}

// ═══════════════════════════════════════════════════════════════
// Parent tracking
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: getParent after addChild", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));

    parent.addChild (child, -1);

    REQUIRE (child.getParent() == parent);
}

TEST_CASE ("PropertyTree: adding child to new parent removes from old parent", "[model][property_tree]")
{
    PropertyTree oldParent (PropertyId ("OldParent"));
    PropertyTree newParent (PropertyId ("NewParent"));
    PropertyTree child (PropertyId ("Child"));

    oldParent.addChild (child, -1);
    REQUIRE (oldParent.getNumChildren() == 1);

    newParent.addChild (child, -1);

    REQUIRE (oldParent.getNumChildren() == 0);
    REQUIRE (newParent.getNumChildren() == 1);
    REQUIRE (child.getParent() == newParent);
}

TEST_CASE ("PropertyTree: root tree getParent returns invalid", "[model][property_tree]")
{
    PropertyTree root (PropertyId ("Root"));
    REQUIRE_FALSE (root.getParent().isValid());
}

TEST_CASE ("PropertyTree: removed child has no parent", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));

    parent.addChild (child, -1);
    parent.removeChild (0);

    REQUIRE_FALSE (child.getParent().isValid());
}

// ═══════════════════════════════════════════════════════════════
// Listener contract
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: propertyChanged fires on setProperty", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));

    PropertyTree receivedTree;
    PropertyId receivedProp ("");
    int callCount = 0;

    struct TestListener : PropertyTree::Listener
    {
        PropertyTree& rTree;
        PropertyId& rProp;
        int& count;

        TestListener (PropertyTree& t, PropertyId& p, int& c)
            : rTree (t), rProp (p), count (c) {}

        void propertyChanged (PropertyTree& tree, PropertyId property) override
        {
            rTree = tree;
            rProp = property;
            ++count;
        }
    } listener (receivedTree, receivedProp, callCount);

    tree.addListener (&listener);
    tree.setProperty (PropertyId ("volume"), Variant (75));
    tree.removeListener (&listener);

    REQUIRE (callCount == 1);
    REQUIRE (receivedTree == tree);
    REQUIRE (receivedProp == PropertyId ("volume"));
}

TEST_CASE ("PropertyTree: childAdded fires on addChild", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));

    PropertyTree receivedParent;
    PropertyTree receivedChild;
    int callCount = 0;

    struct TestListener : PropertyTree::Listener
    {
        PropertyTree& rParent;
        PropertyTree& rChild;
        int& count;

        TestListener (PropertyTree& p, PropertyTree& c, int& n)
            : rParent (p), rChild (c), count (n) {}

        void childAdded (PropertyTree& par, PropertyTree& ch) override
        {
            rParent = par;
            rChild = ch;
            ++count;
        }
    } listener (receivedParent, receivedChild, callCount);

    parent.addListener (&listener);
    parent.addChild (child, -1);
    parent.removeListener (&listener);

    REQUIRE (callCount == 1);
    REQUIRE (receivedParent == parent);
    REQUIRE (receivedChild == child);
}

TEST_CASE ("PropertyTree: childRemoved fires on removeChild", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));
    parent.addChild (child, -1);

    PropertyTree receivedParent;
    PropertyTree receivedChild;
    int receivedIndex = -1;
    int callCount = 0;

    struct TestListener : PropertyTree::Listener
    {
        PropertyTree& rParent;
        PropertyTree& rChild;
        int& rIndex;
        int& count;

        TestListener (PropertyTree& p, PropertyTree& c, int& idx, int& n)
            : rParent (p), rChild (c), rIndex (idx), count (n) {}

        void childRemoved (PropertyTree& par, PropertyTree& ch, int index) override
        {
            rParent = par;
            rChild = ch;
            rIndex = index;
            ++count;
        }
    } listener (receivedParent, receivedChild, receivedIndex, callCount);

    parent.addListener (&listener);
    parent.removeChild (0);
    parent.removeListener (&listener);

    REQUIRE (callCount == 1);
    REQUIRE (receivedParent == parent);
    REQUIRE (receivedChild == child);
    REQUIRE (receivedIndex == 0);
}

TEST_CASE ("PropertyTree: childOrderChanged fires on moveChild", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    parent.addChild (PropertyTree (PropertyId ("A")), -1);
    parent.addChild (PropertyTree (PropertyId ("B")), -1);

    int receivedOld = -1;
    int receivedNew = -1;
    int callCount = 0;

    struct TestListener : PropertyTree::Listener
    {
        int& rOld;
        int& rNew;
        int& count;

        TestListener (int& o, int& n, int& c)
            : rOld (o), rNew (n), count (c) {}

        void childOrderChanged (PropertyTree&, int oldIdx, int newIdx) override
        {
            rOld = oldIdx;
            rNew = newIdx;
            ++count;
        }
    } listener (receivedOld, receivedNew, callCount);

    parent.addListener (&listener);
    parent.moveChild (0, 1);
    parent.removeListener (&listener);

    REQUIRE (callCount == 1);
    REQUIRE (receivedOld == 0);
    REQUIRE (receivedNew == 1);
}

TEST_CASE ("PropertyTree: parentChanged fires when re-parented", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));

    int callCount = 0;

    struct TestListener : PropertyTree::Listener
    {
        int& count;
        TestListener (int& c) : count (c) {}
        void parentChanged (PropertyTree&) override { ++count; }
    } listener (callCount);

    child.addListener (&listener);
    parent.addChild (child, -1);
    child.removeListener (&listener);

    REQUIRE (callCount == 1);
}

TEST_CASE ("PropertyTree: remove listener during callback is safe", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));

    struct SelfRemovingListener : PropertyTree::Listener
    {
        PropertyTree& tree;
        int callCount = 0;

        SelfRemovingListener (PropertyTree& t) : tree (t) {}

        void propertyChanged (PropertyTree&, PropertyId) override
        {
            ++callCount;
            tree.removeListener (this);
        }
    } listener (tree);

    tree.addListener (&listener);
    tree.setProperty (PropertyId ("x"), Variant (1));
    tree.setProperty (PropertyId ("x"), Variant (2));

    // First setProperty fires the callback, which removes the listener.
    // Second setProperty should not fire it again.
    REQUIRE (listener.callCount == 1);
}

TEST_CASE ("PropertyTree: listener on parent notified of child property change (bubbling)", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));
    parent.addChild (child, -1);

    int parentCallCount = 0;
    PropertyTree receivedTree;

    struct TestListener : PropertyTree::Listener
    {
        int& count;
        PropertyTree& rTree;

        TestListener (int& c, PropertyTree& t) : count (c), rTree (t) {}

        void propertyChanged (PropertyTree& tree, PropertyId) override
        {
            rTree = tree;
            ++count;
        }
    } listener (parentCallCount, receivedTree);

    parent.addListener (&listener);
    child.setProperty (PropertyId ("x"), Variant (1));
    parent.removeListener (&listener);

    REQUIRE (parentCallCount == 1);
    // The tree passed to the callback is the child where the property changed
    REQUIRE (receivedTree == child);
}

// ═══════════════════════════════════════════════════════════════
// Deep copy
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: createDeepCopy produces equal tree", "[model][property_tree]")
{
    PropertyTree original (PropertyId ("Root"));
    original.setProperty (PropertyId ("name"), Variant ("test"));
    original.setProperty (PropertyId ("value"), Variant (42));

    PropertyTree child (PropertyId ("Child"));
    child.setProperty (PropertyId ("x"), Variant (100));
    original.addChild (child, -1);

    auto copy = original.createDeepCopy();

    REQUIRE (copy.isValid());
    REQUIRE (copy.getType() == PropertyId ("Root"));
    REQUIRE (copy.getProperty (PropertyId ("name")).toString() == "test");
    REQUIRE (copy.getProperty (PropertyId ("value")).toInt() == 42);
    REQUIRE (copy.getNumChildren() == 1);
    REQUIRE (copy.getChild (0).getType() == PropertyId ("Child"));
    REQUIRE (copy.getChild (0).getProperty (PropertyId ("x")).toInt() == 100);
}

TEST_CASE ("PropertyTree: mutations on copy do not affect original", "[model][property_tree]")
{
    PropertyTree original (PropertyId ("Root"));
    original.setProperty (PropertyId ("x"), Variant (1));

    PropertyTree child (PropertyId ("Child"));
    original.addChild (child, -1);

    auto copy = original.createDeepCopy();

    copy.setProperty (PropertyId ("x"), Variant (999));
    copy.removeAllChildren();

    REQUIRE (original.getProperty (PropertyId ("x")).toInt() == 1);
    REQUIRE (original.getNumChildren() == 1);
}

TEST_CASE ("PropertyTree: deep copy has no parent", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));
    parent.addChild (child, -1);

    auto copy = parent.createDeepCopy();
    REQUIRE_FALSE (copy.getParent().isValid());
}

TEST_CASE ("PropertyTree: deep copy of invalid tree returns invalid", "[model][property_tree]")
{
    PropertyTree invalid;
    auto copy = invalid.createDeepCopy();
    REQUIRE_FALSE (copy.isValid());
}

// ═══════════════════════════════════════════════════════════════
// Undo integration
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: setProperty with undoManager records action", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    UndoManager undo;

    tree.setProperty (PropertyId ("x"), Variant (1), &undo);

    REQUIRE (undo.canUndo());
}

TEST_CASE ("PropertyTree: undo restores previous property value", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    UndoManager undo;

    tree.setProperty (PropertyId ("x"), Variant (1));

    undo.beginTransaction ("change x");
    tree.setProperty (PropertyId ("x"), Variant (2), &undo);

    REQUIRE (tree.getProperty (PropertyId ("x")).toInt() == 2);

    undo.undo();

    REQUIRE (tree.getProperty (PropertyId ("x")).toInt() == 1);
}

TEST_CASE ("PropertyTree: redo re-applies property change", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    UndoManager undo;

    tree.setProperty (PropertyId ("x"), Variant (1));

    undo.beginTransaction ("change x");
    tree.setProperty (PropertyId ("x"), Variant (2), &undo);

    undo.undo();
    REQUIRE (tree.getProperty (PropertyId ("x")).toInt() == 1);

    undo.redo();
    REQUIRE (tree.getProperty (PropertyId ("x")).toInt() == 2);
}

TEST_CASE ("PropertyTree: addChild with undo - undo removes child", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));
    UndoManager undo;

    undo.beginTransaction ("add child");
    parent.addChild (child, -1, &undo);
    REQUIRE (parent.getNumChildren() == 1);

    undo.undo();
    REQUIRE (parent.getNumChildren() == 0);
}

TEST_CASE ("PropertyTree: removeChild with undo - undo re-adds child", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree child (PropertyId ("Child"));
    child.setProperty (PropertyId ("name"), Variant ("myChild"));
    parent.addChild (child, -1);

    UndoManager undo;
    undo.beginTransaction ("remove child");
    parent.removeChild (0, &undo);

    REQUIRE (parent.getNumChildren() == 0);

    undo.undo();
    REQUIRE (parent.getNumChildren() == 1);
    // The restored child is a deep copy, so check property is preserved
    REQUIRE (parent.getChild (0).getProperty (PropertyId ("name")).toString() == "myChild");
}

TEST_CASE ("PropertyTree: setProperty undo for newly-added property removes it", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Node"));
    UndoManager undo;

    undo.beginTransaction ("add prop");
    tree.setProperty (PropertyId ("newProp"), Variant (42), &undo);
    REQUIRE (tree.hasProperty (PropertyId ("newProp")));

    undo.undo();
    // After undo, the old value was Void, so removePropertyInternal is called
    REQUIRE_FALSE (tree.hasProperty (PropertyId ("newProp")));
}

// ═══════════════════════════════════════════════════════════════
// Iterator
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: range-for over children", "[model][property_tree]")
{
    PropertyTree parent (PropertyId ("Parent"));
    PropertyTree a (PropertyId ("A"));
    PropertyTree b (PropertyId ("B"));
    PropertyTree c (PropertyId ("C"));

    parent.addChild (a, -1);
    parent.addChild (b, -1);
    parent.addChild (c, -1);

    std::vector<PropertyTree> visited;
    for (const auto& child : parent)
        visited.push_back (child);

    REQUIRE (visited.size() == 3);
    REQUIRE (visited[0] == a);
    REQUIRE (visited[1] == b);
    REQUIRE (visited[2] == c);
}

TEST_CASE ("PropertyTree: empty tree begin() == end()", "[model][property_tree]")
{
    PropertyTree tree (PropertyId ("Empty"));
    REQUIRE (tree.begin() == tree.end());
}

TEST_CASE ("PropertyTree: invalid tree begin() == end()", "[model][property_tree]")
{
    PropertyTree tree;
    REQUIRE (tree.begin() == tree.end());
}

// ═══════════════════════════════════════════════════════════════
// Identity comparison
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: identity comparison (same shared data)", "[model][property_tree]")
{
    PropertyTree a (PropertyId ("Node"));
    PropertyTree b = a;  // same shared_ptr

    REQUIRE (a == b);
}

TEST_CASE ("PropertyTree: different trees are not equal", "[model][property_tree]")
{
    PropertyTree a (PropertyId ("Node"));
    PropertyTree b (PropertyId ("Node"));

    REQUIRE (a != b);
}

TEST_CASE ("PropertyTree: two invalid trees are equal", "[model][property_tree]")
{
    PropertyTree a;
    PropertyTree b;
    REQUIRE (a == b);
}

// ═══════════════════════════════════════════════════════════════
// Edge cases on invalid trees
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyTree: getProperty on invalid tree returns Void", "[model][property_tree]")
{
    PropertyTree invalid;
    REQUIRE (invalid.getProperty (PropertyId ("x")).isVoid());
}

TEST_CASE ("PropertyTree: getNumChildren on invalid tree returns 0", "[model][property_tree]")
{
    PropertyTree invalid;
    REQUIRE (invalid.getNumChildren() == 0);
}

TEST_CASE ("PropertyTree: getNumProperties on invalid tree returns 0", "[model][property_tree]")
{
    PropertyTree invalid;
    REQUIRE (invalid.getNumProperties() == 0);
}

TEST_CASE ("PropertyTree: indexOf on invalid tree returns -1", "[model][property_tree]")
{
    PropertyTree invalid;
    PropertyTree child (PropertyId ("X"));
    REQUIRE (invalid.indexOf (child) == -1);
}
