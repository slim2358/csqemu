/*
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef QEMU_MAIN_H
#define QEMU_MAIN_H

int qemu_default_main(void);
extern int (*qemu_main)(void);

///////////////////////////////////////////////////////////////
//
//  COSIM stuff
//
///////////////////////////////////////////////////////////////

#include <pthread.h>

typedef struct COSIM_data_
{
    // notification fd's
    int               rfd;
    int               wfd;
    pthread_mutex_t*  cosim_mutex;
    pthread_cond_t*   cosim_cond;
    /*
     * It is temporary hack which works with one CPU only.
     * It is not clear how to make CPU accssible from COSIM-triggered event handler.
     * The diresct access of CPU_FOREACH causes linking error. 
     */
    void*               vcpu;

    /////////////////////////////////////////////////
    unsigned long long  state_pc;

} COSIM_data_t;

///////////////////////////////////////////////////////////////

#endif /* QEMU_MAIN_H */
