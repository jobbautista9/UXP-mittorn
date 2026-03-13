/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "GLContextProvider.h"
#include "GLLibraryLoader.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "prenv.h"
#include <SDL.h>
#include "GLLibraryEGL.h"

namespace mozilla {
// nsAppShell.cpp
SDL_Window* CreateOffscreenSDLWindow();

void DestroyOffscreenSDLWindow(SDL_Window *aWin);


namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

class GLContextSDL : public GLContext {
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextSDL, override)
  // From Window: (possibly for offscreen!)
  GLContextSDL(CreateContextFlags aFlags, const SurfaceCaps &aCaps,
               bool aRobustness, bool aOffscreen, bool aGles, 
               bool aDoubleBuffer, SDL_GLContext aContext, SDL_Window *aWindow)
      : GLContext(aFlags, aCaps, nullptr, aOffscreen)
      , mContext(aContext)
      , mWnd(aWindow)
      , mIsDoubleBuffered(aDoubleBuffer)
      , mHasRobustness(aRobustness)
      , mOffscreen(aOffscreen) {
    if (aGles) {
      SetProfileVersion(ContextProfile::OpenGLES, 200);
    } else {
      SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
    }
  }

  ~GLContextSDL() {
    MarkDestroyed();
    if (mOffscreen && mWnd) {
      DestroyOffscreenSDLWindow(mWnd);
    }
  }

  virtual GLContextType GetContextType() const override {
    return GLContextType::SDL;
  }

  static GLContextSDL *Cast(GLContext *gl) {
    MOZ_ASSERT(gl->GetContextType() == GLContextType::SDL);
    return static_cast<GLContextSDL *>(gl);
  }

  bool Init() override {
    if (!mWnd || !mContext) {
      return false;
    }

    SetupLookupFunction();
    return InitWithPrefix("gl", true);
  }

  virtual bool MakeCurrentImpl(bool aForce) override {
    if (IsDestroyed()) {
      return true;
    }

    bool succeeded = true;
    if (aForce || SDL_GL_GetCurrentContext() != mContext) {

      // sdl have no way to detach context
      if (!mWnd) {
        return true;
      }
      succeeded = SDL_GL_MakeCurrent(mWnd, mContext) == 0;
      NS_ASSERTION(succeeded, SDL_GetError());
    }

    return succeeded;
  }

  virtual bool IsCurrent() override {
    return SDL_GL_GetCurrentContext() == mContext;
  }

  void SetIsDoubleBuffered(bool aIsDB) {
    mIsDoubleBuffered = aIsDB;
  }

  virtual bool IsDoubleBuffered() const override {
    return mIsDoubleBuffered;
  }

  virtual bool SupportsRobustness() const override {
    return mHasRobustness;
  }

  virtual bool SwapBuffers() override {
    SDL_GL_SwapWindow(mWnd);
    return true;
  }

  virtual void GetWSIInfo(nsCString *const out) const override {
    out->AppendLiteral("SDL GL Context");
  }

  virtual bool SetupLookupFunction() override {
    mLookupFunc = (PlatformLookupFunction)SDL_GL_GetProcAddress;
    return true;
  }

  SDL_GLContext Context() {
    return mContext;
  }

protected:
  friend class GLContextProviderSDL;

  SDL_GLContext mContext;
  SDL_Window *mWnd;
  bool mIsDoubleBuffered, mHasRobustness, mOffscreen;
};

static GLContextSDL *GetGlobalContextSDL() {
  return static_cast<GLContextSDL *>(GLContextProviderSDL::GetGlobalContext());
}

already_AddRefed<GLContext>
GLContextProviderSDL::CreateWrappingExisting(void *, void *) {
  return nullptr;
}

already_AddRefed<GLContext> GLContextProviderSDL::CreateForCompositorWidget(
    CompositorWidget *aCompositorWidget, bool aForceAccelerated) {

  return CreateForWindow(aCompositorWidget->RealWidget(), aForceAccelerated);
}

// SDL attributes stored in video driver private data, shared across threads
static SDL_mutex *sContextMutex;
static bool sOnlyGLES = false;

static SDL_GLContext CreateContext(bool aRobustness, bool aGles, bool aDoubleBuffer, bool aForceAccelerated, SDL_Window *aWin) {
  if (!sContextMutex) {
    sContextMutex = SDL_CreateMutex();
    sOnlyGLES = PR_GetEnv("MOZ_SDL_FORCE_GLES");
  }
  if (!aGles && sOnlyGLES) {
    return nullptr;
  }
  SDL_LockMutex(sContextMutex);
  SDL_GL_ResetAttributes();
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, (SDL_bool)aDoubleBuffer);

  if (aForceAccelerated) {
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, SDL_TRUE);
  }

  if (aRobustness) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_RESET_NOTIFICATION, SDL_TRUE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG);
  }

  if (aGles) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  } else {
    // set some defaults to prevent SDL defaulting to GLES when gl is disabled
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
    // todo: what is minimum supported GL version?
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  }

  SDL_GLContext ret = SDL_GL_CreateContext(aWin);
  int profile = -1, minor = -1, major = -1;
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);

  // ignore gles without gles profile
  if (ret && !aGles && profile == SDL_GL_CONTEXT_PROFILE_ES) {
    NS_WARNING("SDL selected wrong GL profile");
    SDL_GL_DeleteContext(ret);
    ret = nullptr;
  }
  // ignore gles1
  if (ret && aGles && major < 2) {
    NS_WARNING("SDL selected unsupported GLES version");
    SDL_GL_DeleteContext(ret);
    ret = nullptr;
  }
  if (ret) {
    // never trust context that is created by SDL, it may successfuly create broken context
    typedef GLubyte* (GLAPIENTRY * PFNGLGETSTRINGPROC) (GLenum);
    PFNGLGETSTRINGPROC fGetString = (PFNGLGETSTRINGPROC)SDL_GL_GetProcAddress("glGetString");

    if (!fGetString || !fGetString(LOCAL_GL_SHADING_LANGUAGE_VERSION)) {
      NS_WARNING("SDL created invalid context");
      SDL_GL_DeleteContext(ret);
      ret = nullptr;
    }
  }

  SDL_UnlockMutex(sContextMutex);
  return ret;
}

