/*
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef QEMU_MAIN_H
#define QEMU_MAIN_H

int qemu_default_main(void);
extern int (*qemu_main)(void);

////////////////////////////////////////////////
//
//  COSIM stuff
//

///////////////////////////////////////////////////////////////
//
//  The prototype of QEMU-COSIM reportig API.
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

//
//  The prototype of QEMU-COSIM registering API.
//  Initially ... only one function (to report an instruction execution)
//  exists
//   
typedef struct COSIM_register_
{
    COSIM_report_inst_t COSIM_insn_report_fn;
    COSIM_validate_t    COSIM_validate_fn; 
} COSIM_register_t;

////////////////////////////////////////////////

#endif /* QEMU_MAIN_H */
