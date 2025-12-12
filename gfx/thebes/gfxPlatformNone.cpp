/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatformNone.h"
#include "gfxUserFontSet.h"
#include "nsUnicodeProperties.h"
#include "gfx2DGlue.h"
#include "cairo.h"
#include "gfxFT2FontList.h"
#include "gfxImageSurface.h"
#include "gfxTextRun.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;


gfxPlatformNone::gfxPlatformNone(){}

gfxPlatformNone::~gfxPlatformNone()
{
}

gfxPlatformFontList*
gfxPlatformNone::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFT2FontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

already_AddRefed<gfxASurface>
gfxPlatformNone::CreateOffscreenSurface(const IntSize& aSize,
                                       gfxImageFormat aFormat)
{
    RefPtr<gfxASurface> newSurface = new gfxImageSurface(aSize, aFormat);

    return newSurface.forget();
}

gfxFontGroup *
gfxPlatformNone::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                const gfxFontStyle *aStyle,
                                gfxTextPerfMetrics* aTextPerf,
                                gfxUserFontSet *aUserFontSet,
                                gfxFloat aDevToCssSize)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf,
                            aUserFontSet, aDevToCssSize);
}

bool
gfxPlatformNone::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");
    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_OPENTYPE |
                        gfxUserFontSet::FLAG_FORMAT_WOFF |
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

// Fallback font list from uikit platform
static const char kFontArialUnicodeMS[] = "Arial Unicode MS";
static const char kFontAppleBraille[] = "Apple Braille";
static const char kFontAppleColorEmoji[] = "Apple Color Emoji";
static const char kFontAppleSymbols[] = "Apple Symbols";
static const char kFontDevanagariSangamMN[] = "Devanagari Sangam MN";
static const char kFontEuphemiaUCAS[] = "Euphemia UCAS";
static const char kFontGeneva[] = "Geneva";
static const char kFontGeezaPro[] = "Geeza Pro";
static const char kFontGujaratiSangamMN[] = "Gujarati Sangam MN";
static const char kFontGurmukhiMN[] = "Gurmukhi MN";
static const char kFontHiraginoKakuGothic[] = "Hiragino Kaku Gothic ProN";
static const char kFontHiraginoSansGB[] = "Hiragino Sans GB";
static const char kFontKefa[] = "Kefa";
static const char kFontKhmerMN[] = "Khmer MN";
static const char kFontLaoMN[] = "Lao MN";
static const char kFontLucidaGrande[] = "Lucida Grande";
static const char kFontMenlo[] = "Menlo";
static const char kFontMicrosoftTaiLe[] = "Microsoft Tai Le";
static const char kFontMingLiUExtB[] = "MingLiU-ExtB";
static const char kFontMyanmarMN[] = "Myanmar MN";
static const char kFontPlantagenetCherokee[] = "Plantagenet Cherokee";
static const char kFontSimSunExtB[] = "SimSun-ExtB";
static const char kFontSongtiSC[] = "Songti SC";
static const char kFontSTHeiti[] = "STHeiti";
static const char kFontSTIXGeneral[] = "STIXGeneral";
static const char kFontTamilMN[] = "Tamil MN";

void
gfxPlatformNone::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                       Script aRunScript,
                                       nsTArray<const char*>& aFontList)
{
    EmojiPresentation emoji = GetEmojiPresentation(aCh);
    EmojiPresentation eNext = GetEmojiPresentation(aNextCh);
    if (aNextCh != kVariationSelector15 &&
        emoji != EmojiPresentation::TextOnly &&
        (emoji != EmojiPresentation::TextDefault ||
         eNext == EmojiPresentation::EmojiComponent)) {
        aFontList.AppendElement(kFontAppleColorEmoji);
    }

    aFontList.AppendElement(kFontLucidaGrande);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) {
            aFontList.AppendElement(kFontAppleSymbols);
            aFontList.AppendElement(kFontSTIXGeneral);
            aFontList.AppendElement(kFontGeneva);
        } else if (p == 2) {
            // OSX installations with MS Office may have these fonts
            aFontList.AppendElement(kFontMingLiUExtB);
            aFontList.AppendElement(kFontSimSunExtB);
        }
    } else {
        uint32_t b = (aCh >> 8) & 0xff;

        switch (b) {
        case 0x03:
        case 0x05:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x07:
            aFontList.AppendElement(kFontGeezaPro);
            break;
        case 0x09:
            aFontList.AppendElement(kFontDevanagariSangamMN);
            break;
        case 0x0a:
            aFontList.AppendElement(kFontGurmukhiMN);
            aFontList.AppendElement(kFontGujaratiSangamMN);
            break;
        case 0x0b:
            aFontList.AppendElement(kFontTamilMN);
            break;
        case 0x0e:
            aFontList.AppendElement(kFontLaoMN);
            break;
        case 0x0f:
            aFontList.AppendElement(kFontSongtiSC);
            break;
        case 0x10:
            aFontList.AppendElement(kFontMenlo);
            aFontList.AppendElement(kFontMyanmarMN);
            break;
        case 0x13:  // Cherokee
            aFontList.AppendElement(kFontPlantagenetCherokee);
            aFontList.AppendElement(kFontKefa);
            break;
        case 0x14:  // Unified Canadian Aboriginal Syllabics
        case 0x15:
        case 0x16:
            aFontList.AppendElement(kFontEuphemiaUCAS);
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x18:  // Mongolian, UCAS
            aFontList.AppendElement(kFontSTHeiti);
            aFontList.AppendElement(kFontEuphemiaUCAS);
            break;
        case 0x19:  // Khmer
            aFontList.AppendElement(kFontKhmerMN);
            aFontList.AppendElement(kFontMicrosoftTaiLe);
            break;
        case 0x1d:
        case 0x1e:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x20:  // Symbol ranges
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2e:
            aFontList.AppendElement(kFontHiraginoKakuGothic);
            aFontList.AppendElement(kFontAppleSymbols);
            aFontList.AppendElement(kFontMenlo);
            aFontList.AppendElement(kFontSTIXGeneral);
            aFontList.AppendElement(kFontGeneva);
            aFontList.AppendElement(kFontAppleColorEmoji);
            break;
        case 0x2c:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x2d:
            aFontList.AppendElement(kFontKefa);
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x28:  // Braille
            aFontList.AppendElement(kFontAppleBraille);
            break;
        case 0x31:
            aFontList.AppendElement(kFontHiraginoSansGB);
            break;
        case 0x4d:
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        case 0xa0:  // Yi
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
            aFontList.AppendElement(kFontSTHeiti);
            break;
        case 0xa6:
        case 0xa7:
            aFontList.AppendElement(kFontGeneva);
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        case 0xab:
            aFontList.AppendElement(kFontKefa);
            break;
        case 0xfc:
        case 0xff:
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        default:
            break;
        }
    }

    // Arial Unicode MS has lots of glyphs for obscure, use it as a last resort
    aFontList.AppendElement(kFontArialUnicodeMS);
}
