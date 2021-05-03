#include "utest.h"
#include <cmon/cmon_builder_st.h>
#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_resolver.h>
#include <cmon/cmon_symbols.h>
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
    while ((key_ref = cmon_hashmap_next(&map2, &it)))
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

UTEST(cmon, dep_graph_tests_success)
{
    size_t i;

    cmon_allocator alloc = cmon_mallocator_make();

    cmon_dep_graph * g = cmon_dep_graph_create(&alloc);

    cmon_idx a, b, c, d;
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    cmon_dyn_arr(cmon_idx) adeps;
    cmon_dyn_arr(cmon_idx) bdeps;
    cmon_dyn_arr(cmon_idx) cdeps;
    cmon_dyn_arr(cmon_idx) ddeps;
    cmon_dyn_arr_init(&adeps, &alloc, 2);
    cmon_dyn_arr_init(&bdeps, &alloc, 2);
    cmon_dyn_arr_init(&cdeps, &alloc, 2);
    cmon_dyn_arr_init(&ddeps, &alloc, 2);

    cmon_dyn_arr_append(&adeps, b);
    cmon_dyn_arr_append(&adeps, c);
    cmon_dyn_arr_append(&bdeps, c);
    cmon_dyn_arr_append(&ddeps, a);
    cmon_dyn_arr_append(&ddeps, c);

    cmon_dep_graph_add(g, a, adeps, cmon_dyn_arr_count(&adeps));
    cmon_dep_graph_add(g, b, bdeps, cmon_dyn_arr_count(&bdeps));
    cmon_dep_graph_add(g, c, cdeps, cmon_dyn_arr_count(&cdeps));
    cmon_dep_graph_add(g, d, ddeps, cmon_dyn_arr_count(&ddeps));

    cmon_dep_graph_result res = cmon_dep_graph_resolve(g);

    if (res.count)
    {
        printf("got a result\n");
        for (i = 0; i < res.count; ++i)
        {
            cmon_idx s = res.array[i];
            if (s == a)
            {
                printf("a\n");
            }
            else if (s == b)
            {
                printf("b\n");
            }
            else if (s == c)
            {
                printf("c\n");
            }
            else if (s == d)
            {
                printf("d\n");
            }
        }
    }

    cmon_dyn_arr_dealloc(&adeps);
    cmon_dyn_arr_dealloc(&bdeps);
    cmon_dyn_arr_dealloc(&cdeps);
    cmon_dyn_arr_dealloc(&ddeps);
    cmon_dep_graph_destroy(g);
    cmon_allocator_dealloc(&alloc);
}

UTEST(cmon, dep_graph_tests_fail)
{
    cmon_allocator alloc = cmon_mallocator_make();

    cmon_dep_graph * g = cmon_dep_graph_create(&alloc);

    cmon_idx a, b, c;
    a = 1;
    b = 2;
    c = 3;

    cmon_dyn_arr(cmon_idx) adeps;
    cmon_dyn_arr(cmon_idx) bdeps;
    cmon_dyn_arr(cmon_idx) cdeps;

    cmon_dyn_arr_init(&adeps, &alloc, 2);
    cmon_dyn_arr_init(&bdeps, &alloc, 2);
    cmon_dyn_arr_init(&cdeps, &alloc, 2);

    cmon_dyn_arr_append(&adeps, b);
    cmon_dyn_arr_append(&bdeps, c);
    cmon_dyn_arr_append(&cdeps, a);

    cmon_dep_graph_add(g, a, adeps, cmon_dyn_arr_count(&adeps));
    cmon_dep_graph_add(g, b, bdeps, cmon_dyn_arr_count(&bdeps));
    cmon_dep_graph_add(g, c, cdeps, cmon_dyn_arr_count(&cdeps));

    cmon_dep_graph_result res = cmon_dep_graph_resolve(g);

    if (!res.count)
    {
        printf("circular dependency between: %lu and %lu\n",
               cmon_dep_graph_conflict_a(g),
               cmon_dep_graph_conflict_b(g));
    }

    EXPECT_EQ(res.array, NULL);

    cmon_dyn_arr_dealloc(&adeps);
    cmon_dyn_arr_dealloc(&bdeps);
    cmon_dyn_arr_dealloc(&cdeps);
    cmon_dep_graph_destroy(g);
    cmon_allocator_dealloc(&alloc);
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

    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_module));
    idx = cmon_tokens_accept(tokens, cmon_tokk_ident);
    EXPECT_NE(-1, idx);
    EXPECT_EQ(cmon_tokk_ident, cmon_tokens_kind(tokens, idx));
    EXPECT_EQ(1, cmon_tokens_line(tokens, idx));
    EXPECT_EQ(8, cmon_tokens_line_offset(tokens, idx));
    cmon_str_view name = cmon_tokens_str_view(tokens, idx);
    EXPECT_EQ(0, strncmp("foo", name.begin, name.end - name.begin));
    idx = cmon_tokens_accept(tokens, cmon_tokk_ident);
    EXPECT_NE(-1, idx);
    EXPECT_EQ(cmon_tokk_ident, cmon_tokens_kind(tokens, idx));
    EXPECT_EQ(2, cmon_tokens_line(tokens, idx));
    EXPECT_EQ(1, cmon_tokens_line_offset(tokens, idx));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_colon));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_assign));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_ident));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_paran_open));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_paran_close));
    EXPECT_NE(-1, cmon_tokens_accept(tokens, cmon_tokk_semicolon));
    EXPECT_EQ(cmon_true, cmon_tokens_is_current(tokens, cmon_tokk_eof));

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