already_AddRefed<GLContext>
GLContextProviderSDL::CreateForWindow(nsIWidget *aWidget,
                                      bool aForceAccelerated)
{
  bool hasRobustness = true;
  bool gles = false;

  sEGLLibrary.SetNativeDisplay((EGLNativeDisplayType)aWidget->GetNativeData(NS_NATIVE_DISPLAY));
  sEGLLibrary.SetEGLPlatform((EGLenum)(uintptr_t)aWidget->GetNativeData(NS_NATIVE_EGL_PLATFORM));
  sEGLLibrary.SetEGLPlatformAttribs(
   (const EGLint*)aWidget->GetNativeData(NS_NATIVE_EGL_PLATFORM_DISPLAY_ATTRIB_LIST));

  SDL_Window *win = (SDL_Window *)aWidget->GetNativeData(NS_NATIVE_WIDGET);
  SDL_GLContext context = CreateContext(hasRobustness, gles, true, aForceAccelerated, win);
  if (!context) {
    hasRobustness = false;
    context = CreateContext(hasRobustness, gles, true, aForceAccelerated, win);
  }
  if (!context) {
    gles = true;
    context = CreateContext(hasRobustness, gles, true, aForceAccelerated, win);
  }

  NS_ASSERTION(context, SDL_GetError());

  if (!context) {
    return nullptr;
  }

  SurfaceCaps caps = SurfaceCaps::ForRGBA();
  RefPtr<GLContextSDL> glContext = new GLContextSDL(
      CreateContextFlags::NONE, caps, hasRobustness, false, gles, true, context, win);
  if (!glContext->Init()) {
    return nullptr;
  }

  glContext->SetIsDoubleBuffered(true);
  SDL_GL_SetSwapInterval(0);

  return glContext.forget();
}

static already_AddRefed<GLContextSDL> CreateWindowOffscreenContext()
{
  bool hasRobustness = true;
  bool doubleBuffer = false;
  bool gles = false;
  SDL_Window *win = CreateOffscreenSDLWindow();

  if (!win) {
    return nullptr;
  }
  SDL_GLContext context = CreateContext(hasRobustness, gles, doubleBuffer, false, win);
  if (!context) {
    hasRobustness = false;
    context = CreateContext(hasRobustness, gles, doubleBuffer, false, win);
  }

  if (!context) {
    doubleBuffer = true;
    context = CreateContext(hasRobustness, gles, doubleBuffer, false, win);
  }

  if (!context) {
    gles = true;
    doubleBuffer = false;
    context = CreateContext(hasRobustness, gles, doubleBuffer, false, win);
  }

  if (!context) {
    doubleBuffer = true;
    context = CreateContext(hasRobustness, gles, doubleBuffer, false, win);
  }

  NS_ASSERTION(context, SDL_GetError());
  if (!context) {
    return nullptr;
  }

  SurfaceCaps caps = SurfaceCaps::ForRGBA();
  RefPtr<GLContextSDL> glContext = new GLContextSDL(
      CreateContextFlags::NONE, caps, hasRobustness, true, gles, doubleBuffer, context, win);
  return glContext.forget();
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderSDL::CreateHeadless(CreateContextFlags flags, nsACString *const out_failureId) {
  // SDL2 does not support pbuffers/headless, create a window context and use a FBO.

  RefPtr<GLContextSDL> glContext = CreateWindowOffscreenContext();

  if (!glContext || !glContext->Init()) {
    return nullptr;
  }

  RefPtr<GLContext> retGL = glContext.get();
  return retGL.forget();
}

/*static*/ already_AddRefed<GLContext> GLContextProviderSDL::CreateOffscreen(
    const IntSize &size, const SurfaceCaps &minCaps, CreateContextFlags flags,
    nsACString *const out_failureId) {
  RefPtr<GLContext> gl = CreateHeadless(flags, out_failureId);
  if (!gl) {
    return nullptr;
  }

  if (!gl->InitOffscreen(size, minCaps)) {
    *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_SDL_GL_INIT");
    return nullptr;
  }

  return gl.forget();
}

/*static*/ GLContext *GLContextProviderSDL::GetGlobalContext() {
  return nullptr;
}

/*static*/ void GLContextProviderSDL::Shutdown() {
  if (sContextMutex) {
    SDL_DestroyMutex(sContextMutex);
    sContextMutex = nullptr;
  }
}

} /* namespace gl */
} /* namespace mozilla */
