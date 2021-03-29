project('cmon', 'c', default_options : ['c_std=gnu99'])

inc_dirs = include_directories('.')

lib_src = [
    'cmon/cmon_allocator.c',
    'cmon/cmon_ast.c',
    'cmon/cmon_dep_graph.c',
    'cmon/cmon_err_report.c',
    'cmon/cmon_hashmap.c',
    'cmon/cmon_modules.c',
    'cmon/cmon_parser.c',
    'cmon/cmon_resolver.c',
    'cmon/cmon_src.c',
    'cmon/cmon_str_builder.c',
    'cmon/cmon_symbols.c',
    'cmon/cmon_tokens.c',
    'cmon/cmon_types.c',
    'cmon/cmon_util.c',
]

thread_dep = dependency('threads')
cmon_lib = library('cmon', lib_src, include_directories : inc_dirs, dependencies: thread_dep)

#cmon_bin = executable('cmon', 
#    ['cmon/cmon_main2.c'], 
#    link_with: [cmon_lib], 
#    c_args : ['-fsanitize=address', '-Wall', '-std=gnu11'], 
#    link_args : '-fsanitize=address')
#

subdir('tests')
