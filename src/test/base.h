/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/base.h: Base header file for use by test routines.
 */

/*
 * This file defines/declares various macros and utility functions for use
 * in test code.  These include:
 *
 * - FAIL(), which logs a test failure and executes the statement defined
 *   by the preprocessor symbol FAIL_ACTION (by default, this is "return 0"
 *   to abort the test function);
 *
 * - WARN(), which logs non-fatal test messages;
 *
 * - SKIP(), which logs an informative message and skips the current test;
 *
 * - the CHECK_*() macros, which check a condition and FAIL() with an
 *   appropriate failure message if the condition does not hold; and
 *
 * - testlog_log(), which can be used to log a failure or warning message
 *   without failing the test function (for example, to log additional
 *   details about a failure or to indicate when a test cannot be run).
 *
 * In addition to these macros and functions, test source files can take
 * advantage of a generic testing framework provided by the build scripts
 * to simplify the implementation of test routines.  To use the generic
 * framework for a given source file, define test functions using the
 * TEST(), TEST_INIT(), and TEST_CLEANUP() macros (see below), then invoke
 * DEFINE_GENERIC_TEST_RUNNER() (anywhere in the file, at file scope) to
 * define the test runner, which will be called from test-harness.c.  The
 * build rules in build/common/rules.mk take care of massaging the source
 * code as needed to build the tests properly.
 *
 * When using the generic testing framework, the name of the current test
 * function can be retrieved by calling the function CURRENT_TEST_NAME().
 * This can be used in initialization and cleanup functions, for example,
 * to alter behavior for specific tests.
 */

#ifndef SIL_SRC_TEST_BASE_H
#define SIL_SRC_TEST_BASE_H

#include "src/memory.h"  // Needed by the generic test framework.
#include "src/utility/log.h"  // Needed by CHECK_DLOG().

EXTERN_C_BEGIN

/*************************************************************************/
/**************** Generic test framework types and macros ****************/
/*************************************************************************/

/**
 * _DEFINE_GENERIC_TESTS:  Internal macro to define the list of tests for
 * the current file.
 *
 * Note that the definition here is a stub that should never be called.
 * Normally, the build process will generate a header for each test source
 * file, defining this macro with the list of test functions in that file.
 * See build/common/rules.mk for details.
 */
