/***********************************/
/*  Synthesizable Verilog Dumping  */
/*       Xifan TANG, EPFL/LSI      */
/***********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* Include vpr structs*/
#include "util.h"
#include "physical_types.h"
#include "vpr_types.h"
#include "globals.h"
#include "rr_graph.h"
#include "vpr_utils.h"
#include "path_delay.h"
#include "stats.h"
#include "route_common.h"

/* Include FPGA-SPICE utils */
#include "read_xml_spice_util.h"
#include "linkedlist.h"
#include "fpga_x2p_types.h"
#include "fpga_x2p_utils.h"
#include "fpga_x2p_pbtypes_utils.h"
#include "fpga_x2p_backannotate_utils.h"
#include "fpga_x2p_globals.h"
#include "fpga_bitstream.h"

/* Include SynVerilog headers */
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_submodules.h"
#include "verilog_pbtypes.h"
#include "verilog_routing.h"
#include "verilog_compact_netlist.h"
#include "verilog_top_testbench.h"
#include "verilog_autocheck_top_testbench.h"
#include "verilog_verification_top_netlist.h"
#include "verilog_modelsim_autodeck.h"
#include "verilog_sdc.h"
#include "verilog_formality_autodeck.h"

/***** Subroutines *****/
/* Alloc array that records Configuration bits for :
 * (1) Switch blocks
 * (2) Connection boxes
 * TODO: Can be improved in alloc strategy to be more memory efficient!
 */
static 
void alloc_global_routing_conf_bits() {
  int i;
  
  /* Alloc array for Switch blocks */
  num_conf_bits_sb = (int**)my_malloc((nx+1)*sizeof(int*));
  for (i = 0; i < (nx + 1); i++) {
    num_conf_bits_sb[i] = (int*)my_calloc((ny+1), sizeof(int));
  }

  /* Alloc array for Connection blocks */
  num_conf_bits_cbx = (int**)my_malloc((nx+1)*sizeof(int*));
  for (i = 0; i < (nx + 1); i++) {
    num_conf_bits_cbx[i] = (int*)my_calloc((ny+1), sizeof(int));
  }

  num_conf_bits_cby = (int**)my_malloc((nx+1)*sizeof(int*));
  for (i = 0; i < (nx + 1); i++) {
    num_conf_bits_cby[i] = (int*)my_calloc((ny+1), sizeof(int));
  }

  return;
}

static 
void free_global_routing_conf_bits() {
  int i;

  /* Free array for Switch blocks */
  for (i = 0; i < (nx + 1); i++) {
    my_free(num_conf_bits_sb[i]);
  }
  my_free(num_conf_bits_sb);
  
  /* Free array for Connection box */
  for (i = 0; i < (nx + 1); i++) {
    my_free(num_conf_bits_cbx[i]);
  }
  my_free(num_conf_bits_cbx);

  for (i = 0; i < (nx + 1); i++) {
    my_free(num_conf_bits_cby[i]);
  }
  my_free(num_conf_bits_cby);

  return;
}
 
