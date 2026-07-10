#include "common.h"

extern hSymbol S_OP_ADD, S_OP_SUB, S_OP_MUL, S_OP_DIV, S_OP_FLDIV, S_OP_MOD;
extern hSymbol S_OP_EQ, S_OP_LT, S_OP_LE;
extern hSymbol S_OP_CALL, S_OP_SET, S_OP_INDEX, S_OP_INDEX_SET;
extern hSymbol S_OP_TO_STRING, S_OP_TO_INT, S_OP_TO_FLOAT, S_OP_TO_LIST, S_OP_TO_HASH;

void sbLib_create_sentinels();
