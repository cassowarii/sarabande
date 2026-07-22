#include "common.h"

struct sbAstNode;
i32 sbAst_count_pipe_underscores(struct sbAstNode *node);

enum sbAstOp;
flag sbIr_constant_fold(enum sbAstOp op, hInteger left, hInteger right, hInteger *result);
