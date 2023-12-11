/*
 * ****** 11/15/2023 ******
 * The rudimentary prototype for the binary to
 * load QEMU.so
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <unistd.h>

#include <pthread.h>
#include <time.h>
#include <errno.h>

////////////////////////////////////////////////////////////////////

typedef int  (*qemu_ep_t)        (int, char**); 
typedef int  (*qemu_2_ep_t)      (void); 
typedef void (*qemu_cosim_API_t) (void*);

static void*             qemu_get_ep  (void * hso, char * ep);
static void*             load_qemu  (char * path);
static void*             qemu_thread_ep (void * arg);

typedef struct qemu_args_
{
    char**     argv;
    int        argc;   
    qemu_ep_t  qemu_ep; 
} qemu_args_t;

// It is not used at least now
typedef enum COSIM_state_
{
    COSIM_WHITE,
    COSIM_GREEN,
    COSIM_RED
} COSIM_state_t;

#define MAX_CPU 16  // enough for starters

typedef struct COSIM_cmp_
{
    pthread_mutex_t  mutex;
    pthread_cond_t   cond_cosim;
    pthread_cond_t   cond_qemu;
    COSIM_state_t    state;
    void*            reported_state;  
}  COSIM_cmp_t;

static COSIM_cmp_t cmp_state[MAX_CPU];

static void run_sims (qemu_args_t*);

///////////////////////////////////////////////////////////////
//
//  The prototype of QEMU-COSIM reportig API.
//
//  The stuff below is the same for COSIM and QEMU - hence
//  should be defined in some common include file - TBD.
//  Moreover COSIM does not use this structure literally .
//  QEMU meanwhile uses the same structure declared separately in QEMU
//
typedef struct COSIM_report_
{
    uint64_t pc_before; 
    uint64_t pc_after; 
    int      cpu_index;
    /////////////////////////////////////////////////////////////
    void*    void_report_fn; 

} COSIM_report_t;

typedef int (*COSIM_report_inst_t) (COSIM_report_t *); 
typedef int (*COSIM_validate_t)    (void);

#define COSIM_KEY 0x778654
 
//
//  The prototype of QEMU-COSIM registering API.
//  Initially ... only one function (to report an instruction execution)
//  and some unnecessary function for valiation - to make sure we do not crash :-)
//  exists
//   
typedef struct COSIM_register_
{
    COSIM_report_inst_t COSIM_insn_report_fn;
    COSIM_validate_t    COSIM_validate_fn; 
} COSIM_register_t;

int COSIM_validate_ep   (void);
int COSIM_report_inst_f (COSIM_report_t *pr);

static int qemu_running = 0; 

////////////////////////////////////////////////////////////////////

static COSIM_register_t* cosim_populate_API (void);

////////////////////////////////////////////////////////////////////


// SL : It is ok to call REGISTER in main thread. But MAIN and RUn shoudd be in the same second thread TBD
int 
main (int argc, char** argv)
{
    printf ("%s() ---------- COSIM --------------\n", __FUNCTION__); 

    int i;
    for (i = 0; i < argc; i++) {
      printf ("%s(): ARGV[%d] = %s\n", __FUNCTION__, i, argv[i]);
    }

    if (argc == 1) {
        printf ("Usage: cosim <QEMU.so path> <args to QEMU.so>\n");
        return 0;
    }

    char **soargv = (char**)malloc(sizeof(char*) * 64);
    int    soargc = 0;
   
    soargv[0] = strdup("qemu.so");
    soargc++;
 
    soargv[1] = strdup("-nographic");
    soargc++;

    soargv[2] = strdup("-accel");
    soargc++;

    soargv[3] = strdup("tcg,one-insn-per-tb=on,thread=single");
    soargc++;

    soargv[4] = strdup("-d");
    soargc++;

    soargv[5] = strdup("nochain,prefix:cosim");
    soargc++; 

    soargv[6] = strdup("-plugin");
    soargc++;

    soargv[7] = strdup(argv[2]);   // plugin
    soargc++;

    soargv[8] = strdup("-machine");   
    soargc++;

    soargv[9] = strdup("virt");   
    soargc++;

    soargv[10] = strdup("-bios");   
    soargc++;

    soargv[11] = strdup(argv[3]);   
    soargc++;

    soargv[12] = strdup("-cosim");
    soargc++;

    //
    // argv [1]  = QEMU.so full name
    // argv [2]  = plugin.so
    // argv [3]  = <bare metal binary>
    // 

    //
    // ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d nochain,prefix:mylog -plugin ${PREFIX_FOR_INSTALL}/plugins/lib${PLUGIN}.so
    //                                               -machine virt -bios ${HELLO}

    void *h = load_qemu (argv[1]);
    void *h_ep = NULL; 
    printf ("%s() <----- load_qemu() \n", __FUNCTION__);

    //////////////////////////////////////////////////////
   
    if (h == NULL) {
        printf ("Unable to load QEMU.SO\n");
        return 0;
    }

    //////////////////////////////////////////////////////
 
    //  Init QEMU enter
    h_ep = qemu_get_ep  (h, "main");
    qemu_ep_t ep_func = (qemu_ep_t)h_ep;

    qemu_args_t* pqemu_arg = (qemu_args_t*) malloc (sizeof(qemu_args_t));
    pqemu_arg->argv    = soargv;
    pqemu_arg->argc    = soargc;
    pqemu_arg->qemu_ep = ep_func;
 
    //////////////////////////////////////////////////////

    // Register QEMU --> cosim API
    h_ep = qemu_get_ep  (h, "qemu_cosim_API");
    qemu_cosim_API_t register_func = (qemu_cosim_API_t)h_ep;

    printf ("%s() --> register COSIM API with QEMU\n", __FUNCTION__);
    COSIM_register_t* papi = cosim_populate_API ();
    register_func (papi); 

    //////////////////////////////////////////////////////

    run_sims (pqemu_arg);
    return 0;   
}

////////////////////////////////////////////////////////////////////

static COSIM_register_t* cosim_populate_API (void)
{
    COSIM_register_t* papi = (COSIM_register_t*) malloc(sizeof(COSIM_register_t));

    papi->COSIM_insn_report_fn = COSIM_report_inst_f;
    papi->COSIM_validate_fn    = COSIM_validate_ep;

    return papi; 
} 

////////////////////////////////////////////////////////////////////

static void*
qemu_get_ep (void * hso, char * ep)
{
    printf ("%s() -- EP = <%s> \n", __FUNCTION__, ep);
 
    void* h = dlsym (hso, ep); 
    printf ("%s(): <---- dlsym() H = %p\n", __FUNCTION__, h);

    return h;
}

////////////////////////////////////////////////////////////////////

static void*
load_qemu  (char * path)
{
    char *full_path = malloc (strlen(path) + 128);
    sprintf (full_path, "%s/%s", path, "qemu-system-riscv64.so");

    printf ("%s() -- full path = <%s> \n", __FUNCTION__, full_path);
 
    /*
     * With the flags below QEMU.so names are properly resolved when 
     * invoked  from PLUGIN.so.  The segfault when using teh "specially" linked
     * PLUGIN.so also disapperas.  
     */
    void* h = dlopen(full_path, RTLD_LAZY | RTLD_GLOBAL); 
    printf ("%s(): <---- dlopen() H = %p\n", __FUNCTION__, h);

    return h;
}

