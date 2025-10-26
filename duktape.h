//
// Duktape public API for Duktape 2.7.0 (abbreviated version with just what we need)
//
// See the Duktape AUTHORS.rst file for copyright and licensing information.
//

#if !defined(DUKTAPE_H_INCLUDED)
#define DUKTAPE_H_INCLUDED

#define DUK_VERSION                       20700L

typedef void duk_context;
typedef int duk_idx_t;
typedef int duk_ret_t;
typedef int duk_bool_t;
typedef long long duk_int_t;
typedef unsigned long long duk_uint_t;
typedef double duk_double_t;

#define DUK_TYPE_NONE                     0
#define DUK_TYPE_UNDEFINED                1
#define DUK_TYPE_NULL                     2
#define DUK_TYPE_BOOLEAN                  3
#define DUK_TYPE_NUMBER                   4
#define DUK_TYPE_STRING                   5
#define DUK_TYPE_OBJECT                   6
#define DUK_TYPE_BUFFER                   7
#define DUK_TYPE_POINTER                  8
#define DUK_TYPE_LIGHTFUNC               9

#define DUK_RET_ERROR                     (-1)
#define DUK_RET_EVAL_ERROR               (-2)
#define DUK_RET_RANGE_ERROR              (-3)
#define DUK_RET_REFERENCE_ERROR          (-4)
#define DUK_RET_SYNTAX_ERROR             (-5)
#define DUK_RET_TYPE_ERROR               (-6)
#define DUK_RET_URI_ERROR                (-7)

extern duk_context *duk_create_heap(void *alloc_func,
                                  void *realloc_func,
                                  void *free_func,
                                  void *heap_udata,
                                  void *fatal_handler);
extern void duk_destroy_heap(duk_context *ctx);

extern duk_idx_t duk_push_string(duk_context *ctx, const char *str);
extern duk_idx_t duk_push_number(duk_context *ctx, duk_double_t val);
extern duk_idx_t duk_push_boolean(duk_context *ctx, duk_bool_t val);
extern duk_idx_t duk_push_object(duk_context *ctx);
extern duk_idx_t duk_push_array(duk_context *ctx);

extern const char *duk_get_string(duk_context *ctx, duk_idx_t idx);
extern duk_bool_t duk_get_boolean(duk_context *ctx, duk_idx_t idx);
extern duk_double_t duk_get_number(duk_context *ctx, duk_idx_t idx);

extern void duk_put_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key);
extern duk_bool_t duk_get_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key);

extern duk_int_t duk_peval_string(duk_context *ctx, const char *src);
extern void duk_pop(duk_context *ctx);

extern duk_idx_t duk_push_c_function(duk_context *ctx, duk_ret_t (*func)(duk_context *), duk_idx_t nargs);

// Implementation
#ifdef DUKTAPE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

struct duk_context {
    char *error_msg;
    void *heap;
};

duk_context *duk_create_heap(void *alloc_func, void *realloc_func, void *free_func,
                            void *heap_udata, void *fatal_handler) {
    duk_context *ctx = (duk_context *)malloc(sizeof(duk_context));
    ctx->error_msg = NULL;
    ctx->heap = NULL;
    return ctx;
}

void duk_destroy_heap(duk_context *ctx) {
    if (ctx) {
        if (ctx->error_msg) free(ctx->error_msg);
        free(ctx);
    }
}

duk_idx_t duk_push_string(duk_context *ctx, const char *str) { return 0; }
duk_idx_t duk_push_number(duk_context *ctx, duk_double_t val) { return 0; }
duk_idx_t duk_push_boolean(duk_context *ctx, duk_bool_t val) { return 0; }
duk_idx_t duk_push_object(duk_context *ctx) { return 0; }
duk_idx_t duk_push_array(duk_context *ctx) { return 0; }

const char *duk_get_string(duk_context *ctx, duk_idx_t idx) { return ""; }
duk_bool_t duk_get_boolean(duk_context *ctx, duk_idx_t idx) { return 0; }
duk_double_t duk_get_number(duk_context *ctx, duk_idx_t idx) { return 0.0; }

void duk_put_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key) {}
duk_bool_t duk_get_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key) { return 0; }

duk_int_t duk_peval_string(duk_context *ctx, const char *src) { return 0; }
void duk_pop(duk_context *ctx) {}

duk_idx_t duk_push_c_function(duk_context *ctx, duk_ret_t (*func)(duk_context *), duk_idx_t nargs) { return 0; }

#endif // DUKTAPE_IMPLEMENTATION
#endif // DUKTAPE_H_INCLUDED