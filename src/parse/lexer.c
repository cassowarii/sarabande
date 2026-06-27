#include "lexer.h"

#include <string.h>
#include "mem/mem.h"

/* OK so this is a module that takes in a series of tokens output by the scanner and modifies
 * the token stream before it gets passed to the parser. In particular, this replaces the
 * spaces and newlines sent from the scanner with various configurations of parentheses and
 * semicolons which are inferred from the structure of the code. Note there is no semantic
 * or syntactic feedback hack here: parentheses and semicolons are inserted solely based on
 * the local structure of the code (plus a small bit of lexer state), not whether things parse
 * correctly (a la JS semicolons) or which functions are in scope (a la Ruby function calls.)
 * It's a similar idea to how the automatic semicolon insertion works in Go. It also allows
 * you to provide parentheses and semicolons manually as long as they are in the right place.
 * (a (b).c gets parsed as a((b).c), but a(b).c gets parsed as written, like Ruby) */

/* This module is also responsible for detecting whether identifiers returned by the scanner
 * are reserved words or not, and if they are, it will convert them to the appropriate token
 * type. */

/* We have input and output queues to buffer input that we receive from the scanner (to allow
 * us to look a few tokens ahead temporarily) and to allow us to output more than one token
 * at a time per input token from the scanner (good as we're inserting a bunch of stuff into
 * the token stream.) */

/* I called this module 'lexer.c', but 'scanner.c' is maybe more traditionally what people
 * think of as a lexer. This kind of mildly-context-sensitive-but-not-parsing thing that sits
 * in between the lexer and the parser I don't know if it has a traditional name, but I think
 * it's kind of a cool idea! A three-layer parser instead of a two-layer parser. */

static void compute_next_token(hLexer lx);
static sbLexToken advance_output_queue(hLexer lx);

void sbLexer_initialize(hLexer lx, hFileReader fr) {
    sbBuffer_initialize(&lx->brackets_stack, 64);
    sbScanner_initialize(&lx->scanner, fr);
    sbTokenQueue_initialize(&lx->input_queue, 8);
    sbTokenQueue_initialize(&lx->output_queue, 8);
    lx->last_token_seen = (sbLexToken) {0};
}

sbLexToken sbLexer_peek(hLexer lx) {
    if (sbTokenQueue_size(&lx->output_queue) == 0) {
        compute_next_token(lx);
    }

    return sbTokenQueue_at(&lx->output_queue, 0);
}

sbLexToken sbLexer_next(hLexer lx) {
    if (sbTokenQueue_size(&lx->output_queue) == 0) {
        compute_next_token(lx);
    }

    return advance_output_queue(lx);
}

void sbLexer_deinitialize(hLexer lx) {
    sbScanner_deinitialize(&lx->scanner);
    sbBuffer_deinitialize(&lx->brackets_stack);
    sbTokenQueue_deinitialize(&lx->input_queue);
    sbTokenQueue_deinitialize(&lx->output_queue);
}

static flag is_stackable(sbTokenType type);
static void stack_token(hLexer lx, sbLexToken token);
static flag is_closing_bracket(sbTokenType type);
static void unstack_visible_bracket(hLexer lx, sbLexToken closing_token);
static void unstack_sticky_invisible_parentheses(hLexer lx);

static void enqueue_output_token(hLexer lx, sbLexToken token) {
    /* if leaving brackets, close invisible parentheses and remove us from
     * brackets stack here */
    if (is_closing_bracket(token.type) && !token.invisible) {
        unstack_visible_bracket(lx, token);
    }

    if (token.line == 0) {
      /* fake tokens that are inserted get their location set as just after the
       * last token seen */
      token.line = lx->last_token_seen.line;
      token.start_col = lx->last_token_seen.end_col;
      token.end_col = lx->last_token_seen.end_col + 1;
    }

    sbTokenQueue_enqueue(&lx->output_queue, token);

    if (is_closing_bracket(token.type) && !token.invisible) {
        unstack_sticky_invisible_parentheses(lx);
    }

    /* track brackets, parentheses, braces etc */
    if (is_stackable(token.type)) {
        stack_token(lx, token);
    }
}

static void enqueue_input_token(hLexer lx, sbLexToken token) {
    sbTokenQueue_enqueue(&lx->input_queue, token);
}

