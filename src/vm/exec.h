#include "common.h"

#include "vm/block.h"

/* OK so, we have two stacks (because iirc, you kind of need two stacks
 * to make a stack machine work.) It's a little bit like FORTH, in that
 * we have one "normal" stack which is used by most instructions (we
 * can push values onto the stack, branch based on these values, etc.)
 * and then one extra stack which tracks the function call structure and
 * can be manipulated using special instructions. */

/* We also have the "rstack". When we call a function, a stack frame is
 * pushed onto the rstack, which saves the return address, the last
 * value of fp and rp, and a pointer to the current code block (so we can
 * find things like constants.) then, *above* this on the stack, we can
 * use the space to store local variables, using instructions to move
 * those slots to/from the main stack by index. when we return, we reset
 * the fp and rp to point to the calling function's stack frame instead. */

/* (we need to track fp so we can remember where the return address is,
 * and we need to track rp so we know where to put the *next* frame.
 * this then lets us leave function arguments and function return values
 * on the normal stack, without worrying about needing to shuffle
 * things around.) */

/* I guess really the rstack is kind of like the "normal" C call stack,
 * in that it's where we put "stack-allocated" local variables and save
 * the stack frame, and the "normal" stack here does things that would
 * normally be accomplished using registers in a non-stack-machine
 * system... but, ah well. */

/* Calling convention for function arguments + return values is to put
 * a number at the top of the stack representing the number of arguments
 * or values returned, followed by the values in order from top to
 * bottom. */

/* Also these stacks grow upwards in memory, from low to high addresses. */

typedef enum {
  CFUNC_END,
  CFUNC_NEXT,
  CFUNC_TAILCALL,
} sbCFuncStatus;

typedef sbCFuncStatus (*sbRuntimeCFunc)(hVm, flag init);

typedef enum sbVmStatus {
  VM_STAT_UNKNOWN,
  VM_STAT_SUCCESS,
  VM_STAT_FAILURE,
} sbVmStatus;

typedef struct sbVmStackFrame {
  const u8 *return_addr;
  struct sbVmStackFrame *last_fp;
  u8 *last_rp;
  flag is_c_func;
  union {
    struct {
      const sbVmBlock *block;
      hClosure closure;
    } block_func;
    sbRuntimeCFunc c_func;
  };
  usize num_locals;
  sbVar locals[];
} sbVmStackFrame;

typedef struct sbVm {
  u8 *vstack;         /* for calculations */
  u8 *rstack;         /* for locals and return addresses, like FORTH */

  const u8 *ip;       /* instruction pointer */
  sbVmStackFrame *fp; /* frame pointer */
  u8 *vsp;            /* vstack pointer */
  u8 *rsp;            /* rstack pointer */

  usize stacksize;    /* to detect overflow, save these */
  usize rstacksize;   /* ^^ */

  sbVmProgram *program;
  flag running;
  flag debugmode;
} sbVm;

void sbVm_initialize(hVm vm, usize stacksize, usize rstacksize, flag debugmode);

void sbVm_deinitialize(hVm vm);

sbVmStatus sbVm_execute(hVm vm, sbVmProgram *pm);

void sbVm_push(hVm vm, hVal *value);

void sbVm_push_immediate(hVm vm, hVal *value);

hVal *sbVm_pop(hVm vm);

hVal *sbVm_npop(hVm vm, usize how_many);

hVal *sbVm_peek(hVm vm, usize where);

void sbVm_swap(hVm vm);

void sbVm_call_func(hVm vm, hVal *func);

void sbVm_transfer_to_func(hVm vm, hVal *func);

void sbVm_call_c_func(hVm vm, sbRuntimeCFunc func);

void sbVm_request_var_space(hVm vm, usize amount);

void sbVm_print_stack(hVm vm);