/* Top-level function*/
void vpr_fpga_verilog(t_vpr_setup vpr_setup,
                      t_arch Arch,
                      char* circuit_name) {
  /* Timer */
  clock_t t_start;
  clock_t t_end;
  float run_time_sec;

  int num_clocks = Arch.spice->spice_params.stimulate_params.num_clocks;
  /* int vpr_crit_path_delay = Arch.spice->spice_params.stimulate_params.vpr_crit_path_delay; */
  
  /* Directory paths */
  char* verilog_dir_formatted = NULL;
  char* submodule_dir_path= NULL;
  char* lb_dir_path = NULL;
  char* rr_dir_path = NULL;
  char* top_netlist_file = NULL;
  char* top_netlist_path = NULL;
  char* top_testbench_file_name = NULL;
  char* top_testbench_file_path = NULL;
  char* blif_testbench_file_name = NULL;
  char* blif_testbench_file_path = NULL;
  char* bitstream_file_name = NULL;
  char* bitstream_file_path = NULL;
  char* formal_verification_top_netlist_file_name = NULL;
  char* formal_verification_top_netlist_file_path = NULL;
  char* autocheck_top_testbench_file_name = NULL;
  char* autocheck_top_testbench_file_path = NULL;

  char* chomped_parent_dir = NULL;
  char* chomped_circuit_name = NULL;
 
  t_sram_orgz_info* sram_verilog_orgz_info = NULL;

  /* Check if the routing architecture we support*/
  if (UNI_DIRECTIONAL != vpr_setup.RoutingArch.directionality) {
    vpr_printf(TIO_MESSAGE_ERROR, "FPGA synthesizable Verilog dumping only support uni-directional routing architecture!\n");
    exit(1);
  }
  
  /* We don't support mrFPGA */
#ifdef MRFPGA_H
  if (is_mrFPGA) {
    vpr_printf(TIO_MESSAGE_ERROR, "FPGA synthesizable verilog dumping do not support mrFPGA!\n");
    exit(1);
  }
#endif
  
  assert ( TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.dump_syn_verilog);

  /* VerilogGenerator formally starts*/
  vpr_printf(TIO_MESSAGE_INFO, "\nFPGA synthesizable verilog generator starts...\n");
 
  /* Start time count */
  t_start = clock();

  /* Format the directory paths */
  split_path_prog_name(circuit_name, '/', &chomped_parent_dir, &chomped_circuit_name);

  if (NULL != vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.syn_verilog_dump_dir) {
    verilog_dir_formatted = format_dir_path(vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.syn_verilog_dump_dir);
  } else { 
    verilog_dir_formatted = format_dir_path(my_strcat(format_dir_path(chomped_parent_dir),default_verilog_dir_name));
  }
  /* lb directory */
  (lb_dir_path) = my_strcat(verilog_dir_formatted, default_lb_dir_name);
  /* routing resources directory */
  (rr_dir_path) = my_strcat(verilog_dir_formatted, default_rr_dir_name);
  /* submodule_dir_path */
  (submodule_dir_path) = my_strcat(verilog_dir_formatted, default_submodule_dir_name);
  /* Top netlists dir_path */
  top_netlist_file = my_strcat(chomped_circuit_name, verilog_top_postfix);
  top_netlist_path = my_strcat(verilog_dir_formatted, top_netlist_file);
  
  /* Create directories */
  create_dir_path(verilog_dir_formatted);
  create_dir_path(lb_dir_path);
  create_dir_path(rr_dir_path);
  create_dir_path(submodule_dir_path);

  /* assign the global variable of SRAM model */
  assert(NULL != Arch.sram_inf.verilog_sram_inf_orgz); /* Check !*/
  sram_verilog_model = Arch.sram_inf.verilog_sram_inf_orgz->spice_model;
  /* initialize the SRAM organization information struct */
  sram_verilog_orgz_info = alloc_one_sram_orgz_info();
  init_sram_orgz_info(sram_verilog_orgz_info, Arch.sram_inf.verilog_sram_inf_orgz->type, sram_verilog_model, nx + 2, ny + 2);

  /* Check all the SRAM port is using the correct SRAM SPICE MODEL */
  config_spice_models_sram_port_spice_model(Arch.spice->num_spice_model, 
                                            Arch.spice->spice_models,
                                            Arch.sram_inf.verilog_sram_inf_orgz->spice_model);

  /* Assign global variables of input and output pads */
  iopad_verilog_model = find_iopad_spice_model(Arch.spice->num_spice_model, Arch.spice->spice_models);
  assert(NULL != iopad_verilog_model);

  /* zero the counter of each spice_model */
  zero_spice_models_cnt(Arch.spice->num_spice_model, Arch.spice->spice_models);

  /* Initialize the user-defined verilog netlists to be included */
  init_list_include_verilog_netlists(Arch.spice);
 
  /* Initial global variables about configuration bits */
  alloc_global_routing_conf_bits();

  /* Initialize the number of configuration bits of all the grids */
  vpr_printf(TIO_MESSAGE_INFO, "Count the number of configuration bits, IO pads in each logic block...\n");
  /* init_grids_num_conf_bits(sram_verilog_orgz_type); */
  init_grids_num_conf_bits(sram_verilog_orgz_info);
  init_grids_num_iopads();
  /* init_grids_num_mode_bits(); */

  dump_verilog_defines_preproc(verilog_dir_formatted,
                               vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  /* Dump routing resources: switch blocks, connection blocks and channel tracks */
  dump_verilog_routing_resources(sram_verilog_orgz_info, verilog_dir_formatted, rr_dir_path, Arch, &vpr_setup.RoutingArch,
                                 num_rr_nodes, rr_node, rr_node_indices,
                                 vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  /* Output routing report_timing script :
   */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_report_timing_tcl) {
    verilog_generate_routing_report_timing(sram_verilog_orgz_info, verilog_dir_formatted,
                                           Arch, &vpr_setup.RoutingArch,
                                           num_rr_nodes, rr_node, rr_node_indices,
                                           vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);
  }

  /* Dump logic blocks 
   * Branches to go: 
   * 1. a compact output
   * 2. a full-size output
   */
  dump_compact_verilog_logic_blocks(sram_verilog_orgz_info, verilog_dir_formatted, lb_dir_path, &Arch,
                                    vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  /* Dump internal structures of submodules */
  dump_verilog_submodules(sram_verilog_orgz_info, verilog_dir_formatted, submodule_dir_path, 
                          Arch, &vpr_setup.RoutingArch, 
                          vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  /* Dump top-level verilog */
  dump_compact_verilog_top_netlist(sram_verilog_orgz_info, chomped_circuit_name, 
                                   top_netlist_path, verilog_dir_formatted, submodule_dir_path, lb_dir_path, rr_dir_path, 
                                   num_rr_nodes, rr_node, rr_node_indices, 
                                   num_clocks,
                                   vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, 
								   *(Arch.spice));
   
  /* Dump SDC constraints */
  /* dump_verilog_sdc_file(); */
  
  /* dump verilog testbench only for input blif */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_input_blif_testbench) {
    blif_testbench_file_name = my_strcat(chomped_circuit_name, blif_testbench_verilog_file_postfix);
    blif_testbench_file_path = my_strcat(verilog_dir_formatted, blif_testbench_file_name);
    dump_verilog_input_blif_testbench(chomped_circuit_name, blif_testbench_file_path, verilog_dir_formatted,
                                      num_clocks, 
                                      vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, *(Arch.spice));
    /* Free */
    my_free(blif_testbench_file_name);
    my_free(blif_testbench_file_path);
  }

  /* Free sram_orgz_info:
   * Free the allocated sram_orgz_info before, we start bitstream generation !
   */
  free_sram_orgz_info(sram_verilog_orgz_info,
                      sram_verilog_orgz_info->type);

  /* Force enable bitstream generator when we need to output Verilog top testbench*/  
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_top_testbench) {
    vpr_setup.FPGA_SPICE_Opts.BitstreamGenOpts.gen_bitstream = TRUE;
  }

  /* Generate bitstream if required, and also Dump bitstream file */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.BitstreamGenOpts.gen_bitstream) {
    bitstream_file_name = my_strcat(chomped_circuit_name, fpga_spice_bitstream_output_file_postfix);
    bitstream_file_path = my_strcat(verilog_dir_formatted, bitstream_file_name);
    /* Run bitstream generation */
    vpr_fpga_generate_bitstream(vpr_setup, Arch, circuit_name, bitstream_file_path, &sram_verilog_orgz_info);
    my_free(bitstream_file_name);
    my_free(bitstream_file_path);
  }

  /* dump verilog testbench only for top-level: ONLY valid when bitstream is generated! */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_top_testbench) {
    top_testbench_file_name = my_strcat(chomped_circuit_name, top_testbench_verilog_file_postfix);
    top_testbench_file_path = my_strcat(verilog_dir_formatted, top_testbench_file_name);
    dump_verilog_top_testbench(sram_verilog_orgz_info, chomped_circuit_name, top_testbench_file_path,
                               verilog_dir_formatted, num_clocks, 
                               vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, *(Arch.spice));
    /* Free */
    my_free(top_testbench_file_name);
    my_free(top_testbench_file_path);
  }

  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_formal_verification_top_netlist) {
    formal_verification_top_netlist_file_name = my_strcat(chomped_circuit_name, formal_verification_verilog_file_postfix);
    formal_verification_top_netlist_file_path = my_strcat(verilog_dir_formatted, formal_verification_top_netlist_file_name);
    dump_verilog_formal_verification_top_netlist(sram_verilog_orgz_info, chomped_circuit_name, 
                                                 formal_verification_top_netlist_file_path, verilog_dir_formatted,
                                                 num_clocks, 
                                                 vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, *(Arch.spice));
/*	write_formality_script(vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts,
							verilog_dir_formatted,
							chomped_circuit_name,
							*(Arch.spice));
    /* Free */
    my_free(formal_verification_top_netlist_file_name);
    my_free(formal_verification_top_netlist_file_path);
  }

  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_autocheck_top_testbench) {
    autocheck_top_testbench_file_name = my_strcat(chomped_circuit_name, autocheck_top_testbench_verilog_file_postfix);
    autocheck_top_testbench_file_path = my_strcat(verilog_dir_formatted, autocheck_top_testbench_file_name);
    dump_verilog_autocheck_top_testbench(sram_verilog_orgz_info, chomped_circuit_name, 
                                         autocheck_top_testbench_file_path, verilog_dir_formatted, num_clocks, 
                                         vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, *(Arch.spice));
    /* Free */
    my_free(autocheck_top_testbench_file_name);
    my_free(autocheck_top_testbench_file_path);
  }

  /* Output Modelsim Autodeck scripts */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_modelsim_autodeck) {
    dump_verilog_modelsim_autodeck(sram_verilog_orgz_info, 
                                   vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts,
                                   *(Arch.spice),
                                   Arch.spice->spice_params.meas_params.sim_num_clock_cycle,
                                   verilog_dir_formatted, chomped_circuit_name);
  }

  if ((TRUE == vpr_setup.FPGA_SPICE_Opts.BitstreamGenOpts.gen_bitstream)
    || (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_top_testbench)
    || (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_autocheck_top_testbench)
    || (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_formal_verification_top_netlist)) {
    /* Free sram_orgz_info:
     * Free the allocated sram_orgz_info before, we start bitstream generation !
     */
    free_sram_orgz_info(sram_verilog_orgz_info,
                        sram_verilog_orgz_info->type);
  }

  /* End time count */
  t_end = clock();
 
  run_time_sec = (float)(t_end - t_start) / CLOCKS_PER_SEC;
  vpr_printf(TIO_MESSAGE_INFO, "Synthesizable verilog dumping took %g seconds\n", run_time_sec);  

  /* Free global array */
  free_global_routing_conf_bits();

  /* Free */
  my_free(verilog_dir_formatted);
  my_free(lb_dir_path);
  my_free(rr_dir_path);
  my_free(top_netlist_file);
  my_free(top_netlist_path);
  my_free(submodule_dir_path);

  return;
}
