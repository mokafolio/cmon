#include "utest.h"
#include <cmon/cmon_tokens.h>
#include <cmon/cmon_dyn_arr.h>

typedef struct
{
    char str[256];
}foo_t;

UTEST(cmon, dyn_arr_tests)
{
    cmon_allocator a = cmon_mallocator_make();

    cmon_dyn_arr(int) vec = NULL;
    cmon_dyn_arr_init(&vec, &a, 2);
    cmon_dyn_arr_append(&vec, 1);
    cmon_dyn_arr_append(&vec, 2);
    EXPECT_EQ(cmon_dyn_arr_count(&vec), 2);
    EXPECT_EQ(cmon_dyn_arr_capacity(&vec), 2);

    cmon_dyn_arr_append(&vec, 3);
    cmon_dyn_arr_append(&vec, 4);
    cmon_dyn_arr_append(&vec, 5);
    cmon_dyn_arr_append(&vec, 6);
    EXPECT_EQ(cmon_dyn_arr_count(&vec), 6);
    for (size_t i = 0; i < cmon_dyn_arr_count(&vec); ++i)
    {
        EXPECT_EQ(vec[i], i + 1);
    }

    EXPECT_EQ(cmon_dyn_arr_last(&vec), 6);
    int last = cmon_dyn_arr_pop(&vec);
    EXPECT_EQ(last, 6);
    EXPECT_EQ(cmon_dyn_arr_count(&vec), 5);

    cmon_dyn_arr_remove(&vec, 1);
    EXPECT_EQ(cmon_dyn_arr_count(&vec), 4);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 3);
    EXPECT_EQ(vec[2], 4);
    EXPECT_EQ(vec[3], 5);

    cmon_dyn_arr_insert(&vec, 1, 2);
    EXPECT_EQ(cmon_dyn_arr_count(&vec), 5);
    for (size_t i = 0; i < cmon_dyn_arr_count(&vec); ++i)
    {
        EXPECT_EQ(vec[i], i + 1);
    }

    cmon_dyn_arr_clear(&vec);
    EXPECT_EQ(cmon_dyn_arr_count(&vec), 0);

    cmon_dyn_arr_dealloc(&vec);


    // cmon_dyn_arr(foo_t) fv;
    // cmon_dyn_arr_init(&fv, &a, 16);

    // foo_t f;
    // strcpy(f.str, "boop");

    // cmon_dyn_arr_append(&fv, f);
    // EXPECT_EQ(1, cmon_dyn_arr_count(&fv));
    // cmon_dyn_arr_dealloc(&fv);

    cmon_allocator_dealloc(&a);
}

static inline cmon_bool _tokens_test(const char * _name, const char * _code)
{
    cmon_allocator alloc;
    cmon_src * src;
    cmon_idx src_idx;
    cmon_tokens * tokens;
    cmon_bool err;

    alloc = cmon_mallocator_make();

    src = cmon_src_create(&alloc);
    src_idx = cmon_src_add(src, _name, _name);
    cmon_src_set_code(src, src_idx, _code);

    tokens = cmon_tokenize(&alloc, src, src_idx, NULL);
    err = !tokens;

    cmon_tokens_destroy(tokens);
    cmon_src_destroy(src);
    cmon_allocator_dealloc(&alloc);

    return err;
}

//@TODO: Make some more in depth tests with expected token counts, checking token types etc.
#define TOKENS_TEST(_name, _code, _should_pass)                                             \
    UTEST(cmon, _name)                                                                             \
    {                                                                                              \
        EXPECT_EQ(_tokens_test(#_name ".cmon", _code), !_should_pass);                       \
    }

TOKENS_TEST(lexer_success,
               "module foo;import bar, joink as j\npub fn whop() -> s32 { return 3 }",
               cmon_true);
TOKENS_TEST(lexer_missing_expo_digits, "foo := 3.12214e", cmon_false);
TOKENS_TEST(lexer_invalid_character, "hello#", cmon_false);


UTEST_MAIN();
