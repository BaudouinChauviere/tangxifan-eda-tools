/***********************************/
/*      SPICE Modeling for VPR     */
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
#include "rr_graph_swseg.h"
#include "vpr_utils.h"

/* Include spice support headers*/
#include "linkedlist.h"
#include "spice_utils.h"
#include "fpga_spice_globals.h"

/* Include verilog support headers*/
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_pbtypes.h"
#include "verilog_primitives.h"

enum e_ff_trigger_type {
  FF_RE, FF_FE
};

/* Subroutines */
void dump_verilog_pb_primitive_ff(FILE* fp,
                            char* subckt_prefix,
                            t_logical_block* mapped_logical_block,
                            t_pb_graph_node* prim_pb_graph_node,
                            int index,
                            t_spice_model* verilog_model) {
  int i;
  /* Default FF settings, applied when this FF is idle*/
  enum e_ff_trigger_type trigger_type = FF_RE;
  int init_val = 0;
 
  int num_input_port = 0;
  t_spice_model_port** input_ports = NULL;
  int num_output_port = 0;
  t_spice_model_port** output_ports = NULL;
  int num_clock_port = 0;
  t_spice_model_port** clock_ports = NULL;

  char* formatted_subckt_prefix = format_verilog_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  t_pb_type* prim_pb_type = NULL;
  char* port_prefix = NULL;

  /* Ensure a valid file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Ensure a valid pb_graph_node */ 
  if (NULL == prim_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid prim_pb_graph_node!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Find ports*/
  input_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_INPUT, &num_input_port);
  output_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_OUTPUT, &num_output_port);
  clock_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_CLOCK, &num_clock_port);

  /* Asserts */
  assert(3 == num_input_port); /* D, Set and Reset*/
  for (i = 0; i < num_input_port; i++) {
    assert(1 == input_ports[i]->size);
  }
  assert(1 == num_output_port);
  assert(1 == output_ports[0]->size);
  assert(1 == num_clock_port);
  assert(1 == clock_ports[0]->size);

  assert(SPICE_MODEL_FF == verilog_model->type);
  
  /* Initialize */
  prim_pb_type = prim_pb_graph_node->pb_type;

  /* Generate Subckt for pb_type*/
  /*
  port_prefix = (char*)my_malloc(sizeof(char)*
                (strlen(formatted_subckt_prefix) + strlen(prim_pb_type->name) + 1
                 + strlen(my_itoa(index)) + 1 + 1));
  sprintf(port_prefix, "%s%s[%d]", formatted_subckt_prefix, prim_pb_type->name, index);
  */
  /* Simplify the port prefix, make SPICE netlist readable */
  port_prefix = (char*)my_malloc(sizeof(char)*
                (strlen(prim_pb_type->name) + 1
                 + strlen(my_itoa(index)) + 1 + 1));
  sprintf(port_prefix, "%s_%d_", prim_pb_type->name, index);
  /* Comment lines */
  fprintf(fp, "//----- Flip-flop Verilog module: %s%s -----\n", 
          formatted_subckt_prefix, port_prefix);
  /* Definition line */
  fprintf(fp, "module %s%s (", formatted_subckt_prefix, port_prefix);
  /* Only dump the global ports belonging to a spice_model */
  if (0 < rec_dump_verilog_spice_model_global_ports(fp, verilog_model, TRUE, TRUE)) {
    fprintf(fp, ",\n");
  }
  /* print ports*/
  dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, TRUE, FALSE); 
  /* Local vdd and gnd*/
  fprintf(fp, ");\n");
  /* Definition ends*/

  /* Call the dff subckt*/
  fprintf(fp, "%s %s_%d_ (", verilog_model->name, verilog_model->prefix, verilog_model->cnt);
  /* Only dump the global ports belonging to a spice_model */
  if (0 < rec_dump_verilog_spice_model_global_ports(fp, verilog_model, FALSE, TRUE)) {
    fprintf(fp, ",\n");
  }
  /* print ports*/
  dump_verilog_pb_type_ports(fp, port_prefix, 1, prim_pb_type, FALSE, FALSE); /* Use global clock for each DFF...*/ 

  /* Local vdd and gnd, verilog_model name
   * TODO: global vdd for ff
   */
  fprintf(fp, ");\n");

  /* Apply rising edge, and init value to the ff*/
  if (NULL != mapped_logical_block) {
    /* Consider the rising edge|falling edge */
    if (0 == strcmp("re", mapped_logical_block->trigger_type)) { 
      trigger_type = FF_RE;
    } else if (0 == strcmp("fe", mapped_logical_block->trigger_type)) { 
      trigger_type = FF_FE;
    } else {
      vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid ff trigger type! Should be [re|fe].\n",
                 __FILE__, __LINE__);
      exit(1);
    }
    /* Assign initial value */
    if (1 == mapped_logical_block->init_val) {
      init_val = 1;
    } else {
      init_val = 0;
    }

    /* Back-annotate to logical block */
    mapped_logical_block->mapped_spice_model = verilog_model;
    mapped_logical_block->mapped_spice_model_index = verilog_model->cnt;
  } else {
    trigger_type = FF_RE;
    init_val = 0;
  }
  /* TODO: apply falling edge, initial value to FF!!!*/
  /*fprintf(fp, "\n");*/

  /* End */
  fprintf(fp, "endmodule\n");

  /* Comment lines */
  fprintf(fp, "//----- END Flip-flop Verilog module: %s%s -----\n\n", 
          formatted_subckt_prefix, port_prefix);

  verilog_model->cnt++;

  /*Free*/ 
  my_free(formatted_subckt_prefix);
  my_free(port_prefix);

  return;
}

