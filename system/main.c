/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2020 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu-main.h"
#include "sysemu/sysemu.h"

#include "qemu/log.h"

#ifdef CONFIG_SDL
#include <SDL.h>
#endif

#include <sys/eventfd.h>

/*
 * Temporary location of COSIM stuff - to be moved to separate files 
 */
///////////////////////////////////////////////////////////////

extern int cosim_ep (void);
void qemu_cosim_API (void* opaque_data);
void COSIM_pass_sync (pthread_mutex_t *mutex_sync_init, pthread_cond_t * cond_sync_init);
void* QEMU_step(void);

static bool qemu_COSIM_init_glue(void);

/*
 * The below global variables are used by COSIM  
 * to propagate some data down to CPU thread.
 */

COSIM_data_t     *COSIM_glue_data  = NULL;
int cosim_mode                     = 0;

#define COSIM_KEY 0x778654

///////////////////////////////////////////////////////////////

/*
 * These two sync primitives are created by COSIM.
 * They are used when QEMU thread opens COSIM thread after initialization.
 * These two are also used  when qemu.STEP() is executed - start/stop mode.
 */
 
static pthread_mutex_t* sync_mutex = NULL;
static pthread_cond_t*  sync_cond  = NULL;

///////////////////////////////////////////////////////////////

int qemu_default_main(void)
{ 
    int status = 0;

LOGIM("--> qemu_main_loop() status = %d", status);

    status = qemu_main_loop();
    if (cosim_mode) {
        fprintf (stderr, "QEMU:%s() <---- qemu_main_loop() status = %d, COSIM_MODE = %d\n", __FUNCTION__, status, cosim_mode);
    }

    if (cosim_mode == 0) {
        qemu_cleanup(status);
    }

    return status;
}

int (*qemu_main)(void) = qemu_default_main;

///////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++) {
        printf ("QEMU:%s() ---- argv [%d] = %s\n ", __FUNCTION__, i, argv[i]);
    }

    bool iam_qemu_so = (strcmp (argv[argc - 1], "-cosim") == 0);

    ///////////////////////////////////////////////////////////

    cosim_mode = iam_qemu_so;
    if (cosim_mode) {
        if (!qemu_COSIM_init_glue()) {
	    fprintf (stderr, "QEMU-cosim: Unable to initialize COSIM interface\n");
            return 0; 
        }
    }

    ///////////////////////////////////////////////////////////

    if (iam_qemu_so) {
        argc--;
    }
    qemu_init (argc, argv);

LOGIM ("<---- qemu_init()");

    int rc = qemu_main();
    if ( cosim_mode) {
        fprintf (stderr, "<---- QEMU:%s() RETURN \n", __FUNCTION__);
    }
    return rc;
}

///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

static bool
qemu_COSIM_init_sync (COSIM_data_t *pgd)
{
    bool ret = false;
    
    if ((sync_mutex == NULL) || (sync_cond == NULL)) {
        fprintf (stderr, "%s(): Interal error - sync MUTEX/CONDVAR are not initialized\n", __FUNCTION__);
        goto FAILURE; 
    }
    pgd->cosim_mutex = sync_mutex;
    pgd->cosim_cond  = sync_cond;

    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd <= 0) {
        fprintf (stderr, "%s(): eventfd() returned %d \n",__FUNCTION__, fd);
        goto FAILURE;  
    }
    pgd->rfd = pgd->wfd = fd;
    ret = true;

FAILURE:
    return ret; 
}

/*
 * Creates globals which are used to propagate
 * COSIM data. 
 */
static bool 
qemu_COSIM_init_glue (void)
{
    bool ret = FALSE;
    COSIM_data_t *pgd = (COSIM_data_t *)malloc (sizeof(COSIM_data_t));
    if (pgd == NULL) {
        fprintf (stderr, "MALLOC: unable to allocate COSIM glue data\n");
        exit (0);
    }
    pgd->rfd = pgd->wfd = -1;

    COSIM_glue_data = pgd;
    printf ("QEMU:%s() ---- pgd = %p ----\n", __FUNCTION__, pgd);
    if (qemu_COSIM_init_sync (pgd)) {
        printf ("%s():  FD = %d\n", __FUNCTION__, pgd->wfd);
        ret = TRUE;
    }
    return ret; 
}

//////////////////////////////////////////////////////////////
/*
 * COSIM provides mutex and condvar (via this entry point)
 * which are used for COSIM-QEMU synchronization.
 */  
void COSIM_pass_sync (pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    sync_mutex = mutex;
    sync_cond  = cond;
    printf ("%s() -- mutex = %p, cond = %p \n", __FUNCTION__, mutex, cond); 
}

//////////////////////////////////////////////////////////////
/*
 * QEMU new entry point for COSIM which kicks off QEMU main thread
 * to execute one guest intsruction.
 * It sends the signalling 8 bytes via file descriptor opened by EVENTFD.
 * In fact it is an extension of the common QEMU sync mehanizm (event loop).
 */ 
void* QEMU_step(void)
{
  int fd = COSIM_glue_data->wfd;
  unsigned long long msg = 0x12345678;

  // printf ("%s(): ====> LOCK () sync_mutex = %p\n", __FUNCTION__, sync_mutex);
  pthread_mutex_lock (sync_mutex);
  // printf ("%s(): <==== LOCK () sync_mutex = %p\n", __FUNCTION__, sync_mutex);

  int rc = write (fd, (char*)&msg, 8);
  // printf ("%s() ====  STEP --> write (FD = %d) , rc = %d  \n", __FUNCTION__, fd, rc);

LOGIM("<======== write (fd = %d) rc = %d  ==> COND_WAIT()", fd, rc);
  pthread_cond_wait (sync_cond, sync_mutex);
LOGIM("<======== COND_WAIT()");

  // printf ("%s(): <==== COND_WAIT () sync_cond = %p\n", __FUNCTION__, sync_cond);
printf ("%s(): PC = 0x%llx\n", __FUNCTION__, COSIM_glue_data->state_pc);

  pthread_mutex_unlock (sync_mutex);
   
  return NULL;
}
