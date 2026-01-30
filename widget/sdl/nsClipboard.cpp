/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include <SDL.h>

using namespace mozilla;
using mozilla::dom::ContentChild;

NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard);


nsClipboard::nsClipboard()
{
}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *anOwner, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard && aWhichClipboard != kSelectionClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsISupports> tmp;
  uint32_t len;
  nsresult rv  = aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(tmp), &len);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
  // No support for non-text data
  NS_ENSURE_TRUE(supportsString, NS_ERROR_NOT_IMPLEMENTED);

  
  nsXPIDLString buffer;
  supportsString->ToString(getter_Copies(buffer));
                          
//  supportsString->GetData(buffer);
  len = buffer.Length();
  char *u8 = ToNewUTF8String(buffer, &len);
  if (aWhichClipboard == kGlobalClipboard) {
    SDL_SetClipboardText(u8);
  } else {
    SDL_SetPrimarySelectionText(u8);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard && aWhichClipboard != kSelectionClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsAutoString buffer;
  const char *text = aWhichClipboard == kGlobalClipboard ? SDL_GetClipboardText() : SDL_GetPrimarySelectionText();
  NS_ConvertUTF8toUTF16 ucs2string(text);
  SDL_free((void*)text);

  buffer.Assign(ucs2string);

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataWrapper->SetData(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // If our data flavor has already been added, this will fail. But we don't care
  aTransferable->AddDataFlavor(kUnicodeMime);

  nsCOMPtr<nsISupports> nsisupportsDataWrapper = do_QueryInterface(dataWrapper);
  rv = aTransferable->SetTransferData(kUnicodeMime, nsisupportsDataWrapper, buffer.Length() * sizeof(PRUnichar));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard && aWhichClipboard != kSelectionClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (aWhichClipboard == kGlobalClipboard) {
    SDL_SetClipboardText("");
  } else {
    SDL_SetPrimarySelectionText("");
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char **aFlavorList,
                                    uint32_t aLength, int32_t aWhichClipboard,
                                    bool *aHasText)
{
  *aHasText = false;
  if (aWhichClipboard != kGlobalClipboard && aWhichClipboard != kSelectionClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  *aHasText = aWhichClipboard == kGlobalClipboard?SDL_HasClipboardText() : SDL_HasPrimarySelectionText();
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *aIsSupported)
{
  *aIsSupported = true;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool *aIsSupported)
{
  *aIsSupported = false;
  return NS_OK;
}


