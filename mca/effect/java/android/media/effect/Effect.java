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


package android.media.effect;


/**
 * Effects are high-performance transformations that can be applied to image frames. These are
 * passed in the form of OpenGLES2 texture names. Typical frames could be images loaded from disk,
 * or frames from the camera or other video stream.
 *
 * Effects are created using the EffectFactory class.
 */
public abstract class Effect {

    /**
     * Get the effect name.
     *
     * Returns the unique name of the effect, which matches the name used for instantiating this
     * effect by the EffectFactory.
     *
     * @return The name of the effect.
     */
    public abstract String getName();

    /**
     * Apply an effect to GL textures.
     *
     * Apply the Effect on the specified input GL texture, and write the result into the
     * output GL texture. The texture names passed must be valid in the current GL context. The
     * input texture must be a fully allocated texture with the given width and height. If the
     * output texture has not been allocated, it will be allocated by the effect, and have the same
     * size as the input. If the output texture was allocated already, and its size does not match
     * the input texture size, the result will be stretched to fit.
     *
     * @param inputTexId The GL texture name of the allocated input texture.
     * @param width The width of the input texture in pixels.
     * @param height The height of the input texture in pixels.
     * @param outputTexId The GL texture name of the output texture.
     */
    public abstract void apply(int inputTexId, int width, int height, int outputTexId);

    /**
     * Set a filter parameter.
     *
     * Consult the effect documentation for a list of supported parameter keys for each effect.
     *
     * @param parameterKey The name of the parameter to adjust.
     * @param value The new value to set the parameter to.
     * @throws InvalidArgumentException if parameterName is not a recognized name, or the value is
     *         not a valid value for this parameter.
     */
    public abstract void setParameter(String parameterKey, Object value);

    /**
     * Set an effect listener.
     *
     * Some effects may report state changes back to the host, if a listener is set. Consult the
     * individual effect documentation for more details.
     *
     * @param listener The listener to receive update callbacks on.
     */
    public void setUpdateListener(EffectUpdateListener listener) {
    }

    /**
     * Release an effect.
     *
     * Releases the effect and any resources associated with it. You may call this if you need to
     * make sure acquired resources are no longer held by the effect. Releasing an effect makes it
     * invalid for reuse.
     */
    public abstract void release();
}