// UTEST(cmon, basic_ast_test)
// {
//     cmon_allocator alloc;
//     cmon_astb * builder;
//     cmon_ast * ast;

//     alloc = cmon_mallocator_make();
//     builder = cmon_astb_create(&alloc);

//     cmon_idx l = cmon_astb_add_float_lit(builder, 1);
//     cmon_idx r = cmon_astb_add_float_lit(builder, 3);
//     cmon_idx b = cmon_astb_add_binary(builder, 2, l, r);
//     cmon_idx c = cmon_astb_add_var_decl(builder, 0, cmon_true, cmon_true, 1, b);

//     cmon_idx stms[] = {c};

//     cmon_idx block = cmon_astb_add_block(builder, 0, stms, sizeof(stms) / sizeof(cmon_idx));
//     cmon_astb_set_root_block(builder, block);

//     ast = cmon_astb_ast(builder);

//     cmon_idx root_block = cmon_ast_root_block(ast);
//     EXPECT_EQ(cmon_astk_block, cmon_ast_kind(ast, root_block));
//     // printf("end %lu begin %lu\n", cmon_ast_block_end(ast, root_block),
//     cmon_ast_block_begin(ast, root_block));
// EXPECT_EQ(1, cmon_ast_block_end(ast, root_block) -
//     cmon_ast_block_begin(ast, root_block));

//     size_t count = 0;
//     cmon_ast_iter it = cmon_ast_block_iter(ast, root_block);
//     cmon_idx idx;
//     while(cmon_is_valid_idx(idx = cmon_ast_iter_next(ast, &it)))
//     {
//         printf("da focking idx %lu %lu\n", idx, c);
//         EXPECT_EQ(cmon_astk_var_decl, cmon_ast_kind(ast, idx));
//         ++count;
//     }
//     EXPECT_EQ(1, count);

//     cmon_astb_destroy(builder);
//     cmon_allocator_dealloc(&alloc);
// }

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

    if (err)
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

