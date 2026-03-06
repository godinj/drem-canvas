#include "vim/adapters/MixerAdapter.h"
#include "vim/ActionRegistry.h"
#include "model/Track.h"
#include "model/Project.h"
#include <algorithm>

namespace dc
{

static bool isEscapeOrCtrlC (const dc::KeyPress& key)
{
    if (key == dc::KeyCode::Escape)
        return true;

    if (key.control)
    {
        auto c = key.getTextCharacter();
        if (c == 3 || c == 'c' || c == 'C')
            return true;
    }

    return false;
}

MixerAdapter::MixerAdapter (Arrangement& arr, VimContext& ctx)
    : arrangement (arr), context (ctx)
{
}

int MixerAdapter::getMixerPluginCount() const
{
    if (context.isMasterStripSelected())
    {
        auto masterBus = arrangement.getProject().getState().getChildWithType (IDs::MASTER_BUS);
        if (masterBus.isValid())
        {
            auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
            return chain.isValid() ? chain.getNumChildren() : 0;
        }
        return 0;
    }

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return 0;

    Track track = arrangement.getTrack (trackIdx);
    return track.getNumPlugins();
}

bool MixerAdapter::handleRawKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape / Ctrl-C
    if (isEscapeOrCtrlC (key))
    {
        if (onContextChanged) onContextChanged();
        return true;
    }

    // h/l: move between strips
    if (keyChar == 'h')
    {
        if (context.isMasterStripSelected())
        {
            // Move from master to last regular track
            context.setMasterStripSelected (false);
            int numTracks = arrangement.getNumTracks();
            if (numTracks > 0)
                arrangement.selectTrack (numTracks - 1);
        }
        else
        {
            int idx = arrangement.getSelectedTrackIndex();
            if (idx > 0)
                arrangement.selectTrack (idx - 1);
        }
        context.setSelectedPluginSlot (0);
        if (onContextChanged) onContextChanged();
        return true;
    }

    if (keyChar == 'l')
    {
        if (! context.isMasterStripSelected())
        {
            int idx = arrangement.getSelectedTrackIndex();
            int numTracks = arrangement.getNumTracks();

            if (idx < numTracks - 1)
            {
                arrangement.selectTrack (idx + 1);
            }
            else
            {
                // Past last track -> select master
                context.setMasterStripSelected (true);
            }
        }
        context.setSelectedPluginSlot (0);
        if (onContextChanged) onContextChanged();
        return true;
    }

    // j/k: focus cycling and plugin slot navigation
    auto focus = context.getMixerFocus();

    if (keyChar == 'j')
    {
        if (focus == VimContext::FocusPlugins)
        {
            // Navigate plugin slots downward
            int numPlugins = getMixerPluginCount();
            int maxSlot = std::max (numPlugins, 3);
            int slot = context.getSelectedPluginSlot();
            if (slot < maxSlot)
            {
                context.setSelectedPluginSlot (slot + 1);
                if (onContextChanged) onContextChanged();
            }
        }
        else if (focus == VimContext::FocusVolume)
        {
            context.setMixerFocus (VimContext::FocusPan);
            if (onContextChanged) onContextChanged();
        }
        else if (focus == VimContext::FocusPan)
        {
            context.setMixerFocus (VimContext::FocusPlugins);
            if (onContextChanged) onContextChanged();
        }
        return true;
    }

    if (keyChar == 'k')
    {
        if (focus == VimContext::FocusPlugins)
        {
            int slot = context.getSelectedPluginSlot();
            if (slot > 0)
            {
                context.setSelectedPluginSlot (slot - 1);
                if (onContextChanged) onContextChanged();
            }
            else
            {
                // At slot 0, exit back to Pan focus
                context.setMixerFocus (VimContext::FocusPan);
                if (onContextChanged) onContextChanged();
            }
        }
        else if (focus == VimContext::FocusPan)
        {
            context.setMixerFocus (VimContext::FocusVolume);
            if (onContextChanged) onContextChanged();
        }
        return true;
    }

    // Return: open plugin view or add plugin
    if (key == dc::KeyCode::Return && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins)
        {
            if (onOpenPluginView)
                onOpenPluginView (trackIdx, slot);
        }
        else
        {
            if (onMixerPluginAdd)
                onMixerPluginAdd (trackIdx);
        }
        return true;
    }

    // x: remove plugin
    if (keyChar == 'x' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins && onMixerPluginRemove)
        {
            onMixerPluginRemove (trackIdx, slot);
            // Clamp slot index after removal
            int newNum = getMixerPluginCount();
            if (context.getSelectedPluginSlot() > newNum)
                context.setSelectedPluginSlot (newNum);
            if (onContextChanged) onContextChanged();
        }
        return true;
    }

    // b: toggle bypass
    if (keyChar == 'b' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins && onMixerPluginBypass)
            onMixerPluginBypass (trackIdx, slot);
        return true;
    }

    // J/K (shift): reorder plugins
    if (keyChar == 'J' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins - 1 && onMixerPluginReorder)
        {
            onMixerPluginReorder (trackIdx, slot, slot + 1);
            context.setSelectedPluginSlot (slot + 1);
            if (onContextChanged) onContextChanged();
        }
        return true;
    }

    if (keyChar == 'K' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();

        if (slot > 0 && onMixerPluginReorder)
        {
            onMixerPluginReorder (trackIdx, slot, slot - 1);
            context.setSelectedPluginSlot (slot - 1);
            if (onContextChanged) onContextChanged();
        }
        return true;
    }

    return false;
}

void MixerAdapter::registerActions (ActionRegistry& registry)
{
    // Mixer-specific actions can be registered here in the future
}

} // namespace dc
