#pragma once

#if defined(DC_LIBRARY_BUILD) && defined(JUCE_DATA_STRUCTURES_H_INCLUDED)
  #error "dc::model must not depend on JUCE data structures — Phase 1 boundary violation. " \
         "See docs/sans-juce/01-observable-model.md"
#endif

#include "PropertyId.h"
#include "Variant.h"
#include "dc/foundation/listener_list.h"

#include <memory>
#include <vector>

namespace dc {

class UndoManager;

class PropertyTree
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void propertyChanged (PropertyTree& /*tree*/,
                                      PropertyId /*property*/) {}
        virtual void childAdded (PropertyTree& /*parent*/,
                                 PropertyTree& /*child*/) {}
        virtual void childRemoved (PropertyTree& /*parent*/,
                                   PropertyTree& /*child*/,
                                   int /*index*/) {}
        virtual void childOrderChanged (PropertyTree& /*parent*/,
                                        int /*oldIndex*/, int /*newIndex*/) {}
        virtual void parentChanged (PropertyTree& /*tree*/) {}
    };

    /// Create a new tree node with the given type
    explicit PropertyTree (PropertyId type);

    /// Invalid/empty tree (like default ValueTree)
    PropertyTree();

    bool isValid() const;

    // --- Type ---
    PropertyId getType() const;

    // --- Properties ---
    Variant getProperty (PropertyId name) const;
    Variant getProperty (PropertyId name, const Variant& fallback) const;
    bool hasProperty (PropertyId name) const;
    int getNumProperties() const;
    PropertyId getPropertyName (int index) const;

    /// Set property. If undoManager is non-null, records an UndoAction.
    void setProperty (PropertyId name, Variant value,
                      UndoManager* undoManager = nullptr);

    /// Remove property. Returns previous value.
    Variant removeProperty (PropertyId name,
                            UndoManager* undoManager = nullptr);

    // --- Children ---
    int getNumChildren() const;
    PropertyTree getChild (int index) const;
    PropertyTree getChildWithType (PropertyId type) const;
    PropertyTree getChildWithProperty (PropertyId prop,
                                       const Variant& value) const;
    int indexOf (const PropertyTree& child) const;

    void addChild (PropertyTree child, int index,
                   UndoManager* undoManager = nullptr);
    void removeChild (int index, UndoManager* undoManager = nullptr);
    void removeChild (PropertyTree child,
                      UndoManager* undoManager = nullptr);
    void removeAllChildren (UndoManager* undoManager = nullptr);
    void moveChild (int currentIndex, int newIndex,
                    UndoManager* undoManager = nullptr);

    // --- Parent ---
    PropertyTree getParent() const;

    // --- Listeners ---
    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    // --- Comparison (identity) ---
    bool operator== (const PropertyTree& other) const;
    bool operator!= (const PropertyTree& other) const;

    // --- Deep copy ---
    PropertyTree createDeepCopy() const;

    // --- Iteration over children ---
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = PropertyTree;
        using difference_type = std::ptrdiff_t;
        using pointer = const PropertyTree*;
        using reference = const PropertyTree&;

        Iterator() = default;
        Iterator (const std::vector<PropertyTree>* children, int index);

        reference operator*() const;
        pointer operator->() const;
        Iterator& operator++();
        Iterator operator++ (int);
        bool operator== (const Iterator& other) const;
        bool operator!= (const Iterator& other) const;

    private:
        const std::vector<PropertyTree>* children_ = nullptr;
        int index_ = 0;
    };

    Iterator begin() const;
    Iterator end() const;

private:
    struct Data : std::enable_shared_from_this<Data>
    {
        PropertyId type;
        std::vector<std::pair<PropertyId, Variant>> properties;
        std::vector<PropertyTree> children;
        Data* parent = nullptr;
        ListenerList<Listener> listeners;

        explicit Data (PropertyId t) : type (std::move (t)) {}
    };

    std::shared_ptr<Data> data_;

    // Private constructor wrapping existing Data (for getParent)
    explicit PropertyTree (std::shared_ptr<Data> data);

    // Internal mutation methods (no undo recording, used by undo actions)
    void setPropertyInternal (PropertyId name, Variant value);
    Variant removePropertyInternal (PropertyId name);
    void addChildInternal (PropertyTree child, int index);
    PropertyTree removeChildInternal (int index);
    void moveChildInternal (int currentIndex, int newIndex);

    // Notify listeners on this node and all ancestors
    void notifyPropertyChanged (PropertyId property);
    void notifyChildAdded (PropertyTree& child);
    void notifyChildRemoved (PropertyTree& child, int index);
    void notifyChildOrderChanged (int oldIndex, int newIndex);
    void notifyParentChanged();

    friend class PropertyChangeAction;
    friend class ChildAddAction;
    friend class ChildRemoveAction;
    friend class ChildMoveAction;
};

} // namespace dc
