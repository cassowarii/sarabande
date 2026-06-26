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
  sbAstOp ast_op;
} binop;

typedef struct unop {
  sbTokenType type;
  u8 precedence;
  sbAstOp ast_op;
} unop;

typedef struct opspelling {
  sbAstOp ast_op;
  const char *name;
} opspelling;

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

static sbAst unop_node(hParser pr, sbAstOp operation, sbAst child) {
  sbAstNode n = (sbAstNode) {
    .type = AST_NODE_OP,
    .op.type = operation,
    .op.left = child,
  };
  return new_node(pr, &n);
}

static sbAst binop_node(hParser pr, sbAstOp operation, sbAst left, sbAst right) {
  sbAstNode n = (sbAstNode) {
    .type = AST_NODE_OP,
    .op.type = operation,
    .op.left = left,
    .op.right = right,
  };
  return new_node(pr, &n);
}

static sbAst seq_node(hParser pr, sbAstType type, sbAst left, sbAst right) {
  sbAstNode n = (sbAstNode) {
    .type = type,
    .seq.left = left,
    .seq.right = right,
  };
  return new_node(pr, &n);
}

static sbAst name_node(hParser pr, sbLexToken token) {
  if (token.type != T_IDENTIFIER) {
    PANIC("can't create name node with token of type %d\n", token.type);
  }

  sbAstNode n = (sbAstNode) {
    .type = AST_NODE_NAME,
    .symb = token.symb,
  };
  return new_node(pr, &n);
}

static binop binops[] = {
  { T_PIPE, 6, 7, AST_OP_PIPE },
  { T_rOR, 10, 11, AST_OP_OR },
  { T_rAND, 20, 21, AST_OP_AND },
  { T_DOUBLEEQUALS, 30, 31, AST_OP_EQ },
  { T_GREATER, 30, 31, AST_OP_GT },
  { T_LESS, 30, 31, AST_OP_LT },
  { T_GREATEREQUALS, 30, 31, AST_OP_GE },
  { T_LESSEQUALS, 30, 31, AST_OP_LE },
  { T_NOTEQUALS, 30, 31, AST_OP_NE },
  { T_DOUBLEPERCENT, 30, 31, AST_OP_DIVBY },
  { T_PLUS, 45, 46, AST_OP_ADD },
  { T_MINUS, 45, 46, AST_OP_SUB },
  { T_ASTERISK, 60, 61, AST_OP_MUL },
  { T_SLASH, 60, 61, AST_OP_DIV },
  { T_PERCENT, 60, 61, AST_OP_MOD },
  { T_DOUBLESLASH, 60, 61, AST_OP_FLDIV },
  { T_DOUBLEASTERISK, 71, 70, AST_OP_POW },
  { T_TWODOT, 80, 81, AST_OP_RANGE },
  { T_DOT, 90, 91, AST_OP_NULL },
  { T_LPAREN, 90, 91, AST_OP_NULL },
  { T_LBRACKET, 90, 91, AST_OP_INDEX },
  { T_PAAMAYIM_NEKUDOTAYIM, 100, 101, AST_OP_SCOPE },
};

static unop unops[] = {
  { T_rNOT, 25, AST_OP_NOT },
  { T_PLUS, 85, AST_OP_UNPLUS },
  { T_MINUS, 85, AST_OP_UNMINUS },
};

static opspelling op_spellings[] = {
  { AST_OP_OR, "or" },
  { AST_OP_AND, "and" },
  { AST_OP_NOT, "not" },
  { AST_OP_EQ, "==" },
  { AST_OP_NE, "!=" },
  { AST_OP_GT, ">" },
  { AST_OP_LT, "<" },
  { AST_OP_GE, ">=" },
  { AST_OP_LE, "<=" },
  { AST_OP_DIVBY, "%%" },
  { AST_OP_ADD, "+" },
  { AST_OP_UNPLUS, "+@" },
  { AST_OP_SUB, "-" },
  { AST_OP_UNMINUS, "-@" },
  { AST_OP_MUL, "*" },
  { AST_OP_DIV, "/" },
  { AST_OP_MOD, "%" },
  { AST_OP_FLDIV, "//" },
  { AST_OP_POW, "**" },
  { AST_OP_RANGE, ".." },
  { AST_OP_INDEX, "[]" },
  { AST_OP_SCOPE, "::" },
};

