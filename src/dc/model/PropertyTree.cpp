#include "PropertyTree.h"
#include "UndoManager.h"
#include "PropertyTreeActions.h"

#include <algorithm>
#include <cassert>

namespace dc {

// ─── Constructors ────────────────────────────────────────────

PropertyTree::PropertyTree (PropertyId type)
    : data_ (std::make_shared<Data> (std::move (type)))
{
}

PropertyTree::PropertyTree() = default;

PropertyTree::PropertyTree (std::shared_ptr<Data> data)
    : data_ (std::move (data))
{
}

bool PropertyTree::isValid() const
{
    return data_ != nullptr;
}

// ─── Type ────────────────────────────────────────────────────

PropertyId PropertyTree::getType() const
{
    assert (data_ != nullptr);
    return data_->type;
}

// ─── Properties ──────────────────────────────────────────────

Variant PropertyTree::getProperty (PropertyId name) const
{
    if (! data_)
        return {};

    for (auto& [k, v] : data_->properties)
        if (k == name)
            return v;

    return {};
}

Variant PropertyTree::getProperty (PropertyId name, const Variant& fallback) const
{
    if (! data_)
        return fallback;

    for (auto& [k, v] : data_->properties)
        if (k == name)
            return v;

    return fallback;
}

bool PropertyTree::hasProperty (PropertyId name) const
{
    if (! data_)
        return false;

    for (auto& [k, v] : data_->properties)
        if (k == name)
            return true;

    return false;
}

int PropertyTree::getNumProperties() const
{
    if (! data_)
        return 0;
    return static_cast<int> (data_->properties.size());
}

PropertyId PropertyTree::getPropertyName (int index) const
{
    assert (data_ != nullptr);
    assert (index >= 0 && index < static_cast<int> (data_->properties.size()));
    return data_->properties[static_cast<size_t> (index)].first;
}

void PropertyTree::setProperty (PropertyId name, Variant value,
                                UndoManager* undoManager)
{
    assert (data_ != nullptr);

    if (undoManager != nullptr)
    {
        auto oldValue = getProperty (name);
        undoManager->addAction (std::make_unique<PropertyChangeAction> (
            *this, name, std::move (oldValue), value));
    }

    setPropertyInternal (name, std::move (value));
}

Variant PropertyTree::removeProperty (PropertyId name,
                                      UndoManager* undoManager)
{
    assert (data_ != nullptr);

    if (undoManager != nullptr)
    {
        auto oldValue = getProperty (name);
        if (! oldValue.isVoid())
        {
            undoManager->addAction (std::make_unique<PropertyChangeAction> (
                *this, name, oldValue, Variant()));
        }
    }

    return removePropertyInternal (name);
}

// ─── Children ────────────────────────────────────────────────

int PropertyTree::getNumChildren() const
{
    if (! data_)
        return 0;
    return static_cast<int> (data_->children.size());
}

PropertyTree PropertyTree::getChild (int index) const
{
    if (! data_ || index < 0 || index >= static_cast<int> (data_->children.size()))
        return {};

    return data_->children[static_cast<size_t> (index)];
}

PropertyTree PropertyTree::getChildWithType (PropertyId type) const
{
    if (! data_)
        return {};

    for (auto& child : data_->children)
        if (child.getType() == type)
            return child;

    return {};
}

PropertyTree PropertyTree::getChildWithProperty (PropertyId prop,
                                                  const Variant& value) const
{
    if (! data_)
        return {};

    for (auto& child : data_->children)
        if (child.getProperty (prop) == value)
            return child;

    return {};
}

int PropertyTree::indexOf (const PropertyTree& child) const
{
    if (! data_)
        return -1;

    for (int i = 0; i < static_cast<int> (data_->children.size()); ++i)
        if (data_->children[static_cast<size_t> (i)] == child)
            return i;

    return -1;
}

void PropertyTree::addChild (PropertyTree child, int index,
                             UndoManager* undoManager)
{
    assert (data_ != nullptr);
    assert (child.isValid());

    int numChildren = static_cast<int> (data_->children.size());
    if (index < 0 || index > numChildren)
        index = numChildren;

    if (undoManager != nullptr)
        undoManager->addAction (std::make_unique<ChildAddAction> (
            *this, child, index));

    addChildInternal (std::move (child), index);
}

void PropertyTree::removeChild (int index, UndoManager* undoManager)
{
    assert (data_ != nullptr);

    if (index < 0 || index >= static_cast<int> (data_->children.size()))
        return;

    if (undoManager != nullptr)
    {
        auto child = data_->children[static_cast<size_t> (index)].createDeepCopy();
        undoManager->addAction (std::make_unique<ChildRemoveAction> (
            *this, std::move (child), index));
    }

    removeChildInternal (index);
}

void PropertyTree::removeChild (PropertyTree child,
                                UndoManager* undoManager)
{
    int idx = indexOf (child);
    if (idx >= 0)
        removeChild (idx, undoManager);
}

void PropertyTree::removeAllChildren (UndoManager* undoManager)
{
    // Remove in reverse order so indices stay valid for undo
    for (int i = getNumChildren() - 1; i >= 0; --i)
        removeChild (i, undoManager);
}

void PropertyTree::moveChild (int currentIndex, int newIndex,
                              UndoManager* undoManager)
{
    assert (data_ != nullptr);

    int numChildren = static_cast<int> (data_->children.size());
    if (currentIndex < 0 || currentIndex >= numChildren)
        return;
    if (newIndex < 0 || newIndex >= numChildren)
        newIndex = numChildren - 1;
    if (currentIndex == newIndex)
        return;

    if (undoManager != nullptr)
        undoManager->addAction (std::make_unique<ChildMoveAction> (
            *this, currentIndex, newIndex));

    moveChildInternal (currentIndex, newIndex);
}

// ─── Parent ──────────────────────────────────────────────────

PropertyTree PropertyTree::getParent() const
{
    if (! data_ || ! data_->parent)
        return {};

    return PropertyTree (data_->parent->shared_from_this());
}

// ─── Listeners ───────────────────────────────────────────────

void PropertyTree::addListener (Listener* listener)
{
    assert (data_ != nullptr);
    data_->listeners.add (listener);
}

void PropertyTree::removeListener (Listener* listener)
{
    assert (data_ != nullptr);
    data_->listeners.remove (listener);
}

// ─── Comparison ──────────────────────────────────────────────

bool PropertyTree::operator== (const PropertyTree& other) const
{
    return data_ == other.data_;
}

bool PropertyTree::operator!= (const PropertyTree& other) const
{
    return data_ != other.data_;
}

// ─── Deep copy ───────────────────────────────────────────────

PropertyTree PropertyTree::createDeepCopy() const
{
    if (! data_)
        return {};

    PropertyTree copy (data_->type);

    // Copy properties
    copy.data_->properties = data_->properties;

    // Deep-copy children
    for (auto& child : data_->children)
    {
        auto childCopy = child.createDeepCopy();
        childCopy.data_->parent = copy.data_.get();
        copy.data_->children.push_back (std::move (childCopy));
    }

    return copy;
}

// ─── Iterator ────────────────────────────────────────────────

PropertyTree::Iterator::Iterator (const std::vector<PropertyTree>* children, int index)
    : children_ (children), index_ (index)
{
}

const PropertyTree& PropertyTree::Iterator::operator*() const
{
    return (*children_)[static_cast<size_t> (index_)];
}

const PropertyTree* PropertyTree::Iterator::operator->() const
{
    return &(*children_)[static_cast<size_t> (index_)];
}

PropertyTree::Iterator& PropertyTree::Iterator::operator++()
{
    ++index_;
    return *this;
}

PropertyTree::Iterator PropertyTree::Iterator::operator++ (int)
{
    Iterator tmp = *this;
    ++index_;
    return tmp;
}

bool PropertyTree::Iterator::operator== (const Iterator& other) const
{
    return index_ == other.index_;
}

bool PropertyTree::Iterator::operator!= (const Iterator& other) const
{
    return index_ != other.index_;
}

PropertyTree::Iterator PropertyTree::begin() const
{
    if (! data_)
        return {};
    return Iterator (&data_->children, 0);
}

PropertyTree::Iterator PropertyTree::end() const
{
    if (! data_)
        return {};
    return Iterator (&data_->children, static_cast<int> (data_->children.size()));
}

// ─── Internal mutation methods ───────────────────────────────

void PropertyTree::setPropertyInternal (PropertyId name, Variant value)
{
    assert (data_ != nullptr);

    for (auto& [k, v] : data_->properties)
    {
        if (k == name)
        {
            if (! (v == value))
            {
                v = std::move (value);
                notifyPropertyChanged (name);
            }
            return;
        }
    }

    // Property not found — add it
    data_->properties.emplace_back (name, std::move (value));
    notifyPropertyChanged (name);
}

Variant PropertyTree::removePropertyInternal (PropertyId name)
{
    assert (data_ != nullptr);

    for (auto it = data_->properties.begin(); it != data_->properties.end(); ++it)
    {
        if (it->first == name)
        {
            Variant old = std::move (it->second);
            data_->properties.erase (it);
            notifyPropertyChanged (name);
            return old;
        }
    }

    return {};
}

void PropertyTree::addChildInternal (PropertyTree child, int index)
{
    assert (data_ != nullptr);
    assert (child.isValid());

    // Remove from old parent if any
    if (child.data_->parent != nullptr)
    {
        auto* oldParent = child.data_->parent;
        auto& siblings = oldParent->children;
        for (auto it = siblings.begin(); it != siblings.end(); ++it)
        {
            if (it->data_ == child.data_)
            {
                siblings.erase (it);
                break;
            }
        }
    }

    child.data_->parent = data_.get();

    auto pos = data_->children.begin() + index;
    data_->children.insert (pos, child);

    notifyChildAdded (child);
    child.notifyParentChanged();
}

PropertyTree PropertyTree::removeChildInternal (int index)
{
    assert (data_ != nullptr);
    assert (index >= 0 && index < static_cast<int> (data_->children.size()));

    auto child = data_->children[static_cast<size_t> (index)];
    child.data_->parent = nullptr;
    data_->children.erase (data_->children.begin() + index);

    notifyChildRemoved (child, index);
    child.notifyParentChanged();

    return child;
}

void PropertyTree::moveChildInternal (int currentIndex, int newIndex)
{
    assert (data_ != nullptr);

    auto& c = data_->children;
    assert (currentIndex >= 0 && currentIndex < static_cast<int> (c.size()));
    assert (newIndex >= 0 && newIndex < static_cast<int> (c.size()));

    if (currentIndex == newIndex)
        return;

    auto child = c[static_cast<size_t> (currentIndex)];
    c.erase (c.begin() + currentIndex);
    c.insert (c.begin() + newIndex, child);

    notifyChildOrderChanged (currentIndex, newIndex);
}

// ─── Listener notification (bubbles up to ancestors) ─────────

void PropertyTree::notifyPropertyChanged (PropertyId property)
{
    auto self = *this;
    Data* node = data_.get();

    while (node != nullptr)
    {
        node->listeners.call ([&self, &property] (Listener& l)
        {
            l.propertyChanged (self, property);
        });

        node = node->parent;
    }
}

void PropertyTree::notifyChildAdded (PropertyTree& child)
{
    auto self = *this;
    Data* node = data_.get();

    while (node != nullptr)
    {
        node->listeners.call ([&self, &child] (Listener& l)
        {
            l.childAdded (self, child);
        });

        node = node->parent;
    }
}

void PropertyTree::notifyChildRemoved (PropertyTree& child, int index)
{
    auto self = *this;
    Data* node = data_.get();

    while (node != nullptr)
    {
        node->listeners.call ([&self, &child, index] (Listener& l)
        {
            l.childRemoved (self, child, index);
        });

        node = node->parent;
    }
}

void PropertyTree::notifyChildOrderChanged (int oldIndex, int newIndex)
{
    auto self = *this;
    Data* node = data_.get();

    while (node != nullptr)
    {
        node->listeners.call ([&self, oldIndex, newIndex] (Listener& l)
        {
            l.childOrderChanged (self, oldIndex, newIndex);
        });

        node = node->parent;
    }
}

void PropertyTree::notifyParentChanged()
{
    auto self = *this;
    data_->listeners.call ([&self] (Listener& l)
    {
        l.parentChanged (self);
    });
}

} // namespace dc
