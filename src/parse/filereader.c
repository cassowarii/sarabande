#include "filereader.h"

#include "mem/mem.h"

#define N_READBACK_LINES 3

/* this seems pointless right now as a wrapper for FILE* but later, we can make
 * this work to read from stdin in interactive mode as well, which will be easier
 * if we just have these few functions separate from the actual lexer code.
 * we may also want to have multiple files open for imports and stuff... */

struct sbFileReader {
    flag valid;
    i32 line;
    i32 col;
    i32 char_index;
    sbBuffer current_line;
    sbBuffer readback_lines[N_READBACK_LINES];
    FILE *file;
};

static void read_in_new_line(hFileReader r);

hFileReader sbFileReader_open(const char *filename) {
  hFileReader r = calloc(1, sizeof(struct sbFileReader));
  r->file = fopen(filename, "r");
  r->line = 1;
  r->char_index = 0;
  r->col = 1;
  if (!r->file) {
    perror("Error opening file for reading");
  } else {
    r->valid = 1;
    for (usize i = 0; i < N_READBACK_LINES; i++) {
      sbBuffer_initialize(&r->readback_lines[i], 128);
    }
    sbBuffer_initialize(&r->current_line, 128);
    read_in_new_line(r);
  }

  return r;
}

flag sbFileReader_ok(hFileReader r) {
    return r->valid;
}

int sbFileReader_peek(hFileReader r) {
  if (r->valid) {
    int c = ((int*)r->current_line.data)[r->char_index];
    return c;
  } else {
    return EOF;
  }
}

int sbFileReader_next(hFileReader r) {
  if (r->valid) {
    r->char_index ++;
    int c = sbFileReader_peek(r);
    if (c == '\t') {
      r->col += 4;
    } else if (c == 0) {
      read_in_new_line(r);
      r->line ++;
      r->char_index = 0;
      r->col = 1;
      c = sbFileReader_peek(r);
    } else {
      r->col ++;
    }
    return c;
  } else {
    return EOF;
  }
}

int sbFileReader_get_line(hFileReader r) {
  return r->line;
}

int sbFileReader_get_col(hFileReader r) {
  return r->col;
}

void sbFileReader_fprint_line_at(FILE *out, hFileReader r, int line_num) {
  isize readback_amount = r->line - line_num;
  sbBuffer *buffer;
  if (readback_amount == 0) {
    buffer = &r->current_line;
  } else if (readback_amount > 0 && readback_amount <= N_READBACK_LINES) {
    buffer = &r->readback_lines[N_READBACK_LINES - readback_amount];
  } else {
    fprintf(out, "<line not available>\n");
    return;
  }
  int *to_read = (int*)buffer->data;
  for (int i = 0; i < buffer->size / sizeof(int); i++) {
    if (to_read[i] == 0 || to_read[i] == '\n') break;
    if (to_read[i] == '\t') {
      fputs("    ", out);
    } else {
      fputc(to_read[i], out);
    }
  }
  fputc('\n', out);
}

void sbFileReader_close(hFileReader r) {
    if (r->valid) {
        fclose(r->file);
    }
    sbBuffer_deinitialize(&r->current_line);
    for (usize i = 0; i < N_READBACK_LINES; i++) {
      sbBuffer_deinitialize(&r->readback_lines[i]);
    }
    free(r);
}

/* --- */

static void read_in_new_line(hFileReader r) {
    if (!r->valid) return;

    /* recycle line buffers */
    sbBuffer oldest_line_buffer = r->readback_lines[0];
    for (int i = 0; i < N_READBACK_LINES - 1; i++) {
      r->readback_lines[i] = r->readback_lines[i + 1];
    }
    r->readback_lines[N_READBACK_LINES - 1] = r->current_line;
    r->current_line = oldest_line_buffer;
    sbBuffer_reset(&r->current_line);

    int c;
    do {
      c = fgetc(r->file);
      sbBuffer_append(&r->current_line, &c, sizeof(int));
    } while (c != '\n' && c != EOF);
    int zero = 0;
    sbBuffer_append(&r->current_line, &zero, sizeof(int));
}
