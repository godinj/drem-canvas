#pragma once

#include "include/core/SkFont.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMgr.h"

namespace dc
{
namespace gfx
{

class FontManager
{
public:
    static FontManager& getInstance();

    const SkFont& getDefaultFont() const { return defaultFont; }
    const SkFont& getSmallFont() const { return smallFont; }
    const SkFont& getLargeFont() const { return largeFont; }
    const SkFont& getMonoFont() const { return monoFont; }

    SkFont makeFont (float size) const;
    SkFont makeMonoFont (float size) const;

private:
    FontManager();

    sk_sp<SkFontMgr> fontMgr;
    sk_sp<SkTypeface> defaultTypeface;
    sk_sp<SkTypeface> monoTypeface;

    SkFont defaultFont;
    SkFont smallFont;
    SkFont largeFont;
    SkFont monoFont;
};

} // namespace gfx
} // namespace dc