#ifndef _DEFINE_GENERIC_TESTS
# define _DEFINE_GENERIC_TESTS \
    {0}}; \
    DLOG("_DEFINE_GENERIC_TESTS definition missing for %s -- build error?", \
         __FILE__); \
    return 0; \
    static struct {enum TestType a; void *b, *c;} dummy_for_missing_tests[] = {
#endif

/* Logging helper for the generic test runner. */
#ifdef SIL_TEST_VERBOSE_LOGGING
# define _TEST_VLOG  DLOG
#else
# define _TEST_VLOG(...)  /*nothing*/
#endif

/**
 * DEFINE_GENERIC_TEST_RUNNER:  Define a test runner for test functions
 * using the generic framework.  The function defined by this macro takes
 * no parameters, and returns true if all tests succeeded, false otherwise.
 *
 * Test functions are called in the order they are defined in the source
 * file.  For each test function defined with the TEST() macro (see below),
 * the test runner does the following:
 *
 * - Calls each initialization function defined with TEST_INIT(), in the
 *   order those functions are defined in the file.  If any initialization
 *   function fails, the test runner immediately aborts and returns
 *   failure.
 *
 * - Calls the test function itself.
 *
 * - Calls each cleanup function defined with TEST_CLEANUP(), in the order
 *   those functions are defined in the file.  If any cleanup function
 *   fails, the test runner immediately aborts and returns failure.
 *
 * _Do not_ include a semicolon after an invocation of this macro.
 *
 * [Parameters]
 *     runner_name: Name of test runner function to define.
 */
#define DEFINE_GENERIC_TEST_RUNNER(runner_name)                         \
int runner_name(void) {                                                 \
    enum TestType {_DUMMY, _INIT, _TEST, _CLEANUP};                     \
    static struct {                                                     \
        enum TestType type;                                             \
        const char *name;                                               \
        int (*routine)(void);                                           \
    } const tests[] = {                                                 \
        _DEFINE_GENERIC_TESTS                                           \
        {_DUMMY, NULL, NULL}  /* Avoid zero-length array warnings. */   \
    };                                                                  \
    int result = 1;                                                     \
    for (int i = 0; i < lenof(tests); i++) {                            \
        if (tests[i].type == _TEST) {                                   \
            _TEST_VLOG("=== START %s ===", tests[i].name);              \
            _current_test_name = tests[i].name;                         \
            const int64_t used_before = mem_debug_bytes_allocated();    \
            for (int j = 0; j < lenof(tests); j++) {                    \
                if (tests[j].type == _INIT) {                           \
                    _TEST_VLOG("--- INIT %s (%s) ---",                  \
                               tests[i].name, tests[j].name);           \
                    if (!(*tests[j].routine)()) {                       \
                        FAIL("Init routine %s failed for test %s,"      \
                             " aborting", tests[j].name, tests[i].name);\
                    }                                                   \
                }                                                       \
            }                                                           \
            _TEST_VLOG("--- RUN %s ---", tests[i].name);                \
            int this_result = (*tests[i].routine)();                    \
            for (int j = 0; j < lenof(tests); j++) {                    \
                if (tests[j].type == _CLEANUP) {                        \
                    _TEST_VLOG("--- CLEANUP %s (%s) ---",               \
                               tests[i].name, tests[j].name);           \
                    if (!(*tests[j].routine)()) {                       \
                        FAIL("Cleanup routine %s failed for test %s,"   \
                             " aborting", tests[j].name, tests[i].name);\
                    }                                                   \
                }                                                       \
            }                                                           \
            if (this_result) {                                          \
                const int64_t used_after = mem_debug_bytes_allocated(); \
                if (used_after > used_before) {                         \
                    testlog_log(                                        \
                        __FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL, \
                        "FAIL: Test %s leaked memory (%lld bytes)",     \
                        tests[i].name,                                  \
                        (long long)(used_after - used_before));         \
                    mem_debug_report_allocs();                          \
                    DLOG("End of leak report for test %s", tests[i].name);\
                    this_result = 0;                                    \
                }                                                       \
            }                                                           \
            _TEST_VLOG("=== END %s ===", tests[i].name);                \
            result &= this_result;                                      \
        }                                                               \
    }                                                                   \
    return result;                                                      \
}

/**
 * TEST:  Define a test function (a "test case" in xUnit parlance) using
 * the generic testing framework.  The function should return true if the
 * test succeeds, false if not.
 *
 * Use this macro in place of a function definition line.  For example:
 *
 * TEST(test_something)
 * {
 *     CHECK_INTEQUAL(foo, 42);
 *     return 1;
 * }
 *
 * The above will define a test function "int test_something(void)", which
 * will automatically be called by the file's test runner (defined with
 * DEFINE_GENERIC_TEST_RUNNER()).
 *
 * [Parameters]
 *     name: Function name.
 */
#ifndef TEST
# define TEST(name)  static int name(void)
#endif

/**
 * TEST_INIT, TEST_CLEANUP:  Define common initialization or cleanup
 * functions for all test functions in the source file ("test fixture
 * setup/teardown" in xUnit parlance).  The function should return true
 * if initialization or cleanup is successful, false if not.
 *
 * As with TEST(), use these macros in place of function definition lines.
 *
 * [Parameters]
 *     name: Function name.
 */
#ifndef TEST_INIT
# define TEST_INIT(name)  static int name(void)
#endif
#ifndef TEST_CLEANUP
# define TEST_CLEANUP(name)  static int name(void)
#endif

/*-----------------------------------------------------------------------*/

/* Name of the current test.  This is set by the generic test runner and
 * should not be referenced outside this header file. */
UNUSED static const char *_current_test_name;

/**
 * CURRENT_TEST_NAME:  Return the function name of the current test (the
 * name given to the TEST() macro).
 */
UNUSED static inline const char *CURRENT_TEST_NAME(void)
{
    return _current_test_name;
}

/*************************************************************************/
/******************* Test helper functions and macros ********************/
/*************************************************************************/

/**
 * FLOAT_NAN, DOUBLE_NAN:  Return a single- or double-precision quiet NaN.
 *
 * [Return value]
 *     qnan(0x0)
 */
static inline CONST_FUNCTION float FLOAT_NAN(void)
{
    union {float f; uint32_t i;} u;
    u.i = 0x7FC00000;
    return u.f;
}

static inline CONST_FUNCTION double DOUBLE_NAN(void)
{
    union {double f; uint64_t i;} u;
    u.i = 0x7FF8000000000000;
    return u.f;
}

/*-----------------------------------------------------------------------*/

/**
 * FLOAT_BITS, DOUBLE_BITS:  Return the bits of a "float" or "double"
 * variable, respectively, as an integer.  Useful for reporting errors in
 * floating-point values.  The return values are of "long" and "long long"
 * size rather than sized integer types to facilitate use in printf()
 * regardless of the environment.
 */
static inline CONST_FUNCTION unsigned long FLOAT_BITS(float f)
{
    union {float f; uint32_t i;} u;
    u.f = f;
    return u.i;
}

static inline CONST_FUNCTION unsigned long long DOUBLE_BITS(double d)
{
    union {double d; uint64_t i;} u;
    u.d = d;
    return u.i;
}

/*-----------------------------------------------------------------------*/

/**
 * FAIL_ACTION:  Action to take on failure.  This may be defined (or
 * redefined) by the including file to any valid statement; for example,
 * a statement like "failed = 1" can be used to make test failures "soft",
 * allowing the test to continue running even after a failure.
 *
 * The default failure action is to abort the current function and return
 * false to the caller.
 */
#ifndef FAIL_ACTION
# define FAIL_ACTION  return 0
#endif

/*-----------------------------------------------------------------------*/

/**
 * FAIL:  Report a test failure via DLOG() and execute the test failure
 * action.  The test failure is also logged for later retrieval.
 *
 * [Parameters]
 *     ...: Arguments to DLOG().
 */
#define FAIL(...)  do {                              \
    testlog_log(__FILE__, __LINE__, __FUNCTION__,    \
                TESTLOG_FAIL, "FAIL: " __VA_ARGS__); \
    FAIL_ACTION;                                     \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * WARN:  Report a non-fatal test error via DLOG().  The error is also
 * logged for later retrieval.
 *
 * [Parameters]
 *     ...: Arguments to DLOG().
 */
#define WARN(...)  testlog_log(__FILE__, __LINE__, __FUNCTION__, \
                               TESTLOG_WARN, "WARN: " __VA_ARGS__)

