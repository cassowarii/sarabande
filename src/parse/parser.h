#include "common.h"

#include "parse/ast.h"
#include "parse/lexer.h"

/* Phase 4 of the compiler.
 * We take the stream of tokens coming from lexer (some of which tokens
 * are not actually real and are ghosts) and attempt to form them into a
 * coherent syntax that actually has some kind of structure. If we cannot
 * figure out how to parse a given token, we return a syntax error to
 * the user and compilation stops here. (The earlier stages can send us
 * errors as well through the means of a few special "error" tokens.) */

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