/* Print hardlogic SPICE subckt*/
void dump_verilog_pb_primitive_hardlogic(FILE* fp,
                                   char* subckt_prefix,
                                   t_logical_block* mapped_logical_block,
                                   t_pb_graph_node* prim_pb_graph_node,
                                   int index,
                                   t_spice_model* verilog_model) {
  int num_input_port = 0;
  t_spice_model_port** input_ports = NULL;
  int num_output_port = 0;
  t_spice_model_port** output_ports = NULL;
  int num_clock_port = 0;
  t_spice_model_port** clock_ports = NULL;

  char* formatted_subckt_prefix = format_verilog_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  t_pb_type* prim_pb_type = NULL;
  char* port_prefix = NULL;

  /* Ensure a valid file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Ensure a valid pb_graph_node */ 
  if (NULL == prim_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid prim_pb_graph_node!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Find ports*/
  input_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_INPUT, &num_input_port);
  output_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_OUTPUT, &num_output_port);
  clock_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_CLOCK, &num_clock_port);

  /* Asserts */
  assert(SPICE_MODEL_HARDLOGIC == verilog_model->type);
  
  /* Initialize */
  prim_pb_type = prim_pb_graph_node->pb_type;

  /* Generate Subckt for pb_type*/
  /*
  port_prefix = (char*)my_malloc(sizeof(char)*
                (strlen(formatted_subckt_prefix) + strlen(prim_pb_type->name) + 1
                 + strlen(my_itoa(index)) + 1 + 1));
  sprintf(port_prefix, "%s%s[%d]", formatted_subckt_prefix, prim_pb_type->name, index);
  */
  /* Simplify the port prefix, make SPICE netlist readable */
  port_prefix = (char*)my_malloc(sizeof(char)*
                (strlen(prim_pb_type->name) + 1
                 + strlen(my_itoa(index)) + 1 + 1));
  sprintf(port_prefix, "%s_%d_", prim_pb_type->name, index);
  /* Comment lines */
  fprintf(fp, "//----- Hardlogic Verilog module: %s%s -----\n", 
          formatted_subckt_prefix, port_prefix);
  /* Definition line */
  fprintf(fp, "module %s%s (", formatted_subckt_prefix, port_prefix);
  fprintf(fp, "\n");
  /* Only dump the global ports belonging to a spice_model */
  if (0 < rec_dump_verilog_spice_model_global_ports(fp, verilog_model, TRUE, TRUE)) {
    fprintf(fp, ",\n");
  }
  /* print ports*/
  dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, TRUE, FALSE); 
  /* Local vdd and gnd*/
  fprintf(fp, ");\n");
  /* Definition ends*/

  /* Back-annotate to logical block */
  if (NULL != mapped_logical_block) {
    mapped_logical_block->mapped_spice_model = verilog_model;
    mapped_logical_block->mapped_spice_model_index = verilog_model->cnt;
  }

  /* Call the hardlogic subckt*/
  fprintf(fp, "%s %s_%d_ (", verilog_model->name, verilog_model->prefix, verilog_model->cnt);
  fprintf(fp, "\n");
  /* Only dump the global ports belonging to a spice_model */
  if (0 < rec_dump_verilog_spice_model_global_ports(fp, verilog_model, FALSE, TRUE)) {
    fprintf(fp, ",\n");
  }
  /* print ports*/
  dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, FALSE, FALSE); 
  /* Local vdd and gnd, verilog_model name, 
   * Global vdd for hardlogic to split
   */
  fprintf(fp, ");\n");

  /* End */
  fprintf(fp, "endmodule\n");
  /* Comment lines */
  fprintf(fp, "//----- EDN Hardlogic Verilog module: %s%s -----\n", 
          formatted_subckt_prefix, port_prefix);

  verilog_model->cnt++;

  /*Free*/ 
  free(formatted_subckt_prefix);
  free(port_prefix);

  return;
}