////////////////////////////////////////////////////////////////////

typedef int (*qemu_report_cosim_insn) (void* opaque_state, uint64_t pc_insn, uint64_t pc_next); 

static void *qemu_thread_ep(void *arg);
static pthread_t thr_qemu;

///////////////////////////////////////////////////////////////////

/*
 * for now this function is empty - just a placeholder
 * =============  MAIN THREAD ============= 
 */
static void COSIM_compare (COSIM_cmp_t *pstate)
{
    int i = 0; 
    for  (i = 0; i < 100; i++) {
        pstate->state = COSIM_GREEN;
    }
}

/*
 * For starters we do not block QEMU callback with COND_WAIT.
 * Instead it spins on COSIM_ STATE = {COSIM WAIT, COMPARISON SUCCESS,  COMPARISON FAILED }
 * =============  MAIN THREAD ============= 
 */ 
static void COSIM_cmp_run (void *args)
{
    int first_time = 1; 
    struct timespec tm;

    int i;
    for (i = 0; i < MAX_CPU; i++) {
        cmp_state[i].state = COSIM_WHITE;
        pthread_mutex_init(&cmp_state[i].mutex, NULL);
        pthread_cond_init(&cmp_state[i].cond_cosim, NULL);
        pthread_cond_init(&cmp_state[i].cond_qemu, NULL);
    }

    // For now deal with CPU 0 only
    COSIM_cmp_t *pstate = &cmp_state[0];

    pthread_create(&thr_qemu, NULL, qemu_thread_ep, args);

    while ( qemu_running == 0) {
        sleep(1);
    }    

    pthread_mutex_lock(&pstate->mutex);
    
    // The green light for QEMU thread 
    pthread_cond_broadcast(&pstate->cond_qemu);

    while (1) {
        tm.tv_sec  = time(0) + 2;
        tm.tv_nsec = 0;

        // Now waiting for QEMU intsruction execution callback to wake me up .

        printf ("COSIM  ----> pthread_cond_timedwait() \n");
        int rc = pthread_cond_timedwait(&pstate->cond_cosim,  &pstate->mutex, &tm);
        printf ("COSIM  <---- pthread_cond_timedwait() rc = %d \n", rc);
        if (rc != 0) {
            printf ("COSIM: <---- pthread_cond_timedwait() rc = %d ETIMEDOUT = %d  \n", rc , ETIMEDOUT);
        }
        pthread_mutex_unlock(&pstate->mutex);

        // usually the timeout happens after the last instruction is executed
        if (rc == ETIMEDOUT) {
	    break;       
        }

        // The fake result comparison - should it be under the lock or not 
        COSIM_compare (pstate);

        pthread_mutex_lock(&pstate->mutex);

        // Wake up QEMU thread in case of success
        pthread_cond_broadcast(&pstate->cond_qemu);

    }
    pthread_join (thr_qemu, NULL);
    printf(" COSIM/QEMU exit after pthread_join() \n");
    exit (0);    
}

