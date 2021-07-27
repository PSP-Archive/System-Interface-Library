/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/test-harness.c: Top-level control routine for running tests.
 */

#include "src/base.h"
#include "src/test.h"

#ifndef SIL_INCLUDE_TESTS

int run_internal_tests(UNUSED const char *tests_to_run)
{
    return 1;
}

#else  // SIL_INCLUDE_TESTS, to the end of the file.

#include "src/main.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/test/base.h"

/*************************************************************************/
/******************************* Local data ******************************/
/*************************************************************************/

/* Structure describing a single test routine (which may encompass several
 * distinct tests). */
typedef struct TestInfo TestInfo;
struct TestInfo {
    /* Data describing the test. */
    const char *name;           // Case-sensitive.
    const char *depends;        // Space-separated list of dependencies.
    int (*testfunc)(void);
    uint8_t run_before_cleanup; // Run before the cleanup_all() call?

    /* Internal state data. */
    uint8_t running;
    uint8_t finished;
    uint8_t result;
    uint8_t skipped;
};

/* List of all tests. */
static TestInfo tests[] = {
#define DEFINE_TEST(testname, dependencies) \
    {.name = #testname, .depends = dependencies, .testfunc = test_##testname}
#define DEFINE_EARLY_TEST(testname, dependencies) \
    {.name = #testname, .depends = dependencies, .testfunc = test_##testname, \
     .run_before_cleanup = 1}
#define DEFINE_GROUP(groupname, dependencies) \
    {.name = "GROUP:" #groupname, .depends = dependencies, .testfunc = NULL}

    /* Top-level routines */
    DEFINE_TEST (condvar,         "mutex semaphore thread utility_id_array"),
    DEFINE_TEST (debug,           "GROUP:graphics input"),
    DEFINE_TEST (endian,          ""),
    DEFINE_TEST (input,           "time"),
    DEFINE_TEST (main,            "GROUP:graphics GROUP:resource GROUP:sound"
                                  " input time"),
    DEFINE_TEST (memory,          "random"),
    DEFINE_TEST (mutex,           "thread utility_id_array"),
    DEFINE_TEST (random,          ""),
    DEFINE_TEST (semaphore,       "thread utility_id_array"),
    DEFINE_TEST (thread,          "utility_id_array"),
    DEFINE_TEST (time,            ""),
    DEFINE_TEST (userdata,        "graphics_texture memory thread"
                                  " utility_id_array"),
    DEFINE_TEST (workqueue,       "memory mutex semaphore thread"
                                  " utility_id_array"),

    /* font/... */
    DEFINE_GROUP(font, "font_bitmap font_core font_freetype font_sysfont"),
    DEFINE_TEST (font_bitmap,   "graphics_state graphics_texture memory"
                                " utility_font_file utility_id_array"),
    DEFINE_TEST (font_core,     "font_bitmap"),
    DEFINE_TEST (font_freetype, "font_core sys_files"),
    DEFINE_TEST (font_sysfont,  "font_core"),

    /* graphics/... */
    DEFINE_GROUP(graphics, "graphics_base graphics_clear_depth"
                           " graphics_clear_grab graphics_framebuffer"
                           " graphics_misc graphics_primitive"
                           " graphics_shader_gen graphics_shader_obj"
                           " graphics_state graphics_texture"
                           " graphics_texture_formats"),
    DEFINE_TEST (graphics_base,        "memory"),
    DEFINE_TEST (graphics_clear_depth, "graphics_state"),
    DEFINE_TEST (graphics_clear_grab,  "graphics_base"),
    DEFINE_TEST (graphics_framebuffer, "graphics_clear_depth graphics_state"
                                       " utility_id_array"),
    DEFINE_TEST (graphics_misc,        "graphics_state"),
    DEFINE_TEST (graphics_primitive,   "graphics_clear_grab graphics_texture"
                                       " utility_id_array"),
    DEFINE_TEST (graphics_shader_gen,  "graphics_state"),
    DEFINE_TEST (graphics_shader_obj,  "graphics_state graphics_texture"),
    DEFINE_TEST (graphics_state,       "graphics_primitive"),
    DEFINE_TEST (graphics_texture,     "graphics_clear_grab memory utility_dds"
                                       " utility_id_array utility_pixformat"
                                       " utility_png utility_tex_file"),
    DEFINE_TEST (graphics_texture_formats, "graphics_misc graphics_state"
                                           " graphics_texture"),

    /* math/... */
    DEFINE_GROUP(math, "math_dtrig math_matrix math_matrix_cxx math_rounding"
                       " math_vector math_vector_cxx"),
    DEFINE_TEST (math_dtrig,      ""),
    DEFINE_TEST (math_matrix,     ""),
    DEFINE_TEST (math_matrix_cxx, "math_matrix"),
    DEFINE_TEST (math_rounding,   ""),
    DEFINE_TEST (math_vector,     ""),
    DEFINE_TEST (math_vector_cxx, "math_vector"),

    /* movie/... */
    DEFINE_GROUP(movie, "movie_core movie_webm"),
    DEFINE_TEST (movie_core, "GROUP:graphics GROUP:resource GROUP:sound"
                             " memory utility_id_array"),
    DEFINE_TEST (movie_webm, "movie_core"),

    /* resource/... */
    DEFINE_GROUP(resource, "resource_core resource_pkg"),
    DEFINE_TEST (resource_core, "graphics_texture memory sys_files workqueue"
                                " GROUP:font GROUP:sound GROUP:utility"),
    DEFINE_TEST (resource_pkg,  "memory resource_core GROUP:utility"),

    /* sound/... */
    DEFINE_GROUP(sound, "sound_core"),
    DEFINE_TEST (sound_core,          "memory mutex sound_decode"
                                      " sound_decode_wav sound_decode_ogg"
                                      " sound_filter sound_filter_flange"
                                      " sound_mixer"),
    DEFINE_TEST (sound_decode,        "memory sys_files"),
    DEFINE_TEST (sound_decode_ogg,    "memory sound_decode"),
    DEFINE_TEST (sound_decode_wav,    "memory sound_decode"),
    DEFINE_TEST (sound_filter,        "memory"),
    DEFINE_TEST (sound_filter_flange, "memory sound_filter"),
    DEFINE_TEST (sound_mixer,         "memory mutex utility_memory"),

    /* sysdep/... (These only test the parts of sysdep which are not
     * separately tested by higher-level code: sys_debug is replaced with
     * stubs when testing but the real functions can also be reliably
     * tested, and sys_files and sys_log are internal modules with no
     * high-level interface.) */
    DEFINE_TEST (sys_debug, ""),
    DEFINE_TEST (sys_files, "memory"
#if !defined(SIL_PLATFORM_PSP)
                            " misc_ioqueue"
#endif
    ),
    DEFINE_TEST (sys_log,   "sys_files"
#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS)
                            " posix_fileutil"
#elif defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
                            " posix_fileutil posix_userdata"
#elif defined(SIL_PLATFORM_WINDOWS)
                            " windows_userdata"
#endif
    ),

    /* sysdep/android/... */
#if defined(SIL_PLATFORM_ANDROID)
    DEFINE_GROUP(android, "android_misc"),
    DEFINE_TEST (android_misc, "graphics_base semaphore thread"),
#endif

    /* sysdep/darwin/... */
#if defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)
    DEFINE_GROUP(darwin, "darwin_time"),
    DEFINE_TEST (darwin_time, ""),
#endif

    /* sysdep/ios/... */
#if defined(SIL_PLATFORM_IOS)
    DEFINE_GROUP(ios, "ios_graphics ios_graphics_device_size_early ios_util"),
    DEFINE_TEST (ios_graphics, "graphics_base"),
    DEFINE_EARLY_TEST(ios_graphics_device_size_early, ""),
    DEFINE_TEST (ios_util,     ""),
#endif

    /* sysdep/linux/... */
#if defined(SIL_PLATFORM_LINUX)
    DEFINE_GROUP(linux, "linux_graphics_fs_early linux_graphics_fs_methods"
                        " linux_graphics_fs_minimize linux_graphics_fs_mode"
                        " linux_graphics_modes linux_graphics_window"
                        " linux_graphics_x11_base linux_graphics_x11_events"
                        " linux_graphics_xinerama linux_input linux_main"
                        " linux_meminfo linux_misc linux_posix_fileutil"
                        " linux_sound linux_sysfont linux_userdata"),
    DEFINE_TEST (linux_graphics_fs_early,    "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_fs_methods,  "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_fs_minimize, "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_fs_mode,     "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_modes,       "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_window,      "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_x11_base,    "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_x11_events,  "GROUP:graphics"),
    DEFINE_TEST (linux_graphics_xinerama,    "GROUP:graphics"),
    DEFINE_TEST (linux_input,    "graphics_base input memory"),
    DEFINE_TEST (linux_main,     ""),
    DEFINE_TEST (linux_meminfo,  ""),
    DEFINE_TEST (linux_misc,     "memory posix_fileutil utility_misc"),
    DEFINE_TEST (linux_posix_fileutil, "posix_fileutil"),
    DEFINE_TEST (linux_sound,    "GROUP:sound"),
    DEFINE_TEST (linux_sysfont,  "font_sysfont"),
    DEFINE_TEST (linux_userdata, "posix_userdata"),
#endif

    /* sysdep/macosx/... */
#if defined(SIL_PLATFORM_MACOSX)
    DEFINE_GROUP(macosx, "macosx_graphics macosx_util"),
    DEFINE_TEST (macosx_graphics, "GROUP:graphics"),
    DEFINE_TEST (macosx_input,    "graphics_base input"),
    DEFINE_TEST (macosx_util,     ""),
#endif

    /* sysdep/misc/... */
#if !defined(SIL_PLATFORM_PSP)
    DEFINE_TEST (misc_ioqueue,      "condvar memory mutex thread"),
#endif
#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
    DEFINE_TEST (misc_joystick_db,  ""),
#endif
#if defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
    DEFINE_TEST (misc_joystick_hid, "memory"),
#endif
#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
    DEFINE_TEST (misc_log_stdio,    "sys_files posix_fileutil posix_userdata"),
#endif

    /* sysdep/opengl/... */
#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
    DEFINE_GROUP(opengl, "GROUP:opengl_features opengl_framebuffer"
                         " opengl_graphics opengl_primitive opengl_shader"
                         " opengl_shader_gen opengl_state opengl_texture"
                         " opengl_version"),
    DEFINE_GROUP(opengl_features, "opengl_features_delayed_delete"
                                  " opengl_features_delayed_delete_vao"
                                  " opengl_features_no_genmipmap"
                                  " opengl_features_no_getteximage"
                                  " opengl_features_no_int_attrib"
                                  " opengl_features_no_quads"
                                  " opengl_features_no_rg"
                                  " opengl_features_no_separate_shaders"
                                  " opengl_features_vao_mandatory"
                                  " opengl_features_vao_static"
                                  " opengl_features_wrap_dsa"),
    DEFINE_TEST (opengl_features_delayed_delete,
                                  "graphics_framebuffer graphics_primitive"
                                  " graphics_state graphics_texture"
                                  " opengl_graphics"),
    DEFINE_TEST (opengl_features_delayed_delete_vao,
                                  "graphics_primitive"
                                  " opengl_features_delayed_delete"
                                  " opengl_features_vao_mandatory"),
    DEFINE_TEST (opengl_features_no_genmipmap,
                                  "graphics_primitive graphics_texture"
                                  " opengl_texture"),
    DEFINE_TEST (opengl_features_no_getteximage,
                                  "graphics_texture opengl_texture"),
    DEFINE_TEST (opengl_features_no_int_attrib,
                                  "graphics_shader_obj"),
    DEFINE_TEST (opengl_features_no_quads,
                                  "graphics_primitive graphics_state"
                                  " opengl_primitive"),
    DEFINE_TEST (opengl_features_no_rg,
                                  "graphics_primitive graphics_texture"
                                  " graphics_texture_formats opengl_texture"),
    DEFINE_TEST (opengl_features_no_separate_shaders,
                                  "graphics_shader_obj opengl_shader"),
    DEFINE_TEST (opengl_features_vao_mandatory,
                                  "graphics_primitive graphics_shader_gen"
                                  " graphics_shader_obj opengl_primitive"),
    DEFINE_TEST (opengl_features_vao_static,
                                  "graphics_primitive graphics_shader_gen"
                                  " graphics_shader_obj opengl_primitive"),
    DEFINE_TEST (opengl_features_wrap_dsa,
                                  "graphics_framebuffer graphics_primitive"
                                  " graphics_shader_gen graphics_shader_obj"
                                  " graphics_state graphics_texture"
                                  " graphics_texture_formats"
                                  " opengl_framebuffer opengl_primitive"
                                  " opengl_state opengl_shader"
                                  " opengl_shader_gen opengl_texture"),
    DEFINE_TEST (opengl_framebuffer, "GROUP:graphics"),
    DEFINE_TEST (opengl_graphics,    "GROUP:graphics"),
    DEFINE_TEST (opengl_primitive,   "GROUP:graphics"),
    DEFINE_TEST (opengl_shader,      "GROUP:graphics"),
    DEFINE_TEST (opengl_shader_gen,  "GROUP:graphics"),
    DEFINE_TEST (opengl_state,       "GROUP:graphics"),
    DEFINE_TEST (opengl_texture,     "GROUP:graphics"),
    DEFINE_TEST (opengl_version,     "GROUP:graphics"),
#endif

    /* sysdep/posix/... */
#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
    DEFINE_GROUP(posix, "posix_files posix_fileutil posix_misc posix_thread"
# if !defined(SIL_PLATFORM_IOS) && !defined(SIL_PLATFORM_MACOSX)
                        " posix_time"
# endif
                        " posix_userdata"),
    DEFINE_TEST (posix_files,    "random sys_files posix_fileutil"),
    DEFINE_TEST (posix_fileutil, "memory random utility_strformat"),
    DEFINE_TEST (posix_misc,     "misc_ioqueue sys_files"),
    DEFINE_TEST (posix_thread,   "semaphore thread"),
    /* OSX and iOS use the Darwin timekeeping routines instead. */
# if !defined(SIL_PLATFORM_IOS) && !defined(SIL_PLATFORM_MACOSX)
    DEFINE_TEST (posix_time,     "thread"),
# endif
    DEFINE_TEST (posix_userdata, "memory mutex userdata posix_fileutil"),
#endif

    /* sysdep/psp/... */
#ifdef SIL_PLATFORM_PSP
    DEFINE_GROUP(psp, "psp_files psp_font psp_graphics psp_input psp_misc"
                      " psp_movie psp_sound_mp3 psp_texture psp_time"
                      " psp_userdata"),
    DEFINE_TEST (psp_files,     "sys_files"),
    DEFINE_TEST (psp_font,      "font_bitmap"),
    DEFINE_TEST (psp_graphics,  "GROUP:graphics psp_texture"),
    DEFINE_TEST (psp_input,     ""),
    DEFINE_TEST (psp_misc,      ""),
    DEFINE_TEST (psp_movie,     "movie_core"),
    DEFINE_TEST (psp_sound_mp3, "sound_decode thread"),
    DEFINE_TEST (psp_texture,   "graphics_primitive graphics_texture"
                                " graphics_texture_formats"),
    DEFINE_TEST (psp_time,      ""),
    DEFINE_TEST (psp_userdata,  "resource_core thread userdata"),
#endif

    /* sysdep/windows/... */
#ifdef SIL_PLATFORM_WINDOWS
    DEFINE_GROUP(windows, "windows_condvar windows_files windows_graphics"
                          " windows_input windows_main windows_misc"
                          " windows_thread windows_time windows_userdata"
                          " windows_utf8_wrappers windows_util"),
    DEFINE_TEST (windows_condvar,       "condvar"),
    DEFINE_TEST (windows_d3d_core,      "GROUP:graphics"),
    DEFINE_TEST (windows_files,         "semaphore sys_files thread"
                                        " windows_utf8_wrappers"),
    DEFINE_TEST (windows_graphics,      "GROUP:graphics"),
    DEFINE_TEST (windows_input,         "graphics_base input memory"
                                        " windows_utf8_wrappers"),
    DEFINE_TEST (windows_main,          "memory utility_misc"),
    DEFINE_TEST (windows_misc,          ""),
    DEFINE_TEST (windows_thread,        "thread"),
    DEFINE_TEST (windows_time,          ""),
    DEFINE_TEST (windows_userdata,      "userdata windows_utf8_wrappers"
                                        " utility_utf8"),
    DEFINE_TEST (windows_utf8_wrappers, "memory random windows_util"),
    DEFINE_TEST (windows_util,          "memory utility_utf8"),
#endif

    /* utility/... */
    DEFINE_GROUP(utility, "utility_compress utility_dds utility_font_file"
                          " utility_id_array utility_log utility_memory"
                          " utility_misc utility_png utility_strdup"
                          " utility_strformat utility_stricmp utility_strtof"
                          " utility_tex_file utility_tinflate utility_utf8"
                          " utility_zlib"),
    DEFINE_TEST (utility_compress,  "memory utility_tinflate utility_zlib"),
    DEFINE_TEST (utility_dds,       ""),
    DEFINE_TEST (utility_font_file, ""),
    DEFINE_TEST (utility_id_array,  "memory random"),
    DEFINE_TEST (utility_log,       ""),
    DEFINE_TEST (utility_memory,    ""),
    DEFINE_TEST (utility_misc,      "memory"),
    DEFINE_TEST (utility_pixformat, ""),
    DEFINE_TEST (utility_png,       "memory"),
    DEFINE_TEST (utility_strdup,    ""),
    DEFINE_TEST (utility_strformat, "memory"),
    DEFINE_TEST (utility_stricmp,   ""),
    DEFINE_TEST (utility_strtof,    ""),
    DEFINE_TEST (utility_tex_file,  ""),
    DEFINE_TEST (utility_tinflate,  ""),
    DEFINE_TEST (utility_utf8,      ""),
    DEFINE_TEST (utility_yuv2rgb,   ""),
    DEFINE_TEST (utility_zlib,      "memory sys_files"),

#undef DEFINE_TEST
#undef DEFINE_GROUP
};

/*-----------------------------------------------------------------------*/

/* Flag:  Are we running tests?  (Returned by is_running_tests().) */
static uint8_t running_tests;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * find_test:  Look up a test by name and return its TestInfo structure.
 * Note that test names are case sensitive.
 *
 * [Parameters]
 *     name: Name to look up.
 *     namelen: Length of name, in bytes.
 * [Return value]
 *     TestInfo pointer for the named test, or NULL if the test was not found.
 */
static TestInfo *find_test(const char *name, int namelen);

/**
 * run_tests:  Run all tests selected by the function parameters.
 *
 * [Parameters]
 *     tests_to_run: List of tests to run, as for run_internal_tests().
 *     early: True to run tests with the run_before_cleanup flag set,
 *         false to run tests with the flag clear.
 * [Return value]
 *     True if all tests succeeded, false otherwise.
 */
static int run_tests(const char *tests_to_run, int early);

/**
 * run_one_test:  Run the given test and return its result.  If any
 * dependent tests have not yet run, those tests are run first; if a
 * dependency fails, this test is skipped (but all dependencies are run
 * regardless of any failures).
 *
 * [Parameters]
 *     test: Test to run.
 *     with_dependencies: True to run the given test's dependencies before
 *         the test itself, false to run the given test only.
 * [Return value]
 *     True if the test succeeded; false if the test failed or was skipped
 *     due to failure of a dependency.
 */
static int run_one_test(TestInfo *test, int with_dependencies);

/**
 * show_results:  Report results of all tests via the DLOG() interface.
 *
 * [Parameters]
 *     result: Overall result of tests (true if all tests succeeded, false
 *         otherwise).
 */
static void show_results(int result);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int run_internal_tests(const char *tests_to_run)
{
    PRECOND(tests_to_run != NULL, tests_to_run = "");

    /* First run any tests which need to precede the cleanup_all() call
     * below. */
    int result = run_tests(tests_to_run, 1);

    /* Undo the init_all() in sil__main() before running tests because we
     * test init/cleanup behavior as well. */
    cleanup_all();
    result &= run_tests(tests_to_run, 0);
    ASSERT(init_all());

    show_results(result);
    return result;
}

/*-----------------------------------------------------------------------*/

int is_running_tests(void)
{
    return running_tests;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static TestInfo *find_test(const char *name, int namelen)
{
    for (int i = 0; i < lenof(tests); i++) {
        if (strncmp(name, tests[i].name, namelen) == 0
         && tests[i].name[namelen] == 0) {
            return &tests[i];
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

static int run_tests(const char *tests_to_run, int early)
{
    int result = 1;

    running_tests = 1;
    if (*tests_to_run) {
        const char *name = tests_to_run + strspn(tests_to_run, ",");
        while (*name) {
            const char *name_end = name + strcspn(name, ",");
            int with_dependencies = 1;
            if (*name == '=') {
                name++;
                with_dependencies = 0;
            }
            const int namelen = name_end - name;
            TestInfo *test = find_test(name, namelen);
            if (test) {
                if ((test->run_before_cleanup != 0) == (early != 0)) {
                    result &= run_one_test(test, with_dependencies);
                }
            } else {
                testlog_log(__FILE__, __LINE__, __FUNCTION__, TESTLOG_WARN,
                            "WARN: Test %.*s not found", namelen, name);
            }
            name = name_end + strspn(name_end, ",");
        }
    } else {
        for (int i = 0; i < lenof(tests); i++) {
            if ((tests[i].run_before_cleanup != 0) == (early != 0)) {
                result &= run_one_test(&tests[i], 1);
            }
        }
    }
    running_tests = 0;

    return result;
}

/*-----------------------------------------------------------------------*/

static int run_one_test(TestInfo *test, int with_dependencies)
{
    if (test->finished) {
        return test->result;
    }

    if (test->running) {
        testlog_log(__FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,
                    "FAIL: %s: Dependency loop detected", test->name);
        /* test->skipped will be set by the upper call when it detects the
         * dependency failure. */
        return 0;
    }

    test->running = 1;

    if (with_dependencies) {
        const char *s = test->depends;
        while (*s) {
            const char *eow = s + strcspn(s, " ");
            TestInfo *dependency = find_test(s, eow-s);
            if (dependency) {
                if (!run_one_test(dependency, 1)) {
                    test->skipped = 1;
                }
            } else {
                testlog_log(__FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,
                            "FAIL: %s: Dependency %.*s not found", test->name,
                            (int)(eow-s), s);
                test->skipped = 1;
            }
            s = eow + strspn(eow, " ");
        }
    }
    if (test->skipped) {
        test->result = 0;
    } else {
        if (test->testfunc) {
            const int64_t used_before = mem_debug_bytes_allocated();
            test->result = ((*test->testfunc)() != 0);
            const int64_t used_after = mem_debug_bytes_allocated();
            if (test->result && used_after > used_before) {
                mem_debug_report_allocs();
                testlog_log(__FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,
                            "FAIL: Test %s leaked memory (%lld bytes)",
                            test->name, (long long)(used_after - used_before));
                test->result = 0;
            }
        } else {
            test->result = 1;  // Just a group of other tests.
        }
    }

    test->finished = 1;
    test->running = 0;

    return test->result;
}

/*-----------------------------------------------------------------------*/

static void show_results(int result)
{
    DLOG("======== TEST RESULTS ========");
    if (result) {
        DLOG("All tests passed.");
    } else {
        for (int i = 0; i < lenof(tests); i++) {
            if (tests[i].testfunc     // Don't display groups.
             && tests[i].finished) {  // Don't display tests not run.
                if (tests[i].result) {
                    DLOG("     passed: %s", tests[i].name);
                } else if (tests[i].skipped) {
                    DLOG("[*] skipped: %s", tests[i].name);
                } else {
                    DLOG("[*]  FAILED: %s", tests[i].name);
                }
            }
        }
    }
    DLOG("==============================");
    if (!result) {
        DLOG("Failure log follows:");
        testlog_print();
    } else if (testlog_count_entries(TESTLOG_WARN) > 0) {
        DLOG("Some warnings were generated:");
        testlog_print();
    }
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_INCLUDE_TESTS
