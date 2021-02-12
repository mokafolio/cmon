#include "utest.h"
#include <cmon/cmon_lexer.h>
#include <cmon/cmon_dyn_arr.h>

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

UTEST_MAIN();
