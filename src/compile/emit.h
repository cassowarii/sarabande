#include "common.h"

#include "compile/ir.h"
#include "vm/block.h"

/* Stage 6 of the compiler. The Final Stage.
 * Take the IR that was produced from the ir module, and
 * convert it into bytecode that our VM can understand. */

/* Since control flow and variable scope was already
 * linearized by the previous step, emitting these is
 * fairly straightforward (except that we have to compile
 * each block in two passes in order to compute the
 * position of forward jumps.) */

/* Expressions coming from the IR are still nested in a
 * tree structure like the AST, because it's easier to
 * flatten them in this step, where we are producing
 * code for a stack machine. This is pretty simple; we just
 * have to essentially convert the tree to "reverse polish
 * notation." */

void sbEmit_compile_program(sbVmProgram *vp, sbIrProgram *ir);
