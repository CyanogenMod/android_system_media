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

#ifndef ANDROID_FILTERFW_CORE_GL_ENV_H
#define ANDROID_FILTERFW_CORE_GL_ENV_H

#include <string>
#include <utility>
#include <map>

#include "base/logging.h"
#include "base/utilities.h"

#include <GLES2/gl2.h>
#include <EGL/egl.h>

namespace android {
namespace filterfw {

class WindowHandle {
  public:
    virtual ~WindowHandle() {
    }

    virtual void Destroy() = 0;

    virtual bool Equals(const WindowHandle* window) const {
      return InternalHandle() == window->InternalHandle();
    }

    virtual const void* InternalHandle() const = 0;
};

// The GLEnv class provides functionality related to the EGL environment, which
// includes the display, context, and surface. It is possible to either create
// a new environment or base it off the currently active EGL environment. In
// order to do the latter, an EGL environment must be setup already (though not
// necessarily through this class), and have an active display, context, and
// surface.
class GLEnv {
  public:
    // Constructing and Activating /////////////////////////////////////////////
    // Constructs a new GLEnv object. This does not create a GL context.
    GLEnv();

    // Destructor. Tears down and deallocates any GL objects that were created
    // by this instance.
    ~GLEnv();

    // Returns the currently active GL environment, which was activated using
    // Activate().
    static GLEnv* ActiveEnv() {
      return active_env_;
    }

    // Returns a globally unique id of this environment.
    int id() const {
      return id_;
    }

    // Inits a new GL environment, including a new surface and context. You
    // must call Activate() before performing any GL operations.
    bool InitWithNewContext();

    // Inits the GL environment from the current GL environment. Use this when
    // there is already a display, surface and context available (possibly
    // created by the host application). You do not need to call Activate() as
    // this context is active already.
    bool InitWithCurrentContext();

    // Activates the environment, and makes the associated GL context the
    // current context. Creates the environment, if it has not been created
    // already. Returns true if the activation was successful.
    bool Activate();

    // Deactivates the environment. Returns true if the deactivation was
    // successful. You may want to call this when moving a context to another
    // thread. In this case, deactivate the GLEnv in the old thread, and
    // reactivate it in the new thread.
    bool Deactivate();

    // When rendering to a visible surface, call this to swap between the
    // offscreen and onscreen buffers. Returns true if the buffer swap was
    // successful.
    bool SwapBuffers();

    // Working with Surfaces ///////////////////////////////////////////////////

    // Add a surface to the environment. This surface will now be managed (and
    // owned) by the GLEnv instance. Returns the id of the surface.
    int AddSurface(const EGLSurface& surface);

    // Add a window surface to the environment. The window is passed in as
    // an opaque window handle.
    // This surface will now be managed (and owned) by the GLEnv instance.
    // Returns the id of the surface.
    int AddWindowSurface(const EGLSurface& surface, WindowHandle* window_handle);

    // Switch to the surface with the specified id. This will make the surface
    // active, if it is not active already. Specify an ID of 0 if you would like
    // to switch to the default surface. Returns true if successful.
    bool SwitchToSurfaceId(int surface_id);

    // Release the surface with the specified id. This will deallocate the
    // surface. If this is the active surface, the environment will switch to
    // the default surface (0) first. You cannot release the default surface.
    bool ReleaseSurfaceId(int surface_id);

    // Looks for a surface with the associated window handle. Returns -1 if no
    // surface with such a window was found.
    int FindSurfaceIdForWindow(const WindowHandle* window_handle);

    // Obtain the environment's EGL surface.
    const EGLSurface& surface() const {
      return surfaces_.find(surface_id_)->second.first;
    }

    // Working with Contexts ///////////////////////////////////////////////////

    // Add a context to the environment. This context will now be managed (and
    // owned) by the GLEnv instance. Returns the id of the context.
    int AddContext(const EGLContext& context);

    // Switch to the context with the specified id. This will make the context
    // active, if it is not active already. Specify an ID of 0 if you would like
    // to switch to the default context. Returns true if successful.
    bool SwitchToContextId(int context_id);

