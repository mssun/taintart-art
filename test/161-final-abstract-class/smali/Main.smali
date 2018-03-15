# Created with baksmali.

# Java file for reference.

# import java.lang.reflect.InvocationTargetException;
# import java.lang.reflect.Method;
#
# public class Main {
#     public static void main(String[] args) {
#         try {
#             // Make sure that the abstract final class is marked as erroneous.
#             Class.forName("AbstractFinal");
#             System.out.println("UNREACHABLE!");
#         } catch (VerifyError expected) {
#         } catch (Throwable t) {
#             t.printStackTrace(System.out);
#         }
#         try {
#             // Verification of TestClass.test() used to crash when processing
#             // the final abstract (erroneous) class.
#             Class<?> tc = Class.forName("TestClass");
#             Method test = tc.getDeclaredMethod("test");
#             test.invoke(null);
#             System.out.println("UNREACHABLE!");
#         } catch (InvocationTargetException ite) {
#             if (ite.getCause() instanceof InstantiationError) {
#                 System.out.println(
#                     ite.getCause().getClass().getName() + ": " + ite.getCause().getMessage());
#             } else {
#                 ite.printStackTrace(System.out);
#             }
#         } catch (Throwable t) {
#             t.printStackTrace(System.out);
#         }
#     }
# }

.class public LMain;
.super Ljava/lang/Object;
.source "Main.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    .line 20
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .registers 4

    .line 24
    :try_start_0
    const-string p0, "AbstractFinal"

    invoke-static {p0}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;

    .line 25
    sget-object p0, Ljava/lang/System;->out:Ljava/io/PrintStream;

    const-string v0, "UNREACHABLE!"

    invoke-virtual {p0, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    :try_end_c
    .catch Ljava/lang/VerifyError; {:try_start_0 .. :try_end_c} :catch_14
    .catch Ljava/lang/Throwable; {:try_start_0 .. :try_end_c} :catch_d

    goto :goto_15

    .line 27
    :catch_d
    move-exception p0

    .line 28
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;

    invoke-virtual {p0, v0}, Ljava/lang/Throwable;->printStackTrace(Ljava/io/PrintStream;)V

    goto :goto_16

    .line 26
    :catch_14
    move-exception p0

    .line 29
    :goto_15
    nop

    .line 33
    :goto_16
    :try_start_16
    const-string p0, "TestClass"

    invoke-static {p0}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;

    move-result-object p0

    .line 34
    const-string v0, "test"

    const/4 v1, 0x0

    new-array v2, v1, [Ljava/lang/Class;

    invoke-virtual {p0, v0, v2}, Ljava/lang/Class;->getDeclaredMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;

    move-result-object p0

    .line 35
    const/4 v0, 0x0

    new-array v1, v1, [Ljava/lang/Object;

    invoke-virtual {p0, v0, v1}, Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;

    .line 36
    sget-object p0, Ljava/lang/System;->out:Ljava/io/PrintStream;

    const-string v0, "UNREACHABLE!"

    invoke-virtual {p0, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    :try_end_32
    .catch Ljava/lang/reflect/InvocationTargetException; {:try_start_16 .. :try_end_32} :catch_3a
    .catch Ljava/lang/Throwable; {:try_start_16 .. :try_end_32} :catch_33

    goto :goto_76

    .line 44
    :catch_33
    move-exception p0

    .line 45
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;

    invoke-virtual {p0, v0}, Ljava/lang/Throwable;->printStackTrace(Ljava/io/PrintStream;)V

    goto :goto_77

    .line 37
    :catch_3a
    move-exception p0

    .line 38
    invoke-virtual {p0}, Ljava/lang/reflect/InvocationTargetException;->getCause()Ljava/lang/Throwable;

    move-result-object v0

    instance-of v0, v0, Ljava/lang/InstantiationError;

    if-eqz v0, :cond_71

    .line 39
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v1, Ljava/lang/StringBuilder;

    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    .line 40
    invoke-virtual {p0}, Ljava/lang/reflect/InvocationTargetException;->getCause()Ljava/lang/Throwable;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/Object;->getClass()Ljava/lang/Class;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/Class;->getName()Ljava/lang/String;

    move-result-object v2

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    const-string v2, ": "

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {p0}, Ljava/lang/reflect/InvocationTargetException;->getCause()Ljava/lang/Throwable;

    move-result-object p0

    invoke-virtual {p0}, Ljava/lang/Throwable;->getMessage()Ljava/lang/String;

    move-result-object p0

    invoke-virtual {v1, p0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object p0

    .line 39
    invoke-virtual {v0, p0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    goto :goto_76

    .line 42
    :cond_71
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;

    invoke-virtual {p0, v0}, Ljava/lang/reflect/InvocationTargetException;->printStackTrace(Ljava/io/PrintStream;)V

    .line 46
    :goto_76
    nop

    .line 47
    :goto_77
    return-void
.end method