/*-----------------------------------------------------------------------*/

/**
 * SKIP:  Report the skipping of a test via DLOG() and return true from
 * the current function.  The message is also logged for later retrieval.
 *
 * [Parameters]
 *     ...: Arguments to DLOG().
 */
#define SKIP(...)  do {                                      \
    testlog_log(__FILE__, __LINE__, __FUNCTION__,            \
                TESTLOG_SKIP, "Test skipped: " __VA_ARGS__); \
    return 1;                                                \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_TRUE, CHECK_FALSE:  Check that the given value is either true
 * (non-zero and non-NULL) or false (zero or NULL), and FAIL() if not.
 *
 * When checking the result of an auxiliary function (such as mem_alloc()
 * or strformat_check() when not testing those functions directly), it is
 * generally preferable to use ASSERT() instead of CHECK_TRUE(), since
 * such failures are usually caused by problems with the test itself
 * rather than bugs in the code under test.
 *
 * [Parameters]
 *     value: Value to check.
 */
#define CHECK_TRUE(value)  do {                                 \
    if (!(value)) {                                             \
        FAIL("%s was not true as expected", #value);            \
    }                                                           \
} while (0)

#define CHECK_FALSE(value)  do {                                \
    if ((value)) {                                              \
        FAIL("%s was not false as expected", #value);           \
    }                                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/* Work around warning bugs in various GCC versions. */
#ifdef __GNUC__
# if __GNUC__ == 8  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87041
#  pragma GCC diagnostic ignored "-Wformat"
# endif
# if __GNUC__ >= 8  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87489
#  pragma GCC diagnostic ignored "-Wnonnull"
# endif
#endif

/**
 * CHECK_INTEQUAL, CHECK_FLOATEQUAL, CHECK_DOUBLEEQUAL, CHECK_PTREQUAL,
 * CHECK_STREQUAL, CHECK_MEMEQUAL:  Check that the given value is equal to
 * the expected value, and FAIL() if not.
 *
 * [Parameters]
 *     value: Value to check.
 *     expected: Expected value.
 */
#define CHECK_INTEQUAL(value,expected)  do {                    \
    const intmax_t _value = (value);                            \
    const intmax_t _expected = (expected);                      \
    if (_value != _expected) {                                  \
        FAIL("%s was %jd but should have been %jd",             \
             #value, _value, _expected);                        \
    }                                                           \
} while (0)

#define CHECK_FLOATEQUAL(value,expected)  do {                  \
    const float _value = (value);                               \
    const float _expected = (expected);                         \
    if (_value != _expected) {                                  \
        FAIL("%s was %g (%08lX) but should have been %g"        \
             " (%08lX)", #value, _value, FLOAT_BITS(_value),    \
             _expected, FLOAT_BITS(_expected));                 \
    }                                                           \
} while (0)

#define CHECK_DOUBLEEQUAL(value,expected)  do {                 \
    const double _value = (value);                              \
    const double _expected = (expected);                        \
    if (_value != _expected) {                                  \
        FAIL("%s was %g (%016llX) but should have been %g"      \
             " (%016llX)", #value, _value, DOUBLE_BITS(_value), \
             _expected, DOUBLE_BITS(_expected));                \
    }                                                           \
} while (0)

