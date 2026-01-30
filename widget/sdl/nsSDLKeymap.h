/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSSDLKEYMAP_H
#define NSSDLKEYMAP_H

#include "mozilla/BasicEvents.h"
#include "mozilla/TextEvents.h"

#include "WidgetUtils.h"
#include <SDL.h>
using namespace mozilla;
using namespace mozilla::dom;

struct nsKeyConverter
{
  int vkCode; // Platform independent key code
  uint keysym; // SDL key code

  // key name indexes, if not set, use string
  CodeNameIndex codeNameIndex;
  KeyNameIndex keyNameIndex;

  // char fallbacks
  char latinChar;
  char shiftedChar;
};

// key lookup table
static struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,        SDLK_CANCEL,       CODE_NAME_INDEX_UNKNOWN, KEY_NAME_INDEX_Cancel },
  { NS_VK_BACK,          SDLK_BACKSPACE,    CODE_NAME_INDEX_Backspace, KEY_NAME_INDEX_Backspace },
  { NS_VK_TAB,           SDLK_TAB,          CODE_NAME_INDEX_Tab, KEY_NAME_INDEX_Tab },
  { NS_VK_CLEAR,         SDLK_CLEAR,        CODE_NAME_INDEX_UNKNOWN, KEY_NAME_INDEX_Clear },
  { NS_VK_RETURN,        SDLK_RETURN,       CODE_NAME_INDEX_Enter, KEY_NAME_INDEX_Enter },
  { NS_VK_RETURN,        SDLK_KP_ENTER,     CODE_NAME_INDEX_NumpadEnter, KEY_NAME_INDEX_Enter },
  { NS_VK_SHIFT,         SDLK_RSHIFT,       CODE_NAME_INDEX_ShiftRight, KEY_NAME_INDEX_Shift },
  { NS_VK_SHIFT,         SDLK_LSHIFT,       CODE_NAME_INDEX_ShiftLeft, KEY_NAME_INDEX_Shift },
  { NS_VK_CONTROL,       SDLK_LCTRL,        CODE_NAME_INDEX_ControlLeft, KEY_NAME_INDEX_Control},
  { NS_VK_CONTROL,       SDLK_RCTRL,        CODE_NAME_INDEX_ControlRight, KEY_NAME_INDEX_Control},
  { NS_VK_ALT,           SDLK_LALT,         CODE_NAME_INDEX_AltLeft, KEY_NAME_INDEX_Alt },
  { NS_VK_ALT,           SDLK_RALT,         CODE_NAME_INDEX_AltRight, KEY_NAME_INDEX_Alt },
  { NS_VK_PAUSE,         SDLK_PAUSE,        CODE_NAME_INDEX_Pause, KEY_NAME_INDEX_Pause },
  { NS_VK_CAPS_LOCK,     SDLK_CAPSLOCK,     CODE_NAME_INDEX_CapsLock, KEY_NAME_INDEX_CapsLock },
  { NS_VK_ESCAPE,        SDLK_ESCAPE,       CODE_NAME_INDEX_Escape, KEY_NAME_INDEX_Escape },
  { NS_VK_SPACE,         SDLK_SPACE,        CODE_NAME_INDEX_Space},
  { NS_VK_PAGE_UP,       SDLK_PAGEUP,       CODE_NAME_INDEX_PageUp, KEY_NAME_INDEX_PageUp },
  { NS_VK_PAGE_DOWN,     SDLK_PAGEDOWN,     CODE_NAME_INDEX_PageDown, KEY_NAME_INDEX_PageDown },
  { NS_VK_END,           SDLK_END,          CODE_NAME_INDEX_End, KEY_NAME_INDEX_End },
  { NS_VK_HOME,          SDLK_HOME,         CODE_NAME_INDEX_Home, KEY_NAME_INDEX_Home },
  { NS_VK_LEFT,          SDLK_LEFT,         CODE_NAME_INDEX_ArrowLeft, KEY_NAME_INDEX_ArrowLeft },
  { NS_VK_UP,            SDLK_UP,           CODE_NAME_INDEX_ArrowUp, KEY_NAME_INDEX_ArrowUp },
  { NS_VK_RIGHT,         SDLK_RIGHT,        CODE_NAME_INDEX_ArrowRight, KEY_NAME_INDEX_ArrowRight },
  { NS_VK_DOWN,          SDLK_DOWN,         CODE_NAME_INDEX_ArrowDown, KEY_NAME_INDEX_ArrowDown },
  { NS_VK_PRINTSCREEN,   SDLK_PRINTSCREEN,  CODE_NAME_INDEX_PrintScreen, KEY_NAME_INDEX_PrintScreen },
  { NS_VK_INSERT,        SDLK_INSERT,       CODE_NAME_INDEX_Insert, KEY_NAME_INDEX_Insert },
  { NS_VK_DELETE,        SDLK_DELETE,       CODE_NAME_INDEX_Delete, KEY_NAME_INDEX_Delete },
  { NS_VK_HELP,          SDLK_HELP,         CODE_NAME_INDEX_Help, KEY_NAME_INDEX_Help },

  { NS_VK_0,             SDLK_0,            CODE_NAME_INDEX_Digit0, KEY_NAME_INDEX_USE_STRING, 0, ')' },
  { NS_VK_1,             SDLK_1,            CODE_NAME_INDEX_Digit1, KEY_NAME_INDEX_USE_STRING, 0, '!' },
  { NS_VK_2,             SDLK_2,            CODE_NAME_INDEX_Digit2, KEY_NAME_INDEX_USE_STRING, 0, '@' },
  { NS_VK_3,             SDLK_3,            CODE_NAME_INDEX_Digit3, KEY_NAME_INDEX_USE_STRING, 0, '#' },
  { NS_VK_4,             SDLK_4,            CODE_NAME_INDEX_Digit4, KEY_NAME_INDEX_USE_STRING, 0, '$' },
  { NS_VK_5,             SDLK_5,            CODE_NAME_INDEX_Digit5, KEY_NAME_INDEX_USE_STRING, 0, '%' },
  { NS_VK_6,             SDLK_6,            CODE_NAME_INDEX_Digit6, KEY_NAME_INDEX_USE_STRING, 0, '^' },
  { NS_VK_7,             SDLK_7,            CODE_NAME_INDEX_Digit7, KEY_NAME_INDEX_USE_STRING, 0, '&' },
  { NS_VK_8,             SDLK_8,            CODE_NAME_INDEX_Digit8, KEY_NAME_INDEX_USE_STRING, 0, '*' },
  { NS_VK_9,             SDLK_9,            CODE_NAME_INDEX_Digit9, KEY_NAME_INDEX_USE_STRING, 0, '(' },

  { NS_VK_SEMICOLON,     SDLK_SEMICOLON,    CODE_NAME_INDEX_Semicolon, KEY_NAME_INDEX_USE_STRING, 0, ':' },
  { NS_VK_EQUALS,        SDLK_EQUALS,       CODE_NAME_INDEX_Equal, KEY_NAME_INDEX_USE_STRING, 0, '+' },

  { NS_VK_A,             SDLK_a,            CODE_NAME_INDEX_KeyA },
  { NS_VK_B,             SDLK_b,            CODE_NAME_INDEX_KeyB },
  { NS_VK_C,             SDLK_c,            CODE_NAME_INDEX_KeyC },
  { NS_VK_D,             SDLK_d,            CODE_NAME_INDEX_KeyD },
  { NS_VK_E,             SDLK_e,            CODE_NAME_INDEX_KeyE },
  { NS_VK_F,             SDLK_f,            CODE_NAME_INDEX_KeyF },
  { NS_VK_G,             SDLK_g,            CODE_NAME_INDEX_KeyG },
  { NS_VK_H,             SDLK_h,            CODE_NAME_INDEX_KeyH },
  { NS_VK_I,             SDLK_i,            CODE_NAME_INDEX_KeyI},
  { NS_VK_J,             SDLK_j,            CODE_NAME_INDEX_KeyJ },
  { NS_VK_K,             SDLK_k,            CODE_NAME_INDEX_KeyK },
  { NS_VK_L,             SDLK_l,            CODE_NAME_INDEX_KeyL },
  { NS_VK_M,             SDLK_m,            CODE_NAME_INDEX_KeyM },
  { NS_VK_N,             SDLK_n,            CODE_NAME_INDEX_KeyN },
  { NS_VK_O,             SDLK_o,            CODE_NAME_INDEX_KeyO },
  { NS_VK_P,             SDLK_p,            CODE_NAME_INDEX_KeyP },
  { NS_VK_Q,             SDLK_q,            CODE_NAME_INDEX_KeyQ },
  { NS_VK_R,             SDLK_r,            CODE_NAME_INDEX_KeyR },
  { NS_VK_S,             SDLK_s,            CODE_NAME_INDEX_KeyS },
  { NS_VK_T,             SDLK_t,            CODE_NAME_INDEX_KeyT },
  { NS_VK_U,             SDLK_u,            CODE_NAME_INDEX_KeyU },
  { NS_VK_V,             SDLK_v,            CODE_NAME_INDEX_KeyV },
  { NS_VK_W,             SDLK_w,            CODE_NAME_INDEX_KeyW },
  { NS_VK_X,             SDLK_x,            CODE_NAME_INDEX_KeyX },
  { NS_VK_Y,             SDLK_y,            CODE_NAME_INDEX_KeyY },
  { NS_VK_Z,             SDLK_z,            CODE_NAME_INDEX_KeyZ },

  { NS_VK_NUMPAD0,       SDLK_KP_0,         CODE_NAME_INDEX_Numpad0, KEY_NAME_INDEX_USE_STRING, '0' },
  { NS_VK_NUMPAD1,       SDLK_KP_1,         CODE_NAME_INDEX_Numpad1, KEY_NAME_INDEX_USE_STRING, '1' },
  { NS_VK_NUMPAD2,       SDLK_KP_2,         CODE_NAME_INDEX_Numpad2, KEY_NAME_INDEX_USE_STRING, '2' },
  { NS_VK_NUMPAD3,       SDLK_KP_3,         CODE_NAME_INDEX_Numpad3, KEY_NAME_INDEX_USE_STRING, '3' },
  { NS_VK_NUMPAD4,       SDLK_KP_4,         CODE_NAME_INDEX_Numpad4, KEY_NAME_INDEX_USE_STRING, '4' },
  { NS_VK_NUMPAD5,       SDLK_KP_5,         CODE_NAME_INDEX_Numpad5, KEY_NAME_INDEX_USE_STRING, '5' },
  { NS_VK_NUMPAD6,       SDLK_KP_6,         CODE_NAME_INDEX_Numpad6, KEY_NAME_INDEX_USE_STRING, '6' },
  { NS_VK_NUMPAD7,       SDLK_KP_7,         CODE_NAME_INDEX_Numpad7, KEY_NAME_INDEX_USE_STRING, '7' },
  { NS_VK_NUMPAD8,       SDLK_KP_8,         CODE_NAME_INDEX_Numpad8, KEY_NAME_INDEX_USE_STRING, '8' },
  { NS_VK_NUMPAD9,       SDLK_KP_9,         CODE_NAME_INDEX_Numpad9, KEY_NAME_INDEX_USE_STRING, '9' },
  { NS_VK_MULTIPLY,      SDLK_ASTERISK },
  { NS_VK_MULTIPLY,      SDLK_KP_MULTIPLY,  CODE_NAME_INDEX_NumpadMultiply, KEY_NAME_INDEX_USE_STRING, '*' },
  { NS_VK_ADD,           SDLK_PLUS },
  { NS_VK_ADD,           SDLK_KP_PLUS,      CODE_NAME_INDEX_NumpadAdd, KEY_NAME_INDEX_USE_STRING, '+' },
  { NS_VK_SEPARATOR,     SDLK_SEPARATOR },
  { NS_VK_SUBTRACT,      SDLK_MINUS,        CODE_NAME_INDEX_Minus, KEY_NAME_INDEX_USE_STRING, '-', '_' },
  { NS_VK_SUBTRACT,      SDLK_KP_MINUS,     CODE_NAME_INDEX_NumpadSubtract, KEY_NAME_INDEX_USE_STRING, '-' },
  { NS_VK_DECIMAL,       SDLK_KP_PERIOD,    CODE_NAME_INDEX_NumpadDecimal, KEY_NAME_INDEX_USE_STRING, '.' },
  { NS_VK_DIVIDE,        SDLK_SLASH,        CODE_NAME_INDEX_Slash },
  { NS_VK_DIVIDE,        SDLK_KP_DIVIDE,    CODE_NAME_INDEX_NumpadDivide, KEY_NAME_INDEX_USE_STRING, '/' },
  { NS_VK_F1,            SDLK_F1,           CODE_NAME_INDEX_F1, KEY_NAME_INDEX_F1 },
  { NS_VK_F2,            SDLK_F2,           CODE_NAME_INDEX_F2, KEY_NAME_INDEX_F2 },
  { NS_VK_F3,            SDLK_F3,           CODE_NAME_INDEX_F3, KEY_NAME_INDEX_F3 },
  { NS_VK_F4,            SDLK_F4,           CODE_NAME_INDEX_F4, KEY_NAME_INDEX_F4 },
  { NS_VK_F5,            SDLK_F5,           CODE_NAME_INDEX_F5, KEY_NAME_INDEX_F5 },
  { NS_VK_F6,            SDLK_F6,           CODE_NAME_INDEX_F6, KEY_NAME_INDEX_F6 },
  { NS_VK_F7,            SDLK_F7,           CODE_NAME_INDEX_F7, KEY_NAME_INDEX_F7 },
  { NS_VK_F8,            SDLK_F8,           CODE_NAME_INDEX_F8, KEY_NAME_INDEX_F8 },
  { NS_VK_F9,            SDLK_F9,           CODE_NAME_INDEX_F9, KEY_NAME_INDEX_F9 },
  { NS_VK_F10,           SDLK_F10,          CODE_NAME_INDEX_F10, KEY_NAME_INDEX_F10 },
  { NS_VK_F11,           SDLK_F11,          CODE_NAME_INDEX_F11, KEY_NAME_INDEX_F11 },
  { NS_VK_F12,           SDLK_F12,          CODE_NAME_INDEX_F12, KEY_NAME_INDEX_F12 },
  { NS_VK_F13,           SDLK_F13,          CODE_NAME_INDEX_F13, KEY_NAME_INDEX_F13 },
  { NS_VK_F14,           SDLK_F14,          CODE_NAME_INDEX_F14, KEY_NAME_INDEX_F14 },
  { NS_VK_F15,           SDLK_F15,          CODE_NAME_INDEX_F15, KEY_NAME_INDEX_F15 },
  { NS_VK_F16,           SDLK_F16,          CODE_NAME_INDEX_F16, KEY_NAME_INDEX_F16 },
  { NS_VK_F17,           SDLK_F17,          CODE_NAME_INDEX_F17, KEY_NAME_INDEX_F17 },
  { NS_VK_F18,           SDLK_F18,          CODE_NAME_INDEX_F18, KEY_NAME_INDEX_F18 },
  { NS_VK_F19,           SDLK_F19,          CODE_NAME_INDEX_F19, KEY_NAME_INDEX_F19 },
  { NS_VK_F20,           SDLK_F20,          CODE_NAME_INDEX_F20, KEY_NAME_INDEX_F20 },
  { NS_VK_F21,           SDLK_F21,          CODE_NAME_INDEX_F21, KEY_NAME_INDEX_F21 },
  { NS_VK_F22,           SDLK_F22,          CODE_NAME_INDEX_F22, KEY_NAME_INDEX_F22 },
  { NS_VK_F23,           SDLK_F23,          CODE_NAME_INDEX_F23, KEY_NAME_INDEX_F23 },
  { NS_VK_F24,           SDLK_F24,          CODE_NAME_INDEX_F24, KEY_NAME_INDEX_F24 },

  { NS_VK_NUM_LOCK,      SDLK_NUMLOCKCLEAR, CODE_NAME_INDEX_NumLock, KEY_NAME_INDEX_NumLock },
  { NS_VK_SCROLL_LOCK,   SDLK_SCROLLLOCK,   CODE_NAME_INDEX_ScrollLock, KEY_NAME_INDEX_ScrollLock },

  { NS_VK_COMMA,         SDLK_COMMA,        CODE_NAME_INDEX_Comma, KEY_NAME_INDEX_USE_STRING, 0, '<' },
  { NS_VK_PERIOD,        SDLK_PERIOD,       CODE_NAME_INDEX_Period, KEY_NAME_INDEX_USE_STRING, 0, '>' },
  { NS_VK_SLASH,         SDLK_SLASH,        CODE_NAME_INDEX_Slash, KEY_NAME_INDEX_USE_STRING, 0, '?' },
  { NS_VK_BACK_SLASH,    SDLK_BACKSLASH,    CODE_NAME_INDEX_Backslash, KEY_NAME_INDEX_USE_STRING, 0, '|' },
  { NS_VK_BACK_QUOTE,    SDLK_BACKQUOTE,    CODE_NAME_INDEX_Backquote },
  { NS_VK_OPEN_BRACKET,  SDLK_LEFTBRACKET,  CODE_NAME_INDEX_BracketLeft, KEY_NAME_INDEX_USE_STRING, 0, '{' },
  { NS_VK_CLOSE_BRACKET, SDLK_RIGHTBRACKET, CODE_NAME_INDEX_BracketRight, KEY_NAME_INDEX_USE_STRING, 0, '}' },
  { NS_VK_QUOTE,         SDLK_QUOTE,        CODE_NAME_INDEX_Quote, KEY_NAME_INDEX_USE_STRING, 0, '"' },
  { NS_VK_CONTEXT_MENU,  SDLK_APPLICATION,  CODE_NAME_INDEX_ContextMenu, KEY_NAME_INDEX_ContextMenu },
  { NS_VK_WIN,           SDLK_RGUI,         CODE_NAME_INDEX_OSRight, KEY_NAME_INDEX_OS },
  { NS_VK_WIN,           SDLK_LGUI,         CODE_NAME_INDEX_OSLeft, KEY_NAME_INDEX_OS },
  { 0 }
};

