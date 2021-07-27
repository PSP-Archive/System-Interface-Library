/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/id-array.h: Header for ID array management routines.
 */

/*
 * These routines are used to implement the internal arrays used to assign
 * numeric IDs to objects such as fonts and textures, but client code is
 * free to make use of them as well.
 */

#ifndef SIL_SRC_UTILITY_ID_ARRAY_H
#define SIL_SRC_UTILITY_ID_ARRAY_H

#include "src/sysdep.h"  // For SysMutexID definition.

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * IDArray:  Data structure for an ID array.  Callers should treat these
 * fields as private!
 */
typedef struct IDArray IDArray;
struct IDArray {
    /* Array of pointers to objects.  In a thread-safe array, this is
     * exactly (expand_by + 1) elements long, and the last element is a
     * pointer to the next array (in the same format) or NULL if this
     * array is the last one in the list. */
    void **array;
    /* Index of the first unused element. */
    int first_free;
    /* Number of elements currently in use. */
    int num_used;
    /* Total number of slots allocated in the array. */
    int size;
    /* Number of slots to add to the array when it is full. */
    int expand_by;
    /* Flag indicating whether to use thread-safe behavior for this array.
     * If true, register and unregister operations are protected by a mutex,
     * and the array is expanded by allocating new arrays in a linked list
     * so that existing IDs can be safely looked up without locking. */
    uint8_t threadsafe;
    /* Mutex for locking thread-safe arrays.  This uses the sys_mutex
     * interface rather than the high-level mutex interface to avoid a
     * circular dependency (since the high-level mutex interface uses a
     * thread-safe array to hold mutex IDs). */
    SysMutexID mutex;
};

/**
 * ID_ARRAY_INITIALIZER:  Macro to initialize a static IDArray.  Use this
 * macro instead of writing out the initializer manually; for example:
 *
 * static IDArray my_array = ID_ARRAY_INITIALIZER(100);
 *
 * [Parameters]
 *     expand_by: Number of slots to add when registering an object in a
 *         full array.  Must be positive.
 */
#define ID_ARRAY_INITIALIZER(expand_by_) \
    {.array = NULL, .first_free = 1, .size = 0, .expand_by = (expand_by_), \
     .threadsafe = 0}

/**
 * ID_ARRAY_THREADSAFE_INITIALIZER:  Macro to initialize a static,
 * thread-safe IDArray.
 *
 * Thread-safe arrays provide the following guarantees:
 *
 * - id_array_register(), id_array_release(), and id_array_find() can be
 *   safely called from any thread without external locking.
 *
 * - id_array_get(N) and id_array_set(N, object) for a given N are safe to
 *   call from any thread between the call to id_array_register() which
 *   returned N and the corresponding call to id_array_release(N).  Note
 *   that no guarantees are made about ordering between get and set calls.
 *
 * - id_array_size() returns a valid result when called from any thread,
 *   but the value returned is not guaranteed to match the state of the
 *   array at the time of any future operation.
 *
 * ID lookups in thread-safe arrays are O(N) in the number of array slices
 * allocated (where expand_by defines the length of a single array slice),
 * so expand_by should be set high enough to keep the number of array
 * slices to a minimum.
 *
 * [Parameters]
 *     expand_by: Number of slots to add when registering an object in a
 *         full array.  Must be at least 2.
 */
#define ID_ARRAY_THREADSAFE_INITIALIZER(expand_by_) \
    {.array = NULL, .first_free = 1, .size = 0, .expand_by = (expand_by_), \
     .threadsafe = 1, .mutex = 0}


/**
 * ID_ARRAY_VALIDATE:  Validate an ID against the given ID array.  If the
 * ID is valid, assign the corresponding value to a variable; if the ID is
 * invalid, execute the specified failure actions.
 *
 * The variadic parameter is one or more statements which will be executed
 * on failure, typically ending with a "return" or "break" statement to
 * abort the enclosing function or block.  No semicolon is required at the
 * end, though if multiple statements are used, each non-final statement
 * should be followed by a semicolon as usual.  If referencing the value
 * of the "id" parameter, use "_id", which is an internal variable used to
 * avoid side effects from multiple references to the macro parameter.
 *
 * [Parameters]
 *     array: IDArray against which to validate.
 *     id: ID to validate.
 *     type: Type of data contained in array.
 *     var: Variable (or other lvalue) to modify on auccess.
 *     ...: Statement(s) to execute on failure.
 */