void dump_verilog_pb_primitive_io(FILE* fp,
                            char* subckt_prefix,
                            t_logical_block* mapped_logical_block,
                            t_pb_graph_node* prim_pb_graph_node,
                            int index,
                            t_spice_model* verilog_model) {
  int num_pad_port = 0; /* INOUT port */
  t_spice_model_port** pad_ports = NULL;
  int num_input_port = 0;
  t_spice_model_port** input_ports = NULL;
  int num_output_port = 0;
  t_spice_model_port** output_ports = NULL;
  int num_clock_port = 0;
  t_spice_model_port** clock_ports = NULL;
  int num_sram_port = 0;
  t_spice_model_port** sram_ports = NULL;
  
  int i, j;
  int cur_sram = sram_verilog_model->cnt;
  int num_sram = 0;
  int* sram_bits = NULL;

  char* formatted_subckt_prefix = format_verilog_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  t_pb_type* prim_pb_type = NULL;
  char* port_prefix = NULL;

  /* For each SRAM, we could have multiple BLs/WLs */
  int num_bl_ports = 0;
  t_spice_model_port** bl_port = NULL;
  int num_wl_ports = 0;
  t_spice_model_port** wl_port = NULL;
  int num_bl_per_sram = 0;
  int num_wl_per_sram = 0;
  int* conf_bits_per_sram = NULL;
  int cur_bl, cur_wl, expected_num_sram;

  /* Ensure a valid file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Ensure a valid pb_graph_node */ 
  if (NULL == prim_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid prim_pb_graph_node!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Find ports*/
  pad_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_INOUT, &num_pad_port);
  input_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_INPUT, &num_input_port);
  output_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_OUTPUT, &num_output_port);
  clock_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_CLOCK, &num_clock_port);
  sram_ports = find_spice_model_ports(verilog_model, SPICE_MODEL_PORT_SRAM, &num_sram_port);

  /* Asserts */
  assert((SPICE_MODEL_IOPAD == verilog_model->type) /* Support IO PAD which matches the physical design */
       ||(SPICE_MODEL_INPAD == verilog_model->type)
       ||(SPICE_MODEL_OUTPAD == verilog_model->type));
  
  /* Initialize */
  prim_pb_type = prim_pb_graph_node->pb_type;

  /* Generate Subckt for pb_type*/
  /*
  port_prefix = (char*)my_malloc(sizeof(char)*
                (strlen(formatted_subckt_prefix) + strlen(prim_pb_type->name) + 1
                 + strlen(my_itoa(index)) + 1 + 1));
  sprintf(port_prefix, "%s%s[%d]", formatted_subckt_prefix, prim_pb_type->name, index);
  */
  /* Simplify the port prefix, make SPICE netlist readable */
  port_prefix = (char*)my_malloc(sizeof(char)*
                (strlen(prim_pb_type->name) + 1
                 + strlen(my_itoa(index)) + 1 + 1));
  sprintf(port_prefix, "%s_%d_", prim_pb_type->name, index);
  /* Comment lines */
  fprintf(fp, "//----- IO Verilog module: %s%s -----\n", 
          formatted_subckt_prefix, port_prefix);
  /* Definition line */
  fprintf(fp, "module %s%s (", formatted_subckt_prefix, port_prefix);
  fprintf(fp, "\n");
  /* Only dump the global ports belonging to a spice_model 
   */
  if (0 < rec_dump_verilog_spice_model_global_ports(fp, verilog_model, TRUE, TRUE)) {
    fprintf(fp, ",\n");
  }
  /* print ports*/
  switch (verilog_model->type) {
  case SPICE_MODEL_INPAD:
    assert((0 == strcmp(".input", prim_pb_type->blif_model))
         ||(0 == strcmp(".clock", prim_pb_type->blif_model)));
    /* Add input port to Input Pad */
    /* Print input port */
    fprintf(fp, "input [%d:%d] %s%s, ", 
            verilog_model->cnt, verilog_model->cnt, 
            gio_input_prefix, verilog_model->prefix);
    dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, TRUE, FALSE); 
    /* Local vdd and gnd*/
    fprintf(fp, ");\n");
    break;
  case SPICE_MODEL_OUTPAD:
    assert(0 == strcmp(".output", prim_pb_type->blif_model));
    /* Add output port to Output Pad */
    /* print ports --> input ports */
    dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, TRUE, TRUE); 
    /* Print output port */
    fprintf(fp, "output [%d:%d] %s%s ",
            verilog_model->cnt, verilog_model->cnt,
            gio_output_prefix, verilog_model->prefix);
    /* Add clock port to Input Pad */
    /* Local vdd and gnd*/
    fprintf(fp, ");\n");
    break;
  case SPICE_MODEL_IOPAD:
    /* TODO: assert this is physical mode */
    assert((1 == num_sram_port)&&(NULL != sram_ports)&&(1 == sram_ports[0]->size));
    num_sram = count_num_conf_bits_one_spice_model(verilog_model, -1);
    /* print ports --> input ports */
    dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, TRUE, TRUE); 
    /* Print output port */
    fprintf(fp, "inout [%d:%d] %s%s,\n",
            verilog_model->cnt, verilog_model->cnt,
            gio_inout_prefix, verilog_model->prefix);
    /* Print SRAM ports */
    switch (sram_verilog_orgz_type) {
    case SPICE_SRAM_MEMORY_BANK:
      /* Find the number of BLs/WLs of each SRAM */
      /* Detect the SRAM SPICE model linked to this SRAM port */
      assert(NULL != sram_ports[0]->spice_model);
      assert(SPICE_MODEL_SRAM == sram_ports[0]->spice_model->type);
      find_bl_wl_ports_spice_model(sram_ports[0]->spice_model, 
                                   &num_bl_ports, &bl_port, &num_wl_ports, &wl_port); 
      assert(1 == num_bl_ports);
      assert(1 == num_wl_ports);
      num_bl_per_sram = bl_port[0]->size; 
      num_wl_per_sram = wl_port[0]->size; 
      /* Malloc/Calloc */
      conf_bits_per_sram = (int*)my_calloc(num_bl_per_sram + num_wl_per_sram, sizeof(int));
      /* when memory-bank style is selected,
       * control lines should be bit lines and word lines 
       */
      fprintf(fp, "input wire [%d:%d] %s_bl,\n",
              sram_verilog_model->cnt + num_sram - 1, 
              sram_verilog_model->cnt, sram_verilog_model->prefix); 
      fprintf(fp, "input wire [%d:%d] %s_wl\n",
              sram_verilog_model->cnt + num_sram - 1, 
              sram_verilog_model->cnt, sram_verilog_model->prefix);
      break;
    case SPICE_SRAM_STANDALONE:
    case SPICE_SRAM_SCAN_CHAIN:
      fprintf(fp, "input wire [%d:%d] %s_out,\n",
              sram_verilog_model->cnt + num_sram - 1, sram_verilog_model->cnt, sram_verilog_model->prefix);
      fprintf(fp, "input wire [%d:%d] %s_outb\n",
              sram_verilog_model->cnt + num_sram - 1, sram_verilog_model->cnt, sram_verilog_model->prefix);
      break;
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid SRAM organization type!\n",
                 __FILE__, __LINE__);
      exit(1);
    }
    /* Local vdd and gnd*/
    fprintf(fp, ");\n");
    /* Local wires */
    fprintf(fp, "wire [%d:%d] %s_out;\n", 
               cur_sram, cur_sram, sram_verilog_model->prefix); /* Wires */
    fprintf(fp, "wire [%d:%d] %s_outb;\n", 
               cur_sram, cur_sram, sram_verilog_model->prefix); /* Wires */
    break;
  default:
    /* The rest is invalid */ 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid blif_model(%s) for prim_pb_type(%s)!\n", 
               __FILE__, __LINE__, prim_pb_type->blif_model, prim_pb_type->name);
    exit(1);
  } 
  /* Definition ends*/

  /* Call the I/O subckt*/
  fprintf(fp, "%s %s_%d_ (", verilog_model->name, verilog_model->prefix, verilog_model->cnt);
  fprintf(fp, "\n");
  /* Only dump the global ports belonging to a spice_model 
   * Disable recursive here !
   */
  if (0 < rec_dump_verilog_spice_model_global_ports(fp, verilog_model, FALSE, FALSE)) {
    fprintf(fp, ",\n");
  }
  switch (verilog_model->type) {
  case SPICE_MODEL_INPAD:
    assert((0 == strcmp(".input", prim_pb_type->blif_model))
         ||(0 == strcmp(".clock", prim_pb_type->blif_model)));
    /* Add input port to Input Pad */
    /* Print input port */
    fprintf(fp, "%s%s[%d], ", gio_input_prefix, 
                verilog_model->prefix, verilog_model->cnt);
    /* print ports --> output ports */
    dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, FALSE, FALSE); 
    break;
  case SPICE_MODEL_OUTPAD:
    assert (0 == strcmp(".output", prim_pb_type->blif_model));
    /* Add output port to Output Pad */
    /* print ports --> input ports */
    dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, FALSE, TRUE); 
    /* Print output port */
    fprintf(fp, "%s%s[%d] ", gio_output_prefix, 
                verilog_model->prefix, verilog_model->cnt);
    /* Add clock port to Input Pad */
    break;
  case SPICE_MODEL_IOPAD:
    /* assert */
    assert((1 == num_sram_port)&&(NULL != sram_ports)&&(1 == sram_ports[0]->size));
    num_sram = count_num_sram_bits_one_spice_model(verilog_model, -1);
    /* print ports --> input ports */
    dump_verilog_pb_type_ports(fp, port_prefix, 0, prim_pb_type, FALSE, TRUE); 
    /* Print inout port */
    fprintf(fp, "%s%s[%d], ", gio_inout_prefix, 
                verilog_model->prefix, verilog_model->cnt);
    /* Print SRAM ports */
    fprintf(fp, "%s_out[%d:%d] ",
            sram_verilog_model->prefix, 
            sram_verilog_model->cnt + num_sram - 1,
            sram_verilog_model->cnt);
            //sram_verilog_model->cnt);
    break;
  default:
    /* The rest is invalid */ 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid blif_model(%s) for prim_pb_type(%s)!\n", 
               __FILE__, __LINE__, prim_pb_type->blif_model, prim_pb_type->name);
    exit(1);
  } 
  
  /* Local vdd and gnd, verilog_model name, 
   * TODO: Global vdd for i/o pad to split?
   */
  fprintf(fp, ");\n");

  /* Call SRAM subckt */
  switch (verilog_model->type) {
  case SPICE_MODEL_INPAD:
  case SPICE_MODEL_OUTPAD:
    break;
  case SPICE_MODEL_IOPAD:
    /* assert */
    assert((1 == num_sram_port)&&(NULL != sram_ports)&&(1 == sram_ports[0]->size));
    /* what is the SRAM bit of a mode? */
    /* If logical block is not NULL, we need to decode the sram bit */
    if (NULL != mapped_logical_block) {
      assert(NULL != prim_pb_graph_node->pb_type->mode_bits);
      sram_bits = decode_mode_bits(prim_pb_graph_node->pb_type->mode_bits, &expected_num_sram);
      assert(expected_num_sram == num_sram);
    } else {
      /* Initialize */
      sram_bits = (int*)my_calloc(num_sram, sizeof(int));
      for (i = 0; i < num_sram; i++) { 
        sram_bits[i] = sram_ports[0]->default_val;
      }
    }
    /* SRAM_bit will be later reconfigured according to operating mode */
    /* Call SRAM subckts only 
     * when Configuration organization style is memory bank */
    num_sram = count_num_sram_bits_one_spice_model(verilog_model, -1);
    switch (sram_verilog_orgz_type) {
    case SPICE_SRAM_MEMORY_BANK:
      /* how to handle Bit lines and Word lines */
      cur_sram = sram_verilog_model->cnt;
      /* initial the BL and WL counter */
      cur_bl = cur_sram;
      cur_wl = cur_sram;
      for (i = 0; i < num_sram; i++) {
        fprintf(fp, "%s %s_%d_ (", sram_verilog_model->name, sram_verilog_model->prefix, cur_sram); /* SRAM subckts*/
        fprintf(fp, "\n");
        /* Only dump the global ports belonging to a spice_model */
        if (0 < rec_dump_verilog_spice_model_global_ports(fp, sram_verilog_model, FALSE, TRUE)) {
          fprintf(fp, ",\n");
        }
        fprintf(fp, "%s_out[%d], ", sram_verilog_model->prefix, cur_sram); /* Input*/
        fprintf(fp, "%s_out[%d], %s_outb[%d], ", 
                sram_verilog_model->prefix, cur_sram, 
                sram_verilog_model->prefix, cur_sram); /* Outputs */
        /* Connect to Bit lines and Word lines */
        fprintf(fp, "%s_bl[%d:%d], %s_wl[%d:%d] ", 
                sram_verilog_model->prefix, cur_bl + num_bl_per_sram - 1, cur_bl, 
                sram_verilog_model->prefix, cur_wl + num_wl_per_sram - 1, cur_wl); /* Outputs */
        fprintf(fp, ");\n");  //
        /* Decode the SRAM bits to BL/WL bits.
         * first half part is BL, the other half part is WL 
         */
        /* Store the configuraion bit to linked-list */
        for (j = 0; j < num_bl_per_sram; j++) {
          decode_verilog_one_level_4t1r_mux(sram_bits[i], num_bl_per_sram, conf_bits_per_sram);
          conf_bits_head = add_conf_bit_info_to_llist(conf_bits_head, cur_sram,
                                                      0, conf_bits_per_sram[j], 
                                                      conf_bits_per_sram[j + num_bl_per_sram], 
                                                      verilog_model);
        }
        /* update counter */
        cur_sram++;
        cur_bl = cur_bl + num_bl_per_sram; 
        cur_wl = cur_wl + num_wl_per_sram; 
      }
      /* NUM_SRAM is set to be consistent with number of BL/WLs
       * TODO: NUM_SRAM should be the as they are. 
       * Should use another variable i.e., num_bl
       */
      cur_sram = sram_verilog_model->cnt + num_sram * num_bl_per_sram;
      break;
    case SPICE_SRAM_STANDALONE:
    case SPICE_SRAM_SCAN_CHAIN:
      cur_sram = sram_verilog_model->cnt;
      for (i = 0; i < num_sram; i++) {
        /* Store the configuraion bit to linked-list */
        conf_bits_head = add_conf_bit_info_to_llist(conf_bits_head, cur_sram,
                                                    sram_bits[i], 0, 0, verilog_model);
        cur_sram++;
      }
    break;
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid SRAM organization type!\n",
                 __FILE__, __LINE__);
      exit(1);
    }

    break;
  default:
    /* The rest is invalid */ 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid blif_model(%s) for prim_pb_type(%s)!\n", 
               __FILE__, __LINE__, prim_pb_type->blif_model, prim_pb_type->name);
    exit(1);
  }

  /* End */
  fprintf(fp, "endmodule\n");
  /* Comment lines */
  fprintf(fp, "//----- END IO Verilog module: %s%s -----\n\n", 
          formatted_subckt_prefix, port_prefix);

  /* Back-annotate to logical block */
  if (NULL != mapped_logical_block) {
    mapped_logical_block->mapped_spice_model = verilog_model;
    mapped_logical_block->mapped_spice_model_index = verilog_model->cnt;
  }

  /* Update the verilog_model counter */
  verilog_model->cnt++;
  sram_verilog_model->cnt = cur_sram;

  /*Free*/ 
  free(formatted_subckt_prefix);
  free(port_prefix);
  my_free(input_ports);
  my_free(output_ports);
  my_free(pad_ports);
  my_free(clock_ports);
  my_free(sram_ports);
  my_free(sram_bits);

  return;
}


