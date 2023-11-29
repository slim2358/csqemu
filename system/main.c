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

int cosim_mode = false;

int qemu_default_main(void)
{ 
    int status = 0;

LOGIM("--> qemu_main_loop() status = %d", status);

    status = qemu_main_loop();
    qemu_cleanup(status);

    return status;
}

int (*qemu_main)(void) = qemu_default_main;

int main(int argc, char **argv)
{
    int i;
    for (i = 0; i < argc; i++) {
        printf ("%s() ---- argv [%d] = %s\n ", __FUNCTION__, i, argv[i]);
    }

    bool iam_qemu_so = (strcmp (argv[argc - 1], "-cosim") == 0);
    cosim_mode = iam_qemu_so;
    if (iam_qemu_so) {
        argc--;
    }
    qemu_init (argc, argv);

LOGIM ("<------ qemu_init()");

#if 1
    if (iam_qemu_so) {
        printf("%s() ---- RETURN from QEMU\n", __FUNCTION__);
        return 0;
    }
#endif 
      
    return qemu_main();
}




