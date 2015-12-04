# Test that the verifier does not stash methods incorrectly because they are being invoked with
# the wrong opcode.
#
# This is the non-interface class that implements the interface. We need a subclass of this, so
# that the implementation or miranda method isn't found in the subclass.
#
# We also use this to have a correct call.

.class public abstract LB21869691B;

.super Ljava/lang/Object;
.implements LB21869691I;

.method protected constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

# Have an implementation for the interface method.
.method public a()V
  .registers 1
  return-void
.end method

# Call ourself with invoke-virtual.
.method public callB()V
  .registers 1
  invoke-virtual {p0}, LB21869691B;->a()V
  return-void
.end method

# Call C with invoke-virtual.
.method public callB(LB21869691C;)V
  .registers 2
  invoke-virtual {p1}, LB21869691C;->a()V
  return-void
.end method