#define ID_ARRAY_VALIDATE(array,id,type,var,...)  do { \
    const int _id = (id);                       \
    (var) = (type)id_array_get((array), _id);   \
    if (UNLIKELY(!(var))) {                     \
        __VA_ARGS__;                            \
    }                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * id_array_register:  Register the given object in the ID array and return
 * the ID allocated for it.
 *
 * [Parameters]
 *     array: IDArray in which to register the object.
 *     object: Object to register (must be non-NULL).
 * [Return value]
 *     Allocated ID (nonzero), or zero on error.
 */
extern int id_array_register(IDArray *array, void *object
#ifdef DEBUG
                             , const char *file, int line
#endif
);

/**
 * id_array_get:  Return the object corresponding to the given ID.
 *
 * [Parameters]
 *     array: IDArray from which to retrieve object.
 *     id: ID of object to retrieve.
 * [Return value]
 *     Object pointer associated with the given ID, or NULL if the ID is
 *     unused.
 */
static inline void *id_array_get(const IDArray *array, int id)
{
    PRECOND(array != NULL, return NULL);
    if (UNLIKELY(id <= 0) || UNLIKELY(id >= array->size)) {
        return NULL;
    }
    void **slice = array->array;
    if (array->threadsafe) {
        const int expand_by = array->expand_by;
        while (id >= expand_by) {
            slice = slice[expand_by];
            ASSERT(slice != NULL, return NULL);
            id -= expand_by;
        }
    }
    return slice[id];
}

/**
 * id_array_set:  Assign a new object to the given ID.  Behavior is
 * undefined if the given ID is not an ID which was previously returned by
 * id_array_register() and has not yet been released with id_array_release().
 *
 * [Parameters]
 *     array: IDArray in which to assign object.
 *     id: ID to which to assign object.
 *     object: Object to assign.
 */
extern void id_array_set(IDArray *array, int id, void *object);

/**
 * id_array_find:  Return the ID corresponding to the given object.
 * Runtime is O(n) in the length of the array.
 *
 * [Parameters]
 *     array: IDArray from which to retrieve object.
 *     object: Object for which to retrieve the ID.
 * [Return value]
 *     ID associated with the given object pointer, or 0 if the object is
 *     not found.
 */
extern int id_array_find(const IDArray *array, void *object);

/**
 * id_array_size:  Return the number of slots allocated for the array.
 * This value is always greater than the largest valid ID at that time.
 *
 * [Parameters]
 *     array: IDArray on which to operate.
 * [Return value]
 *     Number of slots allocated.
 */
static inline int id_array_size(const IDArray *array)
{
    PRECOND(array != NULL, return 0);
    return array->size;
}

/**
 * id_array_release:  Release the given ID for reuse.  Does nothing if id
 * is zero.
 *
 * [Parameters]
 *     array: IDArray on which to operate.
 *     id: ID to release.
 */
extern void id_array_release(IDArray *array, int id);

/**
 * id_array_clean:  Reduce the allocated size of the array to the minimum
 * needed to store the set of currently registered objects.  If the array
 * is empty, free all allocated storage and, for thread-safe arrays,
 * destroy the mutex associated with the array.
 *
 * Note that this function is not thread-safe even for thread-safe arrays.
 *
 * [Parameters]
 *     array: IDArray on which to operate.
 */
extern void id_array_clean(IDArray *array
#ifdef DEBUG
                           , const char *file, int line
#endif
);

/*-----------------------------------------------------------------------*/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_id_array_mutex_collisions:  Counter for the number of times
 * multiple threads have collided on creating a mutex for a thread-safe
 * ID array.
 */
extern int TEST_id_array_mutex_collisions;

#endif

/*-----------------------------------------------------------------------*/

/*
 * When debugging is enabled, we wrap allocating calls with these macros
 * which pass down the source file and line at which the call was made,
 * as in ../resource.h.
 */

#ifdef DEBUG
# define id_array_register(array,object) \
    id_array_register((array), (object), __FILE__, __LINE__)
# define id_array_clean(array) \
    id_array_clean((array), __FILE__, __LINE__)
#endif

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_UTILITY_ID_ARRAY_H