static sbLexToken advance_output_queue(hLexer lx) {
    while (sbTokenQueue_size(&lx->output_queue) == 0) {
        compute_next_token(lx);
    }

    return sbTokenQueue_shift(&lx->output_queue);
}

static sbLexToken advance_input_queue(hLexer lx) {
    if (sbTokenQueue_size(&lx->input_queue) == 0) {
        enqueue_input_token(lx, sbScanner_next(&lx->scanner));
    }

    return sbTokenQueue_shift(&lx->input_queue);
}

static sbLexToken input_peek_ahead(hLexer lx, int count) {
    /* 0 = next token; 1 = token after that; etc.
     * so if count is 0, length must be at least 1 */
    while (sbTokenQueue_size(&lx->input_queue) <= count) {
        enqueue_input_token(lx, sbScanner_next(&lx->scanner));
    }

    return sbTokenQueue_at(&lx->input_queue, count);
}

static flag is_literal(sbTokenType type) {
    return type == T_IDENTIFIER
        || type == T_STRING
        || type == T_INTEGER
        || type == T_FLOAT
        || type == T_SYMBOL
        || type == T_rTRUE
        || type == T_rFALSE;
}

static flag insert_semicolon_after(sbTokenType type) {
    return is_literal(type)
        || type == T_RPAREN
        || type == T_RBRACKET
        || type == T_RBRACE
        || type == T_DOUBLEPLUS
        || type == T_DOUBLEMINUS;
}

static flag cancel_semicolon_before(sbTokenType type) {
    return type == T_DOT
        || type == T_PIPE;
}

/* identifier + space + one of these gets a ( inserted */
static flag can_only_start_expression(sbTokenType type, flag brace_terminated_state) {
    return is_literal(type)
        || type == T_LPAREN
        || type == T_LBRACKET
        || type == T_COLONBRACE
        || type == T_FATARROW
        || type == T_SQUIGARROW
        || type == T_rNOT
        || type == T_rDO
        || (type == T_LBRACE && !brace_terminated_state); // danger will robinson!
}

/* identifier + space + one of these + NO SPACE gets a ( inserted but not if
 * surrounded by spaces or neither space */
static flag maybe_can_start_expression(sbTokenType type) {
    return type == T_PLUS
        || type == T_MINUS;
}

/* can come after something like a ) and still be the beginning of a function
 * parameter (notably this isn't true of opening square bracket because it's
 * the indexing operator; there's a difference between a(b) [c] and a(b)[c]
 * lparen is also banned here because if an lparen is after some expression
 * without a space, then it's a function call, not an expr-starting paren
 * (e.g.: a(b)(c + 3) should stay a(b)(c + 3), not become a(b)((c + 3)) */
static flag can_start_expression_without_space(sbTokenType type, flag brace_terminated_state) {
    return can_only_start_expression(type, brace_terminated_state)
        && type != T_LBRACKET
        && type != T_LPAREN;
}

static flag is_stackable(sbTokenType type) {
    return type == T_LPAREN
        || type == T_LBRACKET
        || type == T_LBRACE
        || type == T_COLONBRACE;
}

static flag is_closing_bracket(sbTokenType type) {
    return type == T_RPAREN
        || type == T_RBRACKET
        || type == T_RBRACE;
}

/* header keywords that introduce a block and go until a { */
static flag begins_brace_terminated_state(sbTokenType type) {
    return type == T_rDEF
        || type == T_rIF
        || type == T_rUNLESS
        || type == T_rWHILE
        || type == T_rUNTIL
        || type == T_rDO
        || type == T_rREPEAT
        || type == T_rCASE
        || type == T_rWHEN
        || type == T_FATARROW /* fat arrow => a, b { ... } introduces block header also */
        || type == T_SQUIGARROW; /* squiggle arrow ~> a, b { ... } introduces block header also */
}

/* things we can potentially insert a ( after because they may be a call */
static flag can_end_expression(sbTokenType type) {
    return type == T_IDENTIFIER
        || type == T_RPAREN
        || type == T_RBRACKET
        || type == T_RBRACE;
}

/* when in brace-terminated state, one of these must precede a { character
 * in order to exit brace-terminated state */
