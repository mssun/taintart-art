.class public LB121191566;
.super Ljava/lang/Object;


.method public constructor <init>()V
.registers 1
       invoke-direct {p0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public static run(Ljava/lang/Object;)Z
.registers 5
       move-object v3, v4
       instance-of v4, v3, Ljava/lang/String;
       if-eqz v4, :Branch
       # The peephole must not overwrite v4 (from the move-object). Use an integral move
       # to check.
       move v0, v4
       goto :End
:Branch
       # See above.
       move v0, v4
:End
       # Triple-check: the merge should be consistent.
       return v0
.end method
