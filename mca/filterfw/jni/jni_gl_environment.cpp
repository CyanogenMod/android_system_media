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

#include <stdint.h>
#include <android/native_window_jni.h>

#include "jni/jni_gl_environment.h"
#include "jni/jni_util.h"

#include "native/core/gl_env.h"

using android::filterfw::GLEnv;
using android::filterfw::WindowHandle;

class NativeWindowHandle : public WindowHandle {
  public:
    NativeWindowHandle(ANativeWindow* window) : window_(window) {
    }

    virtual ~NativeWindowHandle() {
    }

    virtual void Destroy() {
      LOGI("Releasing ANativeWindow!");
      ANativeWindow_release(window_);
    }

    virtual const void* InternalHandle() const {
      return window_;
    }

  private:
    ANativeWindow* window_;
};

jboolean Java_android_filterfw_core_GLEnvironment_nativeAllocate(JNIEnv* env, jobject thiz) {
  return ToJBool(WrapObjectInJava(new GLEnv(), env, thiz, true));
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeDeallocate(JNIEnv* env, jobject thiz) {
  return ToJBool(DeleteNativeObject<GLEnv>(env, thiz));
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeInitWithNewContext(JNIEnv* env,
                                                                           jobject thiz) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->InitWithNewContext()) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeInitWithCurrentContext(JNIEnv* env,
                                                                               jobject thiz) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->InitWithCurrentContext()) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeIsActive(JNIEnv* env, jobject thiz) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->IsActive()) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeActivate(JNIEnv* env, jobject thiz) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->Activate()) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeDeactivate(JNIEnv* env, jobject thiz) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->Deactivate()) : JNI_FALSE;
}

jobject Java_android_filterfw_core_GLEnvironment_nativeActiveEnvironment(JNIEnv* env,
                                                                         jclass clazz) {
  GLEnv* active = GLEnv::ActiveEnv();
  return active ? WrapNewObjectInJava(active, env, false) : NULL;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeSwapBuffers(JNIEnv* env, jobject thiz) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->SwapBuffers()) : JNI_FALSE;
}

jint Java_android_filterfw_core_GLEnvironment_nativeAddSurface(JNIEnv* env,
                                                               jobject thiz,
                                                               jobject surface) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  if (!surface) {
    LOGE("GLEnvironment: Null Surface passed!");
    return -1;
  } else if (gl_env) {
    // Get the ANativeWindow
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
      LOGE("GLEnvironment: Error creating window!");
      return -1;
    }

    NativeWindowHandle* winHandle = new NativeWindowHandle(window);
    int result = gl_env->FindSurfaceIdForWindow(winHandle);
    if (result == -1) {
      // Configure surface
      EGLConfig config;
      EGLint numConfigs = -1;
      EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
      };

      eglChooseConfig(gl_env->display(), configAttribs, &config, 1, &numConfigs);
      if (numConfigs < 1) {
        LOGE("GLEnvironment: No suitable EGL configuration found for surface!");
        return -1;
      }

      // Create the EGL surface
      EGLSurface egl_surface = eglCreateWindowSurface(gl_env->display(),
                                                      config,
                                                      window,
                                                      NULL);

      if (GLEnv::CheckEGLError("eglCreateWindowSurface")) {
        LOGE("GLEnvironment: Error creating window surface!");
        return -1;
      }

      // Add it to GL Env and assign ID
      result = gl_env->AddWindowSurface(egl_surface, winHandle);
    } else {
      delete winHandle;
    }
    return result;
  }
  return -1;
}

jint Java_android_filterfw_core_GLEnvironment_nativeAddSurfaceTexture(JNIEnv* env,
                                                                      jobject thiz,
                                                                      jobject surfaceTexture,
                                                                      jint width,
                                                                      jint height) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  if (!surfaceTexture) {
    LOGE("GLEnvironment: Null SurfaceTexture passed!");
    return -1;
  } else if (gl_env) {
    // Get the ANativeWindow
    ANativeWindow* window = ANativeWindow_fromSurfaceTexture(env, surfaceTexture);
    if (!window) {
      LOGE("GLEnvironment: Error creating window!");
      return -1;
    }

    // Don't care about format (will get overridden by SurfaceTexture
    // anyway), but do care about width and height
    ANativeWindow_setBuffersGeometry(window, width, height, 0);

    NativeWindowHandle* winHandle = new NativeWindowHandle(window);
    int result = gl_env->FindSurfaceIdForWindow(winHandle);
    if (result == -1) {
      // Configure surface
      EGLConfig config;
      EGLint numConfigs = -1;
      EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
      };

      eglChooseConfig(gl_env->display(), configAttribs, &config, 1, &numConfigs);
      if (numConfigs < 1) {
        LOGE("GLEnvironment: No suitable EGL configuration found for surface texture!");
        return -1;
      }

      // Create the EGL surface
      EGLSurface egl_surface = eglCreateWindowSurface(gl_env->display(),
                                                      config,
                                                      window,
                                                      NULL);

      if (GLEnv::CheckEGLError("eglCreateWindowSurface")) {
        LOGE("GLEnvironment: Error creating window surface!");
        return -1;
      }

      // Add it to GL Env and assign ID
      result = gl_env->AddWindowSurface(egl_surface, winHandle);
    } else {
      delete winHandle;
    }
    return result;
  }
  return -1;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeActivateSurfaceId(JNIEnv* env,
                                                                          jobject thiz,
                                                                          jint surfaceId) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->SwitchToSurfaceId(surfaceId)) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLEnvironment_nativeRemoveSurfaceId(JNIEnv* env,
                                                                        jobject thiz,
                                                                        jint surfaceId) {
  GLEnv* gl_env = ConvertFromJava<GLEnv>(env, thiz);
  return gl_env ? ToJBool(gl_env->ReleaseSurfaceId(surfaceId)) : JNI_FALSE;
}