constexpr static int nsInvalidKeycodeIndex = ArrayLength(nsKeycodes) - 1;

static int MapKey(uint aKeysym)
{
  for (int i = 0; i < ArrayLength(nsKeycodes) - 1; i++) {
    if (nsKeycodes[i].keysym == aKeysym) {
      return i;
    }
  }
  return nsInvalidKeycodeIndex;
}

static Modifiers ConvertModifiers(Uint16 mod)
{
  Modifiers res = 0;
  if(mod & KMOD_SHIFT)
    res |= MODIFIER_SHIFT;
  if(mod & KMOD_CTRL)
    res |= MODIFIER_CONTROL;
  if(mod & KMOD_GUI)
    res |= MODIFIER_OS;
  if(mod & KMOD_CAPS)
    res |= MODIFIER_CAPSLOCK;
  if(mod & KMOD_NUM)
    res |= MODIFIER_NUMLOCK;
  if(mod & KMOD_SCROLL)
    res |= MODIFIER_SCROLLLOCK;
  if(mod & KMOD_ALT)
    res |= MODIFIER_ALT;
  return res;
}

static inline bool
IsContextMenuKey(const WidgetKeyboardEvent& aKeyEvent)
{
  return (
      (aKeyEvent.mKeyCode == NS_VK_F10 && aKeyEvent.IsShift() &&
       !aKeyEvent.IsControl() && !aKeyEvent.IsMeta() && !aKeyEvent.IsAlt()) ||
      (aKeyEvent.mKeyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.IsShift() &&
       !aKeyEvent.IsControl() && !aKeyEvent.IsMeta() && !aKeyEvent.IsAlt()));
}

