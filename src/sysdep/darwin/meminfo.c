/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/darwin/meminfo.c: System/process memory information functions
 * for Darwin-based systems (Mac/iOS).
 */

#include "src/base.h"
#include "src/sysdep/darwin/meminfo.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>

/*************************************************************************/
/*************************************************************************/

int64_t darwin_get_total_memory(void)
{
    /* We can in theory get this information with host_info(HOST_BASIC_INFO),
     * but the memory_size field is only 32 bits wide, so it can't handle
     * RAM sizes of 4G or more. */
    int64_t memsize;
    if (sysctlbyname("hw.memsize", &memsize, (size_t[]){sizeof(memsize)},
                     NULL, 0) != 0) {
        DLOG("sysctl(hw.memsize) failed: %s", strerror(errno));
        return 0;
    }
    ASSERT(memsize > 0, return 0);
    return memsize;
}

/*-----------------------------------------------------------------------*/

int64_t darwin_get_process_size(void)
{
    task_basic_info_data_t task_stat;
    mach_msg_type_number_t task_stat_size = TASK_BASIC_INFO_COUNT;
    kern_return_t result = task_info(mach_task_self(), TASK_BASIC_INFO,
                                     (task_info_t)&task_stat, &task_stat_size);
    if (UNLIKELY(result != KERN_SUCCESS)) {
        DLOG("task_info(TASK_BASIC_INFO) failed: %d: %s",
             result, mach_error_string(result));
        return 0;
    }
    return task_stat.resident_size;
}

/*-----------------------------------------------------------------------*/

int64_t darwin_get_free_memory(void)
{
    const mach_port_t host_port = mach_host_self();
    kern_return_t result;

    vm_size_t page_size;
    result = host_page_size(host_port, &page_size);
    if (UNLIKELY(result != KERN_SUCCESS)) {
        DLOG("host_page_size() failed: %d: %s",
             result, mach_error_string(result));
        return 0;
    }

    vm_statistics_data_t vm_stat;
    mach_msg_type_number_t vm_stat_size = sizeof(vm_stat) / sizeof(natural_t);
    result = host_statistics(
        host_port, HOST_VM_INFO, (host_info_t)&vm_stat, &vm_stat_size);
    if (UNLIKELY(result != KERN_SUCCESS)) {
        DLOG("host_statistics(HOST_VM_INFO) failed: %d: %s",
             result, mach_error_string(result));
        return 0;
    }
    return (int64_t)(vm_stat.inactive_count + vm_stat.free_count) * page_size;
}

/*************************************************************************/
/*************************************************************************/
