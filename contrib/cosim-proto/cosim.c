/*
 * ****** 11/15/2023 ******
 * The rudimentary prototype for the binary to
 * load QEMU.so
 * ****** 01/03/2024 ******
 * The COSIM prototype.
 * ****** 01/07/2024 ******
 *  1) Loads QEMU.so 
 *  2) Determines/initializes (dlsym) the following QEMU functions/entry points
 *     QEMU_step()
 *     QEMU_pass_sync()
 *     main ()
 *   The two former are new ones and used only in cosim mode
 *  3) Sets up mutex/condvar (QEMU_pass_sync()) to synchronize QEMU and COSIM.
 *  4) Launches QEMU thread which eventually  dives into QEWMU via main()
 *  5) COSM and QEMU thread are start-stop synchronized at the very beginning
 *  6) QEMU thread creates  CPU thread which hangs on CONDVAR (as in GDB mode).
 *     Meanwhile COSIM thread invokes QEMU_step() which wakes up CPU thread via QEMU 
 *     common  event loop mechanizm. After that QEMU_step() hangs on CONDVAR .
 *  7) QEMU thread upon instruction execution  wakes up COSIM thread 
 *  8) COSIM thread invokes QEMU_step() again....
 *
 *   COSIM command line:
 *    cosim  [-qlog] <QEMU.so path>  <QEMU plugin full path>  <RV executable>\n");
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#include <pthread.h>
#include <time.h>
#include <errno.h>

////////////////////////////////////////////////////////////////////

typedef int   (*qemu_ep_t)         (int, char**); 
typedef int   (*qemu_2_ep_t)       (void); 
typedef void  (*qemu_cosim_API_t)  (void*);
typedef void  (*qemu_cosim_sync_t) (pthread_mutex_t *pm, pthread_cond_t *pc);
typedef void* (*qemu_cosim_step_t)  (void);

static void*  qemu_get_ep  (void * hso, char * ep);
static void*  load_qemu  (char * path);
static void*  qemu_thread_ep (void * arg);

typedef struct qemu_args_
{
    char**     argv;
    int        argc;   
    qemu_ep_t  qemu_ep; 
    qemu_cosim_step_t qemu_step_ep;

} qemu_args_t;

#define MAX_CPU 16  // enough for starters

static void COSIM_run_sims (qemu_args_t*);

static volatile int qemu_running = 0; 

////////////////////////////////////////////////////////////////////

/*
 * The mutex/condvar are used for local sync between COSIM and QEMU 
 * main thread.
 */
static pthread_mutex_t  mutex_qemu_sync;
static pthread_cond_t   cond_qemu_sync;

/*
 * The mutex/condvar are used for global synchronization between COSIM
 * and QEMU
 */
static pthread_mutex_t  mutex_qemu_cosim_sync;
static pthread_cond_t   cond_qemu_cosim_sync;

static void* COSIM_init_sync(void*);
static void* COSIM_init_step(void*);

static bool qemu_log = false;

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

int 
main (int argc, char** argv)
{
    printf ("%s() ---------- COSIM --------------\n", __FUNCTION__); 

    int i;
    for (i = 0; i < argc; i++) {
        printf ("%s(): ARGV[%d] = %s\n", __FUNCTION__, i, argv[i]);
    }

    if ((argc < 4) || (argc > 5)) {
        printf ("Usage: cosim  [-qlog] <QEMU.so path>  <QEMU plugin full path>  <RV executable>\n");
        return 0;
    }

    if (strcmp("-qlog", argv[1]) == 0) {
        qemu_log = true;    
        printf ("Running COSIM with the new QEMU log \n");
    }  

    char **soargv = (char**)malloc(sizeof(char*) * 64);
    int    soargc = 0;
   
    soargv[soargc++] = strdup("qemu.so");
 
    soargv[soargc++] = strdup("-nographic");

    soargv[soargc++] = strdup("-accel");

    soargv[soargc++] = strdup("tcg,one-insn-per-tb=on,thread=single");

    soargv[soargc++] = strdup("-d");

    soargv[soargc++] = qemu_log ? strdup("nochain,prefix:cosim") : strdup("nochain");

    soargv[soargc++] = strdup("-plugin");

    int idx_pgn = qemu_log ? 3 : 2;
    soargv[soargc++] = strdup(argv[idx_pgn]);   // SL:  btw plugin is not needed :-)

    soargv[soargc++] = strdup("-machine");   

    soargv[soargc++] = strdup("virt");   

    soargv[soargc++] = strdup("-bios");   

    soargv[soargc++] = strdup(argv[idx_pgn + 1]);   

    soargv[soargc++] = strdup("-cosim");

    //
    // argv [1 or 2]  = QEMU.so full name
    // argv [2 or 3]  = plugin.so
    // argv [3 or 4]  = <bare metal binary>
    // 

    //
    // ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d nochain,prefix:mylog -plugin ${PREFIX_FOR_INSTALL}/plugins/lib${PLUGIN}.so
    //                                               -machine virt -bios ${HELLO}

    /*
     * Load QEMU.so shared library
     */
    void *h = load_qemu (qemu_log ? argv[2] : argv[1]);
    void *h_ep = NULL;
    void *h_sync = NULL; 
    void *h_step = NULL; 
    printf ("%s() <----- load_qemu() \n", __FUNCTION__);

    //////////////////////////////////////////////////////
   
    if (h == NULL) {
        printf ("Unable to load QEMU.SO\n");
        return 0;
    }

    //////////////////////////////////////////////////////
 
    /*
     *   Init QEMU entry point function.
     */
    h_ep = qemu_get_ep  (h, "main");
    qemu_ep_t ep_func = (qemu_ep_t)h_ep;

    qemu_args_t* pqemu_arg = (qemu_args_t*) malloc (sizeof(qemu_args_t));
    pqemu_arg->argv    = soargv;
    pqemu_arg->argc    = soargc;
    pqemu_arg->qemu_ep = ep_func;
 
    //////////////////////////////////////////////////////

    /*
     * Init all mutexex/condvars and pass them  
     * via  QEMU entry point for COSIM/QEMU synchronization.
     */
    h_sync = COSIM_init_sync (h);
    if (h_sync == NULL) {
        fprintf (stderr, "Unable to initialize COSIM-QEMU synchronization\n");
        return 0;
    }

     //////////////////////////////////////////////////////

    /*
     *  Init entry point for QEMU <exec one instruction>
     *  function
     */ 
    h_step = COSIM_init_step (h);
    qemu_cosim_step_t step_func = (qemu_cosim_step_t)h_step;
    pqemu_arg->qemu_step_ep = step_func;

    //////////////////////////////////////////////////////

    COSIM_run_sims (pqemu_arg);
    return 0;   
}

