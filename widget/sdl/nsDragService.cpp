/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDragService.h"
#include "mozilla/RefPtr.h"
#include "nsArrayUtils.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsWidgetsCID.h"

#include "mozilla/MouseEvents.h"

#include <SDL.h>
using namespace mozilla;
using namespace mozilla::gfx;

static nsDragService *gInstance;

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService() { gInstance = this; }

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService() { gInstance = nullptr; }

//-------------------------------------------------------------------------
nsresult nsDragService::InvokeDragSessionImpl(nsIArray *anArrayTransferables,
                                              nsIScriptableRegion *aRegion,
                                              uint32_t aActionType)
{
  // Try and get source URI of the items that are being dragged
  nsIURI *uri = nullptr;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mSourceDocument));
  if (doc) {
    uri = doc->GetDocumentURI();
  }

  uint32_t numItemsToDrag = 0;
  nsresult rv = anArrayTransferables->GetLength(&numItemsToDrag);
  if (!numItemsToDrag) {
    return NS_ERROR_FAILURE;
  }

  LayoutDeviceIntRect dragRect;
  RefPtr<SourceSurface> surface;
  nsPresContext *pc;
  mUserCancelled = false;
  DrawDrag(mSourceNode, aRegion, mScreenPosition, &dragRect, &surface, &pc);
  mDragAction = aActionType;

  StartDragSession();
  OpenDragPopup();
  return NS_OK; // NS_ERROR_FAILURE;
}

/* this handles fake drag
 * While we cannot initiate drag with SDL,
 * we may simulate dragging inside browser windows
 * returns true if performing drag, this should consume original event
 */
// static
bool nsDragService::HandleMouseEvent(nsIWidget *aWidget,
                                     const WidgetMouseEvent &aEvent, int aX,
                                     int aY)
{
  nsDragService *service = gInstance;

  // todo: reference counting? Reset on every mouse event that not dragging
  static nsIWidget *lastWidget;
  static bool changed;

  if (!service || !service->mDoingDrag) {
    lastWidget = nullptr;
    return false;
  }

  if (!changed) {
    // hack: reset sdl mouse grab to allow dragging between windows
    SDL_CaptureMouse(SDL_TRUE);
    SDL_CaptureMouse(SDL_FALSE);
  }

  WidgetDragEvent event(true, eDrop, aWidget);
  nsEventStatus status;
  event.inputSource = service->GetInputSource();
  event.mRefPoint = aEvent.mRefPoint;

  event.mTime = aEvent.mTime;
  if (aEvent.mMessage == eMouseMove) {
    if (lastWidget != aWidget) {
      event.mMessage = eDragExit;

      if (lastWidget) {
        lastWidget->DispatchEvent(&event, status);
      }

      lastWidget = aWidget;
      event.mMessage = eDragEnter;
      aWidget->DispatchEvent(&event, status);
      changed = true;
    }
    service->FireDragEventAtSource(eDrag);
    event.mMessage = eDragOver;
    aWidget->SetCursor(eCursor_grabbing);
  } else if (aEvent.mMessage != eMouseUp)
    return true;

  service->DragMoved(aX, aY);
  aWidget->DispatchEvent(&event, status);
  if (aEvent.mMessage == eMouseMove) {
    return true;
  }

  printf("End drag session\n");
  service->SetDragEndPoint(nsIntPoint(aX, aY));
  service->EndDragSession(true);
  service->mDoingDrag = false;
  lastWidget = nullptr;
  changed = false;
  return true;
}

// todo: can we use transferables for this?
struct DragItem {
  nsString text;
  bool isFile;
};

NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, bool *_retval)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDragService::GetData(nsITransferable *aTransferable, uint32_t anItem)
{
  nsresult res = NS_ERROR_FAILURE;
  if (anItem >= mDragItems.Length()) {
    return res;
  }

  // get flavor list that includes all flavors that can be written (including
  // ones obtained through conversion)
  nsCOMPtr<nsIArray> flavorList;
  res = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(res)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t i;
  uint32_t cnt;
  flavorList->GetLength(&cnt);
  for (i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryElementAt(flavorList, i);
    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

      if (!strcmp(flavorStr, kUnicodeMime) || !strcmp(flavorStr, kURLMime) || !strcmp(flavorStr, kURLDataMime)) {
        nsCOMPtr<nsISupportsString> dataWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &res);
        NS_ENSURE_SUCCESS(res, res);

        res = dataWrapper->SetData(mDragItems[anItem].text);
        NS_ENSURE_SUCCESS(res, res);

        nsCOMPtr<nsISupports> nsisupportsDataWrapper = do_QueryInterface(dataWrapper);
        res = aTransferable->SetTransferData(flavorStr, nsisupportsDataWrapper, mDragItems[anItem].text.Length() * sizeof(PRUnichar));
        break;
      }
      // Hopefully by this point we've found it and can go about our business
      else if (mDragItems[anItem].isFile) {
        unsigned int dataLen = mDragItems[anItem].text.Length() * sizeof(PRUnichar);
        nsCOMPtr<nsISupports> genericDataWrapper;
        if (!strcmp(flavorStr, kFileMime)) {
          // we have a file path in |data|. Create an nsLocalFile object.

          nsCOMPtr<nsIFile> file;
          if (NS_SUCCEEDED(NS_NewLocalFile(mDragItems[anItem].text, false, getter_AddRefs(file)))) {
            genericDataWrapper = do_QueryInterface(file);
            res = aTransferable->SetTransferData(flavorStr, genericDataWrapper,
                                                 dataLen);
            break;
          }
        }
      }
    }
  } // foreach flavor
  return res;
}

NS_IMETHODIMP
nsDragService::GetNumDropItems(uint32_t *aNumItems) {
  *aNumItems = mDragItems.Length();
  return NS_OK;
}

void nsDragService::HandleIncoming(nsIWidget *aWidget, const char *aData,
                                   int aX, int aY, bool aDone, bool aFile,
                                   unsigned int aTime) {
  nsDragService *service = gInstance;
  if (!service) {
    return;
  }
  if (!aDone && !service->mDoingDrag) {
    service->StartDragSession();
  }

  // add new data
  if (aData) {
    service->mDragItems.AppendElement(DragItem{NS_ConvertUTF8toUTF16(aData), aFile});
  } else {
    service->DragMoved(aX, aY);
  }
  if (!aDone) {
    return;
  }
  // end of drop, make event
  WidgetDragEvent event(true, eDragEnter, aWidget);
  event.inputSource = service->GetInputSource();
  event.mRefPoint.x = nscoord(aX);
  event.mRefPoint.y = nscoord(aY);
  event.mTime = aTime;
  nsEventStatus status;
  aWidget->DispatchEvent(&event, status);

  service->SetDragEndPoint(nsIntPoint(aX, aY));
  event.mMessage = eDrop;
  aWidget->DispatchEvent(&event, status);
  service->EndDragSession(true);
  service->mDoingDrag = false;
  service->mDragItems.Clear();
}
