#include "common.h"

#include "parse/ast.h"
#include "parse/lexer.h"

typedef struct sbParser {
  sbLexer lexer;
  sbTokenQueue input_queue;
  sbArena node_arena;
  flag error_state;
  flag any_error;
} sbParser;

typedef sbParser *hParser;

sbAstNode *sbParser_parse_file(hParser pr, const char *filename);

void sbParser_initialize(hParser pr);

void sbParser_deinitialize(hParser pr);
