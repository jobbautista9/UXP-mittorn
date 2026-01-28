/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLookAndFeel.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "nsPrintfCString.h"
#include "nsStyleConsts.h"

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel() {}

nsLookAndFeel::~nsLookAndFeel() {}

// this matches wine and gtk2 redmond themes
// and very near to gtk2's default raleigh
#define BASE_ACTIVE_COLOR NS_RGB(0xd4, 0xd0, 0xc8)
#define BASE_NORMAL_COLOR NS_RGB(0xff, 0xff, 0xff)
#define BASE_SELECTED_COLOR NS_RGB(0x00, 0x24, 0x68)
#define BG_ACTIVE_COLOR NS_RGB(0xff, 0xff, 0xff)
#define BG_INSENSITIVE_COLOR NS_RGB(0xd4, 0xd0, 0xc8)
#define BG_NORMAL_COLOR NS_RGB(0xd4, 0xd0, 0xc8)
#define BG_PRELIGHT_COLOR NS_RGB(0xee, 0xee, 0xee)
#define BG_SELECTED_COLOR NS_RGB(0x00, 0x24, 0x68)
#define DARK_NORMAL_COLOR NS_RGB(0x8D, 0x8A, 0x85)
#define FG_INSENSITIVE_COLOR NS_RGB(0x80, 0x80, 0x80)
#define FG_NORMAL_COLOR NS_RGB(0x00, 0x00, 0x00)
#define FG_PRELIGHT_COLOR NS_RGB(0x44, 0x44, 0x44)
#define FG_SELECTED_COLOR NS_RGB(0xFF, 0xFF, 0xFF)
#define LIGHT_NORMAL_COLOR NS_RGB(0xFF, 0xFF, 0xFF)
#define TEXT_ACTIVE_COLOR NS_RGB(0x30, 0x30, 0x30)
#define TEXT_NORMAL_COLOR NS_RGB(0x00, 0x00, 0x00)
#define TEXT_SELECTED_COLOR NS_RGB(0xFF, 0xFF, 0xFF)
#define BLACK_COLOR NS_RGB(0x00, 0x00, 0x00)
#define DARK_GRAY_COLOR NS_RGB(0x40, 0x40, 0x40)
#define GRAY_COLOR NS_RGB(0x80, 0x80, 0x80)
#define LIGHT_GRAY_COLOR NS_RGB(0xa0, 0xa0, 0xa0)
#define RED_COLOR NS_RGB(0xff, 0x00, 0x00)
#define BLUE_COLOR NS_RGB(0x00, 0x00, 0xff)
#define TOOLTIP_COLOR NS_RGB(0xff, 0xff, 0xdc)