    // Release the context with the specified id. This will deallocate the
    // context. If this is the active context, the environment will switch to
    // the default context (0) first. You cannot release the default context.
    void ReleaseContextId(int context_id);

    // Obtain the environment's EGL context.
    const EGLContext& context() const {
      return contexts_.find(context_id_)->second;
    }

    // Working with the Display ////////////////////////////////////////////////

    // Obtain the environment's EGL display.
    const EGLDisplay& display() const {
      return display_;
    }

    // Inspecting the environment //////////////////////////////////////////////
    // Returns true if the environment is the active (current) environment.
    bool IsActive() const;

    // Static methods //////////////////////////////////////////////////////////
    // These operate on the currently active environment!

    // Checks if the current environment is in a GL error state. If so, it will
    // output an error message referencing the given operation string. Returns
    // true if there was at least one error.
    static bool CheckGLError(const std::string& operation);

    // Checks if the current environment is in an EGL error state. If so, it
    // will output an error message referencing the given operation string.
    // Returns true if there was at least one error.
    static bool CheckEGLError(const std::string& operation);

    // Get the currently used (shader) program.
    static GLuint GetCurrentProgram();

    // Get the currently active display.
    static EGLDisplay GetCurrentDisplay();

    // Returns the number of components for a given GL type. For instance,
    // returns 4 for vec4, and 16 for mat4.
    static int NumberOfComponents(GLenum type);

  private:
    typedef std::pair<EGLSurface, WindowHandle*> SurfaceWindowPair;

    // Initializes a new GL environment.
    bool Init();

    // Returns true if one of the Inits has been called successfully on this
    // instance.
    bool IsInitialized() const;

    // (Weak) pointer to active environment. Managed by the GLEnv instances.
    static GLEnv* active_env_;

    // The maximum id assigned to a GLEnv instance.
    static int max_id_;

    // This GLEnv instance's id.
    int id_;

    // The EGL display, contexts, and surfaces.
    EGLDisplay display_;
    std::map<int, EGLContext> contexts_;
    std::map<int, SurfaceWindowPair> surfaces_;

    // The currently active context and surface ids.
    int context_id_;
    int surface_id_;

    // These bools keep track of which objects this GLEnv has created (and
    // owns).
    bool created_context_;
    bool created_surface_;
    bool initialized_;

    DISALLOW_COPY_AND_ASSIGN(GLEnv);
};

// Helper class for variables that are GL related, and thus bound to the active
// environment. When setting a value, it will be bound to the currently active
// GL environment. Getting a value will result in whatever value was set in
// the current environment, or NULL if no value was set in that environment.
// Note: We rely on the unique ID of the environment to determine the
// association with any one particular GL environment. There may be a conflict
// of IDs when the current process relaunches (and the static max_id_ resets),
// however, in this case all dependant variables have been deallocated anyway
// and will be reinitialized.
template<typename T>
class GLBoundVariable {
  public:
    // Set the value of this variable in the context of the currently active
    // GL environment.
    void SetValue(T* value);

    // Get the value of this variable in the context of the currently active
    // GL environment. Returns NULL if the variable was not set in this
    // environment.
    T* Value();

  private:
    typedef std::map<int, T*> GLVarMap;
    GLVarMap variables_;
};

// GLBoundVariable implementation //////////////////////////////////////////////
template<typename T>
void GLBoundVariable<T>::SetValue(T* value) {
  if (GLEnv::ActiveEnv())
    variables_[GLEnv::ActiveEnv()->id()] = value;
  else
    LOGE("Attempting to set GL bound variable without an active context!");
}

template<typename T>
T* GLBoundVariable<T>::Value() {
  if (GLEnv::ActiveEnv()) {
    return FindPtrOrNull(variables_, GLEnv::ActiveEnv()->id());
  } else {
    LOGE("Attempting to get GL bound variable without an active context!");
    return NULL;
  }
}

} // namespace filterfw
} // namespace android

#endif  // ANDROID_FILTERFW_CORE_GL_ENV_H
