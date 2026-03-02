#pragma once

#include "UndoAction.h"
#include "PropertyTree.h"

namespace dc {

/// Undo action for setProperty / removeProperty
class PropertyChangeAction : public UndoAction
{
public:
    PropertyChangeAction (PropertyTree tree, PropertyId property,
                          Variant oldValue, Variant newValue)
        : tree_ (std::move (tree))
        , property_ (std::move (property))
        , oldValue_ (std::move (oldValue))
        , newValue_ (std::move (newValue))
    {
    }

    void undo() override
    {
        if (oldValue_.isVoid())
            tree_.removePropertyInternal (property_);
        else
            tree_.setPropertyInternal (property_, oldValue_);
    }

    void redo() override
    {
        if (newValue_.isVoid())
            tree_.removePropertyInternal (property_);
        else
            tree_.setPropertyInternal (property_, newValue_);
    }

    std::string getDescription() const override
    {
        return "Change " + property_.toString();
    }

    bool tryMerge (const UndoAction& next) override
    {
        auto* other = dynamic_cast<const PropertyChangeAction*> (&next);
        if (other == nullptr)
            return false;

        if (tree_ == other->tree_ && property_ == other->property_)
        {
            newValue_ = other->newValue_;
            return true;
        }

        return false;
    }

private:
    PropertyTree tree_;
    PropertyId property_;
    Variant oldValue_;
    Variant newValue_;
};

/// Undo action for addChild
class ChildAddAction : public UndoAction
{
public:
    ChildAddAction (PropertyTree parent, PropertyTree child, int index)
        : parent_ (std::move (parent))
        , child_ (std::move (child))
        , index_ (index)
    {
    }

    void undo() override
    {
        parent_.removeChildInternal (index_);
    }

    void redo() override
    {
        parent_.addChildInternal (child_, index_);
    }

    std::string getDescription() const override
    {
        return "Add " + child_.getType().toString();
    }

private:
    PropertyTree parent_;
    PropertyTree child_;
    int index_;
};

/// Undo action for removeChild
class ChildRemoveAction : public UndoAction
{
public:
    ChildRemoveAction (PropertyTree parent, PropertyTree child, int index)
        : parent_ (std::move (parent))
        , child_ (std::move (child))
        , index_ (index)
    {
    }

    void undo() override
    {
        parent_.addChildInternal (child_, index_);
    }

    void redo() override
    {
        parent_.removeChildInternal (index_);
    }

    std::string getDescription() const override
    {
        return "Remove " + child_.getType().toString();
    }

private:
    PropertyTree parent_;
    PropertyTree child_;
    int index_;
};

/// Undo action for moveChild
class ChildMoveAction : public UndoAction
{
public:
    ChildMoveAction (PropertyTree parent, int oldIndex, int newIndex)
        : parent_ (std::move (parent))
        , oldIndex_ (oldIndex)
        , newIndex_ (newIndex)
    {
    }

    void undo() override
    {
        parent_.moveChildInternal (newIndex_, oldIndex_);
    }

    void redo() override
    {
        parent_.moveChildInternal (oldIndex_, newIndex_);
    }

    std::string getDescription() const override
    {
        return "Move child";
    }

private:
    PropertyTree parent_;
    int oldIndex_;
    int newIndex_;
};

} // namespace dc
