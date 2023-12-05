/*
 * Copyright (C) 2023, Al
 *
 *  The training plugin for investigation and evaluation  - sandbox.
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

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static void plugin_exit (qemu_plugin_id_t id, void *p);
static void vcpu_tb_trans (qemu_plugin_id_t id, struct qemu_plugin_tb *tb);
static void vcpu_mem (unsigned int cpu_index, qemu_plugin_meminfo_t info,
                      uint64_t vaddr, void *udata);
static void vcpu_insn_exec(unsigned int cpu_index, void *udata);

typedef struct instr_
{
    uint64_t pc;
} Instr_t;

/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
        GString *s = g_string_new(NULL);
#if 0 /////////////////////////

    for (int i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_auto(GStrv) tokens = g_strsplit(opt, "=", 2);
        if (g_strcmp0(tokens[0], "ifilter") == 0) {
            parse_insn_match(tokens[1]);
        } else if (g_strcmp0(tokens[0], "afilter") == 0) {
            parse_vaddr_match(tokens[1]);
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

#endif

    printf ("%s(): SANDBOX.SO  entry point, ID = %lx \n", __FUNCTION__, id);

    /* Register translation block and exit callbacks */
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
#if 0
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
#endif

    printf ("SANDBOX.So return 0\n");
   return 0;
}

///////////////////////////////////////////////////////////////////////

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    struct qemu_plugin_insn *insn;
    size_t n = qemu_plugin_tb_n_insns(tb);

    //    printf ("%s(): ID = %lx, nTb = %d \n", __FUNCTION__, id, n);

    insn = qemu_plugin_tb_get_insn(tb, 0);

    uint64_t tb_pc = qemu_plugin_insn_vaddr(insn);
    // printf ("== SANDBOX: ==== TB PC = 0x%lx (%d) ====\n", tb_pc, n);    

    for (size_t i = 0; i < n; i++) {
        
        uint64_t insn_vaddr;
        uint32_t *pd = NULL;
        uint32_t instr;

        /*
         * SL: It is very memory consuming to allocate the below structure on
         *     per instruction basis.
         *    Yet ... we have to do it becasue per insn callback does not pass any
         *    internal data and relies on USER DATA provided by this callback
         */
        Instr_t *pinstr = (Instr_t *)malloc(sizeof(Instr_t));

        /*
         * `insn` is shared between translations in QEMU, copy needed data here.
         * `output` is never freed as it might be used multiple times during
         * the emulation lifetime.
         * We only consider the first 32 bits of the instruction, this may be
         * a limitation for CISC architectures.
         */
        insn = qemu_plugin_tb_get_insn(tb, i);
        insn_vaddr = qemu_plugin_insn_vaddr(insn);
        pd = qemu_plugin_insn_data(insn);
        instr = *pd;

        pinstr->pc = insn_vaddr;

        /* Register callback on memory read or write */
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         QEMU_PLUGIN_MEM_RW, NULL);

        /* Register callback on instruction */
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, pinstr);

    }
}

///////////////////////////////////////////////////////////////////////

/**
 * On plugin exit, print last instruction in cache
 */
static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    printf ("%s() ----- plugin exit -----\n", __FUNCTION__);
}

///////////////////////////////////////////////////////////////////////

static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t info,
                     uint64_t vaddr, void *udata)
{
    /* If full system emulation log physical address and device name */
    struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    char *mem_op = NULL;

    if (qemu_plugin_mem_is_store(info)) {
        mem_op = "ST";
    } else {
        mem_op = "LD";
    }

    if (hwaddr) {
        uint64_t addr = qemu_plugin_hwaddr_phys_addr(hwaddr);
        const char *name = qemu_plugin_hwaddr_device_name(hwaddr);
        // printf ("%s <--->[V: 0x%08"PRIx64"] [P: 0x%08"PRIx64"] <%s>\n", mem_op, vaddr, addr, name);
    } else {
        // printf ("%s <--->[V: 0x%08"PRIx64"] <%s>\n", mem_op, vaddr);
    }
}

///////////////////////////////////////////////////////////////////////

static void vcpu_insn_exec(unsigned int cpu_index, void *udata)
{
    Instr_t *pinstr = (Instr_t *)udata;
    
    //printf ("== SANDBOX: ==== cpu %d: PC = 0x%lx ====\n", cpu_index, pinstr->pc);    
}
