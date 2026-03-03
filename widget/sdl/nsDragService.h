/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include "nsTArray.h"
struct DragItem;
class  nsDataObjCollection;

namespace mozilla {
class WidgetMouseEvent;
};
/**
 * SDL fake drag service
 */

class nsDragService : public nsBaseDragService
{
public:
  nsDragService();
  virtual ~nsDragService();
  
  // nsBaseDragService
  virtual nsresult InvokeDragSessionImpl(nsIArray* anArrayTransferables,
                                         nsIScriptableRegion* aRegion,
                                         uint32_t aActionType);

  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable * aTransferable, uint32_t anItem);
  NS_IMETHOD GetNumDropItems(uint32_t * aNumItems);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, bool *_retval);
  bool IsDragging() { return mDoingDrag; }
  static bool HandleMouseEvent(nsIWidget *aWidget, const mozilla::WidgetInputEvent &event, int aX, int aY);
  static void HandleIncoming(nsIWidget *aWidget, const char *aData, int aX, int aY, bool aDone, bool aFile, unsigned int aTime);
  private:
  nsTArray<DragItem> mDragItems;
};

#endif // nsDragService_h__