// PARSE_TEST(parse_basic, "module foo", cmon_true);
// PARSE_TEST(parse_import_fail01, "module foo import bar", cmon_false);
// PARSE_TEST(parse_import_basic01, "module foo; import bar", cmon_true);
// PARSE_TEST(parse_import_basic02, "module foo; import bar as b", cmon_true);
// PARSE_TEST(parse_import_basic03, "module foo; import boink.bar as b", cmon_true);
// PARSE_TEST(parse_import_basic04, "module foo; import bubble, tea", cmon_true);
// PARSE_TEST(parse_import_basic05, "module foo; import bubble.foo as boink, tea as t", cmon_true);
// PARSE_TEST(parse_var_decl01, "mut bar := boink", cmon_true);
// PARSE_TEST(parse_var_decl02, "pub mut bar := 2", cmon_true);
// PARSE_TEST(parse_var_decl03, "bar := 'foo'", cmon_true);
// // PARSE_TEST(parse_var_decl04, "foo, bar : s32 = -99", cmon_true);
// // PARSE_TEST(parse_var_decl05, "foo, bar := 2.3545", cmon_true);
// PARSE_TEST(parse_fn01, "ba := fn(){}", cmon_true);
// PARSE_TEST(parse_fn02, "ba := fn(a : s32)->s32{}", cmon_true);
// // PARSE_TEST(parse_fn03, "ba := fn(a, b : s32)->s32{}", cmon_true);
// PARSE_TEST(parse_call01, "foo := bar()", cmon_true);
// PARSE_TEST(parse_call02, "foo := bar(1, 'hey')", cmon_true);
// PARSE_TEST(parse_call03, "foo := bar(1.4, 3.24,)", cmon_false);
// PARSE_TEST(parse_binop01, "ba := 1 + 2 * 3", cmon_true);
// PARSE_TEST(parse_struct_decl01, "struct Foo{}", cmon_true);
// PARSE_TEST(parse_struct_decl02, "struct {}", cmon_false);
// PARSE_TEST(parse_struct_decl03, "pub struct Foo{ a : f32; b : f32 }", cmon_true);
// PARSE_TEST(parse_struct_decl04, "pub struct Foo{ a : f32 b : f32 }", cmon_false);
// // PARSE_TEST(parse_struct_decl05, "pub struct Foo{ a, b : f32}", cmon_true);
// // PARSE_TEST(parse_struct_decl06, "struct Vec{ x, y : f64 = 10.0 }", cmon_true);
// PARSE_TEST(parse_struct_init01, "a := Foo{}", cmon_true);
// PARSE_TEST(parse_struct_init02, "a := Foo{1}", cmon_true);
// PARSE_TEST(parse_struct_init03, "a := Foo{1, 2, 3}", cmon_true);
// PARSE_TEST(parse_struct_init04, "a := Foo{1,}", cmon_false);
// PARSE_TEST(parse_struct_init05, "a := Foo{foo: 1, bar: 2}", cmon_true);
// PARSE_TEST(parse_selector01, "a := foo.bar", cmon_true);
// PARSE_TEST(parse_selector02, "a := foo.bar.bat", cmon_true);
// PARSE_TEST(parse_selector03, "a := foo.bar().bat", cmon_true);
// PARSE_TEST(parse_index01, "a := foo[0]", cmon_true);
// PARSE_TEST(parse_index02, "a := foo.bar[99 + 3]", cmon_true);
// PARSE_TEST(parse_array_init01, "a := [1, 2, 3]", cmon_true);
// PARSE_TEST(parse_array_init02, "a := []", cmon_true);
// PARSE_TEST(parse_array_init03, "a := [\"foo\", \"bar\",]", cmon_false);
// PARSE_TEST(parse_alias01, "alias Foo = *Bar", cmon_true);
// PARSE_TEST(parse_alias02, "pub alias Foo = Bar", cmon_true);
// PARSE_TEST(parse_alias03, "fn main() { pub alias Foo = Bar }", cmon_false);
// PARSE_TEST(parse_alias04, "fn main() { alias Foo = Bar; }", cmon_true);
// PARSE_TEST(parse_typedef01, "type Foo = Bar", cmon_true);
// PARSE_TEST(parse_top01, "alias Foo = Bar; fn main(){}", cmon_true);
// PARSE_TEST(parse_top02, "alias Foo = Bar\nfn main(){}", cmon_true);
// PARSE_TEST(parse_top03, "alias Foo = Bar fn main(){}", cmon_false);

// UTEST(cmon, basic_symbols_test)
// {
//     cmon_symbols * s;
//     cmon_src * src;
//     cmon_modules * mods;
//     cmon_allocator a;

//     cmon_idx global_scope, file_scope, scope;
//     cmon_idx foo, bar, car;

//     a = cmon_mallocator_make();
//     src = cmon_src_create(&a);
//     mods = cmon_modules_create(&a, src);
//     s = cmon_symbols_create(&a, src, mods);

