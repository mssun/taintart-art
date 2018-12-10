package com.android.class2greylist;

import org.apache.bcel.classfile.DescendingVisitor;
import org.apache.bcel.classfile.EmptyVisitor;
import org.apache.bcel.classfile.Field;
import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.JavaClass;
import org.apache.bcel.classfile.Method;

/**
 * A class file visitor that simply prints to stdout the signature of every member within the class.
 */
public class MemberDumpingVisitor extends EmptyVisitor {

    private final Status mStatus;
    private final DescendingVisitor mDescendingVisitor;

    /**
     * Creates a visitor for a class.
     *
     * @param clazz Class to visit
     */
    public MemberDumpingVisitor(JavaClass clazz, Status status) {
        mStatus = status;
        mDescendingVisitor = new DescendingVisitor(clazz, this);
    }

    public void visit() {
        mDescendingVisitor.visit();
    }

    @Override
    public void visitMethod(Method method) {
        visitMember(method, "L%s;->%s%s");
    }

    @Override
    public void visitField(Field field) {
        visitMember(field, "L%s;->%s:%s");
    }

    private void visitMember(FieldOrMethod member, String signatureFormatString) {
        AnnotationContext context = new AnnotatedMemberContext(mStatus,
            (JavaClass) mDescendingVisitor.predecessor(), member,
            signatureFormatString);
        System.out.println(context.getMemberDescriptor());
    }
}
