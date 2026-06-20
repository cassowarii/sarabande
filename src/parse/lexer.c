#include "lexer.h"

#include <string.h>
#include "mem/mem.h"

static void compute_next_token(hLexer lx);
static sbLexToken advance_output_queue(hLexer lx);

void sbLexer_initialize(hLexer lx, hFileReader fr) {
    sbBuffer_initialize(&lx->brackets_stack, 64);
    sbScanner_initialize(&lx->scanner, fr);
    memset(lx->input_queue, 0, LEXER_QUEUE_LENGTH * sizeof(sbLexToken));
    memset(lx->output_queue, 0, LEXER_QUEUE_LENGTH * sizeof(sbLexToken));
    lx->input_queue_length = 0;
    lx->output_queue_length = 0;
    lx->last_token_seen = (sbLexToken) {0};
    lx->n_brackets = 0;
    lx->brace_terminated_state = 0;
}

sbLexToken sbLexer_peek(hLexer lx) {
    if (lx->output_queue_length == 0) {
        compute_next_token(lx);
    }

    return lx->output_queue[0];
}

sbLexToken sbLexer_next(hLexer lx) {
    if (lx->output_queue_length == 0) {
        compute_next_token(lx);
    }

    return advance_output_queue(lx);
}

void sbLexer_deinitialize(hLexer lx) {
    sbScanner_deinitialize(&lx->scanner);
    sbBuffer_deinitialize(&lx->brackets_stack);
    lx->input_queue_length = 0;
    lx->output_queue_length = 0;
}

struct ReservedWord {
    const char *name;
    sbTokenType token_type;
};

struct ReservedWord reserved_words[] = {
    { "and", T_rAND },
    { "as", T_rAS },
    { "case", T_rCASE },
    { "def", T_rDEF },
    { "do", T_rDO },
    { "else", T_rELSE },
    { "if", T_rIF },
    { "let", T_rLET },
    { "in", T_rIN },
    { "not", T_rNOT },
    { "or", T_rOR },
    { "repeat", T_rREPEAT },
    { "return", T_rREPEAT },
    { "unless", T_rUNLESS },
    { "until", T_rUNTIL },
    { "when", T_rWHEN },
    { "while", T_rWHILE },
};

#define N_RESERVED_WORDS ((sizeof(reserved_words))/(sizeof(reserved_words[0])))

sbLexToken advance_token_queue(sbLexToken *q, int *length, char version) {
    if (*length == 0) {
        if (version == 'o') {
            PANIC("can't advance empty lexer output queue!");
        } else {
            PANIC("can't advance empty lexer input queue!");
        }
    }

    sbLexToken result = q[0];

    (*length)--;
    for (int i = 0; i < *length; i++) {
        if (i < LEXER_QUEUE_LENGTH - 1) {
            q[i] = q[i + 1];
        } else {
            q[i] = (sbLexToken) {0};
        }
    }

    q[*length] = (sbLexToken) {0};

    return result;
}

void enqueue_token(sbLexToken *q, int *length, sbLexToken token, char version) {
    if (*length == LEXER_QUEUE_LENGTH) {
        if (version == 'o') {
            PANIC("output token queue is full!");
        } else {
            PANIC("input token queue is full!");
        }
    }

    q[*length] = token;
    (*length)++;
}

static flag is_stackable(sbTokenType type);
static void stack_token(hLexer lx, sbLexToken token);
static flag is_closing_bracket(sbTokenType type);
static void unstack_visible_bracket(hLexer lx, sbLexToken closing_token);
static void unstack_sticky_invisible_parentheses(hLexer lx);

void enqueue_output_token(hLexer lx, sbLexToken token) {
    /* if leaving brackets, close invisible parentheses and remove us from
     * brackets stack here */
    if (is_closing_bracket(token.type) && !token.invisible) {
        unstack_visible_bracket(lx, token);
    }

    enqueue_token(lx->output_queue, &lx->output_queue_length, token, 'o');

    if (is_closing_bracket(token.type) && !token.invisible) {
        unstack_sticky_invisible_parentheses(lx);
    }

    /* track brackets, parentheses, braces etc */
    if (is_stackable(token.type)) {
        stack_token(lx, token);
    }
}

void enqueue_input_token(hLexer lx, sbLexToken token) {
    enqueue_token(lx->input_queue, &lx->input_queue_length, token, 'i');
}

sbLexToken advance_output_queue(hLexer lx) {
    while (lx->output_queue_length == 0) {
        compute_next_token(lx);
    }
    return advance_token_queue(lx->output_queue, &lx->output_queue_length, 'o');
}

sbLexToken advance_input_queue(hLexer lx) {
    if (lx->input_queue_length == 0) {
        enqueue_input_token(lx, sbScanner_next(&lx->scanner));
    }
    return advance_token_queue(lx->input_queue, &lx->input_queue_length, 'i');
}

