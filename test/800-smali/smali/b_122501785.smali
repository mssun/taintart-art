.class public LB122501785;

# Test that a hard + soft verifier failure in instance field access
# correctly triggers the hard fail to protect the compiler.

.super Ljava/lang/Object;

.method public static run(LB122501785;Ljava/lang/Object;)V
    .registers 4
    const/4 v0, 0
    const/4 v1, 1
    iput-boolean v0, v1, Ldoes/not/Exist;->field:Z
    return-void
.end method
