#include "utest.h"
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
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

UTEST(cmon, hashmap_tests)
{
    cmon_allocator a = cmon_mallocator_make();

    cmon_hashmap(const char *, int) map2;
    cmon_hashmap_str_key_init(&map2, &a);

    cmon_hashmap_set(&map2, "foo", 99);
    EXPECT_NE(NULL, cmon_hashmap_get(&map2, "foo"));
    EXPECT_EQ(99, *cmon_hashmap_get(&map2, "foo"));
    EXPECT_EQ(1, cmon_hashmap_count(&map2));
    cmon_hashmap_set(&map2, "foo", 33);
    EXPECT_EQ(1, cmon_hashmap_count(&map2));
    EXPECT_EQ(33, *cmon_hashmap_get(&map2, "foo"));
    cmon_hashmap_set(&map2, "bar", -3);
    EXPECT_EQ(2, cmon_hashmap_count(&map2));
    EXPECT_EQ(-3, *cmon_hashmap_get(&map2, "bar"));
    
    const char ** key_ref;
    cmon_hashmap_iter_t it = cmon_hashmap_iter(&map2);
    while(key_ref = cmon_hashmap_next(&map2, &it))
    {
        typeof((*(&map2)).tmp) foo = it.node->value;
        printf("da key %s\n", *key_ref);
        printf("WE GOT AN ENTRY %s %i\n", *key_ref, cmon_hashmap_iter_value(&map2, &it));
    }

    EXPECT_EQ(cmon_true, cmon_hashmap_remove(&map2, "foo"));
    EXPECT_EQ(cmon_false, cmon_hashmap_remove(&map2, "not there"));
    EXPECT_EQ(1, cmon_hashmap_count(&map2));
    EXPECT_EQ(NULL, cmon_hashmap_get(&map2, "foo"));
    EXPECT_NE(NULL, cmon_hashmap_get(&map2, "bar"));

    cmon_hashmap_dealloc(&map2);

    int b, c;
    cmon_hashmap(int *, const char *) map3;
    cmon_hashmap_ptr_key_init(&map3, &a);
    cmon_hashmap_set(&map3, &b, "foink");
    cmon_hashmap_set(&map3, &c, "boink");
    EXPECT_EQ(2, cmon_hashmap_count(&map3));
    EXPECT_NE(NULL, cmon_hashmap_get(&map3, &b));
    EXPECT_STREQ("foink", *cmon_hashmap_get(&map3, &b));
    EXPECT_STREQ("boink", *cmon_hashmap_get(&map3, &c));
    EXPECT_EQ(cmon_true, cmon_hashmap_remove(&map3, &c));
    EXPECT_EQ(NULL, cmon_hashmap_get(&map3, &c));
    EXPECT_NE(NULL, cmon_hashmap_get(&map3, &b));
    cmon_hashmap_dealloc(&map3);

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

    if(err)
    {
        cmon_err_report err = cmon_parser_err(parser);
        printf("%s:%lu:%lu: %s", err.filename, err.line, err.line_offset, err.msg);
    }

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
PARSE_TEST(parse_var_decl01, "mut bar := boink", cmon_true);
PARSE_TEST(parse_var_decl02, "pub mut bar := 2", cmon_true);
PARSE_TEST(parse_var_decl03, "bar := 'foo'", cmon_true);
PARSE_TEST(parse_var_decl04, "foo, bar : s32 = -99", cmon_true);
PARSE_TEST(parse_var_decl05, "foo, bar := 2.3545", cmon_true);
PARSE_TEST(parse_fn01, "ba := fn(){}", cmon_true);
PARSE_TEST(parse_fn02, "ba := fn(a : s32)->s32{}", cmon_true);
PARSE_TEST(parse_fn03, "ba := fn(a, b : s32)->s32{}", cmon_true);
PARSE_TEST(parse_call01, "foo := bar()", cmon_true);
PARSE_TEST(parse_call02, "foo := bar(1, 'hey')", cmon_true);
PARSE_TEST(parse_call03, "foo := bar(1.4, 3.24,)", cmon_false);
PARSE_TEST(parse_binop01, "ba := 1 + 2 * 3", cmon_true);
PARSE_TEST(parse_struct_decl01, "struct Foo{}", cmon_true);
PARSE_TEST(parse_struct_decl02, "struct {}", cmon_false);
PARSE_TEST(parse_struct_decl03, "pub struct Foo{ a : f32; b : f32 }", cmon_true);
PARSE_TEST(parse_struct_decl04, "pub struct Foo{ a : f32 b : f32 }", cmon_false);
PARSE_TEST(parse_struct_decl05, "pub struct Foo{ a, b : f32}", cmon_true);
PARSE_TEST(parse_struct_decl06, "struct Vec{ x, y : f64 = 10.0 }", cmon_true);
PARSE_TEST(parse_struct_init01, "a := Foo{}", cmon_true);
PARSE_TEST(parse_struct_init02, "a := Foo{1}", cmon_true);
PARSE_TEST(parse_struct_init03, "a := Foo{1, 2, 3}", cmon_true);
PARSE_TEST(parse_struct_init04, "a := Foo{1,}", cmon_false);
PARSE_TEST(parse_struct_init05, "a := Foo{foo: 1, bar: 2}", cmon_true);

UTEST_MAIN();