sbLexToken input_peek_ahead(hLexer lx, int count) {
    /* 0 = next token; 1 = token after that; etc.
     * so if count is 0, length must be at least 1 */
    while (lx->input_queue_length <= count) {
        enqueue_input_token(lx, sbScanner_next(&lx->scanner));
    }

    return lx->input_queue[count];
}

flag is_literal(sbTokenType type) {
    return type == T_IDENTIFIER
        || type == T_STRING
        || type == T_INTEGER
        || type == T_FLOAT
        || type == T_SYMBOL;
}

flag insert_semicolon_after(sbTokenType type) {
    return is_literal(type)
        || type == T_RPAREN
        || type == T_RBRACKET
        || type == T_RBRACE
        || type == T_DOUBLEPLUS
        || type == T_DOUBLEMINUS;
}

flag cancel_semicolon_before(sbTokenType type) {
    return type == T_DOT
        || type == T_PIPE;
}

/* identifier + space + one of these gets a ( inserted */
flag can_only_start_expression(sbTokenType type, flag brace_terminated_state) {
    return is_literal(type)
        || type == T_LPAREN
        || type == T_LBRACKET
        || type == T_COLONBRACE
        || type == T_FATARROW
        || type == T_rNOT
        || type == T_rDO
        || (type == T_LBRACE && !brace_terminated_state); // danger will robinson!
}

/* identifier + space + one of these + NO SPACE gets a ( inserted but not if
 * surrounded by spaces or neither space */
flag maybe_can_start_expression(sbTokenType type) {
    return type == T_PLUS
        || type == T_MINUS;
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
        || type == T_rWHEN;
}

/* when in brace-terminated state, one of these must precede a { character
 * in order to exit brace-terminated state */
flag can_end_expression(sbTokenType type) {
    return is_literal(type)
        || type == T_RPAREN
        || type == T_RBRACKET
        || type == T_RBRACE;
}

/*static flag is_reserved_word(sbTokenType type) {
    for (int i = 0; i < N_RESERVED_WORDS; i++) {
        if (type == reserved_words[i].token_type) return 1;
    }
    return 0;
}*/

static void stack_token(hLexer lx, sbLexToken token) {
    /* track which invisible and visible brackets we've seen, so we can close invisible brackets
     * at appropriate times */
    char *stack_top = sbBuffer_expand(&lx->brackets_stack, 1);
    if (token.invisible == 1 && token.type == '(') {
        *stack_top = 'G';
    } else if (token.invisible == 2 && token.type == '(') {
        /* this is a special invisible () that gets wrapped around something like "map:{ ... }" */
        *stack_top = 'H';
    } else if (token.type == '(' || token.type == '[' || token.type == '{') {
        *stack_top = token.type;
    } else if (token.type == T_COLONBRACE) {
        *stack_top = ':';
    } else {
        *stack_top = token.type;
    }
}

static void unstack_one_invisible_parenthesis(hLexer lx) {
    if (lx->brackets_stack.size == 0) return;

    char *stack_top = &lx->brackets_stack.data[lx->brackets_stack.size - 1];
    if (lx->brackets_stack.size > 0 && (*stack_top == 'G' || *stack_top == 'H')) {
        sbLexToken invisible_rparen = { .type = T_RPAREN, .invisible = 1 };
        enqueue_output_token(lx, invisible_rparen);
        lx->brackets_stack.size --;
    }
}

