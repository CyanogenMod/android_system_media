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

import java.io.StringWriter;

import java.lang.Float;
import java.lang.Integer;
import java.lang.String;
import java.lang.reflect.Field;

import java.util.HashMap;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;
import java.util.regex.Pattern;

import android.util.Log;
import android.filterfw.core.Protocol;
import android.filterfw.core.ProtocolException;
import android.filterfw.io.PatternScanner;

public class KeyValueMap implements Cloneable {

    private HashMap<String, Object> mHashMap;

    public KeyValueMap() {
        mHashMap = new HashMap<String, Object>();
    }

    public Object clone() {
        try {
            KeyValueMap result = (KeyValueMap)super.clone();
            result.mHashMap = (HashMap<String, Object>)mHashMap.clone();
            return result;
        } catch (CloneNotSupportedException e) {
            throw new AssertionError();
        }
    }

    public void setValue(String key, Object value) {
        mHashMap.put(key, value);
    }

    public void setKeyValues(Object... keyValues) {
        if (keyValues.length % 2 != 0) {
            throw new RuntimeException("Key-Value arguments passed into setKeyValues must be "
            + "an alternating list of keys and values!");
        }
        for (int i = 0; i < keyValues.length; i += 2) {
            if (!(keyValues[i] instanceof String)) {
                throw new RuntimeException("Key-value argument " + i + " must be a key of type "
                    + "String, but found an object of type " + keyValues[i].getClass() + "!");
            }
            String key = (String)keyValues[i];
            Object value = keyValues[i+1];
            setValue(key, value);
        }
    }

    public boolean hasKey(String key) {
        return mHashMap.containsKey(key);
    }

    public Set<String> keySet() {
        return mHashMap.keySet();
    }

    public Object getValue(String key) {
        return mHashMap.get(key);
    }

    public String getStringValue(String key) {
        Object result = mHashMap.get(key);
        return (result != null && result instanceof String)
                ? (String)result
                : null;
    }

    public int getIntValue(String key) {
        Object result = mHashMap.get(key);
        return (result != null && result instanceof Integer)
                ? ((Integer)result).intValue()
                : 0;
    }

    public float getFloatValue(String key) {
        Object result = mHashMap.get(key);
        Log.v("KeyValueMap", "Result = " + result);
        return (result != null && result instanceof Float)
                ? ((Float)result).floatValue()
                : 0.0f;
    }

    public int size() {
        return mHashMap.size();
    }

    public boolean conformsToProtocol(Protocol protocol) {
        try {
            assertConformsToProtocol(protocol);
        } catch (ProtocolException e) {
            return false;
        }
        return true;
    }

    public void assertConformsToProtocol(Protocol protocol) throws ProtocolException {
        protocol.assertKeyValueMapConforms(this);
    }

    public void updateWithMap(KeyValueMap map) {
        mHashMap.putAll(map.mHashMap);
    }

    public String toString() {
        StringWriter writer = new StringWriter();
        for (Map.Entry<String, Object> entry : mHashMap.entrySet()) {
            String valueString;
            Object value = entry.getValue();
            if (value instanceof String) {
                valueString = "\"" + value + "\"";
            } else {
                valueString = value.toString();
            }
            writer.write(entry.getKey() + " = " + valueString + ";\n");
        }
        return writer.toString();
    }

    public void readAssignments(String assignments) {
        final Pattern ignorePattern = Pattern.compile("\\s+");
        PatternScanner scanner = new PatternScanner(assignments, ignorePattern);
        readAssignments(scanner, null, null);
    }

    public void readAssignments(PatternScanner scanner,
                                Pattern endPattern,
                                KeyValueMap references) {
        // Our parser is a state-machine, and these are our states
        final int STATE_IDENTIFIER = 0;
        final int STATE_EQUALS     = 1;
        final int STATE_VALUE      = 2;
        final int STATE_POST_VALUE = 3;

        final Pattern equalsPattern = Pattern.compile("=");
        final Pattern semicolonPattern = Pattern.compile(";");
        final Pattern wordPattern = Pattern.compile("[a-zA-Z]+[a-zA-Z0-9]*");
        final Pattern stringPattern = Pattern.compile("'[^']*'|\\\"[^\\\"]*\\\"");
        final Pattern intPattern = Pattern.compile("[0-9]+");
        final Pattern floatPattern = Pattern.compile("[0-9]*\\.[0-9]+f?");
        final Pattern referencePattern = Pattern.compile("\\$[a-zA-Z]+[a-zA-Z0-9]");
        final Pattern booleanPattern = Pattern.compile("true|false");

        int state = STATE_IDENTIFIER;
        KeyValueMap newVals = new KeyValueMap();
        String curKey = null;
        String curValue = null;

        while (!scanner.atEnd() && !(endPattern != null && scanner.peek(endPattern))) {
            switch (state) {
                case STATE_IDENTIFIER:
                    curKey = scanner.eat(wordPattern, "<identifier>");
                    state = STATE_EQUALS;
                    break;

                case STATE_EQUALS:
                    scanner.eat(equalsPattern, "=");
                    state = STATE_VALUE;
                    break;

                case STATE_VALUE:
                    if ((curValue = scanner.tryEat(stringPattern)) != null) {
                        newVals.setValue(curKey, curValue.substring(1, curValue.length() - 1));
                    } else if ((curValue = scanner.tryEat(referencePattern)) != null) {
                        String refName = curValue.substring(1, curValue.length());
                        Object referencedObject = references != null ? references.getValue(refName) : null;
                        if (referencedObject == null) {
                            throw new RuntimeException("Unknown object reference to '" + refName + "'!");
                        }
                        newVals.setValue(curKey, referencedObject);
                    } else if ((curValue = scanner.tryEat(booleanPattern)) != null) {
                        newVals.setValue(curKey, Boolean.parseBoolean(curValue));
                    } else if ((curValue = scanner.tryEat(floatPattern)) != null) {
                        newVals.setValue(curKey, Float.parseFloat(curValue));
                    } else if ((curValue = scanner.tryEat(intPattern)) != null) {
                        newVals.setValue(curKey, Integer.parseInt(curValue));
                    } else {
                        throw new RuntimeException(scanner.unexpectedTokenMessage("<value>"));
                    }
                    state = STATE_POST_VALUE;
                    break;

                case STATE_POST_VALUE:
                    scanner.eat(semicolonPattern, ";");
                    state = STATE_IDENTIFIER;
                    break;
            }
        }

        // Make sure end is expected
        if (state != STATE_IDENTIFIER && state != STATE_POST_VALUE) {
            throw new RuntimeException("Unexpected end of assignments on line " + scanner.lineNo() + "!");
        }

        // If everything worked out, update our values
        updateWithMap(newVals);
    }

    // Core internal methods /////////////////////////////////////////////////////////////////////////
    void setFilterParameters(Filter filter) {
        Class filterClass = filter.getClass();
        FilterParameter param;
        for (Field field : filterClass.getDeclaredFields()) {
            if ((param = field.getAnnotation(FilterParameter.class)) != null) {
                String paramName = param.name().isEmpty() ? field.getName() : param.name();
                if (hasKey(paramName)) {
                    field.setAccessible(true);
                    try {
                        field.set(filter, getValue(paramName));
                    } catch (IllegalAccessException e) {
                        throw new RuntimeException("Access to field '" + field.getName() + "' was denied!");
                    }
                }
            }
        }
    }
}
