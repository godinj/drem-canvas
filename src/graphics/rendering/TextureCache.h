#pragma once

#include "graphics/core/Node.h"
#include "GpuBackend.h"

namespace dc
{
namespace gfx
{

class TextureCache
{
public:
    explicit TextureCache (GpuBackend& backend);

    // Enable caching for a node
    void enableCaching (Node& node);

    // Disable and release cache for a node
    void disableCaching (Node& node);

    // Invalidate a specific node's cache (forces re-render next frame)
    void invalidate (Node& node);

    // Get or create cached surface for a node
    sk_sp<SkSurface> getOrCreateSurface (Node& node);

    // Release all cached surfaces
    void clear();

    // Stats
    int getCachedCount() const { return cachedCount; }
    size_t getMemoryUsageBytes() const { return memoryUsageBytes; }

private:
    GpuBackend& backend;
    int cachedCount = 0;
    size_t memoryUsageBytes = 0;
};

} // namespace gfx
} // namespace dc