static flag block_header_can_end_after(sbTokenType type) {
    return can_end_expression(type)
        || is_literal(type)
        || type == T_FATARROW
        || type == T_SQUIGARROW
        || type == T_rCASE
        || type == T_rDO;
}

/* these are operators that look weird if they get captured inside
 * of parentheses. */
static flag close_invisible_parens_before(sbTokenType type) {
    return type == T_rAND
        || type == T_rOR
        || type == T_DOUBLEEQUALS
        || type == T_NOTEQUALS
        || type == T_LESS
        || type == T_GREATER
        || type == T_LESSEQUALS
        || type == T_GREATEREQUALS
        || type == T_PIPE;
}

/*static flag is_reserved_word(sbTokenType type) {
    for (int i = 0; i < N_RESERVED_WORDS; i++) {
        if (type == reserved_words[i].token_type) return 1;
    }
    return 0;
}*/

static char brackets_stack_top(hLexer lx) {
    if (lx->brackets_stack.size == 0) return 0;

    return lx->brackets_stack.data[lx->brackets_stack.size - 1];
}

static char brackets_stack_pop(hLexer lx) {
    if (lx->brackets_stack.size == 0) return 0;

    char *result = sbBuffer_shrink(&lx->brackets_stack, 1);

    return *result;
}

static void brackets_stack_push(hLexer lx, char c) {
    char *stack_top = sbBuffer_expand(&lx->brackets_stack, 1);
    *stack_top = c;
}

static void stack_token(hLexer lx, sbLexToken token) {
    /* track which invisible and visible brackets we've seen, so we can close invisible brackets
     * at appropriate times */
    if (token.invisible == 1 && token.type == '(') {
        brackets_stack_push(lx, 'G');
    } else if (token.invisible == 2 && token.type == '(') {
        /* this is a special invisible () that gets wrapped around something like "map:{ ... }" */
        brackets_stack_push(lx, 'H');
    } else if (token.type == '(' || token.type == '[' || token.type == '{') {
        brackets_stack_push(lx, token.type);
    } else if (token.type == T_COLONBRACE) {
        brackets_stack_push(lx, ':');
    } else {
        brackets_stack_push(lx, token.type);
    }
}

static void unstack_one_invisible_parenthesis(hLexer lx) {
    if (lx->brackets_stack.size == 0) return;

    char top = brackets_stack_top(lx);
    if (top == 'G' || top == 'H') {
        sbLexToken invisible_rparen = { .type = T_RPAREN, .invisible = 1 };
        enqueue_output_token(lx, invisible_rparen);
        brackets_stack_pop(lx);
    }
}

static void unstack_invisible_parentheses_of_type(hLexer lx, char type) {
    /* this happens when a real bracket closes, and also at the end of a line */
    if (lx->brackets_stack.size == 0) return;

    char top = brackets_stack_top(lx);
    while (lx->brackets_stack.size > 0 && top == type) {
        sbLexToken invisible_rparen = { .type = T_RPAREN, .invisible = 1 };
        enqueue_output_token(lx, invisible_rparen);
        brackets_stack_pop(lx);
        top = brackets_stack_top(lx);
    }
}

static void unstack_all_invisible_parentheses(hLexer lx) {
    unstack_invisible_parentheses_of_type(lx, 'G');
}

static void unstack_sticky_invisible_parentheses(hLexer lx) {
    /* handling special case of 'map:{ ... }.whatever' --> 'map(:{ ... }).whatever .
     * normally this wouldn't work except we add this sticky "H" bracket when there
     * is no space before :{. so *after* right curly brace we check if there are any
     * of these to close too */
    /* i don't know what G and H stand for. i think "ghost" and "hidden" */
    unstack_invisible_parentheses_of_type(lx, 'H');
}

static flag is_in_brace_terminated_state(hLexer lx) {
    /* ignoring invisible parentheses, is the top thing on the stack 'B'? */
    /* this allows us to put braces freely inside, say, parentheses, even
     * in the header of an if, but if we close the parentheses, then we are back
     * in brace-terminated-state land. */
    if (lx->brackets_stack.size == 0) return 0;

    int index = lx->brackets_stack.size - 1;
    char c = lx->brackets_stack.data[index];
    while (index > 0 && (c == 'G' || c == 'H')) {
        index --;
        c = lx->brackets_stack.data[index];
    }

    return c == 'B';
}

