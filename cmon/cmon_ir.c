#include <cmon/cmon_ir.h>
#include <cmon/cmon_dyn_arr.h>

// typedef struct cmon_irb
// {
//     cmon_allocator * alloc;
//     cmon_tokens * tokens;
//     cmon_dyn_arr(cmon_astk) kinds;
//     cmon_dyn_arr(cmon_idx) primary_tokens;
//     cmon_dyn_arr(_left_right) left_right;
//     cmon_dyn_arr(cmon_idx) extra_data;
//     cmon_dyn_arr(cmon_idx) imports; // we put all imports in one additional list for easy dependency
//                                     // tree building later on
//     cmon_idx root_block_idx;
//     cmon_ast ast; // filled in in cmon_astb_ast
// } cmon_irb;
