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

package android.media.effect.effects;

import android.filterfw.core.Filter;
import android.filterfw.core.OneShotScheduler;
import android.media.effect.EffectContext;
import android.media.effect.FilterGraphEffect;

/**
 * @hide
 */
public class LomoishEffect extends FilterGraphEffect {
    private static final String mGraphDefinition =
            "@import android.filterpacks.base;\n" +
            "@import android.filterpacks.imageproc;\n" +
            "\n" +
            "@filter GLTextureSource input {\n" +
            "  texId = 0;\n" + // Will be set by base class
            "  width = 0;\n" +
            "  height = 0;\n" +
            "  repeatFrame = true;\n" +
            "}\n" +
            "\n" +
            "@filter SharpenFilter sharpen {\n" +
            // TODO: scale should be -0.15
            "  scale = 0.15;\n" +
            "}\n" +
            "\n" +
            "@filter CrossProcessFilter crossProcess {\n" +
            "}\n" +
            "\n" +
            "@filter BlackWhiteFilter blackWhite {\n" +
            "  white = 0.8;\n" +
            "  black = 0.15;\n" +
            "}\n" +
            "\n" +
            "@filter VignetteFilter vignette {\n" +
            "  range = 0.73;\n" +
            "}\n" +
            "\n" +
            "@filter GLTextureTarget output {\n" +
            "  texId = 0;\n" +
            "}\n" +
            "\n" +
            "@connect input[frame]  => sharpen[image];\n" +
            "@connect sharpen[image] => crossProcess[image];\n" +
            "@connect crossProcess[image] => blackWhite[image];\n" +
            "@connect blackWhite[image] => vignette[image];\n" +
            "@connect vignette[image] => output[frame];\n";


    public LomoishEffect(EffectContext context, String name) {
        super(context, name, mGraphDefinition, "input", "output", OneShotScheduler.class);
    }
}
