package com.android.class2greylist;

import org.apache.bcel.Const;
import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.JavaClass;

import java.util.Locale;

/**
 * Encapsulates context for a single annotation on a class member.
 */
public class AnnotationContext {

    public final Status status;
    public final FieldOrMethod member;
    public final JavaClass definingClass;
    public final String signatureFormatString;

    public AnnotationContext(
            Status status,
            FieldOrMethod member,
            JavaClass definingClass,
            String signatureFormatString) {
        this.status = status;
        this.member = member;
        this.definingClass = definingClass;
        this.signatureFormatString = signatureFormatString;
    }

    /**
     * @return the full descriptor of enclosing class.
     */
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
    public String getMemberDescriptor() {
        return String.format(Locale.US, signatureFormatString,
                getClassDescriptor(), member.getName(), member.getSignature());
    }

    /**
     * Report an error in this context. The final error message will include
     * the class and member names, and the source file name.
     */
    public void reportError(String message, Object... args) {
        StringBuilder error = new StringBuilder();
        error.append(definingClass.getSourceFileName())
                .append(": ")
                .append(definingClass.getClassName())
                .append(".")
                .append(member.getName())
                .append(": ")
                .append(String.format(Locale.US, message, args));

        status.error(error.toString());
    }

}