//     global_scope = cmon_symbols_scope_begin(s, CMON_INVALID_IDX);
//     file_scope = cmon_symbols_scope_begin(s, global_scope);
//     EXPECT_EQ(global_scope, cmon_symbols_scope_end(s, file_scope));
//     EXPECT_EQ(cmon_true, cmon_symbols_scope_is_global(s, global_scope));
//     EXPECT_EQ(cmon_true, cmon_symbols_scope_is_file(s, file_scope));
//     EXPECT_EQ(cmon_false, cmon_symbols_scope_is_global(s, file_scope));
//     EXPECT_EQ(cmon_false, cmon_symbols_scope_is_file(s, global_scope));

//     foo = cmon_symbols_scope_add_var(
//         s, global_scope, cmon_str_view_make("foo"), 1, cmon_true, cmon_false, 99, 33);
//     EXPECT_EQ(CMON_INVALID_IDX,
//               cmon_symbols_find(s, global_scope, cmon_str_view_make("not found")));
//     EXPECT_EQ(foo, cmon_symbols_find(s, global_scope, cmon_str_view_make("foo")));
//     EXPECT_EQ(foo, cmon_symbols_find(s, file_scope, cmon_str_view_make("foo")));

//     EXPECT_EQ(cmon_symk_var, cmon_symbols_kind(s, foo));
//     EXPECT_EQ(global_scope, cmon_symbols_scope(s, foo));
//     EXPECT_EQ(0, cmon_str_view_cmp(cmon_str_view_make("foo"), cmon_symbols_name(s, foo)));
//     EXPECT_EQ(cmon_true, cmon_symbols_is_pub(s, foo));
//     EXPECT_EQ(99, cmon_symbols_src_file(s, foo));
//     EXPECT_EQ(33, cmon_symbols_ast(s, foo));
//     cmon_symbols_destroy(s);
//     cmon_modules_destroy(mods);
//     cmon_src_destroy(src);
//     cmon_allocator_dealloc(&a);
// }

// static cmon_bool _resolve_test_fn(const char * _name, const char * _code)
// {
//     cmon_bool err;

//     cmon_allocator alloc = cmon_mallocator_make();
//     cmon_src * src = cmon_src_create(&alloc);
//     cmon_idx src_idx = cmon_src_add(src, _name, _name);
//     cmon_src_set_code(src, src_idx, _code);

//     cmon_parser * parser = cmon_parser_create(&alloc);
//     cmon_tokens * tokens = cmon_tokenize(&alloc, src, src_idx, NULL);

//     err = !tokens;
//     if (err)
//         goto end;

//     cmon_ast * ast = cmon_parser_parse(parser, src, src_idx, tokens);

//     if (!ast)
//     {
//         cmon_err_report err = cmon_parser_err(parser);
//         printf("%s:%lu:%lu: %s", err.filename, err.line, err.line_offset, err.msg);
//         goto end;
//     }

//     cmon_src_set_tokens(src, src_idx, tokens);
//     cmon_src_set_ast(src, src_idx, ast);

//     cmon_modules * mods = cmon_modules_create(&alloc);
//     cmon_symbols * s = cmon_symbols_create(&alloc, src, mods);
//     cmon_types * types = cmon_types_create(&alloc, mods);
//     cmon_idx mod = cmon_modules_add(mods, _name, _name);
//     cmon_modules_add_src_file(mods, mod, src_idx);

//     cmon_resolver * r = cmon_resolver_create(&alloc, 12);
//     cmon_resolver_set_input(r, src, types, s, mods, mod);

//     err = cmon_resolver_resolve(r);
//     if (err)
//     {
//         cmon_err_report * errs;
//         size_t err_count;
//         cmon_resolver_errors(r, &errs, &err_count);
//         size_t i;
//         for (i = 0; i < err_count; ++i)
//             cmon_err_report_print(&errs[i]);
//     }

// end:
//     cmon_resolver_destroy(r);
//     cmon_types_destroy(types);
//     cmon_symbols_destroy(s);
//     cmon_modules_destroy(mods);
//     cmon_parser_destroy(parser);
//     cmon_tokens_destroy(tokens);
//     cmon_src_destroy(src);
//     cmon_allocator_dealloc(&alloc);

//     return err;
// }

