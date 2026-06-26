#include "parser.h"

#include "parse/filereader.h"
#include "data/symbol.h"

static sbAst do_parse(hParser pr);

void sbParser_initialize(hParser pr) {
  sbArena_initialize(&pr->node_arena, 32768);
  sbTokenQueue_initialize(&pr->input_queue, 8);
}

void sbParser_deinitialize(hParser pr) {
  sbTokenQueue_deinitialize(&pr->input_queue);
  sbArena_deinitialize(&pr->node_arena);
}

sbAst sbParser_parse_file(hParser pr, const char *filename) {
  hFileReader fr = sbFileReader_open(filename);

  if (!sbFileReader_ok(fr)) {
    fprintf(stderr, "Error getting input stream.\n");
    return NULL;
  }

  sbLexer_initialize(&pr->lexer, fr);

  sbAst result = do_parse(pr);

  sbFileReader_close(fr);

  return result;
}

/* --- */

typedef struct binop {
  sbTokenType type;
  u8 left_precedence;
  u8 right_precedence;
} binop;

static const sbAstNode SENTINEL_VALUE = {0};
static sbAst NO_NODE = &SENTINEL_VALUE;

static sbLexToken peek_ahead(hParser pr, usize count) {
  while (sbTokenQueue_size(&pr->input_queue) < count + 1) {
    sbTokenQueue_enqueue(&pr->input_queue, sbLexer_next(&pr->lexer));
  }

  return sbTokenQueue_at(&pr->input_queue, count);
}

static sbLexToken next_token(hParser pr) {
  if (sbTokenQueue_size(&pr->input_queue) > 0) {
    sbTokenQueue_shift(&pr->input_queue);
  }

  return sbTokenQueue_at(&pr->input_queue, 0);
}

static flag accept(hParser pr, sbTokenType type, sbLexToken *tok_out) {
  sbLexToken up_next = peek_ahead(pr, 0);
  if (up_next.type == type) {
    next_token(pr);
    if (tok_out) *tok_out = up_next;
    return TRUE;
  } else {
    return FALSE;
  }
}

static flag expect(hParser pr, sbTokenType type, sbLexToken *tok_out) {
  sbLexToken up_next = peek_ahead(pr, 0);
  if (up_next.type == type) {
    next_token(pr);
    if (tok_out) *tok_out = up_next;
    return TRUE;
  } else {
    fprintf(stderr, "syntax error ! unexpected type %d\n", up_next.type); // TODO add line information, etc.
    return FALSE;
  }
}

static sbAst new_node(hParser pr, sbAstNode *n) {
  sbAstNode *new_node = sbArena_alloc(&pr->node_arena, sizeof(sbAstNode));
  *new_node = *n;
  return new_node;
}

static sbAst unop_node(hParser pr, sbTokenType operation, sbAstNode *child) {
  sbAstNode n = (sbAstNode) {
    .type = AST_NODE_OP,
    .op.type = operation,
    .op.left = child,
  };
  return new_node(pr, &n);
}

static sbAst binop_node(hParser pr, sbTokenType operation, sbAst left, sbAst right) {
  sbAstNode n = (sbAstNode) {
    .type = AST_NODE_OP,
    .op.type = operation,
    .op.left = left,
    .op.right = right,
  };
  return new_node(pr, &n);
}

static binop binops[] = {
  { T_rOR, 3, 4 },
  { T_rAND, 6, 7 },
  { T_DOUBLEEQUALS, 10, 11 },
  { T_GREATER, 10, 11 },
  { T_LESS, 10, 11 },
  { T_GREATEREQUALS, 10, 11 },
  { T_LESSEQUALS, 10, 11 },
  { T_NOTEQUALS, 10, 11 },
  { T_PLUS, 15, 16 },
  { T_MINUS, 15, 16 },
  { T_ASTERISK, 30, 31 },
  { T_SLASH, 30, 31 },
  { T_PERCENT, 30, 31 },
  { T_DOUBLESLASH, 30, 31 },
  { T_DOUBLEPERCENT, 30, 31 },
  { T_DOUBLEASTERISK, 51, 50 },
  { T_TWODOT, 60, 61 },
  { T_PAAMAYIM_NEKUDOTAYIM, 70, 71 },
};

const int NUM_BINOPS = sizeof(binops) / sizeof(binops[0]);

/* https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html */
static sbAst parse_expr(hParser pr, u8 min_precedence) {
  sbLexToken t = peek_ahead(pr, 0);
  sbAst lhs = NO_NODE;
  if (t.type == T_IDENTIFIER) {
    expect(pr, T_IDENTIFIER, NULL);
    sbAstNode n = { .type = AST_NODE_NAME, .symb = t.symb };
    lhs = new_node(pr, &n);
  } else if (t.type == T_LPAREN) {
    next_token(pr);
    lhs = parse_expr(pr, 0);
    if (!expect(pr, T_RPAREN, NULL)) {
      fprintf(stderr, "expected closing parenthesis!\n");
      return NO_NODE;
    }
  } else {
    fprintf(stderr, "expected identifier!\n");
    return NO_NODE;
  }

  while (1) {
    sbLexToken op = peek_ahead(pr, 0);
    if (op.type == T_EOF || op.type == T_SEMICOLON) {
      break;
    } else {
      binop *infix = NULL;
      for (int i = 0; i < NUM_BINOPS; i++) {
        if (binops[i].type == op.type) {
          infix = &binops[i];
          break;
        }
      }

      if (!infix) {
        /* something that isn't an infix operator: means end of expr */
        break;
      }

      if (infix->left_precedence < min_precedence) {
        /* end of this expr because something with lower precedence */
        break;
      }

      next_token(pr); /* consume operator token */

      sbAst rhs = parse_expr(pr, infix->right_precedence);

      lhs = binop_node(pr, infix->type, lhs, rhs);
    }
  }

  return lhs;
}

static void print_ast_node(sbAst n) {
  if (n->type == AST_NODE_NAME) {
    printf("%s", sbSymbol_name(n->symb));
  } else {
    printf("(");
    if (n->type == AST_NODE_OP) {
      printf("%c ", n->op.type);
      print_ast_node(n->op.left);
      printf(" ");
      print_ast_node(n->op.right);
    } else {
      printf("?<%d>", n->type);
    }
    printf(")");
  }
}

static sbAst do_parse(hParser pr) {
  sbAst program = parse_expr(pr, 0);
  print_ast_node(program);
  return program;
}
