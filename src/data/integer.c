#include "integer.h"

#include "vm/exec.h"
#include "data/symbol.h"
#include "data/string.h"

#define METHOD_IS(name) (!sbstrncmp(method_name, name, sizeof(name)))

#define BIGINT_PIECE_MAX 1000000000

/* reserve high bit for sign bit.
 * second-highest bit marks this as a handle to a bigint, which
 * means normally integers have to be less than +/- 2^62. */
#define FLAG_BIGINT (1LL << 62)

#define NUM_PER_BLOCK 256

typedef u32 Piece;

/* TODO don't want to have these global i think */
sbPool g_bigint_pool;

typedef struct bigint {
  flag allocated : 1;
  flag sign_bit : 1;
  hInteger handle;
  sbBuffer buf;
} bigint;

flag is_bigint(hInteger n);
static usize int_size(bigint *bi);
static i8 int_sign(bigint *bi, hInteger h);
static void set_bigint_size(bigint *i, usize new_size);
static bigint *new_bigint_to_fit(bigint *a, bigint *b);
static bigint *find_bigint_for_handle(hInteger handle);
static bigint *new_bigint(usize initial_size);
static u64 int_piece(bigint *bi, hInteger h, usize index);
static void trim_bigint(bigint *bi);

void sbInteger_sys_init() {
  sbPool_initialize(&g_bigint_pool, sizeof(bigint), NUM_PER_BLOCK);
}

void sbInteger_sys_deinit() {
  //sbPool_deinitialize(&g_bigint_pool);
}

hInteger sbInteger_new(i64 value) {
  if (value >= SARABANDE_INT_MIN && value <= SARABANDE_INT_MAX) {
    return value;
  } else {
    bigint *i = new_bigint(2);

    if (value < 0) {
      i->sign_bit = 1;
      value *= -1;
    }
    i->buf.data[0] = (value & 0xFFFFFFFF);
    i->buf.data[1] = ((value >> 32) & 0xFFFFFFFF);

    return i->handle;
  }
}

hInteger sbInteger_sum(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    return sbInteger_new(a + b);
  } else {
    bigint *biga = find_bigint_for_handle(a);
    bigint *bigb = find_bigint_for_handle(b);
    if (int_sign(biga, a) < 0 || int_sign(bigb, b) < 0) {
      PANIC("I haven't implemented this one yet!");
    }

    bigint *result = new_bigint_to_fit(biga, bigb);

    usize asize = int_size(biga);
    usize bsize = int_size(bigb);
    u64 carry = 0;
  /* + 1 here because we might need to carry into another slice */
    for (int i = 0; i < asize || i < bsize; i++) {
      u64 a_piece = int_piece(biga, a, i);
      u64 b_piece = int_piece(bigb, b, i);

      u64 digit_sum = (u64)a_piece + (u64)b_piece + carry;
      BUFFER_INDEX_SET(result->buf, Piece, i, digit_sum % BIGINT_PIECE_MAX);
      carry = 0;
      if (digit_sum > BIGINT_PIECE_MAX) {
        carry = digit_sum / BIGINT_PIECE_MAX;
      }
    }

    trim_bigint(result);

    return result->handle;
  }
}

hInteger sbInteger_diff(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    /* if the sum is too big, this will create a bigint. we don't need to
     * worry about signed overflow, because non-bigints are limited to
     * 62 bits of magnitude, so the sum of two can't be bigger than 2^63 */
    return sbInteger_new(a - b);
  }
  if (is_bigint(a)) {
    /* i will use this later */
    bigint *i = find_bigint_for_handle(a);
    (void)i;
  }
  PANIC("I haven't implemented this yet!");
}

double sbInteger_as_double(hInteger a) {
  if (!is_bigint(a)) {
    return (double)a;
  } else {
    PANIC("agh! i haven't done this!");
  }
}

