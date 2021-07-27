/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/id-array.c: Tests for the ID array management routines.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/random.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/utility/id-array.h"

#ifdef SIL_DEBUG_USE_VALGRIND
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND  0
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure passed to torture_thread(). */

typedef struct TortureData TortureData;
struct TortureData {
    IDArray *array;         // Array to operate on.
    uint64_t random_state;  // Initial state for random number generation.
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * torture_thread:  Thread routine which registers and releases a single ID
 * in an ID array.  Used to (attempt to) test mutex creation collision for
 * thread-safe arrays.
 *
 * [Parameters]
 *     data: Thread data (TortureData *).
 * [Return value]
 *     True if all operations succeeded, false otherwise.
 */
static int torture_thread(void *data_);

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_id_array)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_basic)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object1));
    CHECK_PTREQUAL(id_array_get(&array, id), object1);
    CHECK_INTEQUAL(id_array_find(&array, object1), id);
    CHECK_INTRANGE(id_array_size(&array), 2, 11);

    id_array_set(&array, id, object2);
    CHECK_PTREQUAL(id_array_get(&array, id), object2);
    CHECK_INTEQUAL(id_array_find(&array, object1), 0);
    CHECK_INTEQUAL(id_array_find(&array, object2), id);

    id_array_release(&array, id);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_basic_threadsafe)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object1));
    CHECK_PTREQUAL(id_array_get(&array, id), object1);
    CHECK_INTEQUAL(id_array_find(&array, object1), id);
    CHECK_INTEQUAL(id_array_size(&array), 10);

    id_array_set(&array, id, object2);
    CHECK_PTREQUAL(id_array_get(&array, id), object2);
    CHECK_INTEQUAL(id_array_find(&array, object1), 0);
    CHECK_INTEQUAL(id_array_find(&array, object2), id);

    id_array_release(&array, id);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_basic_threadsafe_multiple_slices)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0x9ABC;
    void * const object4 = (void *)0xDEF0;
    void * const object5 = (void *)0x4321;
    void * const object6 = (void *)0x8765;
    void * const object7 = (void *)0xCBA9;
    void * const object8 = (void *)0x0FED;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(2);
    int id1, id2, id3, id4;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id4 = id_array_register(&array, object4));
    CHECK_PTREQUAL(id_array_get(&array, id1), object1);
    CHECK_PTREQUAL(id_array_get(&array, id2), object2);
    CHECK_PTREQUAL(id_array_get(&array, id3), object3);
    CHECK_PTREQUAL(id_array_get(&array, id4), object4);
    CHECK_INTEQUAL(id_array_find(&array, object1), id1);
    CHECK_INTEQUAL(id_array_find(&array, object2), id2);
    CHECK_INTEQUAL(id_array_find(&array, object3), id3);
    CHECK_INTEQUAL(id_array_find(&array, object4), id4);
    CHECK_INTEQUAL(id_array_size(&array), 6);

    id_array_set(&array, id1, object5);
    id_array_set(&array, id2, object6);
    id_array_set(&array, id3, object7);
    id_array_set(&array, id4, object8);
    CHECK_PTREQUAL(id_array_get(&array, id1), object5);
    CHECK_PTREQUAL(id_array_get(&array, id2), object6);
    CHECK_PTREQUAL(id_array_get(&array, id3), object7);
    CHECK_PTREQUAL(id_array_get(&array, id4), object8);
    CHECK_INTEQUAL(id_array_find(&array, object1), 0);
    CHECK_INTEQUAL(id_array_find(&array, object2), 0);
    CHECK_INTEQUAL(id_array_find(&array, object3), 0);
    CHECK_INTEQUAL(id_array_find(&array, object4), 0);
    CHECK_INTEQUAL(id_array_find(&array, object5), id1);
    CHECK_INTEQUAL(id_array_find(&array, object6), id2);
    CHECK_INTEQUAL(id_array_find(&array, object7), id3);
    CHECK_INTEQUAL(id_array_find(&array, object8), id4);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    id_array_release(&array, id3);
    id_array_release(&array, id4);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_memory_failure)
{
    void * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id;

    CHECK_MEMORY_FAILURES(id = id_array_register(&array, object));
    CHECK_PTREQUAL(id_array_get(&array, id), object);

    id_array_release(&array, id);
    CHECK_FALSE(id_array_get(&array, id));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_memory_failure_threadsafe)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(2);
    int id1, id2;

    CHECK_MEMORY_FAILURES(
        (id1 = id_array_register(&array, object1))
        || (id_array_clean(&array), 0));
    CHECK_MEMORY_FAILURES(id2 = id_array_register(&array, object2));
    CHECK_PTREQUAL(id_array_get(&array, id1), object1);
    CHECK_PTREQUAL(id_array_get(&array, id2), object2);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    CHECK_FALSE(id_array_get(&array, id1));
    CHECK_FALSE(id_array_get(&array, id2));

    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_expand_array)
{
    char * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id[11];

    for (int i = 0; i < 11; i++) {
        CHECK_TRUE(id[i] = id_array_register(&array, object + i));
    }
    for (int i = 0; i < 11; i++) {
        CHECK_PTREQUAL(id_array_get(&array, id[i]), object + i);
    }
    CHECK_INTRANGE(id_array_size(&array), 12, 21);

    for (int i = 0; i < 11; i++) {
        id_array_release(&array, id[i]);
    }
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_expand_array_threadsafe)
{
    char * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    int id[11];

    for (int i = 0; i < 11; i++) {
        CHECK_TRUE(id[i] = id_array_register(&array, object + i));
    }
    for (int i = 0; i < 11; i++) {
        CHECK_PTREQUAL(id_array_get(&array, id[i]), object + i);
    }
    CHECK_INTEQUAL(id_array_size(&array), 20);

    for (int i = 0; i < 11; i++) {
        id_array_release(&array, id[i]);
    }
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_id_reuse)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0xABCD;
    void * const object4 = (void *)0x4321;
    void * const object5 = (void *)0x8765;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id1, id2, id3;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id1 < id2);
    CHECK_TRUE(id2 < id3);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    CHECK_INTEQUAL(id_array_register(&array, object4), id1);
    CHECK_INTEQUAL(id_array_register(&array, object5), id2);
    CHECK_PTREQUAL(id_array_get(&array, id1), object4);
    CHECK_PTREQUAL(id_array_get(&array, id2), object5);
    CHECK_PTREQUAL(id_array_get(&array, id3), object3);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    id_array_release(&array, id3);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_id_reuse_threadsafe)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0xABCD;
    void * const object4 = (void *)0x4321;
    void * const object5 = (void *)0x8765;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    int id1, id2, id3;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id1 < id2);
    CHECK_TRUE(id2 < id3);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    CHECK_INTEQUAL(id_array_register(&array, object4), id1);
    CHECK_INTEQUAL(id_array_register(&array, object5), id2);
    CHECK_PTREQUAL(id_array_get(&array, id1), object4);
    CHECK_PTREQUAL(id_array_get(&array, id2), object5);
    CHECK_PTREQUAL(id_array_get(&array, id3), object3);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    id_array_release(&array, id3);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_id_reuse_threadsafe_second_slice)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0xABCD;
    void * const object4 = (void *)0x4321;
    void * const object5 = (void *)0x8765;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(3);
    int id1, id2, id3;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id1 < id2);
    CHECK_TRUE(id2 < id3);

    id_array_release(&array, id1);
    id_array_release(&array, id3);
    CHECK_INTEQUAL(id_array_register(&array, object4), id1);
    CHECK_INTEQUAL(id_array_register(&array, object5), id3);
    CHECK_PTREQUAL(id_array_get(&array, id1), object4);
    CHECK_PTREQUAL(id_array_get(&array, id2), object2);
    CHECK_PTREQUAL(id_array_get(&array, id3), object5);

    id_array_release(&array, id1);
    id_array_release(&array, id2);
    id_array_release(&array, id3);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_get_id_out_of_range)
{
    void * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object));
    CHECK_PTREQUAL(id_array_get(&array, id), object);
    CHECK_FALSE(id_array_get(&array, 0));
    CHECK_FALSE(id_array_get(&array, 11));

    id_array_release(&array, id);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_get_id_out_of_range_threadsafe)
{
    void * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object));
    CHECK_PTREQUAL(id_array_get(&array, id), object);
    CHECK_FALSE(id_array_get(&array, 0));
    CHECK_FALSE(id_array_get(&array, 11));

    id_array_release(&array, id);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_find_missing)
{
    void * const object = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object));
    CHECK_INTEQUAL(id_array_find(&array, object2), 0);

    id_array_release(&array, id);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_find_missing_threadsafe)
{
    void * const object = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object));
    CHECK_INTEQUAL(id_array_find(&array, object2), 0);

    id_array_release(&array, id);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_release_id_out_of_range)
{
    void * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object));
    id_array_release(&array, 0);
    id_array_release(&array, 11);
    CHECK_PTREQUAL(id_array_get(&array, id), object);

    id_array_release(&array, id);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_release_id_out_of_range_threadsafe)
{
    void * const object = (void *)0x1234;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    int id;

    CHECK_TRUE(id = id_array_register(&array, object));
    id_array_release(&array, 0);
    id_array_release(&array, 11);
    CHECK_PTREQUAL(id_array_get(&array, id), object);

    id_array_release(&array, id);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_initially_empty)
{
    IDArray array = ID_ARRAY_INITIALIZER(10);
    CHECK_FALSE(id_array_get(&array, 1));
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_initially_empty_threadsafe)
{
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    CHECK_FALSE(id_array_get(&array, 1));
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_find_when_empty)
{
    IDArray array = ID_ARRAY_INITIALIZER(10);
    CHECK_INTEQUAL(id_array_find(&array, &array), 0);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_find_when_empty_threadsafe)
{
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    CHECK_INTEQUAL(id_array_find(&array, &array), 0);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_release_when_empty)
{
    IDArray array = ID_ARRAY_INITIALIZER(10);
    id_array_release(&array, 0);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_release_when_empty_threadsafe)
{
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(10);
    id_array_release(&array, 0);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_clean)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0x4321;
    void * const object4 = (void *)0x8765;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id1, id2, id3, id4;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id4 = id_array_register(&array, object4));
    CHECK_INTRANGE(id_array_size(&array), 5, 14);

    id_array_release(&array, id2);
    id_array_release(&array, id4);
    id_array_clean(&array);
    CHECK_INTEQUAL(id_array_size(&array), 4);

    id_array_release(&array, id1);
    id_array_release(&array, id3);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_clean_threadsafe)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0x4321;
    void * const object4 = (void *)0x8765;
    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(2);
    int id1, id2, id3, id4;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id4 = id_array_register(&array, object4));
    CHECK_INTEQUAL(id_array_size(&array), 6);

    id_array_release(&array, id2);
    id_array_release(&array, id4);
    id_array_clean(&array);
    CHECK_INTEQUAL(id_array_size(&array), 4);

    id_array_release(&array, id1);
    id_array_release(&array, id3);
    id_array_clean(&array);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_clean_memory_failure)
{
    void * const object1 = (void *)0x1234;
    void * const object2 = (void *)0x5678;
    void * const object3 = (void *)0x4321;
    void * const object4 = (void *)0x8765;
    IDArray array = ID_ARRAY_INITIALIZER(10);
    int id1, id2, id3, id4;

    CHECK_TRUE(id1 = id_array_register(&array, object1));
    CHECK_TRUE(id2 = id_array_register(&array, object2));
    CHECK_TRUE(id3 = id_array_register(&array, object3));
    CHECK_TRUE(id4 = id_array_register(&array, object4));
    CHECK_INTRANGE(id_array_size(&array), 5, 14);

    id_array_release(&array, id2);
    id_array_release(&array, id4);
    CHECK_MEMORY_FAILURES_SHRINK(
        (id_array_clean(&array), id_array_size(&array) == 4));

    id_array_release(&array, id1);
    id_array_release(&array, id3);
    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_threadsafe_torture)
{
#if defined(SIL_PLATFORM_PSP)
    /* Mutex collisions don't seem to occur on the PSP (maybe a side
     * effect of scheduling algorithms or some such?) and the test takes
     * forever to run, so skip it. */
    SKIP("Not relevant on this platform.");
#endif
    if (RUNNING_ON_VALGRIND) {
        SKIP("Can't test scheduling behavior under Valgrind.");
    }

    const int MAX_ITERATIONS = 50000;
    enum {MAX_THREADS = 4};

    CHECK_TRUE(thread_init());
    const int num_cores = thread_get_num_cores();
    const int num_threads = bound(num_cores, 2, MAX_THREADS);
    uint64_t base_core_mask = 0;
    for (int i = 0; i < 64; i += num_cores) {
        base_core_mask |= UINT64_C(1) << i;
    }

    IDArray array = ID_ARRAY_THREADSAFE_INITIALIZER(MAX_THREADS);
    TortureData data[MAX_THREADS];
    char name[MAX_THREADS][16];
    ThreadAttributes attr[MAX_THREADS];
    srandom_env();
    for (int i = 0; i < num_threads; i++) {
        data[i].array = &array;
        data[i].random_state = urandom64();
        srandom64(data[i].random_state ^ urandom64());
        ASSERT(strformat_check(name[i], sizeof(name[i]),
                               "IDArrayTorture%d", i));
        attr[i].priority = 0;
        attr[i].stack_size = 0;
        attr[i].affinity = base_core_mask << (i % num_cores);
        attr[i].name = name[i];
    }

    DLOG("Trying to cause a mutex collision using %d threads (this may"
         " take a while)...", num_threads);
    TEST_id_array_mutex_collisions = 0;
    for (int i = 0; !TEST_id_array_mutex_collisions; i++) {
        if (i >= MAX_ITERATIONS) {
            WARN("Failed to cause a mutex collision after %d iterations."
                 "  Try increasing MAX_ITERATIONS or adding more threads.", i);
            break;
        }
        int thread[MAX_THREADS];
        for (int j = 0; j < num_threads; j++) {
            CHECK_TRUE(thread[j] = thread_create_with_attr(
                           &attr[j], torture_thread, &data[j]));
        }
        for (int j = 0; j < num_threads; j++) {
            CHECK_TRUE(thread_wait(thread[j]));
        }
        id_array_clean(&array);
    }

    id_array_clean(&array);
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int torture_thread(void *data_)
{
    TortureData *data = data_;
    IDArray *array = data->array;
    uint64_t random_state = data->random_state;

    void *object = (void *)(uintptr_t)urandom64_r(&random_state);
    int id;
    CHECK_TRUE(id = id_array_register(array, object));
    CHECK_PTREQUAL(id_array_get(array, id), object);
    CHECK_INTEQUAL(id_array_find(array, object), id);
    id_array_release(array, id);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
