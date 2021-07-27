/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/shader-table.c: OpenGL shader hash table management.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/opengl/shader-table.h"

/*************************************************************************/
/********************** Global data (for debugging) **********************/
/*************************************************************************/

#ifdef DEBUG
int opengl_shader_table_overflow_count;
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Dynamically-allocated hash table of shader objects.  last_used == 0
 * indicates an unused entry. */
typedef struct TableEntry {
    uint64_t last_used;
    uint32_t key;
    ShaderInfo shader;
} TableEntry;
static TableEntry *shader_table;
static int shader_table_size;

/* True if dynamic resizing is enabled. */
static uint8_t allow_dynamic_resize = 1;

/* Next "last_used" value to use for LRU management.  Always nonzero. */
static uint64_t last_used_counter = 1;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * lookup_entry:  Look up and return the index of the table entry for the
 * given key.  If the key is not found, a new entry will be created for it.
 *
 * [Parameters]
 *     key: Key to look up.
 * [Return value]
 *     Associated index value, or -1 if not found and the table is full.
 */
static int lookup_entry(uint32_t key);

/**
 * update_last_used:  Set the given table entry's last_used field to the
 * current counter value, then increment the counter.
 *
 * [Parameters]
 *     index: Index of entry to update.
 */
static inline void update_last_used(int index);

/**
 * expand_table:  Reallocate the shader hash table so that it has the given
 * number of entries, and rehash existing entries for the new table size.
 *
 * [Parameters]
 *     new_size: New size of table.
 * [Return value]
 *     True on success, false on error or if dynamic resizing is disabled.
 */
static int expand_table(int new_size);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int shader_table_init(int table_size, int dynamic_resize)
{
    PRECOND(table_size >= (dynamic_resize ? 0 : 1), table_size = 1);

    allow_dynamic_resize = 1;  // Force the table buffer to be freed.
    shader_table_clear();

    allow_dynamic_resize = (dynamic_resize != 0);
    last_used_counter = 1;
    return table_size == 0 || expand_table(table_size);
}

/*-----------------------------------------------------------------------*/

ShaderInfo *shader_table_lookup(uint32_t key, int *invalidate_ret)
{
    const int index = lookup_entry(key);
    if (invalidate_ret) {
        *invalidate_ret = (index < 0);
    }
    if (LIKELY(index >= 0)) {
        return &shader_table[index].shader;
    }

    if (allow_dynamic_resize) {
        const int new_size = shader_table_size + 100;  // Arbitrary size.
        DLOG("WARNING: shader table full, expanding to %d", new_size);
        if (expand_table(new_size)) {
            return shader_table_lookup(key, NULL);
        } else {
            DLOG("Failed to expand shader table");
        }
    }

    if (shader_table_size < 1) {
        DLOG("Shader table size is 0 and it cannot be expanded!");
        return NULL;
    }

#ifdef DEBUG
    opengl_shader_table_overflow_count++;
#endif
    int oldest_index = 0;
    uint64_t oldest_age = last_used_counter - shader_table[0].last_used;
    for (int i = 1; i < shader_table_size; i++) {
        const uint64_t age = last_used_counter - shader_table[i].last_used;
        if (age > oldest_age) {
            oldest_index = i;
            oldest_age = age;
        }
    }
    DLOG("WARNING: shader table full, discarding oldest entry %d and"
         " rehashing", oldest_index);
    ASSERT(shader_table[oldest_index].shader.program != 0);
    opengl_delete_program(shader_table[oldest_index].shader.program);
    shader_table[oldest_index].shader.program = 0;
    shader_table[oldest_index].key = key;
    update_last_used(oldest_index);
    /* "Expand" to the current size to force a rehash, then repeat the
     * lookup to find the new location of the entry. */
    expand_table(shader_table_size);
    return shader_table_lookup(key, NULL);
}

/*-----------------------------------------------------------------------*/

int shader_table_used(void)
{
    int used = 0;
    for (int i = 0; i < shader_table_size; i++) {
        if (shader_table[i].last_used != 0) {
            used++;
        }
    }
    return used;
}

/*-----------------------------------------------------------------------*/

void shader_table_clear(void)
{
    for (int i = 0; i < shader_table_size; i++) {
        if (shader_table[i].last_used != 0) {
            opengl_delete_program(shader_table[i].shader.program);
            shader_table[i].shader.program = 0;
            mem_free(shader_table[i].shader.user_uniforms);
            shader_table[i].shader.user_uniforms = NULL;
            shader_table[i].last_used = 0;
        }
    }
    if (allow_dynamic_resize) {
        mem_free(shader_table);
        shader_table = NULL;
        shader_table_size = 0;
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int lookup_entry(uint32_t key)
{
    /* Preconvert the size to an unsigned int to help optimization. */
    const unsigned int u_size = (unsigned int)shader_table_size;

    if (LIKELY(shader_table_size > 0)) {
        const unsigned int base_index = key % u_size;
        if (LIKELY(shader_table[base_index].key == key)) {
            update_last_used(base_index);
            return base_index;
        }
        if (shader_table[base_index].last_used == 0) {
            shader_table[base_index].key = key;
            update_last_used(base_index);
            return base_index;
        }

        unsigned int index = (base_index+1) % u_size;
        for (; index != base_index; index = (index+1) % u_size) {
            if (shader_table[index].key == key) {
                update_last_used(index);
                return index;
            }
            if (shader_table[index].last_used == 0) {
                shader_table[index].key = key;
                update_last_used(index);
                return index;
            }
        }
    }

    return -1;
}

/*-----------------------------------------------------------------------*/

static inline void update_last_used(int index)
{
    shader_table[index].last_used = last_used_counter;
    /* Increment by 2 at a time as a cheap way to avoid 0 on wraparound.
     * Not that we're likely to wrap around anyway with 63 bits of counter
     * space, but just in case. */
    last_used_counter += 2;
}

/*-----------------------------------------------------------------------*/

static int expand_table(int new_size)
{
    PRECOND(new_size >= shader_table_size, return 1);

    TableEntry * const new_table = mem_alloc(sizeof(*new_table) * new_size, 0,
                                             MEM_ALLOC_TOP | MEM_ALLOC_CLEAR);
    if (!new_table) {
        DLOG("Failed to allocate shader hash table of size %u", new_size);
        return 0;
    }

    TableEntry * const old_table = shader_table;
    const int old_size = shader_table_size;
    shader_table = new_table;
    shader_table_size = new_size;

    for (int i = 0; i < old_size; i++) {
        /* This function is only called when the table is full, so there
         * should never be any empty entries. */
        ASSERT(old_table[i].last_used != 0, continue);
        const int new_index = lookup_entry(old_table[i].key);
        ASSERT(new_index >= 0, continue);
        shader_table[new_index] = old_table[i];
    }

    mem_free(old_table);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
