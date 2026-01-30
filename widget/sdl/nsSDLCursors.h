/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSSDLCURSORS_H
#define NSSDLCURSORS_H
#include <SDL.h>

#include "gfxImageSurface.h"

#include "../gtk/nsGtkCursors.h"
#include "imgIContainer.h"
#include "nsIWidget.h"


// reuse gtk cursors
static SDL_Cursor *LoadSDLCursor(uint8_t aNewType)
{
#if 1
  SDL_Surface *surf = SDL_CreateRGBSurface(0, 32, 32, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
  SDL_LockSurface(surf);
  unsigned char *data = (unsigned char*)surf->pixels;

  // Read data from GtkCursors and compose RGBA surface from 1bit bitmap and mask
  // GtkCursors bits and mask are 32x32 monochrome bitmaps (1 bit for each pixel)
  // so it's 128 byte array (4 bytes for are one bitmap row and there are 32 rows here).
  const unsigned char *bits = GtkCursors[aNewType].bits;
  const unsigned char *mask_bits = GtkCursors[aNewType].mask_bits;

  for (int i = 0; i < 128; i++) {
    char bit = *bits++;
    char mask = *mask_bits++;
    for (int j = 0; j < 8; j++) {
      unsigned char pix = ~(((bit >> j) & 0x01) * 0xff);
      *data++ = pix;
      *data++ = pix;
      *data++ = pix;
      *data++ = (((mask >> j) & 0x01) * 0xff);
    }
  }
  SDL_UnlockSurface(surf);
  return SDL_CreateColorCursor(surf, GtkCursors[aNewType].hot_x, GtkCursors[aNewType].hot_y);

#else
  // this causes buggy x-flipped cursor, probably SDL/X11 bug?
  return SDL_CreateCursor(GtkCursors[aNewType].bits,
                          GtkCursors[aNewType].mask_bits,
                          32, 32,
                          GtkCursors[aNewType].hot_x,
                          GtkCursors[aNewType].hot_y);
#endif
}


static SDL_Cursor *GetSDLCursor(nsCursor aCursor)
{
  static SDL_Cursor *sCachedCursors[eCursorCount];
  SDL_Cursor *&newCursor = sCachedCursors[aCursor];

  if(newCursor) {
    return newCursor;
  }
  switch (aCursor) {
    case eCursor_select:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
      break;

    case eCursor_wait:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
      break;

    case eCursor_hyperlink:
    {
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
      break;
    }

    case eCursor_standard:
    case eCursor_context_menu: // XXX See bug 258960.
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
      break;

    case eCursor_n_resize:
    case eCursor_s_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
      break;

    case eCursor_w_resize:
    case eCursor_e_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
      break;

    case eCursor_nw_resize:
    case eCursor_se_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
      break;

    case eCursor_ne_resize:
    case eCursor_sw_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
      break;

    case eCursor_crosshair:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
      break;

    case eCursor_move:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
      break;

    case eCursor_help: // missing
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
      break;

    case eCursor_copy: // CSS3
      newCursor = LoadSDLCursor(MOZ_CURSOR_COPY);
      break;

    case eCursor_alias:
      newCursor = LoadSDLCursor(MOZ_CURSOR_ALIAS);
      break;

    case eCursor_cell: // missing
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
      break;

    case eCursor_grab:
      newCursor = LoadSDLCursor(MOZ_CURSOR_HAND_GRAB);
      break;

    case eCursor_grabbing:
      newCursor = LoadSDLCursor(MOZ_CURSOR_HAND_GRABBING);
      break;

    case eCursor_spinning:
      newCursor = LoadSDLCursor(MOZ_CURSOR_SPINNING);
      break;

    case eCursor_zoom_in:
      newCursor = LoadSDLCursor(MOZ_CURSOR_ZOOM_IN);
      break;

    case eCursor_zoom_out:
      newCursor = LoadSDLCursor(MOZ_CURSOR_ZOOM_OUT);
      break;

    case eCursor_not_allowed:
    case eCursor_no_drop:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
      break;

    case eCursor_col_resize: // missing
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
      break;

    case eCursor_row_resize: // missing
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
      break;

    case eCursor_vertical_text:
      newCursor = LoadSDLCursor(MOZ_CURSOR_VERTICAL_TEXT);
      break;

    case eCursor_all_scroll:
      // XXX not 100% appropriate perhaps
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
      break;

    case eCursor_nesw_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
      break;

    case eCursor_nwse_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
      break;

    case eCursor_ns_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
      break;

    case eCursor_ew_resize:
      newCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
      break;

    case eCursor_none:
      newCursor = LoadSDLCursor(MOZ_CURSOR_NONE);
      break;

    default:
      NS_ERROR("Invalid cursor type");
      break;
  }
  return newCursor;
}


inline unsigned char
unpremultiply (unsigned char color,
              unsigned char alpha)
{
  if (alpha == 0) {
    return 0;
  }
  // plus alpha/2 to round instead of truncate
  return (color * 255 + alpha / 2) / alpha;
}

SDL_Surface *ImageToSurface(imgIContainer* aImage)
{
  using namespace mozilla::gfx;
  RefPtr<SourceSurface> surface =
    aImage->GetFrame(imgIContainer::FRAME_CURRENT,
                     imgIContainer::FLAG_SYNC_DECODE);

  // If the last call failed, it was probably because our call stack originates
  // in an imgINotificationObserver event, meaning that we're not allowed request
  // a sync decode. Presumably the originating event is something sensible like
  // OnStopFrame(), so we can just retry the call without a sync decode.
  if (!surface) {
    surface = aImage->GetFrame(imgIContainer::FRAME_CURRENT,
                               imgIContainer::FLAG_NONE);
  }

  NS_ENSURE_TRUE(surface, nullptr);
  RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map))
    return nullptr;

  uint8_t* srcData = map.mData;
  int32_t srcStride = map.mStride;

  SurfaceFormat format = dataSurface->GetFormat();
  int w = surface->GetSize().width;
  int h = surface->GetSize().height;
  SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
  SDL_LockSurface(surf);
  int destStride = surf->pitch;
  unsigned char *destPixels = (unsigned char*)surf->pixels;
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      unsigned char* destPixel = destPixels + row * destStride + 4 * col;

      uint32_t* srcPixel = reinterpret_cast<uint32_t*>((srcData + row * srcStride + 4 * col));

      if (format == SurfaceFormat::B8G8R8A8) {
        const uint8_t a = (*srcPixel >> 24) & 0xFF;
        const uint8_t r = unpremultiply((*srcPixel >> 16) & 0xFF, a);
        const uint8_t g = unpremultiply((*srcPixel >>  8) & 0xFF, a);
        const uint8_t b = unpremultiply((*srcPixel >>  0) & 0xFF, a);

        *destPixel++ = r;
        *destPixel++ = g;
        *destPixel++ = b;
        *destPixel++ = a;
      } else {
        MOZ_ASSERT(format == SurfaceFormat::B8G8R8X8);

        const uint8_t r = (*srcPixel >> 16) & 0xFF;
        const uint8_t g = (*srcPixel >>  8) & 0xFF;
        const uint8_t b = (*srcPixel >>  0) & 0xFF;

        *destPixel++ = r;
        *destPixel++ = g;
        *destPixel++ = b;
        *destPixel++ = 0xFF; // A
      }
    }
  }
  SDL_UnlockSurface(surf);

  dataSurface->Unmap();
  return surf;
}
#endif // NSSDLCURSORS_H