typedef void (*module_adder_fn)(cmon_src *, cmon_modules *);

static cmon_bool _resolve_test_fn(module_adder_fn _fn)
{
    cmon_bool err = cmon_false;
    cmon_allocator alloc = cmon_mallocator_make();
    cmon_src * src = cmon_src_create(&alloc);
    cmon_modules * mods = cmon_modules_create(&alloc, src);

    _fn(src, mods);

    cmon_builder_st * builder = cmon_builder_st_create(&alloc, 1, src, mods);

    if (cmon_builder_st_build(builder))
    {
        cmon_err_report * errs;
        size_t count;
        cmon_builder_st_errors(builder, &errs, &count);
        size_t i;
        for (i = 0; i < count; ++i)
        {
            cmon_err_report_print(&errs[i]);
        }
        err = cmon_true;
    }

    // end:
    cmon_builder_st_destroy(builder);
    cmon_modules_destroy(mods);
    cmon_src_destroy(src);
    return err;
}

#define RESOLVE_TEST(_name, _code, _should_pass)                                                   \
    static void _name##_mod_adder_fn(cmon_src * _src, cmon_modules * _mods)                        \
    {                                                                                              \
        cmon_idx src_idx = cmon_src_add(_src, #_name, #_name);                                     \
        cmon_src_set_code(_src, src_idx, "module " #_name "\n\n" _code);                           \
        cmon_idx mod = cmon_modules_add(_mods, #_name, #_name);                                    \
        cmon_modules_add_src_file(_mods, mod, src_idx);                                            \
    }                                                                                              \
    UTEST(cmon, _name)                                                                             \
    {                                                                                              \
        EXPECT_EQ(!_should_pass, _resolve_test_fn(_name##_mod_adder_fn));                          \
    }

RESOLVE_TEST(resolve_empty, "", cmon_true);
RESOLVE_TEST(resolve_import01, "import foo", cmon_false);
RESOLVE_TEST(resolve_var_decl01, "a : s32 = 1", cmon_true);
RESOLVE_TEST(resolve_var_decl02, "boink := \"test\"", cmon_true);
RESOLVE_TEST(resolve_var_decl03, "boink : fn() -> s32 = fn()->s32{}", cmon_true);
RESOLVE_TEST(resolve_var_decl04, "boink : fn() = fn() -> u32 {} ", cmon_false);
RESOLVE_TEST(resolve_var_decl05, "fn main() { a : s32 = 1 } ", cmon_true);
RESOLVE_TEST(resolve_var_decl06, "foo := 1; foo := 2", cmon_false);
RESOLVE_TEST(resolve_var_decl07, "bar := 1; fn bar() {}", cmon_false);
RESOLVE_TEST(resolve_var_decl08, "struct Bar{}; Bar := 1;", cmon_false);
RESOLVE_TEST(resolve_var_decl09, "boink : fn(f64, f64) -> f64 = fn(a : f64, b : f64) -> f64 {}", cmon_true);
RESOLVE_TEST(resolve_var_decl10, "boink : fn(f64, f64) -> f64 = fn(a : f64) -> f64 {}", cmon_false);
RESOLVE_TEST(resolve_fn_decl01, "main := fn(){}", cmon_true);
RESOLVE_TEST(resolve_fn_decl02, "pub fn main(){}", cmon_true);
RESOLVE_TEST(resolve_fn_decl03, "mut woop : fn()->s32 = fn()->s32 {}", cmon_true);
RESOLVE_TEST(resolve_fn_decl04, "mut woop : fn(s32, s32)->s32 = fn(a : s32, a : s32)->s32 {}", cmon_false);
RESOLVE_TEST(resolve_addr01, "a := 1; b := &a", cmon_true);
RESOLVE_TEST(resolve_addr02, "a : u32 = 1; b : *u32 = &a", cmon_true);
RESOLVE_TEST(resolve_addr03, "fn main() { a : u32 = 1; b : *mut u32 = &a }", cmon_false);
RESOLVE_TEST(resolve_addr04, "fn main() { mut a : u32 = 1; b : *mut u32 = &a }", cmon_true);
RESOLVE_TEST(resolve_deref01, "a := 1; b := &a; mut c : s32 = *b;", cmon_true);
RESOLVE_TEST(resolve_assign01, "mut a := 1; fn main() { a = 2 }", cmon_true);
RESOLVE_TEST(resolve_assign02, "a := 1; fn main() { a = 2 }", cmon_false);
RESOLVE_TEST(resolve_assign03, "fn main() { mut a := 1; a = 3 }", cmon_true);
RESOLVE_TEST(resolve_assign04, "fn main() { mut a := 1; a += 3 }", cmon_true);
RESOLVE_TEST(resolve_assign05, "fn main() { mut a := 1; a -= 3 }", cmon_true);
RESOLVE_TEST(resolve_assign06, "fn main() { mut a := 1; a /= 3 }", cmon_true);
RESOLVE_TEST(resolve_assign07, "fn main() { mut a := 1; a *= 3 }", cmon_true);
RESOLVE_TEST(resolve_assign08, "fn main() { mut a := 1; a %= 3 }", cmon_true);
RESOLVE_TEST(resolve_assign09, "fn main() { a := 1; a = 3 }", cmon_false);
RESOLVE_TEST(resolve_assign10, "fn main() { a := 1; a += 3 }", cmon_false);
RESOLVE_TEST(resolve_assign11, "fn main() { a := 1; a -= 3 }", cmon_false);
RESOLVE_TEST(resolve_assign12, "fn main() { a := 1; a /= 3 }", cmon_false);
RESOLVE_TEST(resolve_assign13, "fn main() { a := 1; a *= 3 }", cmon_false);
RESOLVE_TEST(resolve_assign14, "fn main() { a := 1; a %= 3 }", cmon_false);
RESOLVE_TEST(resolve_prefix01, "boink : s32 = -2", cmon_true);
RESOLVE_TEST(resolve_binop01, "boink := 1 + 2", cmon_true);
RESOLVE_TEST(resolve_binop02, "pub boop := 1.1 + 2.3", cmon_true);
RESOLVE_TEST(resolve_binop03, "boink : s32 = 1 + 2.3", cmon_false);
RESOLVE_TEST(resolve_binop04, "pub mut wee := 11 % 2", cmon_true);
RESOLVE_TEST(resolve_binop05, "boink := 33 % 2.3", cmon_false);
RESOLVE_TEST(resolve_typecheck_loop01, "a := b; b := c; c := a", cmon_false);
RESOLVE_TEST(resolve_typecheck_loop02, "a := b; b := 1", cmon_true);
RESOLVE_TEST(resolve_struct01, "struct Foo{}", cmon_true);
RESOLVE_TEST(resolve_struct02, "struct Foo{ bar : s32 }", cmon_true);
RESOLVE_TEST(resolve_struct03, "struct Foo{ bar : s32; bat : s32 = 1 }", cmon_true);
RESOLVE_TEST(resolve_struct04, "struct Foo{ bar : s32 bat : s32 }", cmon_false);
RESOLVE_TEST(resolve_struct05, "struct Boink{ bar : f32; bar : f64 }", cmon_false);
RESOLVE_TEST(resolve_struct_init01,
             "struct Boink{ x : f32; y : f32 }; fn main() { b := Boink{1.0, 2.0} }",
             cmon_true);
// RESOLVE_TEST(resolve_struct_init02, "struct Boink{ x : f32; y : f32 }; fn main() { b := Boink{1,
// 2} }", cmon_true);
RESOLVE_TEST(resolve_struct_init03,
             "struct Boink{ x : f32; y : f32 }; fn main() { b := Boink{1.0} }",
             cmon_false);
RESOLVE_TEST(resolve_struct_init04,
             "struct Boink{ x : f32; y : f32 }; fn main() { b := Boink{1.0, 2.0, 3.0} }",
             cmon_false);
RESOLVE_TEST(resolve_struct_init05,
             "struct Boink{ x : f32; y : f32 }; b := Boink{x: 1.0, y: 2.0}",
             cmon_true);
RESOLVE_TEST(resolve_struct_init06,
             "struct Boink{ x : f32; y : f32 }; b : Boink = Boink{1.0, y: 2.0}",
             cmon_false);
RESOLVE_TEST(resolve_struct_init07,
             "struct Boink{ x : f32; y : f32 }; b : Boink = Boink{x: 1.0, z: 2.0}",
             cmon_false);
RESOLVE_TEST(resolve_alias01,
             "struct Bar{}; fn main(){ alias Foo = Bar; boop : Foo = Foo{}; boop2 : Bar = Foo{} }",
             cmon_true);
RESOLVE_TEST(resolve_alias02,
             "alias Boop = Bar; struct Bar{}; mut man := Boop{}; mut bar_man : Bar = man",
             cmon_true);
RESOLVE_TEST(resolve_alias03, "alias Boop = f64", cmon_true);
RESOLVE_TEST(resolve_alias04, "alias Boop = NoExist", cmon_false);
RESOLVE_TEST(resolve_alias05, "alias Boop = no.Exist", cmon_false);
RESOLVE_TEST(resolve_alias06, "alias Foo = Bar; alias Bar = Foo", cmon_false);
RESOLVE_TEST(resolve_alias07, "alias Bat = Foo; alias Foo = Bar; alias Bar = Bat", cmon_false);
RESOLVE_TEST(resolve_alias08, "alias Foo = Bar; struct Bar{ foo : Foo }", cmon_false);
RESOLVE_TEST(resolve_call01, "fn foo(){}; fn main(){ foo() }", cmon_true);
RESOLVE_TEST(resolve_call02, "fn foo(a : f32)->f32{}; bar := foo(1.3)", cmon_true);
RESOLVE_TEST(resolve_call03, "fn foo(a : f32)->f32{}; bar : f32 = foo(1.3)", cmon_true);
RESOLVE_TEST(resolve_call04, "fn foo(a : f32)->f32{}; bar := foo(99)", cmon_false);
RESOLVE_TEST(resolve_call05, "fn foo(a : f32)->f32{}; bar := foo(1.3, 99)", cmon_false);
RESOLVE_TEST(resolve_call06, "fn foo(a : f32)->f32{}; bar := foo()", cmon_false);

void _module_selector_test_adder_fn(cmon_src * _src, cmon_modules * _mods)
{
    cmon_idx src01_idx = cmon_src_add(_src, "foo/foo.cmon", "foo.cmon");
    cmon_src_set_code(_src, src01_idx, "module foo; pub fn foo_fn(_arg : s32) -> s32{}; pub struct FooType{ a : s32 }; pub foo_glob := 99;");
    cmon_idx foo_mod = cmon_modules_add(_mods, "foo", "foo");
    cmon_modules_add_src_file(_mods, foo_mod, src01_idx);
    cmon_idx src02_idx = cmon_src_add(_src, "bar/bar.cmon", "bar.cmon"); 
    cmon_src_set_code(_src, src02_idx, "module bar; import foo; boink := foo.foo_glob; foo_type := foo.FooType{a: 2}; val : s32 = foo.foo_fn(-33)");
    cmon_idx bar_mod = cmon_modules_add(_mods, "bar", "bar");
    cmon_modules_add_src_file(_mods, bar_mod, src02_idx);
}

UTEST(cmon, resolve_module_selector_test)
{
    EXPECT_EQ(cmon_false, _resolve_test_fn(_module_selector_test_adder_fn));
}

void _module_circ_dep_test_adder_fn(cmon_src * _src, cmon_modules * _mods)
{
    cmon_idx src01_idx = cmon_src_add(_src, "foo/foo.cmon", "foo.cmon");
    cmon_src_set_code(_src, src01_idx, "module foo; import bar");
    cmon_idx foo_mod = cmon_modules_add(_mods, "foo", "foo");
    cmon_modules_add_src_file(_mods, foo_mod, src01_idx);
    cmon_idx src02_idx = cmon_src_add(_src, "bar/bar.cmon", "bar.cmon"); 
    cmon_src_set_code(_src, src02_idx, "module bar; import foo;");
    cmon_idx bar_mod = cmon_modules_add(_mods, "bar", "bar");
    cmon_modules_add_src_file(_mods, bar_mod, src02_idx);
}

UTEST(cmon, resolve_module_circ_dep)
{
    EXPECT_EQ(cmon_true, _resolve_test_fn(_module_circ_dep_test_adder_fn));
}

UTEST_MAIN();