////////////////////////////////////////////////////////////////////
/*
 *  This function creates QEMU thread, waits for report/notify callback,
 *  blocks the callback, performs data comparison and in case of success
 *  lets QEMU to proceed
 */ 
static void run_sims (qemu_args_t* qemu_args)
{
    int rc = 0;
    COSIM_cmp_run (qemu_args);
    return;
}

////////////////////////////////////////////////////////////////////
/*
 * =============  QEMU THREAD =============
 */ 
static void *qemu_thread_ep (void * arg)
{
    qemu_args_t* qemu_args = (qemu_args_t*) arg;

    COSIM_cmp_t *pstate = &cmp_state[0]; 

    pthread_mutex_lock(&pstate->mutex);
    qemu_running = 1; 
    pthread_cond_wait(&pstate->cond_qemu, &pstate->mutex);

    pthread_mutex_unlock(&pstate->mutex);

    //
    // Here simply QEMU main() is called which invokes
    // report/notify callback after an every instruction is executed
    //
    printf("COSIM ----> qemu.main()\n");
    qemu_args->qemu_ep (qemu_args->argc, qemu_args->argv);      
    printf("COSIM <---- qemu.main()\n");
}

////////////////////////////////////////////////////////////////////

int COSIM_validate_ep (void)
{
    return COSIM_KEY;
}

///////////////////////////////////////////////////////////////////

/*
 * COSIM insn report/notify callback invoked by QEMU
 * =============  QEMU THREAD =============
 */
int COSIM_report_inst_f (COSIM_report_t *pr)
{
    printf ("\nQEMU:  PC = 0x%lx ====================\n", pr->pc_before);

    int idx = 0;

    if ((idx = pr->cpu_index) != 0) {
        printf ("Internal error: COSIM supports only one CPU for now (cpu_index = %d)\n", pr->cpu_index);
        exit (0);
    }

    COSIM_cmp_t *pstate = &cmp_state[idx];

    // The data delivered for comparison
    pstate->reported_state = pr;

    pthread_mutex_lock (&pstate->mutex);

    // Wake up COSIM thread to validate instruction data.

    pthread_cond_broadcast(&pstate->cond_cosim);
    pthread_cond_wait(&pstate->cond_qemu, &pstate->mutex);
    pthread_mutex_unlock(&pstate->mutex);

    // Return back to QEMU - next instruction 
    return 0;
}

////////////////////////////////////////////////////////////////////
