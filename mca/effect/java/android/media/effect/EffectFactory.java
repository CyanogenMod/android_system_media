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

import java.lang.reflect.Constructor;
import java.util.HashMap;

/**
 * <p>The EffectFactory class defines the list of available Effects, and provides functionality to
 * inspect and instantiate them. Some effects may not be available on all platforms, so before
 * creating a certain effect, the application should confirm that the effect is supported on this
 * platform by calling {@link #isEffectSupported(String)}.</p>
 */
public class EffectFactory {

    private EffectContext mEffectContext;

    private final static String[] EFFECT_PACKAGES = {
        "android.media.effect.effects.",  // Default effect package
        ""                                // Allows specifying full class path
    };

    /** List of Effects */
    /**
     * <p>Copies the input texture to the output.</p>
     * @hide
     */
    public final static String EFFECT_IDENTITY = "IdentityEffect";

    /**
     * <p>Adjusts the brightness of the image.</p>
     * <p>Available parameters:</p>
     * <table>
     * <tr><td>Parameter name</td><td>Meaning</td><td>Valid values</td></tr>
     * <tr><td><code>brightness</code></td>
     *     <td>The brightness multiplier.</td>
     *     <td>Positive float. 1.0 means no change;
               larger values will increase brightness.</td>
     * </tr>
     * </table>
     */
    public final static String EFFECT_BRIGHTNESS =
            "android.media.effect.effects.BrightnessEffect";

    /**
     * <p>Adjusts the contrast of the image.</p>
     * <p>Available parameters:</p>
     * <table>
     * <tr><td>Parameter name</td><td>Meaning</td><td>Valid values</td></tr>
     * <tr><td><code>contrast</code></td>
     *     <td>The contrast multiplier.</td>
     *     <td>Float. 1.0 means no change;
               larger values will increase contrast.</td>
     * </tr>
     * </table>
     */
    public final static String EFFECT_CONTRAST =
            "android.media.effect.effects.ContrastEffect";

    /**
     * <p>Applies a fisheye lens distortion to the image.</p>
     * <p>Available parameters:</p>
     * <table>
     * <tr><td>Parameter name</td><td>Meaning</td><td>Valid values</td></tr>
     * <tr><td><code>scale</code></td>
     *     <td>The scale of the distortion.</td>
     *     <td>Float, between 0 and 1. Zero means no distortion.</td>
     * </tr>
     * </table>
     */
    public final static String EFFECT_FISHEYE =
            "android.media.effect.effects.FisheyeEffect";

    /**
     * <p>Replaces the background of the input frames with frames from a
     * selected video.  Requires an initial learning period with only the
     * background visible before the effect becomes active. The effect will wait
     * until it does not see any motion in the scene before learning the
     * background and starting the effect.</p>
     *
     * <p>Available parameters:</p>
     * <table>
     * <tr><td>Parameter name</td><td>Meaning</td><td>Valid values</td></tr>
     * <tr><td><code>source</code></td>
     *     <td>A URI for the background video to use. This parameter must be
     *         supplied before calling apply() for the first time.</td>
     *     <td>String, such as from
     *         {@link android.net.Uri#toString Uri.toString()}</td>
     * </tr>
     * </table>
     *
     * <p>If the update listener is set for this effect using
     * {@link Effect#setUpdateListener}, it will be called when the effect has
     * finished learning the background, with a null value for the info
     * parameter.</p>
     */
    public final static String EFFECT_BACKDROPPER =
            "android.media.effect.effects.BackDropperEffect";

    /**
     * Applies histogram equalization on the image.<br/>
     * Parameters: scale (float): the scale of histogram equalization.
     */
    public final static String EFFECT_AUTOFIX =
            "android.media.effect.effects.AutoFixEffect";

    /**
     * Adjusts the range of minimal and maximal values of color pixels.<br/>
     * Parameters: black (float): the value of the minimal pixel.
     * Parameters: white (float): the value of the maximal pixel.
     */
    public final static String EFFECT_BLACKWHITE =
            "android.media.effect.effects.BlackWhiteEffect";

    /**
     * Crops an upright rectangular area from the image.<br/>
     * Parameters: xorigin (int): xorigin.
     *             yorigin (int): yorigin.
     *             width (int): rectangle width.
     *             height (int): rectangle height.
     */
    public final static String EFFECT_CROP =
            "android.media.effect.effects.CropEffect";

    /**
     * Applies cross process effect on image.<br/>
     * Parameters: contrast (float): The strength of the color contrast.
     */
    public final static String EFFECT_CROSSPROCESS =
            "android.media.effect.effects.CrossProcessEffect";

    /**
     * Applies documentary effect on image.<br/>
     * Parameters: contrast (float): The strength of the color contrast.
     */
    public final static String EFFECT_DOCUMENTARY =
            "android.media.effect.effects.DocumentaryEffect";

