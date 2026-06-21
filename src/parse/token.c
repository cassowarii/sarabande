#include "token.h"

void sbTokenQueue_initialize(hTokenQueue q, i16 initial_count) {
    sbBuffer_initialize(&q->buffer, initial_count * sizeof(sbLexToken));
}

void sbTokenQueue_enqueue(hTokenQueue q, sbLexToken t) {
    sbLexToken *where = sbBuffer_expand(&q->buffer, sizeof(sbLexToken));

    *where = t;
}

i32 sbTokenQueue_size(hTokenQueue q) {
    return q->buffer.size / sizeof(sbLexToken);
}

sbLexToken sbTokenQueue_shift(hTokenQueue q) {
    i32 size = sbTokenQueue_size(q);

    sbLexToken *token_list = (sbLexToken*)q->buffer.data;

    /* copy first entry */
    sbLexToken result = token_list[0];

    /* move everything down one */
    for (int i = 0; i < size - 1; i++) {
        token_list[i] = token_list[i + 1];
    }

    sbBuffer_shrink(&q->buffer, sizeof(sbLexToken));

    return result;
}

sbLexToken sbTokenQueue_at(hTokenQueue q, i32 index) {
    i32 size = sbTokenQueue_size(q);

    if (index >= size) {
        return (sbLexToken) {0};
    }

    sbLexToken *token_list = (sbLexToken*)q->buffer.data;

    return token_list[index];
}

void sbTokenQueue_deinitialize(hTokenQueue q) {
    sbBuffer_deinitialize(&q->buffer);
}
