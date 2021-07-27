/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/id-array.c: ID array management routines.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/utility/id-array.h"

/* Disable the memory debugging macros from the bottom of id-array.h.  Also
 * define convenience macros for declaring debug parameters, to avoid
 * unneeded messiness in the actual code. */
#ifdef DEBUG
# undef id_array_register
# undef id_array_clean
# define __DEBUG_PARAMS  , const char *file, int line
#else
# define __DEBUG_PARAMS  /*nothing*/
#endif

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
int TEST_id_array_mutex_collisions;
#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int id_array_register(IDArray *array, void *object __DEBUG_PARAMS)
{
    PRECOND(array != NULL, return 0);
    PRECOND(array->expand_by > 0, return 0);
    PRECOND(object != NULL, return 0);

    if (array->threadsafe) {
        /* expand_by==1 will confuse the array extension logic when adding
         * the first entry, so reject it out of hand. */
        ASSERT(array->expand_by != 1, return 0);

        if (!array->mutex) {
            SysMutexID mutex = sys_mutex_create(0, 0);
            if (UNLIKELY(!mutex)) {
                DLOG("Failed to create mutex for ID array");
                return 0;
            }
            SysMutexID other_mutex;
            int success;
#if (defined(__clang__) && (__clang_major__ >= 4 || (__clang_major__ == 3 && __clang_minor__ >= 2))) || (defined(__GNUC__) && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)))
            other_mutex = 0;
            success = __atomic_compare_exchange_n(
                &array->mutex, &other_mutex, mutex, 0,
                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#elif defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ >= 1
            other_mutex = __sync_val_compare_and_swap(&array->mutex, 0, mutex);
            success = (other_mutex == 0);
#elif defined(_MSC_VER)
            if (sizeof(array->mutex) == sizeof(long)) {
                other_mutex = (SysMutexID)_InterlockedCompareExchange(
                    (long *)&array->mutex, (long)mutex, 0);
            } else {  // 64-bit, because long is still 32-bit on Windows (sigh)
                ASSERT(sizeof(array->mutex) == 8);
                other_mutex = (SysMutexID)_InterlockedCompareExchange64(
                    (__int64 *)&array->mutex, (__int64)mutex, 0);
            }
            success = (other_mutex == 0);
#else
# error No compare-and-exchange function available!
#endif
            if (!success) {
                /* Somebody else allocated a mutex first, so use that one. */
                sys_mutex_destroy(mutex);
#ifdef SIL_INCLUDE_TESTS
                TEST_id_array_mutex_collisions++;
#endif
            }
        }
        sys_mutex_lock(array->mutex, -1);
    }

    if (array->first_free < array->size) {
        const int id = array->first_free;
        array->num_used++;
        if (array->threadsafe) {
            const int expand_by = array->expand_by;
            void **slice = array->array;
            int i;
            for (i = id; i >= expand_by; i -= expand_by) {
                slice = slice[expand_by];
                /* The slice must exist because there are enough slices
                 * for array->size elements. */
                ASSERT(slice, sys_mutex_unlock(array->mutex); return 0);
            }
            slice[i] = object;
            int first_free;
            for (first_free = array->first_free + 1; first_free < array->size;
                 first_free++)
            {
                i++;
                if (i == expand_by) {
                    i = 0;
                    slice = slice[expand_by];
                    ASSERT(slice, sys_mutex_unlock(array->mutex); return 0);
                }
                if (!slice[i]) {
                    break;
                }
            }
            array->first_free = first_free;
            sys_mutex_unlock(array->mutex);
        } else {
            array->array[id] = object;
            int first_free = array->first_free + 1;
            for (; first_free < array->size; first_free++) {
                if (array->array[first_free] == NULL) {
                    break;
                }
            }
            array->first_free = first_free;
        }
        return id;
    }

    if (array->threadsafe) {

        const int expand_by = array->expand_by;
        int new_size = array->size + expand_by;
        ASSERT(new_size > array->size, return 0); // Sanity check for overflow.
        /* Allocate from the top of memory to reduce fragmentation. */
        void **new_array = debug_mem_alloc(
            (expand_by + 1) * sizeof(void *), sizeof(void *),
            /* This isn't "temporary" memory, but since reallocation could
             * happen at potentially any time, we use MEM_ALLOC_TEMP to reduce
             * the risk of fragmentation in the main memory pool (on systems
             * where it matters). */
            MEM_ALLOC_TEMP | MEM_ALLOC_TOP, file, line, MEM_INFO_MANAGE);
        if (UNLIKELY(!new_array)) {
            DLOG("Failed to expand array %p to %u entries", array, new_size);
            sys_mutex_unlock(array->mutex);
            return 0;
        }
        for (int i = 0; i < expand_by + 1; i++) {
            new_array[i] = NULL;
        }
        if (!array->array) {
            array->array = new_array;
        } else {
            void **slice = array->array;
            while (slice[expand_by]) {
                slice = slice[expand_by];
            }
            slice[expand_by] = new_array;
        }
        array->size = new_size;

        const int id = array->first_free;
        new_array[id % expand_by] = object;
        array->first_free++;  // Already known to be unused; no need to check.
        array->num_used++;
        sys_mutex_unlock(array->mutex);
        return id;

    } else {  // !array->threadsafe

        int new_size = array->first_free + array->expand_by;
        ASSERT(new_size > array->size, return 0);
        void **new_array = debug_mem_realloc(
            array->array, new_size * sizeof(void *),
            MEM_ALLOC_TEMP | MEM_ALLOC_TOP, file, line, MEM_INFO_MANAGE);
        if (UNLIKELY(!new_array)) {
            DLOG("Failed to expand array %p to %u entries", array, new_size);
            return 0;
        }
        for (int i = array->size; i < new_size; i++) {
            new_array[i] = NULL;
        }
        array->array = new_array;
        array->size = new_size;

        const int id = array->first_free;
        array->array[id] = object;
        array->first_free++;
        array->num_used++;
        return id;

    }  // if (array->threadsafe)
}

/*-----------------------------------------------------------------------*/

int id_array_find(const IDArray *array, void *object)
{
    PRECOND(array != NULL, return 0);

    if (array->threadsafe) {
        if (UNLIKELY(!array->mutex)) {
            return 0;  // Must be empty.
        }
        sys_mutex_lock(array->mutex, -1);
        void **slice = array->array;
        const int expand_by = array->expand_by;
        for (int id = 1, i = 1; id < array->size; id++, i++) {
            if (i == expand_by) {
                i = 0;
                slice = slice[expand_by];
                ASSERT(slice, sys_mutex_unlock(array->mutex); return 0);
            }
            if (slice[i] == object) {
                sys_mutex_unlock(array->mutex);
                return id;
            }
        }
        sys_mutex_unlock(array->mutex);
    } else {
        for (int id = 1; id < array->size; id++) {
            if (array->array[id] == object) {
                return id;
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

void id_array_set(IDArray *array, int id, void *object)
{
    PRECOND(array != NULL, return);
    PRECOND(id > 0 && id < array->size, return);
    void **slice = array->array;
    if (array->threadsafe) {
        const int expand_by = array->expand_by;
        while (id >= expand_by) {
            slice = slice[expand_by];
            ASSERT(slice, return);
            id -= expand_by;
        }
    }
    slice[id] = object;
}

/*-----------------------------------------------------------------------*/

void id_array_release(IDArray *array, int id)
{
    PRECOND(array != NULL, return);

    if (array->threadsafe) {
        if (UNLIKELY(!array->mutex)) {
            return;  // Must be empty.
        }
        sys_mutex_lock(array->mutex, -1);
    }
    if (id > 0 && id < array->size) {
        if (id < array->first_free) {
            array->first_free = id;
        }
        void **slice = array->array;
        if (array->threadsafe) {
            const int expand_by = array->expand_by;
            while (id >= expand_by) {
                slice = slice[expand_by];
                ASSERT(slice, sys_mutex_unlock(array->mutex); return);
                id -= expand_by;
            }
        }
        slice[id] = 0;

        array->num_used--;
        if (array->num_used == 0 && !array->threadsafe) {
            mem_free(array->array);
            array->array = NULL;
            array->size = 0;
            return;
        }
    }
    if (array->threadsafe) {
        sys_mutex_unlock(array->mutex);
    }
}

/*-----------------------------------------------------------------------*/

void id_array_clean(IDArray *array __DEBUG_PARAMS)
{
    PRECOND(array != NULL, return);

    int last_used = -1;
    if (array->threadsafe) {
        const int expand_by = array->expand_by;
        void **slice = array->array;
        for (int id = 1, i = 1; id < array->size; id++, i++) {
            if (i == expand_by) {
                i = 0;
                slice = slice[expand_by];
                ASSERT(slice, return);
            }
            if (slice[i]) {
                last_used = id;
            }
        }
    } else {
        for (int id = 1; id < array->size; id++) {
            if (array->array[id]) {
                last_used = id;
            }
        }
    }

    if (array->threadsafe) {
        const int expand_by = array->expand_by;
        const int new_size = align_up(last_used + 1, expand_by);
        void **next_ptr = (void **)&array->array;
        void **slice = array->array;
        for (int i = 0; i < new_size; i += expand_by) {
            next_ptr = &slice[expand_by];
            slice = slice[expand_by];
        }
        void **next = *next_ptr;
        *next_ptr = NULL;
        while ((slice = next) != NULL) {
            next = slice[expand_by];
            mem_free(slice);
        }
        array->size = new_size;
        if (new_size == 0 && array->mutex) {
            sys_mutex_destroy(array->mutex);
            array->mutex = 0;
        }
    } else if (last_used > 0) {
        const int new_size = last_used + 1;
        void *new_array = debug_mem_realloc(
            array->array, new_size * sizeof(void *),
            MEM_ALLOC_TEMP | MEM_ALLOC_TOP, file, line, MEM_INFO_MANAGE);
        if (new_array) {
            array->array = new_array;
            array->size = new_size;
        }
    } else {
        debug_mem_free(array->array, file, line);
        array->array = NULL;
        array->size = 0;
    }
}

/*************************************************************************/
/*************************************************************************/
