/*
 * Copyright (C) 2021, Alexandre Iooss <erdnaxe@crans.org>
 *
 * Log instruction execution with memory access.
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <qemu-plugin.h>
#include "qemu-plugin-cpu.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static st_rvfi_t *cosim = NULL;
static bool plugin_init = true;

static CPUArchState prev_cpu_state;

#if 0
/**
 * Add memory read or write information to current instruction log
 */
static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t info,
                     uint64_t vaddr, void *udata)
{
    GString *s;

    /* Find vCPU in array */
    g_rw_lock_reader_lock(&expand_array_lock);
    g_assert(cpu_index < last_exec->len);
    s = g_ptr_array_index(last_exec, cpu_index);
    g_rw_lock_reader_unlock(&expand_array_lock);

    /* Indicate type of memory access */
    if (qemu_plugin_mem_is_store(info)) {
        g_string_append(s, ", store");
    } else {
        g_string_append(s, ", load");
    }

    /* If full system emulation log physical address and device name */
    struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    if (hwaddr) {
        uint64_t addr = qemu_plugin_hwaddr_phys_addr(hwaddr);
        const char *name = qemu_plugin_hwaddr_device_name(hwaddr);
        g_string_append_printf(s, ", 0x%08"PRIx64", %s", addr, name);
    } else {
        g_string_append_printf(s, ", 0x%08"PRIx64, vaddr);
    }
}
#endif

static void vcpu_insn_after_exec(unsigned int cpu_index, void *udata)
{
    CPUState *state = (CPUState *)qemu_plugin_get_cpu(cpu_index);
    CPUArchState *cpu;
    cosim_args_t *regs;
    
    if (state == NULL) {
        qemu_plugin_outs("CPU State does not exist\n");
        return;
    }
    cpu = (CPUArchState *)(state + 1);
    regs = &cpu->cosim_args;
    
    fprintf(stderr,"**** After insn npc=%lx insn=%0x\n", cpu->pc, cpu->cosim_args.insn);
    fprintf(stderr,"     Regs: %08x\n", cpu->cosim_args.insn_p_regs);

    if (regs->insn_regs.rs1 != 0) fprintf(stderr,"           rs1=%2d val=%016lx\n",
        regs->insn_regs.rs1, prev_cpu_state.gpr[regs->insn_regs.rs1]);

    if (regs->insn_regs.rs2 != 0) fprintf(stderr,"           rs2=%2d val=%016lx\n",
        regs->insn_regs.rs2, prev_cpu_state.gpr[regs->insn_regs.rs2]);
        
    if (regs->insn_regs.rs3 != 0) fprintf(stderr,"           rs3=%2d val=%016lx\n",
        regs->insn_regs.rs3, prev_cpu_state.gpr[regs->insn_regs.rs3]);

    if (regs->insn_regs.rd != 0)  fprintf(stderr,"           rd =%2d val=%016lx\n",
        regs->insn_regs.rd, cpu->gpr[regs->insn_regs.rd]);
         
}

/**
 * Log instruction execution
 */
static void vcpu_insn_exec(unsigned int cpu_index, void *udata)
{

    CPUState *state = (CPUState *)qemu_plugin_get_cpu(cpu_index);
    CPUArchState *cpu; // Same as CPURISCVState
    
    if (state == NULL) {
        qemu_plugin_outs("CPU State does not exist\n");
        return;
    }
    cpu = (CPUArchState *)(state + 1);
    fprintf(stderr,"+++++ BEFORE INSN pc=%08lx\n",cpu->pc);
    prev_cpu_state = *cpu;
}

/**
 * On translation block new translation
 *
 * QEMU convert code by translation block (TB). By hooking here we can then hook
 * a callback on each instruction and memory access.
 */
static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    struct qemu_plugin_insn *insn;

    if (plugin_init) {
        CPUState *state = (CPUState *)qemu_plugin_get_cpu(0);
        CPUArchState *cpu;

        plugin_init = false;
        if (state == NULL) {
            fprintf(stderr, "CPU State does not exist\n");
        } else {
            cpu = (CPUArchState *)(state + 1);
            cpu->cosim_state = cosim;
        }
    }

    size_t n = qemu_plugin_tb_n_insns(tb);

    if ( n == 1) {
        insn = qemu_plugin_tb_get_insn(tb, 0);

//            /* Register callback on memory read or write */
//            qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem,
//                                             QEMU_PLUGIN_CB_NO_REGS,
//                                             QEMU_PLUGIN_MEM_RW, NULL);

            /* Register callback on instruction */
            qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                                   QEMU_PLUGIN_CB_NO_REGS, NULL);

            /* Register callback after instruction execution */
            qemu_plugin_register_vcpu_insn_after_exec_cb(insn, vcpu_insn_after_exec,
                                                   QEMU_PLUGIN_CB_NO_REGS, NULL);
    }
    
}

/**
 * On plugin exit, free memory
 */
static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_free(cosim);
}


/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
    /*
     * Initialize plugin to get state for co-simulation
     * Check if we translate one-insn-per-tb
     */

    fprintf(stderr, "********* COSIM\n");
    fprintf(stderr, "**** Architetcure: %s\n", info->target_name);
    fprintf(stderr, "**** CPU num: %d\n", info->system.smp_vcpus);
    
    cosim = g_malloc0(sizeof(st_rvfi_t));
    if (cosim == NULL) {
        fprintf(stderr, "g_malloc0: cannot allocate memory\n");
        return -1;
    }

    /* Register translation block and exit callbacks */
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