// initialize event with ascii char
static void HandleCharSyms(WidgetKeyboardEvent &aEvent, const nsKeyConverter &aKey, uint aSym)
{
  // if keycode is missing, tryst from char
  if (!aEvent.mKeyCode) {
    aEvent.mKeyCode =  widget::WidgetUtils::ComputeKeyCodeFromChar(aSym);
  }
  aEvent.SetCharCode(aSym);

  aEvent.mIsChar = true;
  if (!aKey.keyNameIndex){
    aEvent.mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
    aEvent.mKeyValue.Append(aSym);
  }
  if (!aKey.codeNameIndex) {
    aEvent.mCodeNameIndex = CODE_NAME_INDEX_USE_STRING;
    aEvent.mCodeValue.Append(aSym);
  }
}

// handle keys for disabled numlock
static void HandleNumpad(WidgetKeyboardEvent &aEvent, uint aSym)
{
  switch(aSym) {
    case SDLK_KP_0:
      aEvent.mKeyCode = NS_VK_INSERT;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_Insert;
      break;
    case SDLK_KP_1:
      aEvent.mKeyCode = NS_VK_END;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_End;
      break;
    case SDLK_KP_2:
      aEvent.mKeyCode = NS_VK_DOWN;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_ArrowDown;
      break;
    case SDLK_KP_3:
      aEvent.mKeyCode = NS_VK_PAGE_DOWN;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_PageDown;
      break;
    case SDLK_KP_4:
      aEvent.mKeyCode = NS_VK_LEFT;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_ArrowLeft;
      break;
    case SDLK_KP_5:
      aEvent.mKeyCode = NS_VK_UNKNOWN;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_Unidentified;
      break;
    case SDLK_KP_6:
      aEvent.mKeyCode = NS_VK_RIGHT;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_ArrowRight;
      break;
    case SDLK_KP_7:
      aEvent.mKeyCode = NS_VK_HOME;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_Home;
      break;
    case SDLK_KP_8:
      aEvent.mKeyCode = NS_VK_UP;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_ArrowUp;
      break;
    case SDLK_KP_9:
      aEvent.mKeyCode = NS_VK_PAGE_UP;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_PageUp;
      break;
    case SDLK_KP_PERIOD:
      aEvent.mKeyCode = NS_VK_DELETE;
      aEvent.mKeyNameIndex = KEY_NAME_INDEX_Delete;
      break;
  }
}

