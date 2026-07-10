#include "lib/sentinel.h"

#include "data/symbol.h"

hSymbol S_OP_ADD, S_OP_SUB, S_OP_MUL, S_OP_DIV, S_OP_FLDIV, S_OP_MOD;
hSymbol S_OP_EQ, S_OP_LT, S_OP_LE;
hSymbol S_OP_CALL, S_OP_SET, S_OP_INDEX, S_OP_INDEX_SET;
hSymbol S_OP_TO_STRING, S_OP_TO_INT, S_OP_TO_FLOAT, S_OP_TO_LIST, S_OP_TO_HASH;

void sbLib_create_sentinels() {
  S_OP_ADD = SENTINEL("<op::add>");
  S_OP_SUB = SENTINEL("<op::sub>");
  S_OP_MUL = SENTINEL("<op::mul>");
  S_OP_DIV = SENTINEL("<op::div>");
  S_OP_FLDIV = SENTINEL("<op::intdiv>");
  S_OP_MOD = SENTINEL("<op::mod>");
  S_OP_EQ = SENTINEL("<op::eq>");
  S_OP_LT = SENTINEL("<op::lt>");
  S_OP_LE = SENTINEL("<op::le>");
  S_OP_CALL = SENTINEL("<op::call>");
  S_OP_SET = SENTINEL("<op::set>");
  S_OP_INDEX = SENTINEL("<op::index>");
  S_OP_INDEX_SET = SENTINEL("<op::set_index>");
  S_OP_TO_STRING = SENTINEL("<string::convert>");
  S_OP_TO_INT = SENTINEL("<integer::convert>");
  S_OP_TO_FLOAT = SENTINEL("<float::convert>");
  S_OP_TO_LIST = SENTINEL("<list::convert>");
  S_OP_TO_HASH = SENTINEL("<hash::convert>");
}