#define CHECK_PTREQUAL(value,expected)  do {                    \
    const void * const _value = (value);                        \
    const void * const _expected = (expected);                  \
    if (_value != _expected) {                                  \
        FAIL("%s was %p but should have been %p",               \
             #value, _value, _expected);                        \
    }                                                           \
} while (0)

#define CHECK_STREQUAL(value,expected)  do {                    \
    const char * const _value = (value);                        \
    const char * const _expected = (expected);                  \
    if (!_value && _expected) {                                 \
        FAIL("%s was NULL but should have been [%s]",           \
             #value, _expected);                                \
    } else if (_value && !_expected) {                          \
        FAIL("%s was [%s] but should have been NULL",           \
             #value, _value);                                   \
    } else if (_value && _expected                              \
               && strcmp(_value, _expected) != 0) {             \
        FAIL("%s was [%s] but should have been [%s]",           \
             #value, _value, _expected);                        \
    }                                                           \
} while (0)

#define CHECK_MEMEQUAL(value,expected,size)  do {               \
    const void * const _value = (value);                        \
    const void * const _expected = (expected);                  \
    const int _size = (size);                                   \
    if (!_value) {                                              \
        FAIL("%s was NULL but should not have been", #value);   \
    } else if (memcmp(_value, _expected, _size) != 0) {         \
        FAIL("%s %s", #value,                                   \
             _memequal_failure_message(_value, _expected, _size)); \
    }                                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_INTRANGE, CHECK_FLOATRANGE, CHECK_DOUBLERANGE:  Check that the
 * given value is within the expected range, and FAIL() if not.
 *
 * [Parameters]
 *     value: Value to check.
 *     low, high: Minimum and maximum expected values (inclusive).
 */
#define CHECK_INTRANGE(value,low,high)  do {                    \
    const intmax_t _value = (value);                            \
    const intmax_t _low = (low);                                \
    const intmax_t _high = (high);                              \
    if (_value < _low || _value > _high) {                      \
        FAIL("%s was %jd but should have been between %jd and %jd", \
             #value, _value, _low, _high);                      \
    }                                                           \
} while (0)

/* Note the form of the comparison below -- it's written so that
 * NaNs will fail the test. */
#define CHECK_FLOATRANGE(value,low,high)  do {                  \
    const float _value = (value);                               \
    const float _low = (low);                                   \
    const float _high = (high);                                 \
    if (!(_value >= _low && _value <= _high)) {                 \
        FAIL("%s was %g (%08lX) but should have been between"   \
             " %g (%08lX) and %g (%08lX)", #value, _value,      \
             FLOAT_BITS(_value), _low, FLOAT_BITS(_low),        \
             _high, FLOAT_BITS(_high));                         \
    }                                                           \
} while (0)

#define CHECK_DOUBLERANGE(value,low,high)  do {                 \
    const double _value = (value);                              \
    const double _low = (low);                                  \
    const double _high = (high);                                \
    if (!(_value >= _low && _value <= _high)) {                 \
        FAIL("%s was %g (%016llX) but should have been between" \
             " %g (%016llX) and %g (%016llX)", #value, _value,  \
             DOUBLE_BITS(_value), _low, DOUBLE_BITS(_low),      \
             _high, DOUBLE_BITS(_high));                        \
    }                                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_FLOATNEAR, CHECK_DOUBLENEAR:  Check that the given value is "near"
 * the expected value with a given margin of error, and FAIL() if not.
 *
 * [Parameters]
 *     value: Value to check.
 *     expected: Expected value.
 *     max_error: Maximum allowable difference from the expected value.
 */
#define CHECK_FLOATNEAR(value,expected,max_error)  do {         \
    const float _value = (value);                               \
    const float _expected = (expected);                         \
    const float _max_error = (max_error);                       \
    const float _low = _expected - _max_error;                  \
    const float _high = _expected + _max_error;                 \
    if (!(_value >= _low && _value <= _high)) {                 \
        FAIL("%s was %g (%08lX) but should have been near %g"   \
             " (%08lX)", #value, _value, FLOAT_BITS(_value),    \
             _expected, FLOAT_BITS(_expected));                 \
    }                                                           \
} while (0)

#define CHECK_DOUBLENEAR(value,expected,max_error)  do {        \
    const double _value = (value);                              \
    const double _expected = (expected);                        \
    const double _max_error = (max_error);                      \
    const double _low = _expected - _max_error;                 \
    const double _high = _expected + _max_error;                \
    if (!(_value >= _low && _value <= _high)) {                 \
        FAIL("%s was %g (%016llX) but should have been near %g" \
             " (%016llX)", #value, _value, DOUBLE_BITS(_value), \
             _expected, DOUBLE_BITS(_expected));                \
    }                                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_STRSTARTS, CHECK_STRENDS:  Check that the given value starts or
 * ends, respectively, with the given string, and FAIL() if not.
 *
 * [Parameters]
 *     value: Value to check.
 *     expected: Expected initial or final string fragment.
 */
#define CHECK_STRSTARTS(value,expected)  do {                   \
    const char * const _value = (value);                        \
    const char * const _expected = (expected);                  \
    if (!_value && _expected) {                                 \
        FAIL("%s was NULL but should have started with [%s]",   \
             #value, _expected);                                \
    } else if (_value && !_expected) {                          \
        FAIL("%s was [%s] but should have been NULL",           \
             #value, _value);                                   \
    } else if (_value && _expected                              \
               && strncmp(_value, _expected, strlen(_expected)) != 0) { \
        FAIL("%s was [%s] but should have started with [%s]",   \
             #value, _value, _expected);                        \
    }                                                           \
} while (0)

#define CHECK_STRENDS(value,expected)  do {                     \
    const char * const _value = (value);                        \
    const char * const _expected = (expected);                  \
    if (!_value && _expected) {                                 \
        FAIL("%s was NULL but should have ended with [%s]",     \
             #value, _expected);                                \
    } else if (_value && !_expected) {                          \
        FAIL("%s was [%s] but should have been NULL",           \
             #value, _value);                                   \
    } else if (_value && _expected                              \
               && strcmp(_value + strlen(_value) - strlen(_expected), \
                          _expected) != 0) {                    \
        FAIL("%s was [%s] but should have ended with [%s]",     \
             #value, _value, _expected);                        \
    }                                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_MEMORY_FAILURES, CHECK_MEMORY_FAILURES_TO,
 * CHECK_MEMORY_FAILURES_SHRINK:  Check that an operation properly fails in
 * the presence of memory allocation failures.
 *
 * The CHECK_MEMORY_FAILURES macro takes a single expression (expr) which
 * evaluates to false on failure, true on success, and forces a
 * mem_[re]alloc() failure at progressively later points until the
 * expression evaluates to true.
 *
 * The CHECK_MEMORY_FAILURES_TO macro takes an additional max_iter
 * parameter which specifies the maximum number of allocations to allow;
 * CHECK_MEMORY_FAILURES(expr) is equivalent to
 * CHECK_MEMORY_FAILURES_TO(100, expr).
 *
 * The CHECK_MEMORY_FAILURES_SHRINK macro causes shrinking reallocate
 * operations to fail (these are normally allowed even when regular
 * allocations would fail, since shrinking an allocation will normally
 * always succeed).
 *
 * This check fails if the test passes on the first iteration (under the
 * assumption that it did not properly fail) or runs for max_iter iterations
 * without succeeding (under the assumption that it is broken for other
 * reasons).  The check also fails if, after any failing iteration, the
 * number of bytes of memory used has increased since before the expression
 * was evaluated (thus suggesting a memory leak).
 *
 * [Parameters]
 *     max_iter: Maximum number of iterations to check.
 *     expr: Expression to test.
 */
#define CHECK_MEMORY_FAILURES(expr) \
    CHECK_MEMORY_FAILURES_BASE(100, 0, #expr, (expr))
#define CHECK_MEMORY_FAILURES_TO(max_iter,expr) \
    CHECK_MEMORY_FAILURES_BASE((max_iter), 0, #expr, (expr))
#define CHECK_MEMORY_FAILURES_SHRINK(expr) \
    CHECK_MEMORY_FAILURES_BASE(100, 1, #expr, (expr))
#define CHECK_MEMORY_FAILURES_BASE(max_iter,fail_shrink,expr_str,expr)  do { \
    const int _max_iter = (max_iter);                                   \
    for (int _i = 0; ; _i++) {                                          \
        if (_i >= _max_iter) {                                          \
            FAIL("%s did not succeed after %d iterations", expr_str,    \
                 _max_iter);                                            \
            break;                                                      \
        }                                                               \
        const int64_t _used_before = mem_debug_bytes_allocated();       \
        TEST_mem_fail_after(_i, 1, (fail_shrink));                      \
        if ((expr)) {                                                   \
            TEST_mem_fail_after(-1, 0, 0);                              \
            if (_i == 0) {                                              \
                FAIL("%s did not fail on a memory allocation failure",  \
                     expr_str);                                         \
            }                                                           \
            break;                                                      \
        }                                                               \
        TEST_mem_fail_after(-1, 0, 0);                                  \
        const int64_t _used_after = mem_debug_bytes_allocated();        \
        if (_used_after > _used_before) {                               \
            testlog_log(                                                \
                __FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,         \
                "FAIL: %s leaked memory on failure for iteration %d"    \
                " (%lld bytes)", expr_str, _i+1,                        \
                (long long)(_used_after - _used_before));               \
            mem_debug_report_allocs();                                  \
            DLOG("End of leak report for %s", expr_str);                \
            FAIL_ACTION;                                                \
        }                                                               \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_DLOG_TEXT:  Check that the text of the last line output via the
 * DLOG() interface (excluding the "file:line(function): " header) matches
 * the given text, which may include strformat() format tokens.  Assumes
 * both the DLOG() line and the formatted text to compare against are less
 * than 1000 bytes long.
 *
 * [Parameters]
 *     ...: Format string and arguments for expected text.
 */
#define CHECK_DLOG_TEXT(...)  do {                                      \
    char dlog_copy[1000];                                               \
    ASSERT(strformat_check(dlog_copy, sizeof(dlog_copy),                \
                           "%s", test_DLOG_last_message));              \
    const char *s = dlog_copy;                                          \
    s += strcspn(s, "(");  /* Assume no '(' in the filename. */         \
    s += strcspn(s, ")");                                               \
    ASSERT(s[0] == ')');                                                \
    ASSERT(s[1] == ':');                                                \
    ASSERT(s[2] == ' ');                                                \
    s += 3;                                                             \
    char compare[1000];                                                 \
    ASSERT(strformat_check(compare, sizeof(compare), __VA_ARGS__));     \
    if (strcmp(s, compare) != 0) {                                      \
        FAIL("Last DLOG() text was [%s] but should have been [%s]",     \
             s, compare);                                               \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * CHECK_PIXEL:  Check that the given pixel (pointer to 4-byte RGBA data)
 * has the given red, green, blue, and alpha values.  The X and Y values
 * are used in the failure message but do not affect the check itself.
 *
 * [Parameters]
 *     pixel: Pointer to pixel (32-bit RGBA data) to check.
 *     r, g, b, a: Expected color component values (each 0-255).
 *     x, y: Coordinates of pixel (for failure message).
 */
#define CHECK_PIXEL(pixel,r,g,b,a,x,y)  do {                    \
    const uint8_t * const _pixel = (pixel);                     \
    const uint8_t _r = (r), _g = (g), _b = (b), _a = (a);       \
    if (_pixel[0] != _r                                         \
     || _pixel[1] != _g                                         \
     || _pixel[2] != _b                                         \
     || _pixel[3] != _a) {                                      \
        FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should"  \
             " have been (%u,%u,%u,%u)", (x), (y),              \
             _pixel[0], _pixel[1], _pixel[2], _pixel[3],        \
             _r, _g, _b, _a);                                   \
    }                                                           \
} while (0)

/**
 * CHECK_PIXEL_NEAR:  Check that the given pixel (pointer to 4-byte RGBA
 * data) has approximately the given red, green, blue, and alpha values.
 * This macro can be used to account for differences in rounding methods
 * between implementations when the result is not precisely defined.
 *
 * [Parameters]
 *     pixel: Pointer to pixel (32-bit RGBA data) to check.
 *     r, g, b, a: Expected color component values (each 0-255).
 *     maxdiff: Maximum allowed difference from the expected value for
 *         each color component.
 *     x, y: Coordinates of pixel (for failure message).
 */
#define CHECK_PIXEL_NEAR(pixel,r,g,b,a,maxdiff,x,y)  do {       \
    const uint8_t * const _pixel = (pixel);                     \
    const int _r = (r), _g = (g), _b = (b), _a = (a);           \
    if (_check_pixel_near(_pixel, _r, _g, _b, _a, (maxdiff))) { \
        FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should"  \
             " have been near (%d,%d,%d,%d)", (x), (y),         \
             _pixel[0], _pixel[1], _pixel[2], _pixel[3],        \
             _r, _g, _b, _a);                                   \
    }                                                           \
} while (0)
static inline int _check_pixel_near(const uint8_t * const pixel,
                                    int r, int g, int b, int a, int maxdiff)
{
    const int r0 = lbound(r-maxdiff, 0);
    const int r1 = ubound(r+maxdiff, 255);
    const int g0 = lbound(g-maxdiff, 0);
    const int g1 = ubound(g+maxdiff, 255);
    const int b0 = lbound(b-maxdiff, 0);
    const int b1 = ubound(b+maxdiff, 255);
    const int a0 = lbound(a-maxdiff, 0);
    const int a1 = ubound(a+maxdiff, 255);
    return (pixel[0] < r0 || pixel[0] > r1)
        || (pixel[1] < g0 || pixel[1] > g1)
        || (pixel[2] < b0 || pixel[2] > b1)
        || (pixel[3] < a0 || pixel[3] > a1);
}

/*************************************************************************/
/************************ Common utility routines ************************/
/*************************************************************************/

/**
 * TestLogType:  Enumerated type for log type constants passed to
 * testlog_log().
 */
enum TestLogType {
    TESTLOG_FAIL = 1,
    TESTLOG_WARN,
    TESTLOG_SKIP,
};
typedef enum TestLogType TestLogType;

/**
 * testlog_log:  Log a test failure or warning message.
 *
 * [Parameters]
 *     filename: Source filename in which the failure occurred.
 *     line: Source line number at which the failure occurred.
 *     function: Name of function in which the failure occurred.
 *     type: Log message type (a TESTLOG_* constant).
 *     format: Message to log.  May contain printf()-style format tokens.
 *     ...: Format arguments for message text.
 */
extern void testlog_log(const char *filename, int line, const char *function,
                        TestLogType type, const char *format, ...)
     FORMAT(5,6);

/**
 * _memequal_failure_message:  Helper for CHECK_MEMEQUAL() which returns an
 * appropriate failure message for FAIL().
 */
extern const char *_memequal_failure_message(
    const uint8_t *value, const uint8_t *expected, int size);

/**
 * testlog_count_entries:  Return the number of log entries of the given type.
 *
 * [Parameters]
 *     type: Log message type (a TESTLOG_* constant).
 * [Return value]
 *     Number of log messages of the given type stored in the log buffer.
 *     If the log buffer has overflowed, this may be less than the total
 *     number of messages of that type which were logged.
 */
extern int testlog_count_entries(TestLogType type);

/**
 * testlog_print:  Print the test log using the DLOG() interface.
 * TESTLOG_SKIP log entries are ignored.
 */
extern void testlog_print(void);

/**
 * testutil_getenv:  Return the value of the given environment variable,
 * or the empty string if the variable does not exist.
 *
 * This function may be safely called on systems without environment
 * variable support; it always returns the empty string on such systems.
 *
 * [Parameters]
 *     name: Name of environment variable to look up, as a UTF-8 string.
 * [Return value]
 *     Value of the environment variable as a UTF-8 string, or the empty
 *     string if the variable does not exist or on error.  (The returned
 *     pointer is always a valid string pointer.)
 */
extern const char *testutil_getenv(const char *name);

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

/*
 * Each of these routines returns true if all tests performed by the
 * routine passed, false if one or more tests failed.  Each routine
 * corresponds to a single source file; see the individual file for
 * details about the tests performed.
 *
 * Random style note:  We use "..." instead of "*" in the "directory/..."
 * comments below to avoid nested-comment warnings from the compiler.
 */

/* Top-level test sources */
extern int test_condvar(void);
extern int test_debug(void);
extern int test_endian(void);
extern int test_input(void);
extern int test_main(void);
extern int test_memory(void);
extern int test_mutex(void);
extern int test_random(void);
extern int test_semaphore(void);
extern int test_thread(void);
extern int test_time(void);
extern int test_userdata(void);
extern int test_workqueue(void);

/* font/... */
extern int test_font_bitmap(void);
extern int test_font_core(void);
extern int test_font_freetype(void);
extern int test_font_sysfont(void);

/* graphics/... */
extern int test_graphics_base(void);
extern int test_graphics_clear_depth(void);
extern int test_graphics_clear_grab(void);
extern int test_graphics_framebuffer(void);
extern int test_graphics_misc(void);
extern int test_graphics_primitive(void);
extern int test_graphics_shader_gen(void);
extern int test_graphics_shader_obj(void);
extern int test_graphics_state(void);
extern int test_graphics_texture(void);
extern int test_graphics_texture_formats(void);  // graphics/texture.c

/* math/... */
extern int test_math_dtrig(void);
extern int test_math_matrix(void);
extern int test_math_matrix_cxx(void);
extern int test_math_rounding(void);
extern int test_math_vector(void);
extern int test_math_vector_cxx(void);

/* movie/... */
extern int test_movie_core(void);
extern int test_movie_webm(void);

/* resource/... */
extern int test_resource_core(void);
extern int test_resource_pkg(void);

/* sound/... */
extern int test_sound_core(void);
extern int test_sound_decode(void);
extern int test_sound_decode_ogg(void);
extern int test_sound_decode_wav(void);
extern int test_sound_filter(void);
extern int test_sound_filter_flange(void);
extern int test_sound_mixer(void);

/* sysdep/... */
extern int test_sys_debug(void);
extern int test_sys_files(void);
extern int test_sys_log(void);

/* sysdep/android/... */
extern int test_android_misc(void);

/* sysdep/darwin/... */
extern int test_darwin_time(void);

/* sysdep/ios/... */
extern int test_ios_graphics(void);
extern int test_ios_graphics_device_size_early(void);
extern int test_ios_util(void);

/* sysdep/linux/... */
extern int test_linux_graphics_fs_early(void);
extern int test_linux_graphics_fs_methods(void);
extern int test_linux_graphics_fs_minimize(void);
extern int test_linux_graphics_fs_mode(void);
extern int test_linux_graphics_modes(void);
extern int test_linux_graphics_window(void);
extern int test_linux_graphics_x11_base(void);
extern int test_linux_graphics_x11_events(void);
extern int test_linux_graphics_xinerama(void);
extern int test_linux_input(void);
extern int test_linux_main(void);
extern int test_linux_meminfo(void);
extern int test_linux_misc(void);
extern int test_linux_posix_fileutil(void);
extern int test_linux_sound(void);
extern int test_linux_sysfont(void);
extern int test_linux_userdata(void);

/* sysdep/macosx/... */
extern int test_macosx_graphics(void);
extern int test_macosx_input(void);
extern int test_macosx_util(void);

/* sysdep/misc/... */
extern int test_misc_ioqueue(void);
extern int test_misc_joystick_db(void);
extern int test_misc_joystick_hid(void);
extern int test_misc_log_stdio(void);

/* sysdep/opengl/... */
/* The test_opengl_features_*() functions are all defined in
 * sysdep/opengl/features.c.  Each one is a separate top-level test so
 * they can be more easily tested individually. */
extern int test_opengl_features_delayed_delete(void);
extern int test_opengl_features_delayed_delete_vao(void);
extern int test_opengl_features_no_genmipmap(void);
extern int test_opengl_features_no_getteximage(void);
extern int test_opengl_features_no_int_attrib(void);
extern int test_opengl_features_no_quads(void);
extern int test_opengl_features_no_rg(void);
extern int test_opengl_features_no_separate_shaders(void);
extern int test_opengl_features_vao_mandatory(void);
extern int test_opengl_features_vao_static(void);
extern int test_opengl_features_wrap_dsa(void);
extern int test_opengl_framebuffer(void);
extern int test_opengl_graphics(void);
extern int test_opengl_primitive(void);
extern int test_opengl_shader(void);
extern int test_opengl_shader_gen(void);
extern int test_opengl_state(void);
extern int test_opengl_texture(void);
extern int test_opengl_version(void);

/* sysdep/posix/... */
extern int test_posix_files(void);
extern int test_posix_fileutil(void);
extern int test_posix_misc(void);
extern int test_posix_thread(void);
extern int test_posix_time(void);
extern int test_posix_userdata(void);

/* sysdep/psp/... */
extern int test_psp_files(void);
extern int test_psp_font(void);
extern int test_psp_graphics(void);
extern int test_psp_input(void);
extern int test_psp_misc(void);
extern int test_psp_movie(void);
extern int test_psp_sound_mp3(void);
extern int test_psp_texture(void);
extern int test_psp_time(void);
extern int test_psp_userdata(void);

/* sysdep/windows/... */
extern int test_windows_condvar(void);
extern int test_windows_d3d_core(void);
extern int test_windows_files(void);
extern int test_windows_graphics(void);
extern int test_windows_input(void);
extern int test_windows_main(void);
extern int test_windows_misc(void);
extern int test_windows_thread(void);
extern int test_windows_time(void);
extern int test_windows_userdata(void);
extern int test_windows_utf8_wrappers(void);
extern int test_windows_util(void);

/* utility/... */
extern int test_utility_compress(void);
extern int test_utility_dds(void);
extern int test_utility_font_file(void);
extern int test_utility_id_array(void);
extern int test_utility_log(void);
extern int test_utility_memory(void);
extern int test_utility_misc(void);
extern int test_utility_pixformat(void);
extern int test_utility_png(void);
extern int test_utility_strdup(void);
extern int test_utility_strformat(void);
extern int test_utility_stricmp(void);
extern int test_utility_strtof(void);
extern int test_utility_tex_file(void);
extern int test_utility_tinflate(void);
extern int test_utility_utf8(void);
extern int test_utility_yuv2rgb(void);
extern int test_utility_zlib(void);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_TEST_BASE_H
