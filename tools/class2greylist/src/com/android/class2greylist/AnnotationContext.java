/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.class2greylist;

import org.apache.bcel.Const;
import org.apache.bcel.classfile.JavaClass;

/**
 */
public abstract class AnnotationContext {

  public final Status status;
  public final JavaClass definingClass;

  public AnnotationContext(Status status, JavaClass definingClass) {
    this.status = status;
    this.definingClass = definingClass;
  }

  public String getClassDescriptor() {
      // JavaClass.getName() returns the Java-style name (with . not /), so we must fetch
      // the original class name from the constant pool.
      return definingClass.getConstantPool().getConstantString(
              definingClass.getClassNameIndex(), Const.CONSTANT_Class);
  }

  /**
   * @return the full descriptor of this member, in the format expected in
   * the greylist.
   */
  public abstract String getMemberDescriptor();

  /**
   * Report an error in this context. The final error message will include
   * the class and member names, and the source file name.
   */
  public abstract void reportError(String message, Object... args);
}
