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


package android.filterfw.core;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.ProtocolException;

import java.io.StringWriter;
import java.lang.String;
import java.lang.reflect.Field;
import java.lang.annotation.Annotation;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import android.util.Log;

// TODO: Rename to FilterProtocol as it is quite specific to filters (given the updatable flag)?
//       ...or to ParameterProtocol?
public class Protocol {

    private class ParameterSig {
        public Class type;
        public int modifiers;

        public ParameterSig(Class type, int modifiers) {
            this.type = type;
            this.modifiers = modifiers;
        }
    }

    // Parameter modifiers
    public static final int MODIFIER_NONE       = 0x00;
    public static final int MODIFIER_UPDATABLE  = 0x01;
    public static final int MODIFIER_OPTIONAL   = 0x02;

    private HashMap<String, ParameterSig> mParameters;

    public Protocol() {
        mParameters = new HashMap<String, ParameterSig>();
    }

    public static Protocol fromFilter(Filter filter) {
        Protocol result = new Protocol();

        // Go through all filter parameters
        Annotation annotation;
        for (Field field : filter.getClass().getDeclaredFields()) {
            if ((annotation = field.getAnnotation(FilterParameter.class)) != null) {
                addFilterParameter((FilterParameter)annotation, field, result);
            } else if ((annotation = field.getAnnotation(ProgramParameter.class)) != null) {
                addProgramParameter((ProgramParameter)annotation, field, result);
            } else if ((annotation = field.getAnnotation(ProgramParameters.class)) != null) {
                addProgramParameters((ProgramParameters)annotation, field, result);
            }
        }

        return result;
    }

    public static Class getBoxedType(Class type) {
        // Check if type is primitive
        if (type.isPrimitive()) {
            // Yes -> box it
            if (type == boolean.class) {
                return java.lang.Boolean.class;
            } else if (type == byte.class) {
                return java.lang.Byte.class;
            } else if (type == char.class) {
                return java.lang.Character.class;
            } else if (type == short.class) {
                return java.lang.Short.class;
            } else if (type == int.class) {
                return java.lang.Integer.class;
            } else if (type == long.class) {
                return java.lang.Long.class;
            } else if (type == float.class) {
                return java.lang.Float.class;
            } else if (type == double.class) {
                return java.lang.Double.class;
            } else {
                throw new IllegalArgumentException("Unknown primitive type: " + type.getName() + "!");
            }
        } else {
            // No -> return it
            return type;
        }
    }

    public Protocol addParameter(String key, Class valueClass, int modifiers) {
        if (mParameters.containsKey(key)) {
            throw new IllegalArgumentException(
                "Attempting to add field '" + key + "' to protocol, when a  field of " +
                "the same name exists already!");
        }
        mParameters.put(key, new ParameterSig(valueClass, modifiers));
        return this;
    }

    public Protocol addRequired(String key, Class valueClass) {
        return addParameter(key, valueClass, MODIFIER_NONE);
    }

    public Protocol addOptional(String key, Class valueClass) {
        return addParameter(key, valueClass, MODIFIER_OPTIONAL);
    }

    public boolean hasParameter(String key) {
        return mParameters.containsKey(key);
    }

    public boolean isParameterUpdatable(String key) {
        ParameterSig paramSig = mParameters.get(key);
        return paramSig != null && (paramSig.modifiers & MODIFIER_UPDATABLE) != 0;
    }

    public void assertKeyValueMapConforms(KeyValueMap kvMap) throws ProtocolException {
        assertParameterConformance(kvMap);
        assertNoUnknownKeys(kvMap);
    }

    public KeyValueMap updateKVMap(KeyValueMap kvMap, KeyValueMap update) throws ProtocolException {
        // Make sure update updates only updatable items
        assertUpdatable(update);

        // Update the KV-Map
        KeyValueMap result = (KeyValueMap)kvMap.clone();
        result.putAll(update);

        // Make sure the result conforms with this protocol
        assertKeyValueMapConforms(result);

        // Return it
        return result;
    }

    public String toString() {
        StringWriter writer = new StringWriter();
        writer.append("Protocol { \n");
        for (Map.Entry<String, ParameterSig> entry : mParameters.entrySet()) {
            ParameterSig paramSig = entry.getValue();
            String prefix = (paramSig.modifiers & MODIFIER_OPTIONAL) == 0 ? "@Required" : "@Optional";
            String mods = (paramSig.modifiers & MODIFIER_UPDATABLE) == 0 ? "" : "(updatable)";
            writer.append("  " + prefix + mods + " " + paramSig.type.getName() +
                          " " + entry.getKey() + "\n");
        }
        writer.append("}");
        return writer.toString();
    }