////////////////////////////////////////////////////////////////////

static void* COSIM_init_step(void* h)
{
    void* h_sync = qemu_get_ep  (h, "QEMU_step");
    if (h_sync == NULL) {
        fprintf (stderr, "Unable to access <COSIM_step>\n");
        return NULL;
    }
    return h_sync;
}

////////////////////////////////////////////////////////////////////

/*
 * Retrieves the pointer at QEMU function which accepts MUTEX/COND for  
 * COSIM-QEMU synchronization.
 */  
static void* COSIM_init_sync(void* h)
{
    int rc = pthread_mutex_init(&mutex_qemu_sync, NULL);
    if ( rc != 0) {
        fprintf (stderr, "%s(): COSIM: Unable to init MUTEX - rc=%d\n", __FUNCTION__, rc);
    }

    rc = pthread_cond_init(&cond_qemu_sync, NULL);
    if ( rc != 0) {
        fprintf (stderr,"%s(): COSIM: Unable to init MUTEX - rc=%d\n", __FUNCTION__, rc);
    }

    rc = pthread_mutex_init(&mutex_qemu_cosim_sync, NULL);
    if ( rc != 0) {
        fprintf (stderr, "%s(): COSIM: Unable to init MUTEX - rc=%d\n", __FUNCTION__, rc);
    }

    rc = pthread_cond_init(&cond_qemu_cosim_sync, NULL);
    if ( rc != 0) {
        fprintf (stderr,"%s(): COSIM: Unable to init MUTEX - rc=%d\n", __FUNCTION__, rc);
    }

    void* h_sync = qemu_get_ep  (h, "COSIM_pass_sync");
    if (h_sync == NULL) {
        fprintf (stderr, "Unable to access <COSIM_pass_sync>\n");
        return NULL;
    }

    qemu_cosim_sync_t sync_func = (qemu_cosim_sync_t)h_sync;
    sync_func(&mutex_qemu_cosim_sync, &cond_qemu_cosim_sync);

    return h_sync;
}

////////////////////////////////////////////////////////////////////
/*
 * Return QEMU  function pointer by name
 */
static void*
qemu_get_ep (void * hso, char * ep)
{
    void* h = dlsym (hso, ep); 
    printf ("%s(): <---- dlsym() H = %p\n", __FUNCTION__, h);

    return h;
}

////////////////////////////////////////////////////////////////////
/*
 * Loads QEMU.so
 */
static void*
load_qemu  (char * path)
{
    char *full_path = malloc (strlen(path) + 128);
    sprintf (full_path, "%s/%s", path, "qemu-system-riscv64.so");

    //printf ("%s() -- full path = <%s> \n", __FUNCTION__, full_path);
 
    /*
     * With the flags below QEMU.so names are properly resolved when 
     * invoked  from PLUGIN.so.  The segfault when using teh "specially" linked
     * PLUGIN.so also disapperas.  
     */
    void* h = dlopen(full_path, RTLD_LAZY | RTLD_GLOBAL); 
    printf ("%s(): <---- dlopen (%s) H = %p\n", __FUNCTION__, full_path, h);

    return h;
}

