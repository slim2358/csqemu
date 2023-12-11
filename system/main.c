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

/*
 * Temporary location of COSIM stuff - to be moved to separate files 
 */
///////////////////////////////////////////////////////////////

extern int cosim_ep (void);
void qemu_cosim_run (void);
void qemu_cosim_API (void* opaque_data);

COSIM_register_t *COSIM_api_ptr    = NULL;
COSIM_report_t   *COSIM_report_ptr = NULL;

#define COSIM_KEY 0x778654

/*
 * This flag is introduced for COSIM-RTL-QEMU  lockstep mode.
 * It is propagated to CPUState as well
 */

int cosim_mode = false;

///////////////////////////////////////////////////////////////

int qemu_default_main(void)
{ 
    int status = 0;

LOGIM("--> qemu_main_loop() status = %d", status);

    status = qemu_main_loop();
    printf ("QEMU:%s() <---- qemu_main_loop() status = %d\n", __FUNCTION__, status);

    qemu_cleanup(status);
    printf ("QEMU:%s() <---- qemu_cleanup() \n", __FUNCTION__);

    return status;
}

int (*qemu_main)(void) = qemu_default_main;

int main(int argc, char **argv)
{
    int i;
    printf ("QEMU:%s() --------\n", __FUNCTION__);

    for (i = 0; i < argc; i++) {
        printf ("QEMU:%s() ---- argv [%d] = %s\n ", __FUNCTION__, i, argv[i]);
    }

    bool iam_qemu_so = (strcmp (argv[argc - 1], "-cosim") == 0);
    cosim_mode = iam_qemu_so;
    if (iam_qemu_so) {
        argc--;
    }
    qemu_init (argc, argv);
LOGIM ("<------ qemu_init()");

    int rc = qemu_main();
    printf ("<---- QEMU:%s() RETURN \n", __FUNCTION__);
    return rc;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

void qemu_cosim_run (void)
{
    int rc = qemu_main();
    printf ("QEMU:%s() <-- qemu_main() rc = %d\n", __FUNCTION__, rc);   
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
//
//  The prototype of QEMU-COSIM reporting API.
//
///////////////////////////////////////////////////////////////

void qemu_cosim_API (void* opaque_data)
{
    COSIM_register_t* p = (COSIM_register_t*)opaque_data;
    int  cosim_ret_key = p->COSIM_validate_fn ();

    if (cosim_ret_key == COSIM_KEY) {
        printf ("COSIM is valid! Go ahead ....\n");
    }
    else {
        printf ("COSIM is invlaid - EXIT!\n");
        exit(0);
    }

    COSIM_api_ptr = (COSIM_register_t*) malloc(sizeof(COSIM_register_t));
    *COSIM_api_ptr = *p;

    COSIM_report_ptr = (COSIM_report_t*)malloc(sizeof(COSIM_report_t));

    COSIM_report_ptr->pc_before = 0; 
    COSIM_report_ptr->pc_after  = 0;
    COSIM_report_ptr->void_report_fn = (void*)(COSIM_api_ptr->COSIM_insn_report_fn); 
}

//////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
