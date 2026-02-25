#include "TextureCache.h"

namespace dc
{
namespace gfx
{

TextureCache::TextureCache (GpuBackend& b)
    : backend (b)
{
}

void TextureCache::enableCaching (Node& node)
{
    node.useTextureCache = true;
}

void TextureCache::disableCaching (Node& node)
{
    node.useTextureCache = false;
    if (node.hasTextureCache())
    {
        int w = static_cast<int> (node.getBounds().width * backend.getScale());
        int h = static_cast<int> (node.getBounds().height * backend.getScale());
        memoryUsageBytes -= static_cast<size_t> (w * h * 4);
        cachedCount--;
    }
    node.setCachedSurface (nullptr);
}

void TextureCache::invalidate (Node& node)
{
    node.invalidateCache();
}

sk_sp<SkSurface> TextureCache::getOrCreateSurface (Node& node)
{
    if (node.hasTextureCache() && !node.isDirty())
        return node.getCachedSurface();

    int w = static_cast<int> (node.getBounds().width * backend.getScale());
    int h = static_cast<int> (node.getBounds().height * backend.getScale());

    if (w <= 0 || h <= 0)
        return nullptr;

    // Track old surface memory
    if (node.hasTextureCache())
    {
        memoryUsageBytes -= static_cast<size_t> (w * h * 4);
        cachedCount--;
    }

    auto surface = backend.createOffscreenSurface (w, h);
    if (surface)
    {
        node.setCachedSurface (surface);
        memoryUsageBytes += static_cast<size_t> (w * h * 4);
        cachedCount++;
    }

    return surface;
}

void TextureCache::clear()
{
    cachedCount = 0;
    memoryUsageBytes = 0;
}

} // namespace gfx
} // namespace dc
