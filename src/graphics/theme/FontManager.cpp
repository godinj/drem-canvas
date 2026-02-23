#include "FontManager.h"
#include "include/ports/SkFontMgr_mac_ct.h"

namespace dc
{
namespace gfx
{

FontManager& FontManager::getInstance()
{
    static FontManager instance;
    return instance;
}

FontManager::FontManager()
{
    fontMgr = SkFontMgr_New_CoreText (nullptr);

    // Try system fonts: SF Pro (macOS) → Helvetica Neue → default
    defaultTypeface = fontMgr->matchFamilyStyle ("SF Pro Text", SkFontStyle::Normal());
    if (!defaultTypeface)
        defaultTypeface = fontMgr->matchFamilyStyle ("Helvetica Neue", SkFontStyle::Normal());
    if (!defaultTypeface)
        defaultTypeface = fontMgr->legacyMakeTypeface ("", SkFontStyle::Normal());

    // Monospace: SF Mono → Menlo → default
    monoTypeface = fontMgr->matchFamilyStyle ("SF Mono", SkFontStyle::Normal());
    if (!monoTypeface)
        monoTypeface = fontMgr->matchFamilyStyle ("Menlo", SkFontStyle::Normal());
    if (!monoTypeface)
        monoTypeface = fontMgr->legacyMakeTypeface ("", SkFontStyle::Normal());

    defaultFont = SkFont (defaultTypeface, 13.0f);
    defaultFont.setSubpixel (true);
    defaultFont.setEdging (SkFont::Edging::kSubpixelAntiAlias);

    smallFont = SkFont (defaultTypeface, 11.0f);
    smallFont.setSubpixel (true);
    smallFont.setEdging (SkFont::Edging::kSubpixelAntiAlias);

    largeFont = SkFont (defaultTypeface, 16.0f);
    largeFont.setSubpixel (true);
    largeFont.setEdging (SkFont::Edging::kSubpixelAntiAlias);

    monoFont = SkFont (monoTypeface, 12.0f);
    monoFont.setSubpixel (true);
    monoFont.setEdging (SkFont::Edging::kSubpixelAntiAlias);
}

SkFont FontManager::makeFont (float size) const
{
    SkFont font (defaultTypeface, size);
    font.setSubpixel (true);
    font.setEdging (SkFont::Edging::kSubpixelAntiAlias);
    return font;
}

SkFont FontManager::makeMonoFont (float size) const
{
    SkFont font (monoTypeface, size);
    font.setSubpixel (true);
    font.setEdging (SkFont::Edging::kSubpixelAntiAlias);
    return font;
}

} // namespace gfx
} // namespace dc
