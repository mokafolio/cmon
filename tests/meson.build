# tests need to be compiled in gnu c for utest.h. For now we also do c11
# for better unit test error messages. On older c version, utest.h will only print undef :(
tests = executable('cmon_tests', 'cmon_tests.c', 
    include_directories : inc_dirs,
    link_with: [cmon_lib],
    c_args : ['-fsanitize=address', '-Wall'],
    link_args : '-fsanitize=address')

test('cmon tests', tests, workdir: meson.current_build_dir())