static void unstack_invisible_parentheses_of_type(hLexer lx, char type) {
    /* this happens when a real bracket closes, and also at the end of a line */
    if (lx->brackets_stack.size == 0) return;

    char *stack_top = &lx->brackets_stack.data[lx->brackets_stack.size - 1];
    while (lx->brackets_stack.size > 0 && *stack_top == type) {
        sbLexToken invisible_rparen = { .type = T_RPAREN, .invisible = 1 };
        enqueue_output_token(lx, invisible_rparen);
        lx->brackets_stack.size --;
        stack_top = &lx->brackets_stack.data[lx->brackets_stack.size - 1];
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

static void unstack_visible_bracket(hLexer lx, sbLexToken closing_token) {
    unstack_all_invisible_parentheses(lx);

    if (lx->brackets_stack.size != 0) {
        char *stack_top = &lx->brackets_stack.data[lx->brackets_stack.size - 1];
        if (closing_token.type == T_RBRACE) {
            if (*stack_top == '{' || *stack_top == ':') {
                lx->brackets_stack.size --;
                return;
            }
        } else if (closing_token.type == T_RBRACKET) {
            if (*stack_top == '[') {
                lx->brackets_stack.size --;
                return;
            }
        } else if (closing_token.type == T_RPAREN) {
            if (*stack_top == '(') {
                lx->brackets_stack.size --;
                return;
            }
        }
    }

    if (lx->brackets_stack.size != 0) {
        fprintf(stderr, "syntax error: mismatched bracket ('%c' for '%c')\n", lx->brackets_stack.data[lx->brackets_stack.size - 1], closing_token.type);
    } else {
        fprintf(stderr, "syntax error: mismatched bracket\n");
    }
    enqueue_output_token(lx, (sbLexToken) { .type = T_ERROR });
}

void compute_next_token(hLexer lx) {
    sbLexToken token = advance_input_queue(lx);

    /* if we receive an identifier, check if it is a reserved word or not */
    if (token.type == T_IDENTIFIER) {
        for (int i = 0; i < N_RESERVED_WORDS; i++) {
            if (!sbstrncmp(reserved_words[i].name, token.str, token.size + 1)) {
                token.type = reserved_words[i].token_type;
                break;
            }
        }
    }

    if (lx->brace_terminated_state && can_end_expression(lx->last_token_seen.type) && token.type == T_LBRACE) {
        /* TODO: I think this will fail in some obscure case, like where you have a '=> a, b { ... }' inside
         * of the top of an `if'. Annoying. We probably need to track brace_terminated_state for each level
         * of the bracket-stack individually, so that when we exit from a { } inside a block header we know
         * that we are still actually in brace_terminated_state. I think that should work. But I need to think
         * about the specifics a little more. */
        /* The condition for leaving brace-terminated state is correct (end-of-expression token followed
         * by opening brace) but brace-terminated state can come back in some sneaky scenarios after
         * a situation where we weren't in it before. */
        unstack_all_invisible_parentheses(lx);
        lx->brace_terminated_state = 0;
    }

    if (begins_brace_terminated_state(token.type)) {
        lx->brace_terminated_state = 1;
    }

    if (token.type != T_SPACE && token.type != T_NEWLINE) {
        enqueue_output_token(lx, token);
    }

    sbLexToken invisible_lparen = { .type = T_LPAREN, .invisible = 1 };
    if (token.type == T_IDENTIFIER && input_peek_ahead(lx, 0).type == T_COLONBRACE) {
        /* identifier followed by colon-brace with no space between gets this special invisible bracket */
        /* invisible = 2 makes it a sticky H bracket */
        invisible_lparen.invisible = 2;
        enqueue_output_token(lx, invisible_lparen);
    } else if (token.type == T_IDENTIFIER) {
        /* otherwise, if it's still an identifier and not a reserved word, we might need to insert a
         * magic ( into the output stream. so look ahead at what's coming up. */
        if (lx->last_token_seen.type == T_DOT) {
            /* an identifier after a dot always gets an invisible parentheses after it,
             * unless there is a visible parentheses immediately after it. */
            if (input_peek_ahead(lx, 0).type != T_LPAREN) {
                enqueue_output_token(lx, invisible_lparen);

                int space_offset = 0;
                if (input_peek_ahead(lx, 0).type == T_SPACE) {
                    /* in this case, we don't care if we have a space after us. */
                    space_offset = 1;
                }

                sbTokenType next_type = input_peek_ahead(lx, space_offset).type;
                sbTokenType next_next_type = input_peek_ahead(lx, space_offset + 1).type;

                if (maybe_can_start_expression(next_type)) {
                    /* this is something like 'a.b( +c' or 'a.b( + c'. if there is a space
                     * after the operator, or not before the operator, add an invisible ),
                     * otherwise don't because that's the 'a.b( +c' case */
                    if (next_next_type == T_SPACE || space_offset == 0) {
                        unstack_one_invisible_parenthesis(lx);
                    }
                } else if (!can_only_start_expression(next_type, lx->brace_terminated_state)) {
                    /* ok, so this means that we just inserted a ( in some situation like
                     * 'a.b(.c' or 'a.b(, c' or 'a.b( % 3' -- in this situation, we need to also
                     * add a matching invisible right parenthesis immediately. */
                    unstack_one_invisible_parenthesis(lx);
                }
                /* if can_only_start_expression(next_type), then we don't want to add an invisible
                 * right parenthesis because it must be a parameter. */
            }
        } else if (input_peek_ahead(lx, 0).type == T_SPACE) {
            /* suspicious... tell me more */
            if (can_only_start_expression(input_peek_ahead(lx, 1).type, lx->brace_terminated_state)) {
                /* ah ! yes. insert a magic ( into the stream. */
                enqueue_output_token(lx, invisible_lparen);
            } else if (maybe_can_start_expression(input_peek_ahead(lx, 1).type)) {
                /* check if it also has a space after it. if it does NOT, then
                 * we're looking at something like "a +b", so put a ( in */
                if (input_peek_ahead(lx, 2).type != T_SPACE) {
                    enqueue_output_token(lx, invisible_lparen);
                }
            }
        }
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
                sbLexToken invisible_semicolon = { .type = T_SEMICOLON, .invisible = 1 };
                enqueue_output_token(lx, invisible_semicolon);
                lx->last_token_seen = invisible_semicolon;
            }
        }
    }
}
