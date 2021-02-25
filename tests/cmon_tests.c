#include "utest.h"
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_tokens.h>

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
    cmon_allocator_dealloc(&a);
}

UTEST(cmon, basic_tokens_test)
{
    cmon_allocator alloc;
    cmon_src * src;
    cmon_idx src_idx;
    cmon_tokens * tokens;
    cmon_idx idx;

    alloc = cmon_mallocator_make();

    src = cmon_src_create(&alloc);
    src_idx = cmon_src_add(src, "basic_tokens_test.cmon", "basic_tokens_test.cmon");
    cmon_src_set_code(src, src_idx, "module foo\nboop := bar();");

    tokens = cmon_tokenize(&alloc, src, src_idx, NULL);
    EXPECT_NE(NULL, (void *)tokens); // void cast hack to make utest.h work with incomplete type
    EXPECT_EQ(10, cmon_tokens_count(tokens));

    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_module));
    idx = cmon_tokens_accept(tokens, cmon_tk_ident);
    EXPECT_NE(-1, idx);
    EXPECT_EQ(cmon_tk_ident, cmon_tokens_kind(tokens, idx));
    EXPECT_EQ(1, cmon_tokens_line(tokens, idx));
    EXPECT_EQ(8, cmon_tokens_line_offset(tokens, idx));
    cmon_str_view name = cmon_tokens_str_view(tokens, idx);
    EXPECT_EQ(0, strncmp("foo", name.begin, name.end - name.begin));
    idx = cmon_tokens_accept(tokens, cmon_tk_ident);
    EXPECT_NE(-1, idx);
    EXPECT_EQ(cmon_tk_ident, cmon_tokens_kind(tokens, idx));
    EXPECT_EQ(2, cmon_tokens_line(tokens, idx));
    EXPECT_EQ(1, cmon_tokens_line_offset(tokens, idx));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_colon));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_assign));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_ident));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_paran_open));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_paran_close));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tk_semicolon));
    EXPECT_EQ(cmon_true, cmon_tokens_is_current(tokens, cmon_tk_eof));

    cmon_tokens_destroy(tokens);
    cmon_src_destroy(src);
    cmon_allocator_dealloc(&alloc);
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
#define TOKENS_TEST(_name, _code, _should_pass)                                                    \
    UTEST(cmon, _name)                                                                             \
    {                                                                                              \
        EXPECT_EQ(_tokens_test(#_name ".cmon", _code), !_should_pass);                             \
    }

TOKENS_TEST(lexer_success,
            "module foo;import bar, joink as j\npub fn whop() -> s32 { return 3 }",
            cmon_true);
TOKENS_TEST(lexer_missing_expo_digits, "foo := 3.12214e", cmon_false);
TOKENS_TEST(lexer_invalid_character, "hello#", cmon_false);

// #define PARSE_TEST(_name, _code, _should_pass)

static cmon_bool _parse_test_fn(const char * _name, const char * _code)
{
    cmon_allocator alloc;
    cmon_src * src;
    cmon_idx src_idx;
    cmon_tokens * tokens;
    cmon_parser * parser;
    cmon_bool err;

    alloc = cmon_mallocator_make();

    src = cmon_src_create(&alloc);
    src_idx = cmon_src_add(src, _name, _name);
    cmon_src_set_code(src, src_idx, _code);

    parser = cmon_parser_create(&alloc);

    tokens = cmon_tokenize(&alloc, src, src_idx, NULL);
    err = !tokens;
    if (err)
        goto end;

    err = cmon_parser_parse(parser, src, src_idx, tokens) == NULL;

end:
    cmon_parser_destroy(parser);
    cmon_tokens_destroy(tokens);
    cmon_src_destroy(src);
    cmon_allocator_dealloc(&alloc);

    return err;
}

#define PARSE_TEST(_name, _code, _should_pass)                                                     \
    UTEST(cmon, _name)                                                                             \
    {                                                                                              \
        EXPECT_EQ(!_should_pass, _parse_test_fn(#_name, _code));                                   \
    }

PARSE_TEST(parse_basic, "module foo", cmon_true);
PARSE_TEST(parse_import_fail01, "module foo import bar", cmon_false);
PARSE_TEST(parse_import_basic01, "module foo; import bar", cmon_true);
PARSE_TEST(parse_import_basic02, "module foo; import bar as b", cmon_true);
PARSE_TEST(parse_import_basic03, "module foo; import boink.bar as b", cmon_true);
PARSE_TEST(parse_import_basic04, "module foo; import bubble, tea", cmon_true);
PARSE_TEST(parse_import_basic05, "module foo; import bubble.foo as boink, tea as t", cmon_true);
PARSE_TEST(parse_var_decl01, "module foo; mut bar := boink", cmon_true);

UTEST_MAIN();