    public String toHtmlString() {
        StringWriter writer = new StringWriter();
        if (mParameters.size() > 0) {
            for (Map.Entry<String, ParameterSig> entry : mParameters.entrySet()) {
                ParameterSig paramSig = entry.getValue();
                String prefix = (paramSig.modifiers & MODIFIER_OPTIONAL) == 0
                        ? "<b>Required</b>"
                        : "<b>Optional</b>";
                String mods = (paramSig.modifiers & MODIFIER_UPDATABLE) == 0
                        ? ""
                        : "(updatable)";
                writer.append("  " + prefix + mods + " " + paramSig.type.getSimpleName() +
                              ": " + entry.getKey() + "<br>");
            }
        } else {
            writer.append("<i>None</i>");
        }
        return writer.toString();
    }

    private void assertParameterConformance(KeyValueMap kvMap) throws ProtocolException {
        for (Map.Entry<String, ParameterSig> entry : mParameters.entrySet()) {
            ParameterSig paramSig = entry.getValue();

            // Make sure required key is present
            if ((paramSig.modifiers & MODIFIER_OPTIONAL) == 0
                && !kvMap.containsKey(entry.getKey())) {
                throw new ProtocolException(
                    "Protocol conformance error: KeyValueMap [" + kvMap.toString() + "] is missing " +
                    "required key '" + entry.getKey() + "'!");
            }

            // Make sure type is compatible
            Object value = kvMap.get(entry.getKey());
            if (value != null) {
                if (!paramSig.type.isAssignableFrom(value.getClass())) {
                    throw new ProtocolException(
                        "Protocol conformance error: In KeyValueMap [" + kvMap.toString() +
                        "]: Value of key '" + entry.getKey() + "' is of type '" + value.getClass().getName() +
                        "'. Expected a value assignable to '" + paramSig.type.getName() + "'!");
                }
            }
        }
    }

    private void assertNoUnknownKeys(KeyValueMap kvMap) throws ProtocolException {
        for (String key : kvMap.keySet()) {
            if (!hasParameter(key)) {
                throw new ProtocolException(
                    "Protocol conformance error: KeyValueMap [" + kvMap.toString() +
                    "] contains unknown key '" + key + "'!");
            }
        }
    }

    private void assertUpdatable(KeyValueMap updateMap) throws ProtocolException {
        for (String key : updateMap.keySet()) {
            if (!isParameterUpdatable(key)) {
                throw new ProtocolException(
                    "Protocol conformance error: Update KeyValueMap [" + updateMap.toString() +
                    "] contains non-updatable key '" + key + "'!");
            }
        }
    }

    private static void addFilterParameter(FilterParameter filterParam,
                                           Field field,
                                           Protocol result) {
        // Create the modifier mask
        int modifiers = filterParam.isUpdatable() ? MODIFIER_UPDATABLE : MODIFIER_NONE;
        modifiers |= filterParam.isOptional() ? MODIFIER_OPTIONAL : MODIFIER_NONE;

        // Get the parameter name
        String paramName = filterParam.name().isEmpty()
            ? field.getName()
            : filterParam.name();

        // Add the parameter
        result.addParameter(paramName, getBoxedType(field.getType()), modifiers);
    }

    private static void addProgramParameter(ProgramParameter programParam,
                                            Field field,
                                            Protocol result) {
        // Create the modifier mask
        int modifiers = programParam.isUpdatable() ? MODIFIER_UPDATABLE : MODIFIER_NONE;
        modifiers |= programParam.isOptional() ? MODIFIER_OPTIONAL : MODIFIER_NONE;

        // Get the parameter name
        String paramName = programParam.exposedName().isEmpty()
            ? programParam.name()
            : programParam.exposedName();

        // Get the parameter type
        Class type = programParam.type();

        // Add the parameter
        result.addParameter(paramName, getBoxedType(type), modifiers);
    }

    private static void addProgramParameters(ProgramParameters programParams,
                                             Field field,
                                             Protocol result) {
        for (ProgramParameter programParam : programParams.value()) {
            addProgramParameter(programParam, field, result);
        }
    }


}