static void unstack_visible_bracket(hLexer lx, sbLexToken closing_token) {
    unstack_all_invisible_parentheses(lx);

    if (lx->brackets_stack.size != 0) {
        char top = brackets_stack_top(lx);
        if (closing_token.type == T_RBRACE) {
            if (top == '{' || top == ':') {
                brackets_stack_pop(lx);
                return;
            }
        } else if (closing_token.type == T_RBRACKET) {
            if (top == '[') {
                brackets_stack_pop(lx);
                return;
            }
        } else if (closing_token.type == T_RPAREN) {
            if (top == '(') {
                brackets_stack_pop(lx);
                return;
            }
        }
    }

    enqueue_output_token(lx, (sbLexToken) { .type = T_WRONGBRACKET });
}

static void compute_next_token(hLexer lx) {
    sbLexToken token = advance_input_queue(lx);
    sbLexToken invisible_lparen = { .type = T_LPAREN, .invisible = 1 };

    if (token.type != T_SPACE
            && token.type != T_NEWLINE
            && token.type != T_LPAREN
            && (lx->last_token_seen.type == T_FATARROW || lx->last_token_seen.type == T_SQUIGARROW)) {
        /* => a ... or => { ... will always get an invisible lparen after the => .
         * (not caring about whether or not there is a space in this case.)
         * The brace-terminated state thing should handle closing this for us. */
        enqueue_output_token(lx, invisible_lparen);
    }

    if (is_in_brace_terminated_state(lx)
            && block_header_can_end_after(lx->last_token_seen.type)
            && token.type == T_LBRACE) {
        /* The condition for leaving brace-terminated state is end-of-expression token followed
         * by opening brace. This allows us to still leave out parentheses in certain cases, like
         * if a == { ... . */
        /* For => { ...  syntax, this happens immediately after the above: => { gets converted
         * to => () { . */
        unstack_all_invisible_parentheses(lx);
        brackets_stack_pop(lx); /* remove 'B' state from bracket stack */
    }

    if (begins_brace_terminated_state(token.type)) {
        brackets_stack_push(lx, 'B');
    }

    if (close_invisible_parens_before(token.type)) {
        unstack_all_invisible_parentheses(lx);
    }

    /* --- HERE IS WHERE THE TOKEN ACTUALLY GETS OUTPUT TO THE STREAM --- */
    /* Everything before this gets put into the stream ahead of this token. */
    /* Everything after this comes after the token. */
    if (token.type != T_SPACE && token.type != T_NEWLINE) {
        enqueue_output_token(lx, token);
    }

    /* this means we can insert a function call paren even if there is no space next
     * e.g. a(b){ c: d } can become a(b)({ c: d }). but a(b)[c] doesn't become a(b)([c]) */
    flag insert_paren_no_space = token.type != T_IDENTIFIER && can_start_expression_without_space(
        input_peek_ahead(lx, 0).type, is_in_brace_terminated_state(lx)
    );

    if (can_end_expression(token.type) && input_peek_ahead(lx, 0).type == T_COLONBRACE) {
        /* end-of-expr followed by colon-brace with no space between gets this special
         * invisible bracket */
        /* invisible = 2 makes it a sticky H bracket */
        invisible_lparen.invisible = 2;
        enqueue_output_token(lx, invisible_lparen);
    } else if (can_end_expression(token.type)) {
        /* otherwise, if it can end an expression (in particular if it's still an identifier and not a
         * reserved word), we might need to insert a magic ( into the output stream. so look ahead at
         * what's coming up. */

        /* (We don't do this in 'brace terminated states', e.g. between the keyword and brace of if ... { .
         * The reason is that something like "if x < a {" is ambiguous and ordinarily would parse with a as
         * a function call taking a hash: "if x < a({ ...". However, this is obviously stupid when we're in
         * the header of an if. So, if we're in "brace-terminated state" mode (basically, if we're inside a
         * block header), we always assume { after an identifier opens a block, rather than parsing it as a
         * function parameter that is a hash. However, the brace terminated state logic being stored in the
         * bracket stack means that we can wrap values that start with braces in parentheses manually if we
         * really need them for some reason. */
        int space_offset = 0;
        if (input_peek_ahead(lx, 0).type == T_SPACE) {
            /* sometimes, we don't care if we have a space after us. */
            space_offset = 1;
        }

        if (lx->last_token_seen.type == T_DOT && token.type == T_IDENTIFIER) {
            /* an identifier after a dot always gets an invisible parentheses after it,
             * unless there is a visible parentheses immediately after it. */
            if (input_peek_ahead(lx, 0).type != T_LPAREN) {
                enqueue_output_token(lx, invisible_lparen);

                sbTokenType next_type = input_peek_ahead(lx, space_offset).type;
                sbTokenType next_next_type = input_peek_ahead(lx, space_offset + 1).type;

                if (maybe_can_start_expression(next_type)) {
                    /* this is something like 'a.b( +c' or 'a.b( + c'. if there is a space
                     * after the operator, or not before the operator, add an invisible ),
                     * otherwise don't because that's the 'a.b( +c' case */
                    if (next_next_type == T_SPACE || space_offset == 0) {
                        unstack_one_invisible_parenthesis(lx);
                    }
                } else if (!can_only_start_expression(next_type, is_in_brace_terminated_state(lx))) {
                    /* ok, so this means that we just inserted a ( in some situation like
                     * 'a.b(.c' or 'a.b(, c' or 'a.b( % 3' -- in this situation, we need to also
                     * add a matching invisible right parenthesis immediately. */

                    /* In brace-terminated state, { doesn't count as can_only_start_expression.
                     * So this applies before { as well: normally 'a.b {' --> 'a.b({', but
                     * inside block headers 'a.b {' --> 'a.b() {' */
                    unstack_one_invisible_parenthesis(lx);
                }
                /* if can_only_start_expression(next_type), then we don't want to add an invisible
                 * right parenthesis because it must be a parameter. */
            }
        } else if (input_peek_ahead(lx, 0).type == T_SPACE || insert_paren_no_space) {
            /* suspicious... tell me more */
            if (can_only_start_expression(input_peek_ahead(lx, space_offset).type, is_in_brace_terminated_state(lx))) {
                /* ah ! yes. insert a magic ( into the stream. */

                /* as above, { isn't can_only_start_expression when in brace-terminated state.
                 * In this case the implication is that normally 'a {' gets turned into 'a({',
                 * but inside the top of an if or some such, it stays as 'a {'. */
                enqueue_output_token(lx, invisible_lparen);
            } else if (maybe_can_start_expression(input_peek_ahead(lx, space_offset).type)) {
                /* check if it also has a space after it. if it does NOT, then
                 * we're looking at something like "a +b", so put a ( in */
                if (input_peek_ahead(lx, space_offset + 1).type != T_SPACE) {
                    enqueue_output_token(lx, invisible_lparen);
                }
            }
        }
    }

    if (token.line == 0) {
        token.line = lx->last_token_seen.line;
        token.start_col = lx->last_token_seen.start_col;
        token.end_col = lx->last_token_seen.end_col;
    }

    if (token.type != T_SPACE && token.type != T_NEWLINE) {
        lx->last_token_seen = token;
    }

    /* automatic semicolon insertion: insert semicolons at the end of lines if we last saw
     * an identifier, literal, ++ or --, ), ], or }) -- except if at the next line starts
     * with a dot or a pipe */

    /* also close invisible parentheses in this situation even if the next line starts with
     * a dot or a pipe */
    if (token.type == T_NEWLINE) {
        if (insert_semicolon_after(lx->last_token_seen.type)) {
            unstack_all_invisible_parentheses(lx);
            sbLexToken after_newline = input_peek_ahead(lx, 0);

            if (!cancel_semicolon_before(after_newline.type)) {
                sbLexToken invisible_semicolon = {
                  .type = T_SEMICOLON,
                  .invisible = 1,
                  .line = lx->last_token_seen.line,
                  .start_col = lx->last_token_seen.start_col,
                  .end_col = lx->last_token_seen.end_col,
                };
                enqueue_output_token(lx, invisible_semicolon);
                lx->last_token_seen = invisible_semicolon;
            }
        }
    }
}