////////////////////////////////////////////////////////////////////

typedef int (*qemu_report_cosim_insn) (void* opaque_state, uint64_t pc_insn, uint64_t pc_next); 

static void *qemu_thread_ep(void *arg);
static pthread_t thr_qemu;

///////////////////////////////////////////////////////////////////
/*
 *  This function creates QEMU thread.
 *  
 */ 
static void COSIM_run_sims (qemu_args_t* qemu_args)
{
    /*
     *  This global lock is acquired here to prevent QEMU thread to send BCAST
     *  before COSIM thread hangs on WAIT
     */ 

    printf ("%s():COSIM-QEMU --> LOCK () lock = %p \n", __FUNCTION__, &mutex_qemu_cosim_sync);
    pthread_mutex_lock(&mutex_qemu_cosim_sync);
    printf ("%s():COSIM-QEMU <-- LOCK () lock = %p \n", __FUNCTION__, &mutex_qemu_cosim_sync);

    pthread_create(&thr_qemu, NULL, qemu_thread_ep, qemu_args);

    while ( qemu_running == 0) {
        sleep(1);
    }    

    /*
     * Initial synchronization between COSIM tread and QEMU thread 
     */
    printf ("%s():COSIM --> LOCK () - 1 \n", __FUNCTION__);
    pthread_mutex_lock(&mutex_qemu_sync);
    
    // The green light for QEMU thread
    printf("%s():COSIM ----> pthread_cond_broadcast() - 1 \n", __FUNCTION__); 
    pthread_cond_broadcast(&cond_qemu_sync);

    // Now COSIM thread waits for green light from QEMU
    printf ("%s():COSIM  ----> pthread_cond_wait() - 1 \n", __FUNCTION__);
    pthread_cond_wait(&cond_qemu_sync, &mutex_qemu_sync);
    printf ("%s():COSIM  <---- pthread_cond_wait()  - 2\n", __FUNCTION__);
    pthread_mutex_unlock(&mutex_qemu_sync);
    printf ("%s():COSIM <-- UNLOCK () - 1 \n", __FUNCTION__);

    /////////////////////////////////////////////////////////////////////////////
    /*
     * Waiting until QEMU main thread opens the road after 
     * launching CPU thread.
     */ 
    printf ("%s():COSIM --> COND_WAIT_QEMU () - 2\n", __FUNCTION__);
    pthread_cond_wait(&cond_qemu_cosim_sync, &mutex_qemu_cosim_sync);
    printf ("%s():COSIM <-- COND_WAIT_QEMU () - 2 \n", __FUNCTION__);

    printf ("%s():COSIM-QEMU --> UNLOCK () - 2 \n", __FUNCTION__);
    pthread_mutex_unlock(&mutex_qemu_cosim_sync);
    printf ("%s():COSIM-QEMU <-- UNLOCK ()  - 2\n", __FUNCTION__);

    int nn = 0;
    while (qemu_running == 1) {

        printf ("%s(): COSIM --> call QEMU.step() \n", __FUNCTION__);
        // QEMU <stepi> function - entry point
        qemu_args->qemu_step_ep ();
        nn++;
    }
    printf ("The test is completed - %d instructions executed\n", nn); 
    sleep(2);
    pthread_join (thr_qemu, NULL);
    return;
}

////////////////////////////////////////////////////////////////////
/*
 * QEMU thread entry point.
 */ 
static void *qemu_thread_ep (void * arg)
{
    qemu_args_t* qemu_args = (qemu_args_t*) arg;

    /*
     * To sync QEMU thread start with COSIM thread
     */
    pthread_mutex_lock(&mutex_qemu_sync);
    qemu_running = 1; 

    printf ("%s():QEMU ----> pthread_cond_wait () \n", __FUNCTION__);
    pthread_cond_wait(&cond_qemu_sync, &mutex_qemu_sync);
    printf ("%s():QEMU <---- pthread_cond_wait () \n", __FUNCTION__);

    printf ("%s():QEMU ----> pthread_cond_broadcast () \n", __FUNCTION__);
    pthread_cond_broadcast(&cond_qemu_sync);

    printf ("%s():QEMU ----> pthread_mutex_unlock () \n", __FUNCTION__);
    pthread_mutex_unlock(&mutex_qemu_sync);
 
    printf("%s():QEMU ----> qemu.main()\n", __FUNCTION__);
    /*
     * Jump intp QEMU -- QEMU.main (argc, argv)
     */
    qemu_args->qemu_ep (qemu_args->argc, qemu_args->argv);

    pthread_mutex_lock (&mutex_qemu_cosim_sync);
    qemu_running = 0;
    pthread_cond_broadcast (&cond_qemu_cosim_sync);
    pthread_mutex_unlock (&mutex_qemu_cosim_sync);   
    printf("%s():QEMU <---- qemu.main() qemu_running = %d\n", __FUNCTION__, qemu_running);
}

///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
