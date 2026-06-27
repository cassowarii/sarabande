#include "parser.h"

#include "parse/filereader.h"
#include "data/symbol.h"

static sbAst do_parse(hParser pr);

void sbParser_initialize(hParser pr) {
  *pr = (sbParser) {0};
  sbArena_initialize(&pr->node_arena, 32768);
  sbTokenQueue_initialize(&pr->input_queue, 8);
}

void sbParser_deinitialize(hParser pr) {
  sbTokenQueue_deinitialize(&pr->input_queue);
  sbArena_deinitialize(&pr->node_arena);
  *pr = (sbParser) {0};
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

typedef struct tokenspelling {
  sbTokenType type;
  const char *name;
} tokenspelling;

static sbAstNode NONE_SENTINEL_VALUE = {0};
static sbAst NO_NODE = &NONE_SENTINEL_VALUE;
static sbAstNode ERROR_SENTINEL_VALUE = { .type = AST_ERROR };
static sbAst ERROR_NODE = &ERROR_SENTINEL_VALUE;

static tokenspelling token_spellings[] = {
  { T_NEWLINE, "newline" },
  { T_EOF, "end-of-file" },
  { T_IDENTIFIER, "identifier" },
  { T_SYMBOL, "symbol" },
  { T_INTEGER, "integer" },
  { T_FLOAT, "floating-point literal" },
  { T_STRING, "string literal" },
  { T_rAND, "'and'" },
  { T_rAS, "'as'" },
  { T_rCASE, "'case'" },
  { T_rDEF, "'def'" },
  { T_rDO, "'do'" },
  { T_rELSE, "'else'" },
  { T_rFALSE, "'false'" },
  { T_rIF, "'if'" },
  { T_rIN, "'in'" },
  { T_rLET, "'let'" },
  { T_rMATCH, "'match'" },
  { T_rNIL, "'nil'" },
  { T_rNOT, "'not'" },
  { T_rOR, "'or'" },
  { T_rREPEAT, "'repeat'" },
  { T_rRETURN, "'return'" },
  { T_rTRUE, "'true'" },
  { T_rUNLESS, "'unless'" },
  { T_rUNTIL, "'until'" },
  { T_rWHEN, "'when'" },
  { T_rWHILE, "'while'" },
  { T_LPAREN, "'('" },
  { T_RPAREN, "')'" },
  { T_LBRACKET, "'['" },
  { T_RBRACKET, "']'" },
  { T_LBRACE, "'{'" },
  { T_RBRACE, "'}'" },
  { T_ASTERISK, "'*'" },
  { T_SLASH, "'/'" },
  { T_PLUS, "'+'" },
  { T_MINUS, "'-'" },
  { T_PERCENT, "'%'" },
  { T_PIPE, "'|'" },
  { T_DOT, "'.'" },
  { T_COMMA, "','" },
  { T_EQUALS, "'='" },
  { T_LESS, "'<'" },
  { T_GREATER, "'>'" },
  { T_COLON, "':'" },
  { T_SEMICOLON, "';'" },
  { T_BACKSLASH, "'\\'" },
  { T_SPACE, "' '" },
  { T_ARROW, "'->'" },
  { T_FATARROW, "'=>'" },
  { T_SQUIGARROW, "'~>'" },
  { T_BACKSQUIGARROW, "'<~'" },
  { T_COLONBRACE, "':{'" },
  { T_PAAMAYIM_NEKUDOTAYIM, "'::'" },
  { T_DOUBLEEQUALS, "'=='" },
  { T_DOUBLEMINUS, "'--'" },
  { T_DOUBLEPLUS, "'++'" },
  { T_DOUBLEASTERISK, "'**'" },
  { T_DOUBLESLASH, "'//'" },
  { T_DOUBLEPERCENT, "'%%'" },
  { T_DOUBLEGREATER, "'>>'" },
  { T_DOUBLELESS, "'<<'" },
  { T_MINUSEQUALS, "'-='" },
  { T_PLUSEQUALS, "'+='" },
  { T_ASTERISKEQUALS, "'*='" },
  { T_SLASHEQUALS, "'/='" },
  { T_PERCENTEQUALS, "'%='" },
  { T_LESSEQUALS, "'<='" },
  { T_GREATEREQUALS, "'>='" },
  { T_NOTEQUALS, "'!='" },
  { T_TWODOT, "'..'" },
  { T_ELLIPSIS, "'...'" },
};

const char *const TOKEN_SPELLING_UNKNOWN = "<unknown-token>";

static binop binops[] = {
  { T_PIPE, 6, 7, AST_OP_PIPE },
  { T_rOR, 10, 11, AST_OP_OR },
  { T_rAND, 20, 21, AST_OP_AND },
  { T_rIN, 25, 26, AST_OP_IN },
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
  { T_PAAMAYIM_NEKUDOTAYIM, 90, 91, AST_OP_SCOPE },
};

static unop unops[] = {
  { T_rNOT, 25, AST_OP_NOT },
  { T_PLUS, 85, AST_OP_UNPLUS },
  { T_MINUS, 85, AST_OP_UNMINUS },
};

const usize NUM_BINOPS = sizeof(binops) / sizeof(binops[0]);
const usize NUM_UNOPS = sizeof(unops) / sizeof(unops[0]);
const usize NUM_TOKEN_SPELLINGS = sizeof(token_spellings) / sizeof(token_spellings[0]);

static sbLexToken peek_ahead(hParser pr, usize count) {
  while (sbTokenQueue_size(&pr->input_queue) < count + 1) {
    sbTokenQueue_enqueue(&pr->input_queue, sbLexer_next(&pr->lexer));
  }

  return sbTokenQueue_at(&pr->input_queue, count);
}

static void fprint_token(FILE *out, sbLexToken t);
static sbLexToken next_token(hParser pr) {
  if (sbTokenQueue_size(&pr->input_queue) > 0) {
    sbTokenQueue_shift(&pr->input_queue);
  }

  pr->error_state = 0;

  sbLexToken t = peek_ahead(pr, 0);

  return t;
}

flag is_literal_token(sbTokenType type) {
  return type == T_IDENTIFIER
      || type == T_SYMBOL
      || type == T_INTEGER
      || type == T_FLOAT
      || type == T_STRING;
}

const char *token_spelling(sbTokenType type) {
  for (usize i = 0; i < NUM_TOKEN_SPELLINGS; i++) {
    if (token_spellings[i].type == type) {
      return token_spellings[i].name;
    }
  }
  return TOKEN_SPELLING_UNKNOWN;
}

static void fprint_token(FILE *out, sbLexToken t) {
  fprintf(out, "%s", token_spelling(t.type));

  if (t.type == T_IDENTIFIER) {
    fprintf(out, " '%s'", sbSymbol_name(t.symb));
  } else if (t.type == T_SYMBOL) {
    fprintf(out, " ':%s'", sbSymbol_name(t.symb));
  } else if (t.type == T_INTEGER) {
    fprintf(out, " '%d'", t.i);
  } else if (t.type == T_FLOAT) {
    fprintf(out, " '%g'", t.fl);
  }
}

static void fprint_repeat_char(FILE *out, int ch, usize length) {
  for (int i = 0; i < length; i++) {
    fputc(ch, out);
  }
}

static void fprint_left_error_margin(FILE *out, usize line_num) {
  /* indentation */
  if (line_num) {
    fprintf(out, "%6zu | ", line_num);
  } else {
    fprintf(out, "       | ");
  }
}

static sbAst syntax_error(hParser pr) {
  if (pr->error_state) return ERROR_NODE;

  sbLexToken unexpected_token = peek_ahead(pr, 0);
  if (unexpected_token.type == T_ERROR) {
    fprintf(stderr, "syntax error: invalid character");
  } else if (unexpected_token.type == T_WRONGBRACKET) {
    fprintf(stderr, "syntax error: mismatched bracket");
  } else if (unexpected_token.type == T_BADNUMBER) {
    fprintf(stderr, "syntax error: invalid number");
  } else if (unexpected_token.type == T_SEMICOLON && unexpected_token.invisible) {
    fprintf(stderr, "syntax error: unexpected end-of-line");
  } else if (unexpected_token.invisible) {
    fprintf(stderr, "syntax error");
  } else {
    fprintf(stderr, "syntax error: unexpected ");
    fprint_token(stderr, unexpected_token);
  }
  fprintf(stderr, " at line %d\n", unexpected_token.line);
  fprint_left_error_margin(stderr, 0);
  fprintf(stderr, "\n");
  fprint_left_error_margin(stderr, unexpected_token.line);
  sbFileReader_fprint_line_at(stderr, pr->lexer.scanner.file_reader, unexpected_token.line);
  fprint_left_error_margin(stderr, 0);
  fprint_repeat_char(stderr, ' ', unexpected_token.start_col - 1);
  fprint_repeat_char(stderr, '^', unexpected_token.end_col - unexpected_token.start_col + 1);
  fprintf(stderr, "\n\n");

  /* set error flag until we move to the next token so that syntax error
   * isn't printed multiple times */
  pr->error_state = TRUE;
  return ERROR_NODE;
}

static flag expect(hParser pr, sbTokenType type) {
  sbLexToken up_next = peek_ahead(pr, 0);
  if (up_next.type == type) {
    next_token(pr);
    return TRUE;
  } else {
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

static sbAst tri_node(hParser pr, sbAstType type, sbAst left, sbAst center, sbAst right) {
  sbAstNode n = (sbAstNode) {
    .type = type,
    .tri.left = left,
    .tri.center = center,
    .tri.right = right,
  };
  return new_node(pr, &n);
}

static sbAst wrap_node(hParser pr, sbAstType type, sbAst left) {
  sbAstNode n = (sbAstNode) {
    .type = type,
    .seq.left = left,
  };
  return new_node(pr, &n);
}

static sbAst atomic_node(hParser pr, sbAstType type) {
  sbAstNode n = {
    .type = type,
  };
  return new_node(pr, &n);
}

static sbAst name_node(hParser pr, sbLexToken token) {
  if (token.type != T_IDENTIFIER) {
    PANIC("can't create name node with token of type %d", token.type);
  }

  sbAstNode n = (sbAstNode) {
    .type = AST_NODE_NAME,
    .symb = token.symb,
  };
  return new_node(pr, &n);
}

static sbAst parse_expr(hParser pr, u8 min_precedence);
static sbAst parse_comma_exprs(hParser pr, sbAst after) {
  sbAst result = NO_NODE;
  sbAst *put_here = &result;
  sbAst expr;

  if (after) {
    *put_here = seq_node(pr, AST_NODE_MULTIVAL, after, NO_NODE);
    put_here = &(*put_here)->seq.right;
  }

  do {
    if (expect(pr, T_ELLIPSIS)) {
      sbAst splatted_expr = parse_expr(pr, 0);
      if (splatted_expr != NO_NODE) {
        /* "...something" */
        expr = unop_node(pr, AST_OP_SPLAT, splatted_expr);
      } else {
        /* plain "..." */
        expr = atomic_node(pr, AST_NODE_ELLIPSIS);
      }
    } else {
      expr = parse_expr(pr, 0);

      /* this 'break' allows empty () and trailing comma */
      if (expr == NO_NODE) break;
    }

    *put_here = seq_node(pr, AST_NODE_MULTIVAL, expr, NO_NODE);
    put_here = &(*put_here)->seq.right;
  } while (expect(pr, T_COMMA));

  return result;
}

static sbAst parse_name(hParser pr) {
  sbLexToken t = peek_ahead(pr, 0);
  if (t.type == T_IDENTIFIER) {
    next_token(pr);
    return name_node(pr, t);
  }
  return NO_NODE;
}

static sbAst parse_block(hParser pr);

/* https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html */
static sbAst parse_expr(hParser pr, u8 min_precedence) {
  sbLexToken t = peek_ahead(pr, 0);
  sbAst lhs = NO_NODE;
  if (t.type == T_rNIL) {
    next_token(pr);
    sbAstNode n = { .type = AST_VAL_NIL };
    lhs = new_node(pr, &n);
  } else if (t.type == T_rTRUE || t.type == T_rFALSE) {
    next_token(pr);
    sbAstNode n = { .type = AST_VAL_BOOLEAN, .i = (t.type == T_rTRUE) };
    lhs = new_node(pr, &n);
  } else if (t.type == T_INTEGER) {
    next_token(pr);
    sbAstNode n = { .type = AST_VAL_INT, .i = t.i };
    lhs = new_node(pr, &n);
  } else if (t.type == T_FLOAT) {
    next_token(pr);
    sbAstNode n = { .type = AST_VAL_FLOAT, .fl = t.fl };
    lhs = new_node(pr, &n);
  } else if (t.type == T_SYMBOL) {
    next_token(pr);
    sbAstNode n = { .type = AST_VAL_SYMBOL, .symb = t.symb };
    lhs = new_node(pr, &n);
  } else if (t.type == T_STRING) {
    next_token(pr);
    sbAstNode n = { .type = AST_VAL_STRING, .str = t.hstr };
    lhs = new_node(pr, &n);
  } else if (t.type == T_IDENTIFIER) {
    lhs = parse_name(pr);
  } else if (t.type == T_FATARROW) {
    next_token(pr);
    if (!expect(pr, T_LPAREN)) return syntax_error(pr);
    sbAst params = parse_comma_exprs(pr, NULL);
    if (!expect(pr, T_RPAREN)) return syntax_error(pr);
    sbAst body = parse_block(pr);
    return seq_node(pr, AST_VAL_FUNC, params, body);
  } else if (t.type == T_SQUIGARROW) {
    next_token(pr);
    if (!expect(pr, T_LPAREN)) return syntax_error(pr);
    sbAst params = parse_comma_exprs(pr, NULL);
    if (!expect(pr, T_RPAREN)) return syntax_error(pr);
    sbAst body = parse_block(pr);
    return seq_node(pr, AST_VAL_OBJ, params, body);
  } else if (t.type == T_LPAREN) {
    next_token(pr);
    lhs = parse_expr(pr, 0);
    if (!expect(pr, T_RPAREN)) return syntax_error(pr);
  } else {
    sbLexToken op = peek_ahead(pr, 0);
    unop *prefix = NULL;
    for (usize i = 0; i < NUM_UNOPS; i++) {
      if (unops[i].type == op.type) {
        prefix = &unops[i];
        break;
      }
    }

    if (!prefix) {
      /* thing we're looking at isn't an expression, but this might be ok */
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
    for (usize i = 0; i < NUM_BINOPS; i++) {
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
      rhs = parse_comma_exprs(pr, NULL);
      ast_type = AST_NODE_FUNCCALL;
      if (!expect(pr, T_RPAREN)) return syntax_error(pr);
    } else if (op.type == T_LBRACKET) {
      /* indexing */
      rhs = parse_expr(pr, 0);
      if (!expect(pr, T_RBRACKET)) return syntax_error(pr);
    } else if (op.type == T_DOT) {
      sbAst method_name = parse_name(pr);
      if (!expect(pr, T_LPAREN)) return syntax_error(pr);
      sbAst params = parse_comma_exprs(pr, NULL);
      if (!expect(pr, T_RPAREN)) return syntax_error(pr);
      ast_type = AST_NODE_METHODCALL;
      rhs = seq_node(pr, AST_NODE_NEXT, method_name, params);
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

static sbAst parse_stmt(hParser pr);
static sbAst parse_stmtseq(hParser pr);

static sbAst parse_block(hParser pr) {
  if (!expect(pr, '{')) return syntax_error(pr);

  sbAst result = parse_stmtseq(pr);

  if (!expect(pr, '}')) return syntax_error(pr);

  return result;
}

static sbAst parse_stmtseq(hParser pr) {
  sbAst result = NO_NODE;
  sbAst *put_here = &result;

  do {
    sbAst stmt = parse_stmt(pr);
    if (stmt == NO_NODE) {
      /* if failed to parse a statement, try eating an
       * additional semicolon first */
      if (expect(pr, ';')) continue;
      break;
    }
    *put_here = seq_node(pr, AST_NODE_SEQ, stmt, NO_NODE);
    put_here = &(*put_here)->seq.right;
  } while (expect(pr, ';'));

  return result;
}

static sbAst with_trailing_conditional(hParser pr, sbAst stmt) {
  sbAst result = stmt;

  if (expect(pr, T_rIF)) {
    sbAst condition = parse_expr(pr, 0);
    result = tri_node(pr, AST_NODE_IF, condition, stmt, NO_NODE);
  } else if (expect(pr, T_rUNLESS)) {
    sbAst condition = parse_expr(pr, 0);
    result = tri_node(pr, AST_NODE_IF, unop_node(pr, AST_OP_NOT, condition), stmt, NO_NODE);
  }

  return result;
}

static sbAst parse_stmt(hParser pr) {
  sbLexToken t = peek_ahead(pr, 0);
  if (t.type == T_rIF) {
    /* if condition1 { ... } else if condition2 { ... } else { ... } */
    next_token(pr);
    sbAst condition = parse_expr(pr, 0);
    sbAst then_body = parse_block(pr);
    sbAst else_body = NO_NODE;
    if (expect(pr, T_rELSE)) {
      sbLexToken t = peek_ahead(pr, 0);
      if (t.type == T_rIF) {
        /* else if ...as above... */
        else_body = parse_stmt(pr);
      } else {
        else_body = parse_block(pr);
      }
    }
    return tri_node(pr, AST_NODE_IF, condition, then_body, else_body);
  } else if (t.type == T_rUNLESS) {
    /* unless condition { ... } */
    next_token(pr);
    sbAst condition = parse_expr(pr, 0);
    /* won't permit unless..else. it is too confusing. unless can only have 1 block */
    sbAst unless_body = parse_block(pr);
    return tri_node(pr, AST_NODE_IF, unop_node(pr, AST_OP_NOT, condition), unless_body, NO_NODE);
  } else if (t.type == T_rWHILE) {
    /* while condition { ... } */
    next_token(pr);
    sbAst condition = parse_expr(pr, 0);
    sbAst body = parse_block(pr);
    return seq_node(pr, AST_NODE_WHILE, condition, body);
  } else if (t.type == T_rUNTIL) {
    /* until condition { ... } */
    next_token(pr);
    sbAst condition = parse_expr(pr, 0);
    sbAst body = parse_block(pr);
    return seq_node(pr, AST_NODE_WHILE, unop_node(pr, AST_OP_NOT, condition), body);
  } else if (t.type == T_rREPEAT) {
    next_token(pr);
    sbAst body = parse_block(pr);
    if (expect(pr, T_rWHILE)) {
      /* repeat { ... } while */
      sbAst condition = parse_expr(pr, 0);
      return seq_node(pr, AST_NODE_REPEAT, body, condition);
    } else if (expect(pr, T_rUNTIL)) {
      /* repeat { ... } until */
      sbAst condition = parse_expr(pr, 0);
      return seq_node(pr, AST_NODE_REPEAT, body, condition);
    } else {
      return syntax_error(pr);
    }
  } else if (t.type == T_rCASE) {
    /* case x, y { ... } */
    next_token(pr);
    sbAst values = parse_comma_exprs(pr, NULL);
    sbAst body = parse_block(pr);
    return seq_node(pr, AST_NODE_CASE, values, body);
  } else if (t.type == T_rMATCH) {
    /* match :x if j { ... } */
    next_token(pr);
    sbAst pattern = parse_comma_exprs(pr, NULL);
    sbAst guard_clause = NO_NODE;
    if (expect(pr, T_rIF)) {
      next_token(pr);
      guard_clause = parse_expr(pr, 0);
    } else if (expect(pr, T_rUNLESS)) {
      next_token(pr);
      guard_clause = parse_expr(pr, 0);
      guard_clause = unop_node(pr, AST_OP_NOT, guard_clause);
    }
    sbAst body = parse_block(pr);
    return tri_node(pr, AST_NODE_MATCH, pattern, guard_clause, body);
  } else if (t.type == T_rLET) {
    /* let a, b = ... */
    next_token(pr);
    sbAst bindings = parse_comma_exprs(pr, NULL);
    if (!expect(pr, T_EQUALS)) return syntax_error(pr);
    sbAst values = parse_comma_exprs(pr, NULL);
    return seq_node(pr, AST_NODE_LET, bindings, values);
  } else if (t.type == T_rDEF) {
    /* def a b, c { ... } */
    next_token(pr);
    sbAst name = parse_name(pr);
    if (!expect(pr, T_LPAREN)) return syntax_error(pr);
    sbAst params = parse_comma_exprs(pr, NULL);
    if (!expect(pr, T_RPAREN)) return syntax_error(pr);
    sbAst body = parse_block(pr);
    sbAst func_node = seq_node(pr, AST_VAL_FUNC, params, body);
    return seq_node(pr, AST_NODE_DEF, name, func_node);
  } else if (t.type == T_rRETURN) {
    /* return ... */
    next_token(pr);
    sbAst returned_val = parse_comma_exprs(pr, 0);
    sbAst return_node = wrap_node(pr, AST_NODE_RETURN, returned_val);
    /* return ... unless condition */
    return_node = with_trailing_conditional(pr, return_node);
    return return_node;
  } else {
    /* (statement that is just an expression) */
    sbAst expr = parse_expr(pr, 0);
    if (expr == NO_NODE) return NO_NODE;

    if (peek_ahead(pr, 0).type == ',') {
      /* (implicit return) a, b, c */
      next_token(pr);
      expr = parse_comma_exprs(pr, expr);
    }

    if (peek_ahead(pr, 0).type == '=') {
      /* a, b, c = 1, 2, 3 */
      next_token(pr);
      sbAst assigned_values = parse_comma_exprs(pr, 0);
      expr = seq_node(pr, AST_NODE_ASSIGN, expr, assigned_values);
    }

    if (expr != NO_NODE) {
      /* a, b, c = 1, 2, 3 (or other expr) if condition */
      expr = with_trailing_conditional(pr, expr);
    }

    return expr;
  }
}

static sbAst do_parse(hParser pr) {
  sbAst program = parse_stmtseq(pr);
  return program;
}