    /**
     * Attaches doodles to image.<br/>
     * Parameters: contrast (float): The strength of the color contrast.
     */
    public final static String EFFECT_DOODLE =
            "android.media.effect.effects.DoodleEffect";

    /**
     * Applies duotone effect on image.<br/>
     * Parameters: first_color (int): first color in duotone.
     * Parameters: second_color (int): second color in duotone.
     */
    public final static String EFFECT_DUOTONE =
            "android.media.effect.effects.DuotoneEffect";

    /**
     * Adds backlight to the image.<br/>
     * Parameters: backlight (float): The scale of the distortion.
     */
    public final static String EFFECT_FILLLIGHT =
            "android.media.effect.effects.FillLightEffect";

    /**
     * Flips image vertically and/or horizontally.<br/>
     * Parameters: vertical (boolean): flip image vertically.
     * Parameters: horizontal (boolean): flip image horizontally.
     */
    public final static String EFFECT_FLIP =
            "android.media.effect.effects.FlipEffect";

    /**
     * Applies film grain effect on image.<br/>
     */
    public final static String EFFECT_GRAIN =
            "android.media.effect.effects.GrainEffect";

    /**
     * Converts image to grayscale.<br/>
     */
    public final static String EFFECT_GRAYSCALE =
            "android.media.effect.effects.GrayscaleEffect";

    /**
     * Applies lomoish effect on image.<br/>
     */
    public final static String EFFECT_LOMOISH =
            "android.media.effect.effects.LomoishEffect";

    /**
     * Applies negative film effect on image.<br/>
     * Parameters: scale (float): the degree of film grain.
     */
    public final static String EFFECT_NEGATIVE =
            "android.media.effect.effects.NegativeEffect";

    /**
     * Applied posterized effect on image.<br/>
     */
    public final static String EFFECT_POSTERIZE =
            "android.media.effect.effects.PosterizeEffect";

    /**
     * Removes red eyes on specified region.<br/>
     * Parameters: intensity (float): threshold used to indentify red eyes.
     *             redeye (Bitmap): bitmap specifies red eye regions.
     */
    public final static String EFFECT_REDEYE =
            "android.media.effect.effects.RedEyeEffect";

    /**
     * Rotates the image.<br/>
     * Parameters: degree (float): the degree of rotation. shoule be a multiple of 90.
     */
    public final static String EFFECT_ROTATE =
            "android.media.effect.effects.RotateEffect";

    /**
     * Adjusts color saturation on image.<br/>
     * Parameters: scale (float): The scale of color saturation.
     */
    public final static String EFFECT_SATURATE =
            "android.media.effect.effects.SaturateEffect";

    /**
     * Converts image to sepia tone.<br/>
     */
    public final static String EFFECT_SEPIA =
            "android.media.effect.effects.SepiaEffect";

    /**
     * Sharpens the image.<br/>
     * Parameters: scale (float): The degree of sharpening.
     */
    public final static String EFFECT_SHARPEN =
            "android.media.effect.effects.SharpenEffect";

    /**
     * Rotates and resizes the image accroding to specified angle.<br/>
     * Parameters: scale (angle): the angle of rotation.
     */
    public final static String EFFECT_STRAIGHTEN =
            "android.media.effect.effects.StraightenEffect";

    /**
     * Adjusts color temperature in the image.<br/>
     * Parameters: scale (float): the value of color temperature.
     */
    public final static String EFFECT_TEMPERATURE =
            "android.media.effect.effects.ColorTemperatureEffect";

    /**
     * Applies tine effect on image.<br/>
     */
    public final static String EFFECT_TINT =
            "android.media.effect.effects.TintEffect";

    /**
     * Appliies vignette effect on image.<br/>
     * Parameters: range (float): The range of vignetting.
     */
    public final static String EFFECT_VIGNETTE =
            "android.media.effect.effects.VignetteEffect";

    EffectFactory(EffectContext effectContext) {
        mEffectContext = effectContext;
    }

    /**
     * Instantiate a new effect with the given effect name.
     *
     * <p>The effect's parameters will be set to their default values.</p>
     *
     * <p>Note that the EGL context associated with the current EffectContext need not be made
     * current when creating an effect. This allows the host application to instantiate effects
     * before any EGL context has become current.</p>
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
     * <p>Some effects may only be available on certain platforms. Use this method before
     * instantiating an effect to make sure it is supported.</p>
     *
     * @param effectName The name of the effect.
     * @return true, if the effect is supported on this platform.
     * @throws IllegalArgumentException if the effect name is not known.
     */
    public static boolean isEffectSupported(String effectName) {
        return getEffectClassByName(effectName) != null;
    }

    private static Class getEffectClassByName(String className) {
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
