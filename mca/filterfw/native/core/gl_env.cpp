/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/logging.h"
#include "core/gl_env.h"
#include "system/window.h"

#include <map>
#include <string>

namespace android {
namespace filterfw {

GLEnv* GLEnv::active_env_ = NULL;
int GLEnv::max_id_ = 0;

GLEnv::GLEnv()
  : id_(max_id_),
    display_(EGL_NO_DISPLAY),
    context_id_(0),
    surface_id_(0),
    created_context_(false),
    created_surface_(false),
    initialized_(false) {
  ++max_id_;
}

GLEnv::~GLEnv() {
  // We are no longer active if torn down
  if (active_env_ == this)
    active_env_ = NULL;

  // Destroy surfaces
  for (std::map<int, SurfaceWindowPair>::iterator it = surfaces_.begin();
       it != surfaces_.end();
       ++it) {
    if (it->first != 0 || created_surface_) {
      eglDestroySurface(display(), it->second.first);
      if (it->second.second) {
        it->second.second->Destroy();
        delete it->second.second;
      }
    }
  }

  // Destroy contexts
  for (std::map<int, EGLContext>::iterator it = contexts_.begin();
       it != contexts_.end();
       ++it) {
    if (it->first != 0 || created_context_)
      eglDestroyContext(display(), it->second);
  }

  // Destroy display
  if (initialized_)
    eglTerminate(display());

  // Log error if this did not work
  if (CheckEGLError("TearDown!"))
    LOGE("GLEnv: Error tearing down GL Environment!");
}

bool GLEnv::IsInitialized() const {
  return (contexts_.size() > 0 &&
          surfaces_.size() > 0 &&
          display_ != EGL_NO_DISPLAY);
}

bool GLEnv::Deactivate() {
  active_env_ = NULL;
  eglMakeCurrent(display(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  return !CheckEGLError("eglMakeCurrent");
}

bool GLEnv::Activate() {
  if (active_env_ != this ||
      display()   != eglGetCurrentDisplay() ||
      context()   != eglGetCurrentContext() ||
      surface()   != eglGetCurrentSurface(EGL_DRAW)) {
    // Make sure we are initialized
    if (context() == EGL_NO_CONTEXT || surface() == EGL_NO_SURFACE)
      return false;

    // Make our context current
    eglMakeCurrent(display(), surface(), surface(), context());

    // We are no longer active now
    active_env_ = NULL;

    // Set active env to new environment if there was no error
    if (!CheckEGLError("eglMakeCurrent")) {
      active_env_ = this;
      return true;
    }
    return false;
  }
  return true;
}

bool GLEnv::SwapBuffers() {
  const bool result = eglSwapBuffers(display(), surface()) == EGL_TRUE;
  return !CheckEGLError("eglSwapBuffers") && result;
}

bool GLEnv::InitWithCurrentContext() {
  if (IsInitialized())
    return true;

  display_     = eglGetCurrentDisplay();
  contexts_[0] = eglGetCurrentContext();
  surfaces_[0] = SurfaceWindowPair(eglGetCurrentSurface(EGL_DRAW), NULL);

  if ((context() != EGL_NO_CONTEXT) &&
      (display() != EGL_NO_DISPLAY) &&
      (surface() != EGL_NO_SURFACE)) {
    active_env_ = this;
    return true;
  }
  return false;
}

bool GLEnv::InitWithNewContext() {
  if (IsInitialized()) {
    LOGE("GLEnv: Attempting to reinitialize environment!");
    return false;
  }

  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (CheckEGLError("eglGetDisplay")) return false;

  EGLint majorVersion;
  EGLint minorVersion;
  eglInitialize(display(), &majorVersion, &minorVersion);
  if (CheckEGLError("eglInitialize")) return false;
  initialized_ = true;

  // Configure context/surface
  EGLConfig config;
  EGLint numConfigs = -1;

  // TODO(renn): Do we need the window bit here?
  EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  eglChooseConfig(display(), configAttribs, &config, 1, &numConfigs);
  if (numConfigs < 1) {
    LOGE("GLEnv::Init: No suitable EGL configuration found!");
    return false;
  }

  // Create dummy surface
  EGLint pb_attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
  surfaces_[0] = SurfaceWindowPair(eglCreatePbufferSurface(display(), config, pb_attribs), NULL);
  if (CheckEGLError("eglCreatePbufferSurface")) return false;

  // Create context
  EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  contexts_[0] = eglCreateContext(display(),
                                  config,
                                  EGL_NO_CONTEXT,
                                  context_attribs);
  if (CheckEGLError("eglCreateContext")) return false;

  created_context_ = created_surface_ = true;

  return true;
}

bool GLEnv::IsActive() const {
  return context() == eglGetCurrentContext()
    &&   display() == eglGetCurrentDisplay()
    &&   surface() == eglGetCurrentSurface(EGL_DRAW);
}

int GLEnv::AddWindowSurface(const EGLSurface& surface, WindowHandle* window_handle) {
  const int id = surfaces_.size();
  surfaces_[id] = SurfaceWindowPair(surface, window_handle);
  return id;
}

int GLEnv::AddSurface(const EGLSurface& surface) {
  return AddWindowSurface(surface, NULL);
}

bool GLEnv::SwitchToSurfaceId(int surface_id) {
  const SurfaceWindowPair* surface = FindOrNull(surfaces_, surface_id);
  if (surface) {
    if (surface_id_ != surface_id) {
      surface_id_ = surface_id;
      return Activate();
    }
    return true;
  }
  return false;
}

bool GLEnv::ReleaseSurfaceId(int surface_id) {
  if (surface_id > 0) {
    const SurfaceWindowPair* surface = FindOrNull(surfaces_, surface_id);
    if (surface) {
      surfaces_.erase(surface_id);
      if (surface_id_ == surface_id && ActiveEnv() == this)
        SwitchToSurfaceId(0);
      eglDestroySurface(display(), surface->first);
      if (surface->second) {
        surface->second->Destroy();
        delete surface->second;
      }
      return true;
    }
  }
  return false;
}

bool GLEnv::SetSurfaceTimestamp(int64_t timestamp) {
  if (surface_id_ > 0) {
    std::map<int, SurfaceWindowPair>::iterator surfacePair = surfaces_.find(surface_id_);
    if (surfacePair != surfaces_.end()) {
      SurfaceWindowPair &surface = surfacePair->second;
      ANativeWindow *window = static_cast<ANativeWindow*>(surface.second->InternalHandle());
      native_window_set_buffers_timestamp(window, timestamp);
      return true;
    }
  }
  return false;
}

int GLEnv::FindSurfaceIdForWindow(const WindowHandle* window_handle) {
  for (std::map<int, SurfaceWindowPair>::iterator it = surfaces_.begin();
       it != surfaces_.end();
       ++it) {
    const WindowHandle* my_handle = it->second.second;
    if (my_handle && my_handle->Equals(window_handle)) {
      return it->first;
    }
  }
  return -1;
}


int GLEnv::AddContext(const EGLContext& context) {
  const int id = contexts_.size();
  contexts_[id] = context;
  return id;
}

bool GLEnv::SwitchToContextId(int context_id) {
  const EGLContext* context = FindOrNull(contexts_, context_id);
  if (context) {
    if (context_id_ != context_id) {
      context_id_ = context_id;
      return Activate();
    }
    return true;
  }
  return false;
}

void GLEnv::ReleaseContextId(int context_id) {
  if (context_id > 0) {
    const EGLContext* context = FindOrNull(contexts_, context_id);
    if (context) {
      contexts_.erase(context_id);
      if (context_id_ == context_id && ActiveEnv() == this)
        SwitchToContextId(0);
      eglDestroyContext(display(), *context);
    }
  }
}

bool GLEnv::CheckGLError(const std::string& op) {
  bool err = false;
  for (GLint error = glGetError(); error; error = glGetError()) {
    LOGE("GL Error: Operation '%s' caused GL error (0x%x)\n",
         op.c_str(),
         error);
    err = true;
  }
  return err;
}

bool GLEnv::CheckEGLError(const std::string& op) {
  bool err = false;
  for (EGLint error = eglGetError();
       error != EGL_SUCCESS;
       error = eglGetError()) {
    LOGE("EGL Error: Operation '%s' caused EGL error (0x%x)\n",
         op.c_str(),
         error);
    err = true;
  }
  return err;
}

GLuint GLEnv::GetCurrentProgram() {
  GLint result;
  glGetIntegerv(GL_CURRENT_PROGRAM, &result);
  LOG_ASSERT(result >= 0);
  return static_cast<GLuint>(result);
}

EGLDisplay GLEnv::GetCurrentDisplay() {
  return eglGetCurrentDisplay();
}

int GLEnv::NumberOfComponents(GLenum type) {
  switch (type) {
    case GL_BOOL:
    case GL_FLOAT:
    case GL_INT:
      return 1;
    case GL_BOOL_VEC2:
    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
      return 2;
    case GL_INT_VEC3:
    case GL_FLOAT_VEC3:
    case GL_BOOL_VEC3:
      return 3;
    case GL_BOOL_VEC4:
    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
    case GL_FLOAT_MAT2:
      return 4;
    case GL_FLOAT_MAT3:
      return 9;
    case GL_FLOAT_MAT4:
      return 16;
    default:
      return 0;
  }
}

} // namespace filterfw
} // namespace android
