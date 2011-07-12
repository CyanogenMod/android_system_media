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

import java.lang.reflect.Field;

/**
 * @hide
 */
public class FieldPort extends InputPort {

    protected Field mField;
    protected boolean mHasFrame;
    protected Object mValue;

    public FieldPort(Filter filter, String name, Field field, boolean hasDefault) {
        super(filter, name);
        mField = field;
        mHasFrame = hasDefault;
    }

    @Override
    public void clear() {
    }

    @Override
    public void pushFrame(Frame frame) {
        setFieldFrame(frame, false);
    }

    @Override
    public void setFrame(Frame frame) {
        setFieldFrame(frame, true);
    }

    @Override
    public void transfer(FilterContext context) {
        if (mValue != null) {
            try {
                mField.set(mFilter, mValue);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(
                    "Access to field '" + mField.getName() + "' was denied!");
            }
            mValue = null;
            if (context != null) {
                mFilter.notifyFieldPortValueUpdated(mName, context);
            }
        }
    }

    @Override
    public Frame pullFrame() {
        throw new RuntimeException("Cannot pull frame on " + this + "!");
    }

    @Override
    public boolean hasFrame() {
        return mHasFrame;
    }

    @Override
    public boolean acceptsFrame() {
        return mValue == null;
    }

    @Override
    public String toString() {
        return "field " + super.toString();
    }

    protected void setFieldFrame(Frame frame, boolean isAssignment) {
        assertPortIsOpen();
        checkFrameType(frame, isAssignment);

        mValue = frame.getObjectValue();
        mHasFrame = true;
    }
}