const int NUM_BINOPS = sizeof(binops) / sizeof(binops[0]);
const int NUM_UNOPS = sizeof(unops) / sizeof(unops[0]);
const int NUM_SPELLINGS = sizeof(op_spellings) / sizeof(op_spellings[0]);

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
    sbLexToken op = peek_ahead(pr, 0);
    unop *prefix = NULL;
    for (int i = 0; i < NUM_UNOPS; i++) {
      if (unops[i].type == op.type) {
        prefix = &unops[i];
        break;
      }
    }

    if (!prefix) {
      fprintf(stderr, "expected identifier, '(', or operator!\n");
      return NO_NODE;
    }

    next_token(pr);

    sbAst content = parse_expr(pr, prefix->precedence);

    lhs = unop_node(pr, prefix->ast_op, content);
  }

  while (1) {
    sbLexToken op = peek_ahead(pr, 0);
    sbAstType ast_type = AST_NODE_OP;
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

    sbAst rhs;
    if (op.type == T_LPAREN) {
      /* function call */
      rhs = parse_expr(pr, 0);
      ast_type = AST_NODE_FUNCCALL;
      if (!expect(pr, T_RPAREN, NULL)) {
        /* TODO handle commas: we should have a "parse comma separated expr" thing */
        fprintf(stderr, "expected ')'\n");
        return NO_NODE;
      }
    } else if (op.type == T_LBRACKET) {
      /* indexing */
      rhs = parse_expr(pr, 0);
      if (!expect(pr, T_RBRACKET, NULL)) {
        fprintf(stderr, "expected ']'\n");
        return NO_NODE;
      }
    } else if (op.type == T_DOT) {
      sbLexToken method_name = {0};
      if (!expect(pr, T_IDENTIFIER, &method_name)) {
        fprintf(stderr, "expected identifier after '.' token\n");
        return NO_NODE;
      }
      sbAst name = name_node(pr, method_name);
      printf("method name: %s\n", sbSymbol_name(method_name.symb));
      if (!expect(pr, T_LPAREN, NULL)) {
        fprintf(stderr, "expected opening-parenthesis after '.%s'\n", sbSymbol_name(method_name.symb));
        return NO_NODE;
      }
      // TODO handle commas
      sbAst params = parse_expr(pr, 0);
      if (!expect(pr, T_RPAREN, NULL)) {
        fprintf(stderr, "expected closing-parenthesis after '.%s(...'\n", sbSymbol_name(method_name.symb));
        return NO_NODE;
      }
      ast_type = AST_NODE_METHODCALL;
      rhs = seq_node(pr, AST_NODE_NEXT, name, params);
    } else {
      rhs = parse_expr(pr, infix->right_precedence);
    }

    if (ast_type == AST_NODE_OP) {
      lhs = binop_node(pr, infix->ast_op, lhs, rhs);
    } else {
      lhs = seq_node(pr, ast_type, lhs, rhs);
    }
  }

  return lhs;
}

static void print_ast_node(sbAst n, int indent) {
  printf("\n");
  for (int i = 0; i < indent; i++) {
    printf("  ");
  }

  if (n->type == AST_NODE_NAME) {
    printf("%s", sbSymbol_name(n->symb));
  } else {
    printf("(");
    if (n->type == AST_NODE_OP) {
      flag found = 0;
      for (int i = 0; i < NUM_SPELLINGS; i++) {
        if (op_spellings[i].ast_op == n->op.type) {
          printf("%s", op_spellings[i].name);
          found = 1;
          break;
        }
      }
      if (!found) printf("??");
      print_ast_node(n->op.left, indent + 1);
      if (n->op.right) {
        print_ast_node(n->op.right, indent + 1);
      }
    } else if (n->type == AST_NODE_FUNCCALL) {
      print_ast_node(n->seq.left, indent + 1);
      printf("(");
      print_ast_node(n->seq.right, indent + 1);
      printf(")");
    } else if (n->type == AST_NODE_METHODCALL) {
      print_ast_node(n->seq.left, indent + 1);
      printf(".");
      print_ast_node(n->seq.right->seq.left, indent + 1);
      printf("(");
      print_ast_node(n->seq.right->seq.right, indent + 1);
      printf(")");
    } else {
      printf("?<%d>", n->type);
    }

    printf("\n");
    for (int i = 0; i < indent; i++) {
      printf("  ");
    }
    printf(")");
  }
}

static sbAst do_parse(hParser pr) {
  sbAst program = parse_expr(pr, 0);
  print_ast_node(program, 0);
  return program;
}
