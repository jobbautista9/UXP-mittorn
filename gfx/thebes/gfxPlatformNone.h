/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_NONE_H
#define GFX_PLATFORM_NONE_H

#include "gfxPlatform.h"

class gfxPlatformNone : public gfxPlatform {
public:
    gfxPlatformNone();
    virtual ~gfxPlatformNone();

    static gfxPlatformNone *GetPlatform() {
        return (gfxPlatformNone*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& aSize,
                             gfxImageFormat aFormat) override;

    gfxFontGroup*
    CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                    const gfxFontStyle *aStyle,
                    gfxTextPerfMetrics* aTextPerf,
                    gfxUserFontSet *aUserFontSet,
                    gfxFloat aDevToCssSize) override;

    virtual gfxPlatformFontList* CreatePlatformFontList() override;

    bool IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags) override;

    virtual void GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                        Script aRunScript,
                                        nsTArray<const char*>& aFontList) override;


    virtual bool CanRenderContentToDataSurface() const override {
      return true;
    }

    virtual bool SupportsApzWheelInput() const override {
      return false;
    }

    bool RespectsFontStyleSmoothing() const override {
      return false;
    }

    bool RequiresAcceleratedGLContextForCompositorOGL() const override {
      return false;
    }
};

#endif /* GFX_PLATFORM_NONE_H */