// Process any unhandled latin keys for KeyUp, KeyDown events
static void SetLatinKeys(WidgetKeyboardEvent &aEvent, const nsKeyConverter &aKey, bool aShift, bool aCaps)
{
  if (aEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING) {
    return;
  }

  if (aEvent.mKeyValue.Length() != 0 && !aShift) {
    return;
  }

  if(aShift && aKey.shiftedChar) {
    aEvent.mKeyValue = aKey.shiftedChar;
  } else if(aKey.latinChar) {
    aEvent.mKeyValue = aKey.latinChar;
  } else {
    uint ch, sh;
    widget::WidgetUtils::GetLatinCharCodeForKeyCode(aEvent.mKeyCode, aCaps , &ch, &sh);
    if(aShift && sh)
      aEvent.mKeyValue = sh;
    else if(ch)
      aEvent.mKeyValue = ch;
  }
}

static void SetKeyLocation(WidgetKeyboardEvent &aEvent, uint aSym, uint aScancode)
{
  if (aScancode >= SDL_SCANCODE_KP_DIVIDE && aScancode <= SDL_SCANCODE_KP_PERIOD) {
    aEvent.mLocation = nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;
  }
  switch(aSym)
  {
    case SDLK_LALT:
    case SDLK_LSHIFT:
    case SDLK_LCTRL:
    case SDLK_LGUI:
      aEvent.mLocation =  nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;
      break;
    case SDLK_RALT:
    case SDLK_RSHIFT:
    case SDLK_RCTRL:
    case SDLK_RGUI:
      aEvent.mLocation =  nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;
      break;
  }
}

#endif // NSSDLKEYMAP_H