nsresult nsLookAndFeel::NativeGetColor(const ColorID aID, nscolor &aColor) {

  nsresult rv = NS_OK;
  switch (aID) {
    case eColorID_WindowBackground:
      aColor = BASE_NORMAL_COLOR;
      break;
    case eColorID_WindowForeground:
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID_WidgetBackground:
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_WidgetForeground:
      aColor = FG_NORMAL_COLOR;
      break;
    case eColorID_WidgetSelectBackground:
      aColor = BG_SELECTED_COLOR;
      break;
    case eColorID_WidgetSelectForeground:
      aColor = FG_SELECTED_COLOR;
      break;
    case eColorID_Widget3DHighlight:
      aColor = LIGHT_GRAY_COLOR;
      break;
    case eColorID_Widget3DShadow:
      aColor = DARK_GRAY_COLOR;
      break;
    case eColorID_TextBackground:
      // not used?
      aColor = BASE_NORMAL_COLOR;
      break;
    case eColorID_TextForeground:
      // not used?
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
      // still used
      aColor = BASE_SELECTED_COLOR;
      break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
      // still used
      aColor = TEXT_SELECTED_COLOR;
      break;
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;
    case eColorID_SpellCheckerUnderline:
      aColor = RED_COLOR;
      break;

      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColorID_activeborder:
      // active window border
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_activecaption:
      // active window caption background
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_appworkspace:
      // MDI background color
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_background:
      // desktop background
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_captiontext:
      // text in active window caption, size box, and scrollbar arrow box (!)
      aColor = FG_NORMAL_COLOR;
      break;
    case eColorID_graytext:
      // disabled text in windows, menus, etc.
      aColor = FG_INSENSITIVE_COLOR;
      break;
    case eColorID_highlight:
      // background of selected item
      aColor = BASE_SELECTED_COLOR;
      break;
    case eColorID_highlighttext:
      // text of selected item
      aColor = TEXT_SELECTED_COLOR;
      break;
    case eColorID_inactiveborder:
      // inactive window border
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_inactivecaption:
      // inactive window caption
      aColor = BG_INSENSITIVE_COLOR;
      break;
    case eColorID_inactivecaptiontext:
      // text in inactive window caption
      aColor = FG_INSENSITIVE_COLOR;
      break;
    case eColorID_infobackground:
      // tooltip background color
      aColor = TOOLTIP_COLOR;
      break;
    case eColorID_infotext:
      // tooltip text color
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID_menu:
      // menu background
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID_menutext:
      // menu text
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID_scrollbar:
      // scrollbar gray area
      aColor = BASE_ACTIVE_COLOR;
      break;

    case eColorID_threedface:
    case eColorID_buttonface:
      // 3-D face color
      aColor = BG_NORMAL_COLOR;
      break;

    case eColorID_buttontext:
      // text on push buttons
      aColor = TEXT_NORMAL_COLOR;
      break;

    case eColorID_buttonhighlight:
      // 3-D highlighted edge color
    case eColorID_threedhighlight:
      // 3-D highlighted outer edge color
      aColor = LIGHT_NORMAL_COLOR;
      break;

    case eColorID_threedlightshadow:
      // 3-D highlighted inner edge color
      aColor = BG_NORMAL_COLOR;
      break;

    case eColorID_buttonshadow:
      // 3-D shadow edge color
    case eColorID_threedshadow:
      // 3-D shadow inner edge color
      aColor = DARK_NORMAL_COLOR;
      break;

    case eColorID_threeddarkshadow:
      // 3-D shadow outer edge color
      aColor = BLACK_COLOR;
      break;

    case eColorID_window:
    case eColorID_windowframe:
      aColor = BG_NORMAL_COLOR;
      break;

    case eColorID_windowtext:
      aColor = FG_NORMAL_COLOR;
      break;

    case eColorID__moz_eventreerow:
    case eColorID__moz_field:
      aColor = BASE_NORMAL_COLOR;
      break;
    case eColorID__moz_fieldtext:
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID__moz_dialog:
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID__moz_dialogtext:
      aColor = FG_NORMAL_COLOR;
      break;
    case eColorID__moz_dragtargetzone:
      aColor = BG_SELECTED_COLOR;
      break;
    case eColorID__moz_buttondefault:
      // default button border color
      aColor = BLACK_COLOR;
      break;
    case eColorID__moz_buttonhoverface:
      aColor = BG_PRELIGHT_COLOR;
      break;
    case eColorID__moz_buttonhovertext:
      aColor = FG_PRELIGHT_COLOR;
      break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
      aColor = BASE_ACTIVE_COLOR;
      break;
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
      aColor = TEXT_ACTIVE_COLOR;
      break;
    case eColorID__moz_menuhover:
      aColor = BG_SELECTED_COLOR;
      break;
    case eColorID__moz_menuhovertext:
      aColor = FG_SELECTED_COLOR;
      break;
    case eColorID__moz_oddtreerow:
      aColor = BASE_ACTIVE_COLOR;
      break;
    case eColorID__moz_nativehyperlinktext:
      aColor = BLUE_COLOR;
      break;
    case eColorID__moz_comboboxtext:
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID__moz_combobox:
      aColor = BG_NORMAL_COLOR;
      break;
    case eColorID__moz_menubartext:
      aColor = TEXT_NORMAL_COLOR;
      break;
    case eColorID__moz_menubarhovertext:
      aColor = FG_NORMAL_COLOR;
      break;
    default: {
      /* default color is BLACK */
      aColor = NS_RGB(0, 0, 0);
      nsCString msg = nsPrintfCString("Someone asked nsILookAndFeel for a color I don't know about: %d", aID);
      NS_WARNING(msg.get());
      rv = NS_ERROR_FAILURE;
    } break;
  }

  return rv;
}

NS_IMETHODIMP
nsLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult) {
  nsresult rv = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  rv = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
      aResult = 500;
      break;

    case eIntID_CaretWidth:
      aResult = 1;
      break;

    case eIntID_ShowCaretDuringSelection:
      aResult = 0;
      break;

    case eIntID_SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by EventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;

    case eIntID_SubmenuDelay:
      aResult = 200;
      break;

    case eIntID_TooltipDelay:
      aResult = 500;
      break;

    case eIntID_MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;
    case eIntID_SwipeAnimationEnabled:
      aResult = 0;
      break;
    case eIntID_UseOverlayScrollbars:
      aResult = 0;
      break;
    case eIntID_AllowOverlayScrollbarsOverlap:
      aResult = 0;
      break;
    case eIntID_ScrollbarDisplayOnMouseMove:
      aResult = 1;
      break;

    case eIntID_ScrollArrowStyle:
      aResult = eScrollArrowStyle_Single;
      break;

    case eIntID_ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;

    case eIntID_TouchEnabled:
      aResult = 0;
      break;

    case eIntID_ColorPickerAvailable:
      aResult = 0;
      break;

    case eIntID_WindowsDefaultTheme:
    case eIntID_WindowsThemeIdentifier:
    case eIntID_OperatingSystemVersionIdentifier:
      aResult = 0;
      rv = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_WindowsClassic:
      // we do not have any visual styles
      aResult = 1;
      break;
    case eIntID_SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
      break;

    case eIntID_ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;

    case eIntID_ContextMenuOffsetVertical:
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 2;
      break;

    default:
      aResult = 0;
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult) {
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) {
    return res;
  }
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

bool nsLookAndFeel::GetFontImpl(FontID aID, nsString &aFontName,
                           gfxFontStyle &aFontStyle,
                           float aDevPixPerCSSPixel) {
  // prepare some font name
  aFontStyle.style = NS_FONT_STYLE_NORMAL;
  aFontStyle.weight = NS_FONT_WEIGHT_NORMAL;
  aFontStyle.stretch = NS_FONT_STRETCH_NORMAL;
  aFontStyle.size = 11 * aDevPixPerCSSPixel;
  aFontStyle.systemFont = true;

  aFontName.AssignLiteral("liberation sans");
  return true;
}
