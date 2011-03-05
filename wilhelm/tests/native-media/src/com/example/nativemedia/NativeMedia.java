/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.example.nativemedia;

import android.app.Activity;
import android.media.MediaPlayer.OnPreparedListener;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View.OnClickListener;
import android.view.View;
import android.widget.Button;
import java.io.IOException;
import android.graphics.SurfaceTexture;

public class NativeMedia extends Activity {
    public static final String TAG = "NativeMedia";

    static boolean isPlayingStreaming = false;
    MediaPlayer mp;
    SurfaceView javaSurfaceView;
    SurfaceView openmaxalSurfaceView;
    SurfaceHolder javaHolder;
    SurfaceHolder openmaxalHolder;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.main);

        // initialize native media system

        createEngine();

        javaSurfaceView = (SurfaceView) findViewById(R.id.java_surface);
        openmaxalSurfaceView = (SurfaceView) findViewById(R.id.openmaxal_surface);

        javaHolder = javaSurfaceView.getHolder();
        openmaxalHolder = openmaxalSurfaceView.getHolder();

        openmaxalHolder.addCallback(new SurfaceHolder.Callback() {

            public void surfaceChanged(SurfaceHolder holder, int format,
                    int width, int height) {
                Log.v(TAG, "surfaceChanged format=" + format + ", width=" + width + ", height=" +
                        height);
            }

            public void surfaceCreated(SurfaceHolder holder) {
                Log.v(TAG, "surfaceCreated");
                setSurface(holder.getSurface());
                Log.v(TAG, "surfaceCreated 2");
            }

            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.v(TAG, "surfaceDestroyed");
            }

        });

        openmaxalHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        ((Button) findViewById(R.id.java_player)).setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                mp.setOnPreparedListener(new OnPreparedListener() {
                    public void onPrepared(MediaPlayer xy) {
                        int width = xy.getVideoWidth();
                        int height = xy.getVideoHeight();
                        Log.v(TAG, "onPrepared width=" + width + ", height=" + height);
                        if (width!=0 && height!=0) {
                          javaHolder.setFixedSize(width, height);
                          xy.setOnVideoSizeChangedListener(
                            new MediaPlayer.OnVideoSizeChangedListener() {
                              public void onVideoSizeChanged(MediaPlayer ab, int width,
                                int height) {
                                  int w2 = ab.getVideoWidth();
                                  int h2 = ab.getVideoHeight();
                                  Log.v(TAG, "onVideoSizeChanged width=" + w2 + " (" + width +
                                        "), height=" + h2 + " (" + height + ")");
                                  if (w2 != 0 && h2 != 0) {
                                      javaHolder.setFixedSize(w2, h2);
                                  }
                              }
                          });
                          xy.start();

                        }
                    }
                });
                mp.prepareAsync();

            }
        });

        // initialize button click handlers

        ((Button) findViewById(R.id.openmaxal_player)).setOnClickListener(new OnClickListener() {
            boolean created = false;
            public void onClick(View view) {
                if (!created) {
                    created = createStreamingMediaPlayer("/sdcard/videos/ts/bar.ts");
                } else {
                    isPlayingStreaming = !isPlayingStreaming;
                    setPlayingStreamingMediaPlayer(isPlayingStreaming);
                }
            }
        });

        mp = new MediaPlayer();
        mp.setDisplay(javaHolder);
        try {
            mp.setDataSource("/sdcard/videos/burnAfterReading.m4v");
        } catch (IOException e) {

        }

    }

    /** Called when the activity is about to be paused. */
    @Override
    protected void onPause()
    {
        isPlayingStreaming = false;
        setPlayingStreamingMediaPlayer(false);
        super.onPause();
    }

    /** Called when the activity is about to be destroyed. */
    @Override
    protected void onDestroy()
    {
        shutdown();
        super.onDestroy();
    }

    /** Native methods, implemented in jni folder */
    public static native void createEngine();
    public static native boolean createStreamingMediaPlayer(String filename);
    public static native void setPlayingStreamingMediaPlayer(boolean isPlaying);
    public static native void shutdown();
    public static native void setSurface(Surface surface);
    // currently unused in this test app
    public static native void setSurfaceTexture(SurfaceTexture surfaceTexture);

    /** Load jni .so on initialization */
    static {
         System.loadLibrary("native-media-jni");
    }

}