hInteger sbInteger_mul(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    flag zero_ok = (a == 0);
    flag pos_ok = (a > 0 && b < SARABANDE_INT_MAX / a && b > SARABANDE_INT_MIN / a);
    flag neg_ok = (a < 0 && b < SARABANDE_INT_MIN / a && b > SARABANDE_INT_MAX / a);
    if (zero_ok || pos_ok || neg_ok) {
      return sbInteger_new(a * b);
    }
  }

  bigint *biga = find_bigint_for_handle(a);
  bigint *bigb = find_bigint_for_handle(b);

  usize asize = int_size(biga);
  usize bsize = int_size(bigb);
  bigint *result = new_bigint(asize + bsize);
  result->sign_bit = int_sign(biga, a) < 0 || int_sign(bigb, b) < 0;

  u64 carry = 0;
  for (int bi = 0; bi < bsize; bi++) {
    /* + 1 because we might need to carry into an extra piece after both numbers finish */
    for (int ai = 0; ai < asize + 1; ai++) {
      u64 a_piece = int_piece(biga, a, ai);
      u64 b_piece = int_piece(bigb, b, bi);

      u64 digit_sum = a_piece * b_piece + carry;
      BUFFER_INDEX_SET(result->buf, Piece, ai + bi, digit_sum % BIGINT_PIECE_MAX);
      carry = 0;
      if (digit_sum > BIGINT_PIECE_MAX) {
        carry = digit_sum / BIGINT_PIECE_MAX;
      }
    }
  }

  trim_bigint(result);

  return result->handle;
}

hInteger sbInteger_floordiv(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    return sbInteger_new(a / b);
  }
  PANIC("I haven't implemented this yet!");
}

void sbInteger_fprint(FILE *out, hInteger a) {
  if (!is_bigint(a)) {
    fprintf(out, "%lld", (long long)a);
  } else {
    bigint *big = find_bigint_for_handle(a);
    i8 sgn = int_sign(big, a);
    if (sgn == 0) {
      fprintf(out, "0");
      return;
    } else if (sgn < 0) {
      fprintf(out, "-");
    }
    isize sz = int_size(big);
    for (isize i = sz - 1; i >= 0; i--) {
      if (i == sz - 1) {
        fprintf(out, "%lld", (long long)int_piece(big, a, i));
      } else {
        fprintf(out, "%09lld", (long long)int_piece(big, a, i));
      }
    }
  }
}

/* --- */

flag is_bigint(hInteger n) {
  /* bigint flag is the second bit after the sign bit, normally for smaller
   * numbers this bit is equal to the sign bit, so we see if it's NOT equal
   * as a flag that a number is a bigint */
  return (n > 0 && (n & FLAG_BIGINT)) || (n < 0 && !(n & FLAG_BIGINT));
}

static void set_bigint_size(bigint *bi, usize new_size) {
  sbBuffer_set_size(&bi->buf, new_size * sizeof(Piece));
}

static usize int_size(bigint *bi) {
  if (bi == NULL) return 1;
  return bi->buf.size / sizeof(Piece);
}

static i8 int_sign(bigint *bi, hInteger h) {
  if (bi == NULL) {
    if (h == 0) {
      return 0;
    } else if (h < 0) {
      return -1;
    } else {
      return 1;
    }
  } else {
    /* scan to see if it is zero, but short circuit if it isn't */
    usize len = int_size(bi);
    for (usize i = 0; i < len; i++) {
      if (int_piece(bi, h, i) != 0) {
        if (bi->sign_bit) {
          return -1;
        } else {
          return 1;
        }
      }
    }
    return 0;
  }
}

static bigint *find_bigint_for_handle(hInteger handle) {
  if (!is_bigint(handle)) {
    return NULL;
  } else {
    hInteger h = handle & ~FLAG_BIGINT;
    return sbPool_get_entry(&g_bigint_pool, h);
  }
}

static bigint *new_bigint(usize initial_size) {
  usize index;
  bigint *i = sbPool_alloc(&g_bigint_pool, &index);
  sbBuffer_initialize(&i->buf, initial_size * sizeof(u32));
  set_bigint_size(i, initial_size);
  i->handle = index | FLAG_BIGINT;
  return i;
}

static bigint *new_bigint_to_fit(bigint *a, bigint *b) {
  usize a_size = int_size(a);
  usize b_size = int_size(b);
  return new_bigint(a_size > b_size ? a_size + 1 : b_size + 1);
}

static void trim_bigint(bigint *a) {
  if (!a) return;
  usize trim_count = 0;
  usize sz = int_size(a);
  for (isize i = sz - 1; i >= 0; i --) {
    if (BUFFER_INDEX(a->buf, Piece, i) != 0) {
      break;
    }
    trim_count ++;
  }
  if (trim_count > 0) {
    set_bigint_size(a, sz - trim_count);
  }
}

static u64 int_piece(bigint *bi, hInteger h, usize index) {
  if (!bi) {
    if (index == 0) {
      return h;
    } else {
      return 0;
    }
  } else if (index < int_size(bi)) {
    return BUFFER_INDEX(bi->buf, Piece, index);
  } else {
    return 0;
  }
}
