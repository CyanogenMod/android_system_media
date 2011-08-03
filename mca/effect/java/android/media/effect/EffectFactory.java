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

import android.media.effect.effects.BrightnessEffect;
import android.media.effect.effects.ContrastEffect;
import android.media.effect.effects.FisheyeEffect;

import java.lang.reflect.Constructor;
import java.util.HashMap;

/**
 * The EffectFactory class defines the list of available Effects, and provides functionality to
 * inspect and instantiate them. Some effects may not be available on all platforms, so before
 * creating a certain effect, the application should confirm that the effect is supported on this
 * platform by calling {@link #isEffectSupported(String)}.
 */
public class EffectFactory {

    private EffectContext mEffectContext;

    private final static String[] EFFECT_PACKAGES = {
        "android.media.effect.effects.",  // Default effect package
        ""                                // Allows specifying full class path
    };

    /** List of Effects */
    /**
     * Adjusts the brightness of the image.<br/>
     * Parameters: brightness (float): The factor by which to multiply the color channels.
     */
    public final static String EFFECT_BRIGHTNESS = "BrightnessEffect";

    /**
     * Adjusts the contrast of the image.<br/>
     * Parameters: contrast (float): The strength of the color contrast.
     */
    public final static String EFFECT_CONTRAST = "ContrastEffect";

    /**
     * Applies a fisheye lens distortion to the image.<br/>
     * Parameters: scale (float): The scale of the distortion.
     */
    public final static String EFFECT_FISHEYE = "FisheyeEffect";

    /** ...Many more effects to follow ... */

    EffectFactory(EffectContext effectContext) {
        mEffectContext = effectContext;
    }

    /**
     * Instantiate a new effect with the given effect name.
     *
     * The effect's parameters will be set to their default values.
     *
     * @param effectName The name of the effect to create.
     * @return A new Effect instance.
     * @throws IllegalArgumentException if the effect with the specified name is not supported or
     *         not known.
     */
    public Effect createEffect(String effectName) {
        Class effectClass = getEffectClassByName(effectName);
        if (effectClass == null) {
            throw new IllegalArgumentException("Cannot instantiate unknown effect '" +
                effectName + "'!");
        }
        return instantiateEffect(effectClass, effectName);
    }

    /**
     * Check if an effect is supported on this platform.
     *
     * Some effects may only be available on certain platforms. Use this method before
     * instantiating an effect to make sure it is supported.
     *
     * @param effectName The name of the effect.
     * @return true, if the effect is supported on this platform.
     * @throws IllegalArgumentException if the effect name is not known.
     */
    public boolean isEffectSupported(String effectName) {
        return getEffectClassByName(effectName) != null;
    }

    private Class getEffectClassByName(String className) {
        Class effectClass = null;

        // Get context's classloader; otherwise cannot load non-framework effects
        ClassLoader contextClassLoader = Thread.currentThread().getContextClassLoader();

        // Look for the class in the imported packages
        for (String packageName : EFFECT_PACKAGES) {
            try {
                effectClass = contextClassLoader.loadClass(packageName + className);
            } catch (ClassNotFoundException e) {
                continue;
            }
            // Exit loop if class was found.
            if (effectClass != null) {
                break;
            }
        }
        return effectClass;
    }

    private Effect instantiateEffect(Class effectClass, String name) {
        // Make sure this is an Effect subclass
        try {
            effectClass.asSubclass(Effect.class);
        } catch (ClassCastException e) {
            throw new IllegalArgumentException("Attempting to allocate effect '" + effectClass
                + "' which is not a subclass of Effect!", e);
        }

        // Look for the correct constructor
        Constructor effectConstructor = null;
        try {
            effectConstructor = effectClass.getConstructor(EffectContext.class, String.class);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException("The effect class '" + effectClass + "' does not have "
                + "the required constructor.", e);
        }

        // Construct the effect
        Effect effect = null;
        try {
            effect = (Effect)effectConstructor.newInstance(mEffectContext, name);
        } catch (Throwable t) {
            throw new RuntimeException("There was an error constructing the effect '" + effectClass
                + "'!", t);
        }

        return effect;
    }
}

