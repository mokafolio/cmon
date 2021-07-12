#ifndef CMON_CMON_PM_H
#define CMON_CMON_PM_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_pm cmon_pm;
typedef struct cmon_pm_lock_file cmon_pm_lock_file;

// create an empty package manager that should clone to
CMON_API cmon_pm * cmon_pm_create(cmon_allocator * _a, const char * _dep_dir);
CMON_API void cmon_pm_destroy(cmon_pm * _pm);
// find or create a module with a url and version (in the future this should allow you to specify
// what kind it is, i.e. for now its git only, in the future possible other things could include
// wget etc.)
CMON_API cmon_idx cmon_pm_find_or_add_module(cmon_pm * _pm,
                                             cmon_str_view _url,
                                             cmon_str_view _version);
CMON_API cmon_idx cmon_pm_find_or_add_module_c_str(cmon_pm * _pm,
                                                   const char * _url,
                                                   const char * _version);
CMON_API cmon_idx cmon_pm_find_c_str(cmon_pm * _pm, const char * _url, const char * _version);
CMON_API size_t cmon_pm_module_count(cmon_pm * _pm);
CMON_API const char * cmon_pm_module_url(cmon_pm * _pm, cmon_idx _idx);
CMON_API const char * cmon_pm_module_version(cmon_pm * _pm, cmon_idx _idx);
CMON_API size_t cmon_pm_module_dep_count(cmon_pm * _pm, cmon_idx _idx);
CMON_API cmon_idx cmon_pm_module_dep(cmon_pm * _pm, cmon_idx _mod_idx, size_t _dep_idx);

// add _dep module index to module _mod as a dependency
CMON_API cmon_bool cmon_pm_add_dep(cmon_pm * _pm, cmon_idx _mod, cmon_idx _dep);
// loads a tini file and adds all the dependencies to _mod
CMON_API cmon_bool cmon_pm_load_deps_file(cmon_pm * _pm, cmon_idx _mod, const char * _path);
// saves all dependencies of _mod to a tini file
CMON_API cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, cmon_idx _mod, const char * _path);
// recursively pulls and resolves all dependencies to generate a lock file.
CMON_API cmon_pm_lock_file * cmon_pm_resolve(cmon_pm * _pm);
// get the error message if an error occured on any pm function calls
CMON_API const char * cmon_pm_err_msg(cmon_pm * _pm);

// load a lock file from disk
//@TODO: Add more functions to get info about individual deps? not really neccessary for now but might be
//useful for testing alone...
CMON_API cmon_pm_lock_file * cmon_pm_lock_file_load(cmon_allocator * _alloc,
                                                    const char * _path,
                                                    char * _err_msg_buf,
                                                    size_t _buf_size);
CMON_API void cmon_pm_lock_file_destroy(cmon_pm_lock_file * _lf);
// clones all the dependencies specified in the lock file to the specified directory
CMON_API cmon_bool cmon_pm_lock_file_pull(cmon_pm_lock_file * _lf, const char * _dep_dir);
// remove all dependencies specified in the lock file from the provided directory
CMON_API cmon_bool cmon_pm_lock_file_clean_dep_dir(cmon_pm_lock_file * _lf, const char * _dep_dir);
// saves a lock file to disk
CMON_API cmon_bool cmon_pm_lock_file_save(cmon_pm_lock_file * _lf, const char * _path);
CMON_API const char * cmon_pm_lock_file_err_msg(cmon_pm_lock_file * _lf);

#endif // CMON_CMON_PM_H
