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
#include "vpr_utils.h"

/* Include SPICE support headers*/
#include "linkedlist.h"
#include "spice_utils.h"
#include "spice_mux.h"

/* Include Synthesizable Verilog headers */
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_lut.h"
#include "verilog_primitives.h"
#include "verilog_pbtypes.h"

/***** Subroutines *****/

/* Find spice_model_name definition in pb_types
 * Try to match the name with defined spice_models
 */
void match_pb_types_verilog_model_rec(t_pb_type* cur_pb_type,
                                    int num_verilog_model,
                                    t_spice_model* verilog_models) {
  int imode, ipb, jinterc;
  
  if (NULL == cur_pb_type) {
    vpr_printf(TIO_MESSAGE_WARNING,"(File:%s,LINE[%d])cur_pb_type is null pointor!\n",__FILE__,__LINE__);
    return;
  }

  /* If there is a spice_model_name, this is a leaf node!*/
  if (NULL != cur_pb_type->spice_model_name) {
    /* What annoys me is VPR create a sub pb_type for each lut which suppose to be a leaf node
     * This may bring software convience but ruins SPICE modeling
     */
    /* Let's find a matched verilog model!*/
    printf("INFO: matching cur_pb_type=%s with spice_model_name=%s...\n",cur_pb_type->name, cur_pb_type->spice_model_name);
    assert(NULL == cur_pb_type->spice_model);
    cur_pb_type->spice_model = find_name_matched_spice_model(cur_pb_type->spice_model_name, num_verilog_model, verilog_models);
    if (NULL == cur_pb_type->spice_model) {
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d]) Fail to find a defined SPICE model called %s, in pb_type(%s)!\n",__FILE__, __LINE__, cur_pb_type->spice_model_name, cur_pb_type->name);
      exit(1);
    }
    return;
  }
  /* Traversal the hierarchy*/
  for (imode = 0; imode < cur_pb_type->num_modes; imode++) {
    /* Task 1: Find the interconnections and match the spice_model */
    for (jinterc = 0; jinterc < cur_pb_type->modes[imode].num_interconnect; jinterc++) {
      assert(NULL == cur_pb_type->modes[imode].interconnect[jinterc].spice_model);
      /* If the spice_model_name is not defined, we use the default*/
      if (NULL == cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name) {
        switch (cur_pb_type->modes[imode].interconnect[jinterc].type) {
        case DIRECT_INTERC:
          cur_pb_type->modes[imode].interconnect[jinterc].spice_model = 
            get_default_spice_model(SPICE_MODEL_WIRE,num_verilog_model,verilog_models);
          break;
        case COMPLETE_INTERC:
          /* Special for Completer Interconnection:
           * 1. The input number is 1, this infers a direct interconnection.
           * 2. The input number is larger than 1, this infers multplexers
           * according to interconnect[j].num_mux identify the number of input at this level
           */
          if (0 == cur_pb_type->modes[imode].interconnect[jinterc].num_mux) {
            cur_pb_type->modes[imode].interconnect[jinterc].spice_model = 
              get_default_spice_model(SPICE_MODEL_WIRE,num_verilog_model,verilog_models);
          } else {
            cur_pb_type->modes[imode].interconnect[jinterc].spice_model = 
              get_default_spice_model(SPICE_MODEL_MUX,num_verilog_model,verilog_models);
          } 
          break;
        case MUX_INTERC:
          cur_pb_type->modes[imode].interconnect[jinterc].spice_model = 
            get_default_spice_model(SPICE_MODEL_MUX,num_verilog_model,verilog_models);
          break;
        default:
          break; 
        }        
      } else {
        cur_pb_type->modes[imode].interconnect[jinterc].spice_model = 
          find_name_matched_spice_model(cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name, num_verilog_model, verilog_models);
        if (NULL == cur_pb_type->modes[imode].interconnect[jinterc].spice_model) {
          vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d]) Fail to find a defined SPICE model called %s, in pb_type(%s)!\n",__FILE__, __LINE__, cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name, cur_pb_type->name);
          exit(1);
        } 
        switch (cur_pb_type->modes[imode].interconnect[jinterc].type) {
        case DIRECT_INTERC:
          if (SPICE_MODEL_WIRE != cur_pb_type->modes[imode].interconnect[jinterc].spice_model->type) {
            vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d]) Invalid type of matched SPICE model called %s, in pb_type(%s)! Sould be wire!\n",__FILE__, __LINE__, cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name, cur_pb_type->name);
            exit(1);
          }
          break;
        case COMPLETE_INTERC:
          if (0 == cur_pb_type->modes[imode].interconnect[jinterc].num_mux) {
            if (SPICE_MODEL_WIRE != cur_pb_type->modes[imode].interconnect[jinterc].spice_model->type) {
              vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d]) Invalid type of matched SPICE model called %s, in pb_type(%s)! Sould be wire!\n",__FILE__, __LINE__, cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name, cur_pb_type->name);
              exit(1);
            }
          } else {
            if (SPICE_MODEL_MUX != cur_pb_type->modes[imode].interconnect[jinterc].spice_model->type) {
              vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d]) Invalid type of matched SPICE model called %s, in pb_type(%s)! Sould be MUX!\n",__FILE__, __LINE__, cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name, cur_pb_type->name);
              exit(1);
            }
          }
          break;
        case MUX_INTERC:
          if (SPICE_MODEL_MUX != cur_pb_type->modes[imode].interconnect[jinterc].spice_model->type) {
            vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d]) Invalid type of matched SPICE model called %s, in pb_type(%s)! Sould be MUX!\n",__FILE__, __LINE__, cur_pb_type->modes[imode].interconnect[jinterc].spice_model_name, cur_pb_type->name);
            exit(1);
          }
          break;
        default:
          break; 
        }        
      }
    }
    /* Task 2: Find the child pb_type, do matching recursively */
    //if (1 == cur_pb_type->modes[imode].define_spice_model) {
    for (ipb = 0; ipb < cur_pb_type->modes[imode].num_pb_type_children; ipb++) {
      match_pb_types_verilog_model_rec(&cur_pb_type->modes[imode].pb_type_children[ipb],
                                     num_verilog_model,
                                     verilog_models);
    }
    //}
  } 
  return;  
}

int verilog_find_path_id_between_pb_rr_nodes(t_rr_node* local_rr_graph,
                                     int src_node,
                                     int des_node) {
  int path_id = -1;
  int prev_edge = -1;
  int path_count = 0;
  int iedge;
  t_interconnect* cur_interc = NULL;

  /* Check */
  assert(NULL != local_rr_graph);
  assert((0 == src_node)||(0 < src_node));
  assert((0 == des_node)||(0 < des_node));

  prev_edge = local_rr_graph[des_node].prev_edge;
  check_pb_graph_edge(*(local_rr_graph[src_node].pb_graph_pin->output_edges[prev_edge]));
  assert(local_rr_graph[src_node].pb_graph_pin->output_edges[prev_edge]->output_pins[0] == local_rr_graph[des_node].pb_graph_pin);
 
  cur_interc = local_rr_graph[src_node].pb_graph_pin->output_edges[prev_edge]->interconnect;
  /* Search des_node input edges */ 
  for (iedge = 0; iedge < local_rr_graph[des_node].pb_graph_pin->num_input_edges; iedge++) {
    if (local_rr_graph[des_node].pb_graph_pin->input_edges[iedge]->input_pins[0] 
        == local_rr_graph[src_node].pb_graph_pin) {
      /* Strict check */
      assert(local_rr_graph[src_node].pb_graph_pin->output_edges[prev_edge]
              == local_rr_graph[des_node].pb_graph_pin->input_edges[iedge]);
      path_id = path_count;
      break;
    }
    if (cur_interc == local_rr_graph[des_node].pb_graph_pin->input_edges[iedge]->interconnect) {
      path_count++;
    }
  }

  return path_id; 
}

/* Find the interconnection type of pb_graph_pin edges*/
enum e_interconnect verilog_find_pb_graph_pin_in_edges_interc_type(t_pb_graph_pin pb_graph_pin) {
  enum e_interconnect interc_type;
  int def_interc_type = 0;
  int iedge;

  for (iedge = 0; iedge < pb_graph_pin.num_input_edges; iedge++) {
    /* Make sure all edges are legal: 1 input_pin, 1 output_pin*/
    check_pb_graph_edge(*(pb_graph_pin.input_edges[iedge]));
    /* Make sure all the edges interconnect type is the same*/
    if (0 == def_interc_type) {
      interc_type = pb_graph_pin.input_edges[iedge]->interconnect->type;
    } else if (interc_type != pb_graph_pin.input_edges[iedge]->interconnect->type) {
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d])Interconnection type are not same for port(%s),pin(%d).\n",
                 __FILE__, __LINE__, pb_graph_pin.port->name,pb_graph_pin.pin_number);
      exit(1);
    }
  }

  return interc_type;  
}


/* Find the interconnection type of pb_graph_pin edges*/
t_spice_model* find_pb_graph_pin_in_edges_interc_verilog_model(t_pb_graph_pin pb_graph_pin) {
  t_spice_model* interc_verilog_model;
  int def_interc_model = 0;
  int iedge;

  for (iedge = 0; iedge < pb_graph_pin.num_input_edges; iedge++) {
    /* Make sure all edges are legal: 1 input_pin, 1 output_pin*/
    check_pb_graph_edge(*(pb_graph_pin.input_edges[iedge]));
    /* Make sure all the edges interconnect type is the same*/
    if (0 == def_interc_model) {
      interc_verilog_model= pb_graph_pin.input_edges[iedge]->interconnect->spice_model;
    } else if (interc_verilog_model != pb_graph_pin.input_edges[iedge]->interconnect->spice_model) {
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,LINE[%d])Interconnection spice_model are not same for port(%s),pin(%d).\n",
                 __FILE__, __LINE__, pb_graph_pin.port->name,pb_graph_pin.pin_number);
      exit(1);
    }
  }

  return interc_verilog_model;  
}

/* Recursively do statistics for the
 * multiplexer verilog models inside pb_types
 */
void stats_mux_verilog_model_pb_type_rec(t_llist** muxes_head,
                                       t_pb_type* cur_pb_type) {
  
  int imode, ichild, jinterc;
  t_spice_model* interc_verilog_model = NULL;
 
  if (NULL == cur_pb_type) {
    vpr_printf(TIO_MESSAGE_WARNING,"(File:%s,LINE[%d])cur_pb_type is null pointor!\n",__FILE__,__LINE__);
    return;
  }

  /* If there is spice_model_name, this is a leaf node!*/
  if (NULL != cur_pb_type->spice_model_name) {
    /* What annoys me is VPR create a sub pb_type for each lut which suppose to be a leaf node
     * This may bring software convience but ruins SPICE modeling
     */
    assert(NULL != cur_pb_type->spice_model);
    return;
  }
  /* Traversal the hierarchy*/
  for (imode = 0; imode < cur_pb_type->num_modes; imode++) {
    /* Then we have to statisitic the interconnections*/
    for (jinterc = 0; jinterc < cur_pb_type->modes[imode].num_interconnect; jinterc++) {
      /* Check the num_mux and fan_in*/
      assert((0 == cur_pb_type->modes[imode].interconnect[jinterc].num_mux)
            ||(0 < cur_pb_type->modes[imode].interconnect[jinterc].num_mux));
      if (0 == cur_pb_type->modes[imode].interconnect[jinterc].num_mux) {
        continue;
      }
      interc_verilog_model = cur_pb_type->modes[imode].interconnect[jinterc].spice_model;
      assert(NULL != interc_verilog_model); 
      check_and_add_mux_to_linked_list(muxes_head,
                                       cur_pb_type->modes[imode].interconnect[jinterc].fan_in,
                                       interc_verilog_model);
    }
    for (ichild = 0; ichild < cur_pb_type->modes[imode].num_pb_type_children; ichild++) {
      stats_mux_verilog_model_pb_type_rec(muxes_head,
                                        &cur_pb_type->modes[imode].pb_type_children[ichild]);
    }
  }
  return;
}

/* Statistics the MUX SPICE MODEL with the help of pb_graph
 * Not the most efficient function to finish the job 
 * Abandon it. But remains a good framework that could be re-used in connecting
 * verilog components together
 */
void stats_mux_verilog_model_pb_node_rec(t_llist** muxes_head,
                                       t_pb_graph_node* cur_pb_node) {
  int imode, ipb, ichild, iport, ipin;
  t_pb_type* cur_pb_type = cur_pb_node->pb_type;
  t_spice_model* interc_verilog_model = NULL;
  enum e_interconnect pin_interc_type;
  
  if (NULL == cur_pb_node) {
    vpr_printf(TIO_MESSAGE_WARNING,"(File:%s,LINE[%d])cur_pb_node is null pointor!\n",__FILE__,__LINE__);
    return;
  }

  if (NULL == cur_pb_type) {
    vpr_printf(TIO_MESSAGE_WARNING,"(File:%s,LINE[%d])cur_pb_type is null pointor!\n",__FILE__,__LINE__);
    return;
  }

  /* If there is 0 mode, this is a leaf node!*/
  if (NULL != cur_pb_type->blif_model) {
    assert(0 == cur_pb_type->num_modes);
    assert(NULL == cur_pb_type->modes);
    /* Ensure there is blif_model, and spice_model*/
    assert(NULL != cur_pb_type->model);
    assert(NULL != cur_pb_type->spice_model_name);
    assert(NULL != cur_pb_type->spice_model);
    return;
  }
  /* Traversal the hierarchy*/
  for (imode = 0; imode < cur_pb_type->num_modes; imode++) {
    /* Then we have to statisitic the interconnections*/
    /* See the input ports*/
    for (iport = 0; iport < cur_pb_node->num_input_ports; iport++) {
      for (ipin = 0; ipin < cur_pb_node->num_input_pins[iport]; ipin++) {
        /* Ensure this is an input port */
        assert(IN_PORT == cur_pb_node->input_pins[iport][ipin].port->type);
        /* See the edges, if the interconnetion type infer a MUX, we go next step*/
        pin_interc_type = verilog_find_pb_graph_pin_in_edges_interc_type(cur_pb_node->input_pins[iport][ipin]);
        if ((COMPLETE_INTERC != pin_interc_type)&&(MUX_INTERC != pin_interc_type)) {
          continue;
        }
        /* We shoule check the size of inputs, in some case of complete, the input_edge is one...*/
        if ((COMPLETE_INTERC == pin_interc_type)&&(1 == cur_pb_node->input_pins[iport][ipin].num_input_edges)) {
          continue;
        }
        /* Note: i do care the input_edges only! They may infer multiplexers*/
        interc_verilog_model = find_pb_graph_pin_in_edges_interc_verilog_model(cur_pb_node->input_pins[iport][ipin]);
        check_and_add_mux_to_linked_list(muxes_head,
                                         cur_pb_node->input_pins[iport][ipin].num_input_edges,
                                         interc_verilog_model);
      }
    }
    /* See the output ports*/
    for (iport = 0; iport < cur_pb_node->num_output_ports; iport++) {
      for (ipin = 0; ipin < cur_pb_node->num_output_pins[iport]; ipin++) {
        /* Ensure this is an input port */
        assert(OUT_PORT == cur_pb_node->output_pins[iport][ipin].port->type);
        /* See the edges, if the interconnetion type infer a MUX, we go next step*/
        pin_interc_type = verilog_find_pb_graph_pin_in_edges_interc_type(cur_pb_node->output_pins[iport][ipin]);
        if ((COMPLETE_INTERC != pin_interc_type)&&(MUX_INTERC != pin_interc_type)) {
          continue;
        }
        /* We shoule check the size of inputs, in some case of complete, the input_edge is one...*/
        if ((COMPLETE_INTERC == pin_interc_type)&&(1 == cur_pb_node->output_pins[iport][ipin].num_input_edges)) {
          continue;
        }
        /* Note: i do care the input_edges only! They may infer multiplexers*/
        interc_verilog_model = find_pb_graph_pin_in_edges_interc_verilog_model(cur_pb_node->output_pins[iport][ipin]);
        check_and_add_mux_to_linked_list(muxes_head,
                                         cur_pb_node->output_pins[iport][ipin].num_input_edges,
                                         interc_verilog_model);
      }
    }
    /* See the clock ports*/
    for (iport = 0; iport < cur_pb_node->num_clock_ports; iport++) {
      for (ipin = 0; ipin < cur_pb_node->num_clock_pins[iport]; ipin++) {
        /* Ensure this is an input port */
        assert(IN_PORT == cur_pb_node->clock_pins[iport][ipin].port->type);
        /* See the edges, if the interconnetion type infer a MUX, we go next step*/
        pin_interc_type = verilog_find_pb_graph_pin_in_edges_interc_type(cur_pb_node->clock_pins[iport][ipin]);
        if ((COMPLETE_INTERC != pin_interc_type)&&(MUX_INTERC != pin_interc_type)) {
          continue;
        }
        /* We shoule check the size of inputs, in some case of complete, the input_edge is one...*/
        if ((COMPLETE_INTERC == pin_interc_type)&&(1 == cur_pb_node->clock_pins[iport][ipin].num_input_edges)) {
          continue;
        }
        /* Note: i do care the input_edges only! They may infer multiplexers*/
        interc_verilog_model = find_pb_graph_pin_in_edges_interc_verilog_model(cur_pb_node->clock_pins[iport][ipin]);
        check_and_add_mux_to_linked_list(muxes_head,
                                         cur_pb_node->clock_pins[iport][ipin].num_input_edges,
                                         interc_verilog_model);
      }
    }
    for (ichild = 0; ichild < cur_pb_type->modes[imode].num_pb_type_children; ichild++) {
      /* num_pb is the number of such pb_type in a mode*/
      for (ipb = 0; ipb < cur_pb_type->modes[imode].pb_type_children[ichild].num_pb; ipb++) {
        /* child_pb_grpah_nodes: [0..num_modes-1][0..num_pb_type_in_mode-1][0..num_pb_type-1]*/
        stats_mux_verilog_model_pb_node_rec(muxes_head,
                                          &cur_pb_node->child_pb_graph_nodes[imode][ichild][ipb]);
      }
    }
  } 
  return;  
}

/* Print ports of pb_types,
 * SRAM ports are not printed here!!! 
 * Important feature: manage the comma between ports
 * Make sure there is no redundant comma and there is no comma after the last element if specified
 */
void dump_verilog_pb_type_ports(FILE* fp,
                          char* port_prefix,
                          int use_global_clock,
                          t_pb_type* cur_pb_type,
                          boolean dump_port_type,
                          boolean dump_last_comma) {
  int iport, ipin;
  int num_pb_type_input_port = 0;
  t_port** pb_type_input_ports = NULL;

  int num_pb_type_output_port = 0;
  t_port** pb_type_output_ports = NULL;

  int num_pb_type_inout_port = 0;
  t_port** pb_type_inout_ports = NULL;

  int num_pb_type_clk_port = 0;
  t_port** pb_type_clk_ports = NULL;

  char* formatted_port_prefix = chomp_spice_node_prefix(port_prefix);
  /* A counter to stats the number of dumped ports and pins */
  int num_dumped_port = 0;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
  }

  /* INOUT ports */
  num_dumped_port = 0;
  /* Find pb_type inout ports */
  pb_type_inout_ports = find_pb_type_ports_match_spice_model_port_type(cur_pb_type, SPICE_MODEL_PORT_INOUT, &num_pb_type_inout_port); 

  /* Print all the inout ports  */
  for (iport = 0; iport < num_pb_type_inout_port; iport++) {
    for (ipin = 0; ipin < pb_type_inout_ports[iport]->num_pins; ipin++) {
      if (TRUE == dump_port_type) {
        fprintf(fp, "inout");
      }
      fprintf(fp, "%s__%s_%d_, ", formatted_port_prefix, pb_type_inout_ports[iport]->name, ipin);
      /* Update the counter */
      num_dumped_port++;
      /* We do not put a comma at the last element to be dumped */
      if ((iport == num_pb_type_inout_port - 1)&&(ipin == pb_type_inout_ports[iport]->num_pins - 1)) {
        break;
      }
      /* We can put a comma since this is not the last element to be dumped */
      if (TRUE == dump_port_type) {
        fprintf(fp, ",\n");
      } else {
        fprintf(fp, ",");
      }
    }
  }

  /* Inputs */
  /* Find pb_type input ports */
  pb_type_input_ports = find_pb_type_ports_match_spice_model_port_type(cur_pb_type, SPICE_MODEL_PORT_INPUT, &num_pb_type_input_port); 
  /* Need a comma if this is not the last elemet */
  if ((0 < num_dumped_port)&&(0 < num_pb_type_input_port)) {
    if (TRUE == dump_port_type) {
      fprintf(fp, ",\n");
    } else {
      fprintf(fp, ", ");
    }
  } else if (0 < num_pb_type_input_port) { /* We keep on dumping, reset the counter */
    num_dumped_port = 0;
  }
  /* Print all the input ports  */
  for (iport = 0; iport < num_pb_type_input_port; iport++) {
    for (ipin = 0; ipin < pb_type_input_ports[iport]->num_pins; ipin++) {
      if (TRUE == dump_port_type) {
        fprintf(fp, "input");
      }
      fprintf(fp, " %s__%s_%d_", formatted_port_prefix, pb_type_input_ports[iport]->name, ipin);
      /* Update the counter */
      num_dumped_port++;
      /* We do not put a comma at the last element to be dumped */
      if ((iport == num_pb_type_input_port - 1)&&(ipin == pb_type_input_ports[iport]->num_pins - 1)) {
        break;
      }
      /* We can put a comma since this is not the last element to be dumped */
      if (TRUE == dump_port_type) {
        fprintf(fp, ",\n");
      } else {
        fprintf(fp, ", ");
      }
    }
  }
  /* Outputs */
  /* Find pb_type output ports */
  pb_type_output_ports = find_pb_type_ports_match_spice_model_port_type(cur_pb_type, SPICE_MODEL_PORT_OUTPUT, &num_pb_type_output_port); 

  /* Need a comma if this is not the last elemet */
  if ((0 < num_dumped_port)&&(0 < num_pb_type_output_port)) {
    if (TRUE == dump_port_type) {
      fprintf(fp, ",\n");
    } else {
      fprintf(fp, ", ");
    }
  } else if (0 < num_pb_type_output_port) { /* We keep on dumping, reset the counter */
    num_dumped_port = 0;
  }

  /* Print all the output ports  */
  for (iport = 0; iport < num_pb_type_output_port; iport++) {
    for (ipin = 0; ipin < pb_type_output_ports[iport]->num_pins; ipin++) {
      if (TRUE == dump_port_type) {
        fprintf(fp, "output");
      }
      fprintf(fp, " %s__%s_%d_", formatted_port_prefix, pb_type_output_ports[iport]->name, ipin);
      /* Update the counter */
      num_dumped_port++;
      /* We do not put a comma at the last element to be dumped */
      if ((iport == num_pb_type_output_port - 1)&&(ipin == pb_type_output_ports[iport]->num_pins - 1)) {
        break;
      }
      /* We can put a comma since this is not the last element to be dumped */
      if (TRUE == dump_port_type) {
        fprintf(fp, ",\n");
      } else {
        fprintf(fp, ",");
      }
    }
  }
  
  /* Clocks */
  /* Find pb_type clock ports */
  pb_type_clk_ports = find_pb_type_ports_match_spice_model_port_type(cur_pb_type, SPICE_MODEL_PORT_CLOCK, &num_pb_type_clk_port); 
  /* Need a comma if this is not the last elemet */
  if ((0 < num_dumped_port)&&(0 < num_pb_type_clk_port)) {
    if (TRUE == dump_port_type) {
      fprintf(fp, ",\n");
    } else {
      fprintf(fp, ", ");
    }
  } else if (0 < num_pb_type_clk_port) { /* We keep on dumping, reset the counter */
    num_dumped_port = 0;
  }
  /* Print all the clk ports  */
  for (iport = 0; iport < num_pb_type_clk_port; iport++) {
    for (ipin = 0; ipin < pb_type_clk_ports[iport]->num_pins; ipin++) {
      if (TRUE == dump_port_type) {
        fprintf(fp, "input");
      }
      fprintf(fp, " %s__%s_%d_", formatted_port_prefix, pb_type_clk_ports[iport]->name, ipin);
      /* Update the counter */
      num_dumped_port++;
      /* We do not put a comma at the last element to be dumped */
      if ((iport == num_pb_type_clk_port - 1)&&(ipin == pb_type_clk_ports[iport]->num_pins - 1)) {
        break;
      }
      /* We can put a comma since this is not the last element to be dumped */
      if (TRUE == dump_port_type) {
        fprintf(fp, ",\n");
      } else {
        fprintf(fp, ",");
      }
    }
  }

  /* Dump the last comma, when the option is enabled and there is something dumped */
  if ((0 < num_dumped_port)&&(TRUE == dump_last_comma)) {
    if (TRUE == dump_port_type) {
      fprintf(fp, ",\n");
    } else {
      fprintf(fp, ",");
    }
  }

  /* Free */
  free(formatted_port_prefix);
  my_free(pb_type_input_ports);
  my_free(pb_type_output_ports);
  my_free(pb_type_inout_ports);
  my_free(pb_type_clk_ports);

  return;
}

/* This is a truncated version of generate_verilog_src_des_pb_graph_pin_prefix
 * Only used to generate prefix for those dangling pin in PB Types
 */
void dump_verilog_dangling_des_pb_graph_pin_interc(FILE* fp,
                                                   t_pb_graph_pin* des_pb_graph_pin,
                                                   t_mode* cur_mode,
                                                   enum e_pin2pin_interc_type pin2pin_interc_type,
                                                   char* parent_pin_prefix) {
  t_pb_graph_node* des_pb_graph_node = NULL;
  t_pb_type* des_pb_type = NULL;
  int des_pb_type_index = -1;
  int fan_in = 0;
  t_interconnect* cur_interc = NULL;
  char* des_pin_prefix = NULL;
  
  /* char* formatted_parent_pin_prefix = format_spice_node_prefix(parent_pin_prefix);*/  /* Complete a "_" at the end if needed*/
  //char* chomped_parent_pin_prefix = chomp_verilog_node_prefix(parent_pin_prefix); /* Remove a "_" at the end if needed*/

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check the pb_graph_nodes*/ 
  if (NULL == des_pb_graph_pin) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid pointer: des_pb_graph_pin.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  verilog_find_interc_fan_in_des_pb_graph_pin(des_pb_graph_pin, cur_mode, &cur_interc, &fan_in);
  if ((NULL != cur_interc)&&(0 != fan_in)) {
    return;
    /*
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Cur_interc not NULL & fan_in not zero!\n", 
               __FILE__, __LINE__); 
    exit(1);
    */
  }

  /* Initialize */
  des_pb_graph_node = des_pb_graph_pin->parent_node;
  des_pb_type = des_pb_graph_node->pb_type; 
  des_pb_type_index = des_pb_graph_node->placement_index;

  /* generate the pin prefix for src_pb_graph_node and des_pb_graph_node */
  switch (pin2pin_interc_type) {
  case INPUT2INPUT_INTERC:
    /* src_pb_graph_node.input_pins -----------------> des_pb_graph_node.input_pins 
     * des_pb_graph_node is a child of src_pb_graph_node 
     * parent_pin_prefix is the prefix from parent pb_graph_node, in this case, src_pb_graph_node 
     * src_pin_prefix: we need to handle the feedbacks, they comes from the same-level pb_graph_node 
     * src_pin_prefix = <formatted_parent_pin_prefix>
     * OR
     * src_pin_prefix = <formatted_parent_pin_prefix>_<src_pb_type>[<src_pb_type_index>]
     * des_pin_prefix = <formatted_parent_pin_prefix>mode[<mode_name>]_<des_pb_type>[<des_pb_type_index>]_
     */
    /*
    des_pin_prefix = (char*)my_malloc(sizeof(char)*
                        (strlen(formatted_parent_pin_prefix) + 5 + strlen(cur_mode->name)
                         + 2 + strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
    sprintf(des_pin_prefix, "%smode[%s]_%s[%d]",
            formatted_parent_pin_prefix, cur_mode->name, des_pb_type->name, des_pb_type_index);
    */
    /*Simplify the prefix, make the SPICE netlist readable*/
    des_pin_prefix = (char*)my_malloc(sizeof(char)*
                        (strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
    sprintf(des_pin_prefix, "%s_%d_",
             des_pb_type->name, des_pb_type_index);
    break;
  case OUTPUT2OUTPUT_INTERC:
    /* src_pb_graph_node.output_pins -----------------> des_pb_graph_node.output_pins 
     * src_pb_graph_node is a child of des_pb_graph_node 
     * parent_pin_prefix is the prefix from parent pb_graph_node, in this case, des_pb_graph_node 
     * src_pin_prefix = <formatted_parent_pin_prefix>mode[<mode_name>]_<src_pb_type>[<src_pb_type_index>]_
     * des_pin_prefix = <formatted_parent_pin_prefix>
     */
    /*
    des_pin_prefix = (char*)my_malloc(sizeof(char)*
                         (strlen(formatted_parent_pin_prefix) + 5 + strlen(cur_mode->name)
                         + 2 + strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
    sprintf(des_pin_prefix, "%smode[%s]_%s[%d]",
            formatted_parent_pin_prefix, cur_mode->name, des_pb_type->name, des_pb_type_index);
    */
    if (des_pb_type == cur_mode->parent_pb_type) { /* Interconnection from parent pb_type*/
      des_pin_prefix = (char*)my_malloc(sizeof(char)*
                           (5 + strlen(cur_mode->name) + 2 ));
      sprintf(des_pin_prefix, "mode_%s_", cur_mode->name);
    } else {
      des_pin_prefix = (char*)my_malloc(sizeof(char)*
                          (strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
      sprintf(des_pin_prefix, "%s_%d_",
               des_pb_type->name, des_pb_type_index);
    }
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s [LINE%d])Invalid pin to pin interconnection type!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  my_free(des_pin_prefix);

  return;
}

void generate_verilog_src_des_pb_graph_pin_prefix(t_pb_graph_node* src_pb_graph_node,
                                                t_pb_graph_node* des_pb_graph_node,
                                                enum e_pin2pin_interc_type pin2pin_interc_type,
                                                t_interconnect* pin2pin_interc,
                                                char* parent_pin_prefix,
                                                char** src_pin_prefix,
                                                char** des_pin_prefix) {
  t_pb_type* src_pb_type = NULL;
  int src_pb_type_index = -1;

  t_pb_type* des_pb_type = NULL;
  int des_pb_type_index = -1;
  
  //char* formatted_parent_pin_prefix = format_spice_node_prefix(parent_pin_prefix); /* Complete a "_" at the end if needed*/
  //char* chomped_parent_pin_prefix = chomp_verilog_node_prefix(parent_pin_prefix); /* Remove a "_" at the end if needed*/
  
  t_mode* pin2pin_interc_parent_mode = NULL; 

  /* Check the pb_graph_nodes*/ 
  if (NULL == src_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid pointer: src_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  if (NULL == des_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid pointer: des_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  if (NULL == pin2pin_interc) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid pointer: pin2pin_interc.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Initialize */
  src_pb_type = src_pb_graph_node->pb_type; 
  src_pb_type_index = src_pb_graph_node->placement_index;
  des_pb_type = des_pb_graph_node->pb_type; 
  des_pb_type_index = des_pb_graph_node->placement_index;
  
  pin2pin_interc_parent_mode = pin2pin_interc->parent_mode;

  assert(NULL == (*src_pin_prefix));
  assert(NULL == (*des_pin_prefix));
  /* generate the pin prefix for src_pb_graph_node and des_pb_graph_node */
  switch (pin2pin_interc_type) {
  case INPUT2INPUT_INTERC:
    /* src_pb_graph_node.input_pins -----------------> des_pb_graph_node.input_pins 
     * des_pb_graph_node is a child of src_pb_graph_node 
     * parent_pin_prefix is the prefix from parent pb_graph_node, in this case, src_pb_graph_node 
     * src_pin_prefix: we need to handle the feedbacks, they comes from the same-level pb_graph_node 
     * src_pin_prefix = <formatted_parent_pin_prefix>
     * OR
     * src_pin_prefix = <formatted_parent_pin_prefix>_<src_pb_type>[<src_pb_type_index>]
     * des_pin_prefix = <formatted_parent_pin_prefix>mode[<mode_name>]_<des_pb_type>[<des_pb_type_index>]_
     */
    if (src_pb_type == des_pb_type->parent_mode->parent_pb_type) { /* Interconnection from parent pb_type*/
      /*
      (*src_pin_prefix) = my_strdup(chomped_parent_pin_prefix);
      */
      /*Simplify the prefix, make the SPICE netlist readable*/
      (*src_pin_prefix) = (char*)my_malloc(sizeof(char)*
                           (5 + strlen(des_pb_type->parent_mode->name) + 2));
      sprintf((*src_pin_prefix), "mode_%s_", des_pb_type->parent_mode->name);
    } else {
      /*
      (*src_pin_prefix) = (char*)my_malloc(sizeof(char)*
                           (strlen(formatted_parent_pin_prefix) + 5 + strlen(pin2pin_interc_parent_mode->name)
                           + 2 + strlen(src_pb_type->name) + 1 + strlen(my_itoa(src_pb_type_index)) + 1 + 1));
      sprintf((*src_pin_prefix), "%smode[%s]_%s[%d]",
              formatted_parent_pin_prefix, pin2pin_interc_parent_mode->name, src_pb_type->name, src_pb_type_index);
       */
      /*Simplify the prefix, make the SPICE netlist readable*/
      (*src_pin_prefix) = (char*)my_malloc(sizeof(char)*
                           (strlen(src_pb_type->name) + 1 + strlen(my_itoa(src_pb_type_index)) + 1 + 1));
      sprintf((*src_pin_prefix), "%s_%d_",
              src_pb_type->name, src_pb_type_index);
    }
    /*
    (*des_pin_prefix) = (char*)my_malloc(sizeof(char)*
                        (strlen(formatted_parent_pin_prefix) + 5 + strlen(pin2pin_interc_parent_mode->name)
                         + 2 + strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
    sprintf((*des_pin_prefix), "%smode[%s]_%s[%d]",
            formatted_parent_pin_prefix, pin2pin_interc_parent_mode->name, des_pb_type->name, des_pb_type_index);
    */
    /*Simplify the prefix, make the SPICE netlist readable*/
    (*des_pin_prefix) = (char*)my_malloc(sizeof(char)*
                        (strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
    sprintf((*des_pin_prefix), "%s_%d_",
             des_pb_type->name, des_pb_type_index);
    break;
  case OUTPUT2OUTPUT_INTERC:
    /* src_pb_graph_node.output_pins -----------------> des_pb_graph_node.output_pins 
     * src_pb_graph_node is a child of des_pb_graph_node 
     * parent_pin_prefix is the prefix from parent pb_graph_node, in this case, des_pb_graph_node 
     * src_pin_prefix = <formatted_parent_pin_prefix>mode[<mode_name>]_<src_pb_type>[<src_pb_type_index>]_
     * des_pin_prefix = <formatted_parent_pin_prefix>
     */
    /*
    (*src_pin_prefix) = (char*)my_malloc(sizeof(char)*
                        (strlen(formatted_parent_pin_prefix) + 5 + strlen(pin2pin_interc_parent_mode->name)
                         + 2 + strlen(src_pb_type->name) + 1 + strlen(my_itoa(src_pb_type_index)) + 1 + 1));
    sprintf((*src_pin_prefix), "%smode[%s]_%s[%d]",
            formatted_parent_pin_prefix, pin2pin_interc_parent_mode->name, src_pb_type->name, src_pb_type_index);
    */
    /*Simplify the prefix, make the SPICE netlist readable*/
    (*src_pin_prefix) = (char*)my_malloc(sizeof(char)*
                        (strlen(src_pb_type->name) + 1 + strlen(my_itoa(src_pb_type_index)) + 1 + 1));
    sprintf((*src_pin_prefix), "%s_%d_",
            src_pb_type->name, src_pb_type_index);
    if (des_pb_type == src_pb_type->parent_mode->parent_pb_type) { /* Interconnection from parent pb_type*/
      /*
      (*des_pin_prefix) = my_strdup(chomped_parent_pin_prefix);
      */
      /*Simplify the prefix, make the SPICE netlist readable*/
      (*des_pin_prefix) = (char*)my_malloc(sizeof(char)*
                           (5 + strlen(src_pb_type->parent_mode->name) + 2));
      sprintf((*des_pin_prefix), "mode_%s_", src_pb_type->parent_mode->name);
    } else {
      /*
      (*des_pin_prefix) = (char*)my_malloc(sizeof(char)*
                           (strlen(formatted_parent_pin_prefix) + 5 + strlen(pin2pin_interc_parent_mode->name)
                           + 2 + strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
      sprintf((*des_pin_prefix), "%smode[%s]_%s[%d]",
              formatted_parent_pin_prefix, pin2pin_interc_parent_mode->name, des_pb_type->name, des_pb_type_index);
      */
      /*Simplify the prefix, make the SPICE netlist readable*/
      (*des_pin_prefix) = (char*)my_malloc(sizeof(char)*
                           (strlen(des_pb_type->name) + 1 + strlen(my_itoa(des_pb_type_index)) + 1 + 1));
      sprintf((*des_pin_prefix), "%s_%d_",
              des_pb_type->name, des_pb_type_index);
    }
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s [LINE%d])Invalid pin to pin interconnection type!\n",
               __FILE__, __LINE__);
    exit(1);
  }
  return;
}

void verilog_find_interc_fan_in_des_pb_graph_pin(t_pb_graph_pin* des_pb_graph_pin,
                                         t_mode* cur_mode,
                                         t_interconnect** cur_interc,
                                         int* fan_in) { 
  int iedge;
  
  (*cur_interc) = NULL;
  (*fan_in) = 0;  

  /* Search the input edges only, stats on the size of MUX we may need (fan-in) */
  for (iedge = 0; iedge < des_pb_graph_pin->num_input_edges; iedge++) {
    /* 1. First, we should make sure this interconnect is in the selected mode!!!*/
    if (cur_mode == des_pb_graph_pin->input_edges[iedge]->interconnect->parent_mode) {
      /* Check this edge*/
      check_pb_graph_edge(*(des_pb_graph_pin->input_edges[iedge]));
      /* Record the interconnection*/
      if (NULL == (*cur_interc)) {
        (*cur_interc) = des_pb_graph_pin->input_edges[iedge]->interconnect;
      } else { /* Make sure the interconnections for this pin is the same!*/
        assert((*cur_interc) == des_pb_graph_pin->input_edges[iedge]->interconnect);
      }
      /* Search the input_pins of input_edges only*/
      (*fan_in) += des_pb_graph_pin->input_edges[iedge]->num_input_pins;
    }
  }

  return;
}

/* We check output_pins of cur_pb_graph_node and its the input_edges
 * Built the interconnections between outputs of cur_pb_graph_node and outputs of child_pb_graph_node
 *   src_pb_graph_node.[in|out]_pins -----------------> des_pb_graph_node.[in|out]pins
 *                                        /|\
 *                                         |
 *                         input_pins,   edges,       output_pins
 */ 
void dump_verilog_pb_graph_pin_interc(FILE* fp,
                                      char* parent_pin_prefix,
                                      enum e_pin2pin_interc_type pin2pin_interc_type,
                                      t_pb_graph_pin* des_pb_graph_pin,
                                      t_mode* cur_mode,
                                      int select_edge) {
  int iedge, ilevel, ipin;
  int fan_in = 0;
  t_interconnect* cur_interc = NULL; 
  enum e_interconnect verilog_interc_type = DIRECT_INTERC;

  t_pb_graph_pin* src_pb_graph_pin = NULL;
  t_pb_graph_node* src_pb_graph_node = NULL;
  t_pb_type* src_pb_type = NULL;
  int src_pb_type_index = -1; 

  t_pb_graph_node* des_pb_graph_node = NULL;
  t_pb_type* des_pb_type = NULL; 
  int des_pb_type_index = -1; 

  char* formatted_parent_pin_prefix = chomp_spice_node_prefix(parent_pin_prefix); /* Complete a "_" at the end if needed*/
  char* src_pin_prefix = NULL;
  char* des_pin_prefix = NULL;

  int num_sram_bits = 0;
  int* sram_bits = NULL;
  int num_sram = 0;
  int mux_level = 0;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* 1. identify pin interconnection type, 
   * 2. Identify the number of fan-in (Consider interconnection edges of only selected mode)
   * 3. Select and print the SPICE netlist
   */
  fan_in = 0;
  cur_interc = NULL;
  verilog_find_interc_fan_in_des_pb_graph_pin(des_pb_graph_pin, cur_mode, &cur_interc, &fan_in);
  if ((NULL == cur_interc)||(0 == fan_in)) { 
    /* No interconnection matched */
    /* Connect this pin to GND for better convergence */
    /* TODO: find the correct pin name!!!*/
    /*
    dump_verilog_dangling_des_pb_graph_pin_interc(fp, des_pb_graph_pin, cur_mode, pin2pin_interc_type,
                                                  formatted_parent_pin_prefix);
    */
    return;
  }
  /* Initialize the interconnection type that will be implemented in SPICE netlist*/
  switch (cur_interc->type) {
    case DIRECT_INTERC:
      assert(1 == fan_in);
      verilog_interc_type = DIRECT_INTERC;
      break;
    case COMPLETE_INTERC:
      if (1 == fan_in) {
        verilog_interc_type = DIRECT_INTERC;
      } else {
        assert((2 == fan_in)||(2 < fan_in));
        verilog_interc_type = MUX_INTERC;
      }
      break;
    case MUX_INTERC:
      assert((2 == fan_in)||(2 < fan_in));
      verilog_interc_type = MUX_INTERC;
      break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid interconnection type for %s (Arch[LINE%d])!\n",
               __FILE__, __LINE__, cur_interc->name, cur_interc->line_num);
    exit(1);
  }
  /* This time, (2nd round), we print the subckt, according to interc type*/ 
  switch (verilog_interc_type) {
  case DIRECT_INTERC:
    /* Check : 
     * 1. Direct interc has only one fan-in!
     */
    assert(1 == fan_in);
    //assert(1 == des_pb_graph_pin->num_input_edges);
    /* For more than one mode defined, the direct interc has more than one input_edge ,
     * We need to find which edge is connected the pin we want
     */
    for (iedge = 0; iedge < des_pb_graph_pin->num_input_edges; iedge++) {
      if (cur_interc == des_pb_graph_pin->input_edges[iedge]->interconnect) {
        break;
      }
    }
    assert(iedge < des_pb_graph_pin->num_input_edges);
    /* 2. spice_model is a wire */ 
    assert(NULL != cur_interc->spice_model);
    assert(SPICE_MODEL_WIRE == cur_interc->spice_model->type);
    assert(NULL != cur_interc->spice_model->wire_param);
    /* Initialize*/
    /* Source pin, node, pb_type*/
    src_pb_graph_pin = des_pb_graph_pin->input_edges[iedge]->input_pins[0];
    src_pb_graph_node = src_pb_graph_pin->parent_node;
    src_pb_type = src_pb_graph_node->pb_type;
    src_pb_type_index = src_pb_graph_node->placement_index;
    /* Des pin, node, pb_type */
    des_pb_graph_node  = des_pb_graph_pin->parent_node;
    des_pb_type = des_pb_graph_node->pb_type;
    des_pb_type_index = des_pb_graph_node->placement_index;
    /* Generate the pin_prefix for src_pb_graph_node and des_pb_graph_node*/
    generate_verilog_src_des_pb_graph_pin_prefix(src_pb_graph_node, des_pb_graph_node, pin2pin_interc_type, 
                                               cur_interc, formatted_parent_pin_prefix, &src_pin_prefix, &des_pin_prefix);
    /* Call the subckt that has already been defined before */
    fprintf(fp, "%s ", cur_interc->spice_model->name);
    fprintf(fp, "%s_%d_ (", cur_interc->spice_model->prefix, cur_interc->spice_model->cnt); 
    cur_interc->spice_model->cnt++; /* Stats the number of spice_model used*/
    /* Print the pin names! Input and output
     * Input: port_prefix_<child_pb_graph_node>-><port_name>[pin_index]
     * Output: port_prefix_<port_name>[pin_index]
     */
    /* Input */
    /* Make sure correctness*/
    assert(src_pb_type == des_pb_graph_pin->input_edges[iedge]->input_pins[0]->port->parent_pb_type);
    /* Print */
    fprintf(fp, "%s__%s_%d_, ", 
            src_pin_prefix, src_pb_graph_pin->port->name, src_pb_graph_pin->pin_number);
    /* Output */
    fprintf(fp, "%s__%s_%d_ ", 
            des_pin_prefix, des_pb_graph_pin->port->name, des_pb_graph_pin->pin_number); 
    /* Middle output for wires in logic blocks: TODO: Abolish to save simulation time */
    /* fprintf(fp, "gidle_mid_out "); */
    /* Local vdd and gnd, TODO: we should have an independent VDD for all local interconnections*/
    fprintf(fp, ");\n");
    /* Free */
    my_free(src_pin_prefix);
    my_free(des_pin_prefix);
    src_pin_prefix = NULL;
    des_pin_prefix = NULL;
    break;
  case COMPLETE_INTERC:
  case MUX_INTERC:
    /* Check : 
     * MUX should have at least 2 fan_in
     */
    assert((2 == fan_in)||(2 < fan_in));
    /* 2. spice_model is a wire */ 
    assert(NULL != cur_interc->spice_model);
    assert(SPICE_MODEL_MUX == cur_interc->spice_model->type);
    /* Create a local bus */
    fprintf(fp, "wire [0:%d] in_bus_%s_size%d_%d_ ;\n", fan_in - 1, 
            cur_interc->spice_model->name, fan_in, cur_interc->spice_model->cnt);
    ipin = 0;
    for (iedge = 0; iedge < des_pb_graph_pin->num_input_edges; iedge++) {
      if (cur_mode != des_pb_graph_pin->input_edges[iedge]->interconnect->parent_mode) {
        continue;
      }
      check_pb_graph_edge(*(des_pb_graph_pin->input_edges[iedge]));
      /* Initialize*/
      /* Source pin, node, pb_type*/
      src_pb_graph_pin = des_pb_graph_pin->input_edges[iedge]->input_pins[0];
      src_pb_graph_node = src_pb_graph_pin->parent_node;
      src_pb_type = src_pb_graph_node->pb_type;
      src_pb_type_index = src_pb_graph_node->placement_index;
      /* Des pin, node, pb_type */
      des_pb_graph_node  = des_pb_graph_pin->parent_node;
      des_pb_type = des_pb_graph_node->pb_type;
      des_pb_type_index = des_pb_graph_node->placement_index;
      /* Generate the pin_prefix for src_pb_graph_node and des_pb_graph_node*/
      generate_verilog_src_des_pb_graph_pin_prefix(src_pb_graph_node, des_pb_graph_node, pin2pin_interc_type, 
                                                 cur_interc, formatted_parent_pin_prefix, &src_pin_prefix, &des_pin_prefix);
      /* We need to find out if the des_pb_graph_pin is in the mode we want !*/
      /* Print */
      fprintf(fp, "assign in_bus_%s_size%d_%d_[%d] = ",
            cur_interc->spice_model->name, fan_in, cur_interc->spice_model->cnt, ipin);
      fprintf(fp, "%s_%s_%d_ ; \n", 
              src_pin_prefix, src_pb_graph_pin->port->name, src_pb_graph_pin->pin_number);
      ipin++;
      /* Free */
      my_free(src_pin_prefix);
      my_free(des_pin_prefix);
      src_pin_prefix = NULL;
      des_pin_prefix = NULL;
    }
    assert(ipin == fan_in);
    /* Call the subckt that has already been defined before */
    fprintf(fp, "%s_size%d ", cur_interc->spice_model->name, fan_in);
    fprintf(fp, "%s_size%d_%d_ (", cur_interc->spice_model->prefix, fan_in, cur_interc->spice_model->cnt);
    fprintf(fp, "in_bus_%s_size%d_%d_, ",
            cur_interc->spice_model->name, fan_in, cur_interc->spice_model->cnt);
    cur_interc->spice_model->cnt++;
    /* Inputs */
    /* Generate the pin_prefix for src_pb_graph_node and des_pb_graph_node*/
    generate_verilog_src_des_pb_graph_pin_prefix(src_pb_graph_node, des_pb_graph_node, pin2pin_interc_type, 
                                               cur_interc, formatted_parent_pin_prefix, &src_pin_prefix, &des_pin_prefix);
    /* Outputs */
    fprintf(fp, "%s__%s_%d_, ", 
            des_pin_prefix, des_pb_graph_pin->port->name, des_pb_graph_pin->pin_number);

    assert(select_edge < fan_in);
    /* SRAMs */
    switch (cur_interc->spice_model->design_tech) {
    case SPICE_MODEL_DESIGN_CMOS:
      decode_cmos_mux_sram_bits(cur_interc->spice_model, fan_in, select_edge, 
                                &num_sram_bits, &sram_bits, &mux_level);
      break;
    case SPICE_MODEL_DESIGN_RRAM:
      decode_verilog_rram_mux(cur_interc->spice_model, fan_in, select_edge, 
                              &num_sram_bits, &sram_bits, &mux_level);
      break;
    default:
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid design technology for verilog model (%s)!\n",
                 __FILE__, __LINE__, cur_interc->spice_model->name);
    }
    
    /* Create wires to sram outputs*/
    num_sram = sram_verilog_model->cnt;
    fprintf(fp,"%s_out[%d:%d], ", sram_verilog_model->prefix, num_sram + num_sram_bits - 1, num_sram);
    fprintf(fp,"%s_outb[%d:%d] ", sram_verilog_model->prefix, num_sram + num_sram_bits - 1, num_sram);
    num_sram = sram_verilog_model->cnt + num_sram_bits;
    
    /* Local vdd and gnd, TODO: we should have an independent VDD for all local interconnections*/
    fprintf(fp, ");\n");
    /* Print the encoding in SPICE netlist for debugging */
    switch (cur_interc->spice_model->design_tech) {
    case SPICE_MODEL_DESIGN_CMOS:
      fprintf(fp, "//----- SRAM bits for MUX[%d], level=%d, select_path_id=%d. -----\n", 
              cur_interc->spice_model->cnt, mux_level, select_edge);
      fprintf(fp, "//-----");
      num_sram = sram_verilog_model->cnt;
      for (ilevel = 0; ilevel < num_sram_bits; ilevel++) {
        fprintf(fp, "%d", sram_bits[ilevel]);
        /* Store the configuraion bit to linked-list */
        conf_bits_head = add_conf_bit_info_to_llist(conf_bits_head, num_sram,
                                                    sram_bits[ilevel], 0, 0, cur_interc->spice_model);
        num_sram++;
      }
      fprintf(fp, "-----\n");
      break;
    case SPICE_MODEL_DESIGN_RRAM:
      fprintf(fp, "//----- BL/WL bits for 4T1R MUX[%d], level=%d, select_path_id=%d. -----\n", 
              cur_interc->spice_model->cnt, mux_level, select_edge);
      fprintf(fp, "//---- BL: ");
      for (ilevel = 0; ilevel < num_sram_bits/2; ilevel++) {
        fprintf(fp, "%d", sram_bits[ilevel]);
      }
      fprintf(fp, "-----\n");
      fprintf(fp, "//---- WL: ");
      for (ilevel = 0; ilevel < num_sram_bits/2; ilevel++) {
        fprintf(fp, "%d", sram_bits[ilevel+num_sram_bits/2]);
      }
      fprintf(fp, "-----\n");
      /* Store the configuraion bit to linked-list */
      num_sram = sram_verilog_model->cnt;
      for (ilevel = 0; ilevel < num_sram_bits/2; ilevel++) {
        conf_bits_head = add_conf_bit_info_to_llist(conf_bits_head, num_sram,
                                                    0, sram_bits[ilevel], sram_bits[ilevel+num_sram_bits/2], 
                                                    cur_interc->spice_model);
        num_sram++;
      }
      fprintf(fp, "-----\n");
      break;
    default:
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid design technology for verilog model (%s)!\n",
                 __FILE__, __LINE__, cur_interc->spice_model->name);
    }
    /* Print all the srams*/
//    cur_sram = sram_verilog_model->cnt; 
//    for (ilevel = 0; ilevel < num_sram_bits; ilevel++) {
//      fprintf(fp, "%s ", sram_verilog_model->name);  //
//      fprintf(fp, "%s[%d] (", sram_verilog_model->prefix, cur_sram); /* SRAM subckts*/
//      fprintf(fp, "%s_out[%d] ", sram_verilog_model->prefix, cur_sram); /* Input*/
//      fprintf(fp, "%s_out[%d], %s_outb[%d]);", 
//              sram_verilog_model->prefix, cur_sram, 
//              sram_verilog_model->prefix, cur_sram); /* Outputs */
//      cur_sram++;
//    }
//    assert(cur_sram == num_sram);
    sram_verilog_model->cnt = num_sram;
    //for (ilevel = 0; ilevel < mux_level; ilevel++) {
    //  fprintf(fp, "X%s[%d] ", ); /* SRAM name*/
    //  fprintf(fp, "sram_carry[%d] sram[%d]_out sram[%d]_outb %s gvdd_sram sgnd %s\n");
    //}
    /* Free */
    my_free(sram_bits);
    my_free(src_pin_prefix);
    my_free(des_pin_prefix);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid interconnection type for %s (Arch[LINE%d])!\n",
               __FILE__, __LINE__, cur_interc->name, cur_interc->line_num);
    exit(1);
  }
  return;
}


/* Print the SPICE interconnections according to pb_graph */
void dump_verilog_pb_graph_interc(FILE* fp, 
                                  char* pin_prefix,
                                  t_pb_graph_node* cur_pb_graph_node,
                                  t_pb* cur_pb,
                                  int select_mode_index,
                                  int is_idle) {
  int iport, ipin;
  int ipb, jpb;
  t_mode* cur_mode = NULL;
  t_pb_type* cur_pb_type = cur_pb_graph_node->pb_type;
  t_pb_graph_node* child_pb_graph_node = NULL;
  t_pb* child_pb = NULL;
  int is_child_pb_idle = 0;
  
  char* formatted_pin_prefix = format_spice_node_prefix(pin_prefix); /* Complete a "_" at the end if needed*/

  int node_index = -1;
  int prev_node = -1; 
  int prev_edge = -1;
  int path_id = -1;
  t_rr_node* pb_rr_nodes = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_type*/
  if (NULL == cur_pb_type) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_type.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  
  /* Assign current mode */
  cur_mode = &(cur_pb_graph_node->pb_type->modes[select_mode_index]);

  /* We check output_pins of cur_pb_graph_node and its the input_edges
   * Built the interconnections between outputs of cur_pb_graph_node and outputs of child_pb_graph_node
   *   child_pb_graph_node.output_pins -----------------> cur_pb_graph_node.outpins
   *                                        /|\
   *                                         |
   *                         input_pins,   edges,       output_pins
   */ 
  for (iport = 0; iport < cur_pb_graph_node->num_output_ports; iport++) {
    for (ipin = 0; ipin < cur_pb_graph_node->num_output_pins[iport]; ipin++) {
      /* If this is a idle block, we set 0 to the selected edge*/
      if (is_idle) {
        assert(NULL == cur_pb);
        dump_verilog_pb_graph_pin_interc(fp,
                                          formatted_pin_prefix, /* parent_pin_prefix */
                                          OUTPUT2OUTPUT_INTERC,
                                          &(cur_pb_graph_node->output_pins[iport][ipin]),
                                          cur_mode,
                                          0);
      } else {
        /* Get the selected edge of current pin*/
        assert(NULL != cur_pb);
        pb_rr_nodes = cur_pb->rr_graph;
        node_index = cur_pb_graph_node->output_pins[iport][ipin].pin_count_in_cluster;
        prev_node = pb_rr_nodes[node_index].prev_node;
        prev_edge = pb_rr_nodes[node_index].prev_edge;
        /* Make sure this pb_rr_node is not OPEN and is not a primitive output*/
        if (OPEN == prev_node) {
          path_id = 0; //
        } else {
          /* Find the path_id */
          path_id = verilog_find_path_id_between_pb_rr_nodes(pb_rr_nodes, prev_node, node_index);
          assert(-1 != path_id);
        }
        dump_verilog_pb_graph_pin_interc(fp,
                                          formatted_pin_prefix, /* parent_pin_prefix */
                                          OUTPUT2OUTPUT_INTERC,
                                          &(cur_pb_graph_node->output_pins[iport][ipin]),
                                          cur_mode,
                                          path_id);
      }
    }
  }
  
  /* We check input_pins of child_pb_graph_node and its the input_edges
   * Built the interconnections between inputs of cur_pb_graph_node and inputs of child_pb_graph_node
   *   cur_pb_graph_node.input_pins -----------------> child_pb_graph_node.input_pins
   *                                        /|\
   *                                         |
   *                         input_pins,   edges,       output_pins
   */ 
  for (ipb = 0; ipb < cur_pb_type->modes[select_mode_index].num_pb_type_children; ipb++) {
    for (jpb = 0; jpb < cur_pb_type->modes[select_mode_index].pb_type_children[ipb].num_pb; jpb++) {
      child_pb_graph_node = &(cur_pb_graph_node->child_pb_graph_nodes[select_mode_index][ipb][jpb]);
      /* If this is a idle block, we set 0 to the selected edge*/
      if (is_idle) {
        assert(NULL == cur_pb);
        /* For each child_pb_graph_node input pins*/
        for (iport = 0; iport < child_pb_graph_node->num_input_ports; iport++) {
          for (ipin = 0; ipin < child_pb_graph_node->num_input_pins[iport]; ipin++) {
            dump_verilog_pb_graph_pin_interc(fp,
                                              formatted_pin_prefix, /* parent_pin_prefix */
                                              INPUT2INPUT_INTERC,
                                              &(child_pb_graph_node->input_pins[iport][ipin]),
                                              cur_mode,
                                              0);
          }
        }
        /* TODO: for clock pins, we should do the same work */
        for (iport = 0; iport < child_pb_graph_node->num_clock_ports; iport++) {
          for (ipin = 0; ipin < child_pb_graph_node->num_clock_pins[iport]; ipin++) {
            dump_verilog_pb_graph_pin_interc(fp,
                                              formatted_pin_prefix, /* parent_pin_prefix */
                                              INPUT2INPUT_INTERC,
                                              &(child_pb_graph_node->clock_pins[iport][ipin]),
                                              cur_mode,
                                              0);
          }
        }
        continue;
      }
      assert(NULL != cur_pb);
      child_pb = &(cur_pb->child_pbs[ipb][jpb]);
      /* Check if child_pb is empty */
      if (NULL != child_pb->name) { 
        is_child_pb_idle = 0;
      } else {
        is_child_pb_idle = 1;
      }
      /* Get pb_rr_graph of current pb*/
      pb_rr_nodes = child_pb->rr_graph;
      /* For each child_pb_graph_node input pins*/
      for (iport = 0; iport < child_pb_graph_node->num_input_ports; iport++) {
        for (ipin = 0; ipin < child_pb_graph_node->num_input_pins[iport]; ipin++) {
          if (is_child_pb_idle) {
            prev_edge = 0;
          } else {
            /* Get the index of the edge that are selected to pass signal*/
            node_index = child_pb_graph_node->input_pins[iport][ipin].pin_count_in_cluster;
            prev_node = pb_rr_nodes[node_index].prev_node;
            prev_edge = pb_rr_nodes[node_index].prev_edge;
            /* Make sure this pb_rr_node is not OPEN and is not a primitive output*/
            if (OPEN == prev_node) {
              path_id = 0; //
            } else {
              /* Find the path_id */
              path_id = verilog_find_path_id_between_pb_rr_nodes(pb_rr_nodes, prev_node, node_index);
              assert(-1 != path_id);
            }
          }
          /* Write the interconnection*/
          dump_verilog_pb_graph_pin_interc(fp,
                                            formatted_pin_prefix, /* parent_pin_prefix */
                                            INPUT2INPUT_INTERC,
                                            &(child_pb_graph_node->input_pins[iport][ipin]),
                                            cur_mode,
                                            path_id);
        }
      }
      /* TODO: for clock pins, we should do the same work */
      for (iport = 0; iport < child_pb_graph_node->num_clock_ports; iport++) {
        for (ipin = 0; ipin < child_pb_graph_node->num_clock_pins[iport]; ipin++) {
          if (is_child_pb_idle) {
            prev_edge = 0;
          } else {
            /* Get the index of the edge that are selected to pass signal*/
            node_index = child_pb_graph_node->input_pins[iport][ipin].pin_count_in_cluster;
            prev_node = pb_rr_nodes[node_index].prev_node;
            prev_edge = pb_rr_nodes[node_index].prev_edge;
            /* Make sure this pb_rr_node is not OPEN and is not a primitive output*/
            if (OPEN == prev_node) {
              path_id = 0; //
            } else {
              /* Find the path_id */
              path_id = verilog_find_path_id_between_pb_rr_nodes(pb_rr_nodes, prev_node, node_index);
              assert(-1 != path_id);
            }
          }
          /* Write the interconnection*/
          dump_verilog_pb_graph_pin_interc(fp,
                                            formatted_pin_prefix, /* parent_pin_prefix */
                                            INPUT2INPUT_INTERC,
                                            &(child_pb_graph_node->clock_pins[iport][ipin]),
                                            cur_mode,
                                            path_id);
        }
      }
    }
  }

  return; 
}

/* Print the netlist for primitive pb_types*/
void dump_verilog_pb_graph_primitive_node(FILE* fp,
                                          char* subckt_prefix,
                                          t_pb* cur_pb,
                                          t_pb_graph_node* cur_pb_graph_node,
                                          int pb_type_index) {
  t_pb_type* cur_pb_type = NULL;
  char* formatted_subckt_prefix = format_spice_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  t_spice_model* verilog_model = NULL;
  char* subckt_name = NULL;
  char* subckt_port_name = NULL; 
  
  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_graph_node*/
  if (NULL == cur_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  verilog_model = cur_pb_type->spice_model;
  /* If we define a SPICE model for the pb_type,
   * We should print the subckt of it. 
   * 1. If the SPICE model defines an included netlist, we quote the netlist subckt 
   * 2. If not defined an included netlist, we built one only if this is a LUT
   */
  /* Example: <prefix><cur_pb_type_name>[<index>]*/
  subckt_name = (char*)my_malloc(sizeof(char)*
                (strlen(formatted_subckt_prefix) 
                + strlen(cur_pb_type->name) + 1
                + strlen(my_itoa(pb_type_index)) + 1 + 1)); /* Plus the '0' at the end of string*/
  sprintf(subckt_name, "%s%s_%d_", formatted_subckt_prefix, cur_pb_type->name, pb_type_index);
  /* Check if defines an included netlist*/
  if (NULL == verilog_model->model_netlist) {
    if (LUT_CLASS == cur_pb_type->class_type) {
      /* For LUT, we have a built-in netlist, See "verilog_lut.c", So we don't do anything here */
    } else {
      /* We report an error */
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Require an included netlist in Pb_type(%s) SPICE_model(%s)!\n",
                __FILE__, __LINE__, cur_pb_type->name, verilog_model->name);
      exit(1);
    }
  }
  /* Print the definition line 
   * IMPORTANT: NO SRAMs ports are created here, they are fixed when quoting spice_models
   */
  fprintf(fp, "module %s (", subckt_name);
  subckt_port_name = format_spice_node_prefix(subckt_name); 
  /* Inputs, outputs, inouts, clocks */
  dump_verilog_pb_type_ports(fp, subckt_name, 0, cur_pb_type, TRUE, FALSE);
  /* SRAM ports */
  fprintf(fp, ");\n");
  /* Include the spice_model*/
  fprintf(fp, "%s %s[%d] (", verilog_model->name, verilog_model->prefix, verilog_model->cnt); 
  verilog_model->cnt++; /* Stats the number of verilog_model used*/
  /* Make input, output, inout, clocks connected*/
  /* IMPORTANT: (sequence of these ports should be changed!) */
  dump_verilog_pb_type_ports(fp, subckt_name, 0, cur_pb_type, FALSE, FALSE);
  fprintf(fp, ");");
  /* Print end of subckt*/
  fprintf(fp, "endmodule\n");
  /* Free */
  my_free(subckt_name);
  
  return;
}

/* Print the subckt of a primitive pb */
void dump_verilog_pb_primitive_verilog_model(FILE* fp,
                                     char* subckt_prefix,
                                     t_pb* prim_pb,
                                     t_pb_graph_node* prim_pb_graph_node,
                                     int pb_index,
                                     t_spice_model* verilog_model,
                                     int is_idle) {
  t_pb_type* prim_pb_type = NULL;
  t_logical_block* mapped_logical_block = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_graph_node*/
  if (NULL == prim_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
 
  /* Initialize */ 
  prim_pb_type = prim_pb_graph_node->pb_type;
  if (is_idle) {
    mapped_logical_block = NULL;
  } else {
    mapped_logical_block = &logical_block[prim_pb->logical_block];
  }

  /* Asserts*/
  assert(pb_index == prim_pb_graph_node->placement_index);
  assert(0 == strcmp(verilog_model->name, prim_pb_type->spice_model->name));
  if (is_idle) {
    assert(NULL == prim_pb); 
  } else {
    if (NULL == prim_pb) {
      vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid prim_pb.\n", 
                 __FILE__, __LINE__); 
      exit(1);
    }
  }

  /* According to different type, we print netlist*/
  switch (verilog_model->type) {
  case SPICE_MODEL_LUT:
    /* If this is a idle block we should set sram_bits to zero*/
    dump_verilog_pb_primitive_lut(fp, subckt_prefix, mapped_logical_block, prim_pb_graph_node,
                            pb_index, verilog_model);
    break;
  case SPICE_MODEL_FF:
    assert(NULL != verilog_model->model_netlist);
    /* TODO : We should learn trigger type and initial value!!! and how to apply them!!! */
    dump_verilog_pb_primitive_ff(fp, subckt_prefix, mapped_logical_block, prim_pb_graph_node,
                           pb_index, verilog_model);
    break;
  case SPICE_MODEL_INPAD:
  case SPICE_MODEL_OUTPAD:
  case SPICE_MODEL_IOPAD:
    assert(NULL != verilog_model->model_netlist);
    dump_verilog_pb_primitive_io(fp, subckt_prefix, mapped_logical_block, prim_pb_graph_node,
                                 pb_index, verilog_model);
    break;
  case SPICE_MODEL_HARDLOGIC:
    assert(NULL != verilog_model->model_netlist);
    dump_verilog_pb_primitive_hardlogic(fp, subckt_prefix, mapped_logical_block, prim_pb_graph_node,
                                        pb_index, verilog_model);
    break;
  case SPICE_MODEL_VDD:
    /* TODO: Add codes for VDD */
    break;
  case SPICE_MODEL_GND:
    /* TODO: Add codes for GND */
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid type of verilog_model(%s), should be [LUT|FF|HARD_LOGIC|IO]!\n",
               __FILE__, __LINE__, verilog_model->name);
    exit(1);
    break;
  }
 
  return; 
}

/* Print idle pb_types recursively
 * search the idle_mode until we reach the leaf node
 */
void dump_verilog_idle_pb_graph_node_rec(FILE* fp,
                                         char* subckt_prefix,
                                         t_pb_graph_node* cur_pb_graph_node,
                                         int pb_type_index) {
  int mode_index, ipb, jpb, child_mode_index;
  t_pb_type* cur_pb_type = NULL;
  char* subckt_name = NULL;
  char* formatted_subckt_prefix = format_spice_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  char* pass_on_prefix = NULL;
  char* child_pb_type_prefix = NULL;
  char* subckt_port_prefix = NULL;
  
  int num_conf_bits = 0;
  int stamped_sram_cnt = sram_verilog_model->cnt;
  int stamped_sram_lsb = sram_verilog_model->cnt;

  int stamped_inpad_cnt = inpad_verilog_model->cnt;
  int stamped_outpad_cnt = outpad_verilog_model->cnt;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_graph_node*/
  if (NULL == cur_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  cur_pb_type = cur_pb_graph_node->pb_type;

  /* Recursively finish all the child pb_types*/
  if (NULL == cur_pb_type->spice_model) { 
    /* Find the mode that define_idle_mode*/
    mode_index = find_pb_type_idle_mode_index((*cur_pb_type));
    for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
      for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
        /* Pass the SPICE mode prefix on, 
         * <subckt_name>mode[<mode_name>]_
         */
        pass_on_prefix = (char*)my_malloc(sizeof(char)*
                           (strlen(formatted_subckt_prefix) + strlen(cur_pb_type->name) + 1 
                            + strlen(my_itoa(pb_type_index)) + 7 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1 + 1));
        sprintf(pass_on_prefix, "%s%s_%d__mode_%s__", 
                formatted_subckt_prefix, cur_pb_type->name, pb_type_index, cur_pb_type->modes[mode_index].name);
        /* Recursive*/
        dump_verilog_idle_pb_graph_node_rec(fp, pass_on_prefix,
                                            &(cur_pb_graph_node->child_pb_graph_nodes[mode_index][ipb][jpb]), jpb);
        /* Free */
        my_free(pass_on_prefix);
      }
    }
  }

  /* Check if this has defined a spice_model*/
  if (NULL != cur_pb_type->spice_model) {
    switch (cur_pb_type->class_type) {
    case LUT_CLASS: 
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                                NULL, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 1);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb_type->default_mode_num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb_type->default_mode_num_outpads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb_type->default_mode_num_conf_bits;
      break;
    case LATCH_CLASS:
      assert(0 == cur_pb_type->num_modes);
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                      NULL, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 1);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb_type->default_mode_num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb_type->default_mode_num_outpads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb_type->default_mode_num_conf_bits;
      break;
    case UNKNOWN_CLASS:
    case MEMORY_CLASS:
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                      NULL, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 1);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb_type->default_mode_num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb_type->default_mode_num_outpads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb_type->default_mode_num_conf_bits;
      break;  
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Unknown class type of pb_type(%s)!\n",
                 __FILE__, __LINE__, cur_pb_type->name);
      exit(1);
    }

  } else {
    /* Find the mode that define_idle_mode*/
    mode_index = find_pb_type_idle_mode_index((*cur_pb_type));
    /* Create a new subckt */
    /* <formatted_subckt_prefix>mode[<mode_name>]
     */
    subckt_name = (char*)my_malloc(sizeof(char)*
                  (strlen(formatted_subckt_prefix) + strlen(cur_pb_type->name) + 1 
                  + strlen(my_itoa(pb_type_index)) + 7 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1)); 
    /* Definition*/
    sprintf(subckt_name, "%s%s_%d__mode_%s_", 
            formatted_subckt_prefix, cur_pb_type->name, pb_type_index, cur_pb_type->modes[mode_index].name);
    /* Comment lines */
    fprintf(fp, "//----- Idle programmable logic block Verilog module %s -----\n", subckt_name);
    fprintf(fp, "module %s (", subckt_name);
    /* Inputs, outputs, inouts, clocks */
    subckt_port_prefix = (char*)my_malloc(sizeof(char)*
                                         (5 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1));
    sprintf(subckt_port_prefix, "mode_%s_", cur_pb_type->modes[mode_index].name);
    /*
    dump_verilog_pb_type_ports(fp, subckt_name, 0, cur_pb_type);
    */
    /* Print global set and reset */
    fprintf(fp, "input wire greset,\n");
    fprintf(fp, "input wire gset,\n");
    /* Simplify the port prefix, make SPICE netlist readable */
    dump_verilog_pb_type_ports(fp, subckt_port_prefix, 0, cur_pb_type, TRUE, FALSE);
    /* Print Input Pad and Output Pad */
    if (0 < (inpad_verilog_model->cnt - stamped_inpad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  input [%d:%d] %s%s \n", 
              inpad_verilog_model->cnt - 1, 
              stamped_inpad_cnt,
              gio_input_prefix,
              inpad_verilog_model->prefix); 
    }
    if (0 < (outpad_verilog_model->cnt - stamped_outpad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  output [%d:%d] %s%s \n", 
              outpad_verilog_model->cnt - 1, 
              stamped_outpad_cnt,
              gio_output_prefix,
              outpad_verilog_model->prefix);
    }
    /* Print Configuration ports */
    /* sram_verilog_model->cnt should be updated because all the child pbs have been dumped
     * stamped_sram_cnt remains the old sram_verilog_model->cnt before all the child pbs are dumped
     * Note by far, sram_verilog_model->cnt could be smaller than num_conf_bits
     * because the interconnection of current pb_type/pb has not yet dumped, which may contain
     * a few configuration bits. 
     */
    num_conf_bits = cur_pb_type->default_mode_num_conf_bits;
    if (0 < num_conf_bits) {
      fprintf(fp, ",");
      fprintf(fp, "  input [%d:%d] %s_out, \n", 
              stamped_sram_cnt + num_conf_bits - 1, 
              stamped_sram_cnt,
              sram_verilog_model->prefix); 
      /* inverted output of each configuration bit */
      fprintf(fp, "  input [%d:%d] %s_outb \n", 
              stamped_sram_cnt + num_conf_bits - 1, 
              stamped_sram_cnt,
              sram_verilog_model->prefix);
    }
    /* Finish with local vdd and gnd */
    fprintf(fp, ");\n");

    /* Definition ends*/
    /* Quote all child pb_types */
    for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
      /* Each child may exist multiple times in the hierarchy*/
      for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
        /* we should make sure this placement index == child_pb_type[jpb]*/
        assert(jpb == cur_pb_graph_node->child_pb_graph_nodes[mode_index][ipb][jpb].placement_index);
        /* If the pb_type_children is a leaf node, we don't use the mode to name it,
         * else we can use the mode to name it 
         */
        if (NULL == cur_pb_type->modes[mode_index].pb_type_children[ipb].spice_model) { /* Not a leaf node*/
          child_mode_index = find_pb_type_idle_mode_index(cur_pb_type->modes[mode_index].pb_type_children[ipb]);
          fprintf(fp, "%s_%s_%d__mode_%s_ ",
                  subckt_name, cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb, 
                  cur_pb_type->modes[mode_index].pb_type_children[ipb].modes[child_mode_index].name);
        } else { /* Have a verilog model definition, this is a leaf node*/
          fprintf(fp, "%s_%s_%d_ ",
                  subckt_name, cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb); 
        }
        /* <formatted_subckt_prefix>mode[<mode_name>]_<child_pb_type_name>[<ipb>]
         */
        fprintf(fp, "%s_%d_ (", cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        /* Pass the SPICE mode prefix on, 
         * <subckt_name>mode[<mode_name>]_<child_pb_type_name>[<jpb>]
         * <child_pb_type_name>[<jpb>]
         */
        /*
        child_pb_type_prefix = (char*)my_malloc(sizeof(char)*
                                 (strlen(subckt_name) + 1 
                                  + strlen(cur_pb_type->modes[imode].pb_type_children[ipb].name) + 1 
                                  + strlen(my_itoa(jpb)) + 1 + 1));
        sprintf(child_pb_type_prefix, "%s_%s[%d]", subckt_name,
        cur_pb_type->modes[imode].pb_type_children[ipb].name, jpb);
        */
        /* Simplify the prefix! */
        child_pb_type_prefix = (char*)my_malloc(sizeof(char)* 
                                  (strlen(cur_pb_type->modes[mode_index].pb_type_children[ipb].name) + 1 
                                   + strlen(my_itoa(jpb)) + 1 + 1));
        sprintf(child_pb_type_prefix, "%s_%d_",
                cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        /* Print inputs, outputs, inouts, clocks
         * NO SRAMs !!! They have already been fixed in the bottom level
         */
        dump_verilog_pb_type_ports(fp, child_pb_type_prefix, 0, &(cur_pb_type->modes[mode_index].pb_type_children[ipb]),FALSE, FALSE);
        /* Print input and output pads */
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_inpads));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_inpads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_input_prefix,
                  inpad_verilog_model->prefix, 
                  stamped_inpad_cnt,
                  stamped_inpad_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_inpads - 1);
          /* update stamped inpad counter */
          stamped_inpad_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_inpads;
        }
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_outpads));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_outpads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_output_prefix,
                  outpad_verilog_model->prefix, 
                  stamped_outpad_cnt,
                  stamped_outpad_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_outpads - 1);
          /* update stamped outpad counter */
          stamped_outpad_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_outpads;
        }
        /* Print configuration ports */
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_conf_bits));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_conf_bits) {
          fprintf(fp, ",");
          fprintf(fp, "  %s_out[%d:%d], \n", 
                  sram_verilog_model->prefix, 
                  stamped_sram_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_conf_bits - 1,
                  stamped_sram_cnt);
          /* inverted output of each configuration bit */
          fprintf(fp, "  %s_outb[%d:%d] \n", 
                  sram_verilog_model->prefix, 
                  stamped_sram_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_conf_bits - 1,
                  stamped_sram_cnt);
          /* update stamped sram counter */
          stamped_sram_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_conf_bits; 
        }
        fprintf(fp, ");\n"); /* Local vdd and gnd*/

        /* Find the pb_type_children mode */
        if (NULL == cur_pb_type->modes[mode_index].pb_type_children[ipb].spice_model) { /* Find the idle_mode_index, if this is not a leaf node  */
          child_mode_index = find_pb_type_idle_mode_index(cur_pb_type->modes[mode_index].pb_type_children[ipb]);
        }
        my_free(child_pb_type_prefix);
      }
    }
    /* Print interconnections, set is_idle as TRUE*/
    dump_verilog_pb_graph_interc(fp, subckt_name, cur_pb_graph_node, NULL, mode_index, 1);
    /* Check each pins of pb_graph_node */ 
    /* Check and update stamped_sram_cnt */
    assert(!(stamped_sram_cnt > (stamped_sram_lsb + num_conf_bits)));
    stamped_sram_cnt = stamped_sram_lsb + num_conf_bits;
    /* End the subckt */
    fprintf(fp, "endmodule\n");
    /* Comment lines */
    fprintf(fp, "//----- END Idle programmable logic block Verilog module %s -----\n\n", subckt_name);
    /* Free subckt name*/
    my_free(subckt_name);
  }

  if (stamped_sram_cnt != sram_verilog_model->cnt) {
  assert(stamped_sram_cnt == sram_verilog_model->cnt);
  }
  assert(stamped_inpad_cnt == inpad_verilog_model->cnt); 
  assert(stamped_outpad_cnt == outpad_verilog_model->cnt);

  return;
}

/* Print SPICE netlist for each pb and corresponding pb_graph_node*/
void dump_verilog_pb_graph_node_rec(FILE* fp, 
                                    char* subckt_prefix, 
                                    t_pb* cur_pb, 
                                    t_pb_graph_node* cur_pb_graph_node,
                                    int pb_type_index) {
  int mode_index, ipb, jpb, child_mode_index;
  t_pb_type* cur_pb_type = NULL;
  char* subckt_name = NULL;
  char* formatted_subckt_prefix = format_spice_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  char* pass_on_prefix = NULL;
  char* child_pb_type_prefix = NULL;

  char* subckt_port_prefix = NULL;

  int num_conf_bits = 0;
  int child_pb_num_conf_bits = 0;
  int child_pb_num_inpads = 0;
  int child_pb_num_outpads = 0;
  int stamped_sram_cnt = sram_verilog_model->cnt;
  int stamped_sram_lsb = sram_verilog_model->cnt;

  int stamped_inpad_cnt = inpad_verilog_model->cnt;
  int stamped_outpad_cnt = outpad_verilog_model->cnt;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_graph_node*/
  if (NULL == cur_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  cur_pb_type = cur_pb_graph_node->pb_type;
  mode_index = cur_pb->mode; 

  /* Recursively finish all the child pb_types*/
  if (NULL == cur_pb_type->spice_model) { 
    /* recursive for the child_pbs*/
    for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
      for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
        /* Pass the SPICE mode prefix on, 
         * <subckt_name><pb_type_name>[<pb_index>]_mode[<mode_name>]_
         */
        pass_on_prefix = (char*)my_malloc(sizeof(char)*
                          (strlen(formatted_subckt_prefix) + strlen(cur_pb_type->name) + 1 
                           + strlen(my_itoa(pb_type_index)) + 7 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1 + 1));
        sprintf(pass_on_prefix, "%s%s_%d__mode_%s__", 
                formatted_subckt_prefix, cur_pb_type->name, pb_type_index, cur_pb_type->modes[mode_index].name);
        /* Recursive*/
        /* Refer to pack/output_clustering.c [LINE 392] */
        if ((NULL != cur_pb->child_pbs[ipb])&&(NULL != cur_pb->child_pbs[ipb][jpb].name)) {
          dump_verilog_pb_graph_node_rec(fp, pass_on_prefix, &(cur_pb->child_pbs[ipb][jpb]), 
                                         cur_pb->child_pbs[ipb][jpb].pb_graph_node, jpb);
        } else {
          /* Check if this pb has no children, no children mean idle*/
          dump_verilog_idle_pb_graph_node_rec(fp, pass_on_prefix,
                                            cur_pb->child_pbs[ipb][jpb].pb_graph_node, jpb);
        }
        /* Free */
        my_free(pass_on_prefix);
      }
    }
  }

  /* Check if this has defined a spice_model*/
  if (NULL != cur_pb_type->spice_model) {
    switch (cur_pb_type->class_type) {
    case LUT_CLASS: 
      assert(1 == cur_pb_type->modes[mode_index].num_pb_type_children);
      assert(1 == cur_pb_type->modes[mode_index].pb_type_children[0].num_pb);
      /* Consider the num_pb, create all the subckts*/
      for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
        for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
          /* Special care for LUT !!!
           * Mapped logical block information is stored in child_pbs
           */
          dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                          &(cur_pb->child_pbs[ipb][jpb]), cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 0);
          /* update the number of SRAM, I/O pads */
          /* update stamped inpad counter */
          stamped_inpad_cnt += cur_pb->num_inpads;
          /* update stamped outpad counter */
          stamped_outpad_cnt += cur_pb->num_outpads;
          /* update stamped sram counter */
          stamped_sram_cnt += cur_pb->num_conf_bits;
        }
      }
      break;
    case LATCH_CLASS:
      assert(0 == cur_pb_type->num_modes);
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                      cur_pb, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 0);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb->num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb->num_outpads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb->num_conf_bits;
      break;
    case UNKNOWN_CLASS:
    case MEMORY_CLASS:
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                        cur_pb, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 0);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb->num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb->num_outpads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb->num_conf_bits;
      break;  
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Unknown class type of pb_type(%s)!\n",
                 __FILE__, __LINE__, cur_pb_type->name);
      exit(1);
    }
  } else {
    /* Create a new subckt */
    /* <formatted_subckt_prefix>mode[<mode_name>]
     */
    subckt_name = (char*)my_malloc(sizeof(char)*
                  (strlen(formatted_subckt_prefix) + strlen(cur_pb_type->name) + 1 
                   + strlen(my_itoa(pb_type_index)) + 7 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1)); 
    /* Definition*/
    sprintf(subckt_name, "%s%s_%d__mode_%s_", 
            formatted_subckt_prefix, cur_pb_type->name, pb_type_index, cur_pb_type->modes[mode_index].name);
    /* Comment lines */
    fprintf(fp, "//----- Programmable logic block Verilog module %s -----\n", subckt_name);
    fprintf(fp, "module %s (", subckt_name);
    /* Inputs, outputs, inouts, clocks */
    subckt_port_prefix = (char*)my_malloc(sizeof(char)*
                          (5 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1));
    sprintf(subckt_port_prefix, "mode_%s_", cur_pb_type->modes[mode_index].name);
    /*
    dump_verilog_pb_type_ports(fp, subckt_name, 0, cur_pb_type);
    */
    /* Print global set and reset */
    fprintf(fp, "input wire greset,\n");
    fprintf(fp, "input wire gset,\n");
    /* Simplify the prefix! Make the SPICE netlist readable*/
    dump_verilog_pb_type_ports(fp, subckt_port_prefix, 0, cur_pb_type, TRUE, FALSE);
    /* Print Input Pad and Output Pad */
    if (0 < (inpad_verilog_model->cnt - stamped_inpad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  input [%d:%d] %s%s \n", 
              inpad_verilog_model->cnt - 1, 
              stamped_inpad_cnt,
              gio_input_prefix,
              inpad_verilog_model->prefix); 
    }
    if (0 < (outpad_verilog_model->cnt - stamped_outpad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  output [%d:%d] %s%s \n", 
              outpad_verilog_model->cnt - 1, 
              stamped_outpad_cnt,
              gio_output_prefix,
              outpad_verilog_model->prefix);
    }
    /* Print Configuration ports 
     * Note by far, sram_verilog_model->cnt could be smaller than num_conf_bits
     * because the interconnection of current pb_type/pb has not yet dumped, which may contain
     * a few configuration bits. 
     */
    num_conf_bits = cur_pb->num_conf_bits; 
    if (0 < num_conf_bits) {
      fprintf(fp, ",");
      fprintf(fp, "  input [%d:%d] %s_out, \n", 
              stamped_sram_cnt + num_conf_bits - 1, 
              stamped_sram_cnt,
              sram_verilog_model->prefix); 
      /* inverted output of each configuration bit */
      fprintf(fp, "  input [%d:%d] %s_outb \n", 
              stamped_sram_cnt + num_conf_bits - 1, 
              stamped_sram_cnt,
              sram_verilog_model->prefix);
    }
    fprintf(fp, ");\n");

    /* Simplify the prefix! Make the SPICE netlist readable
     * This dump_port function can be commented if the verilog syntax requires to delcare all the ports inside a module!
     */
    /*dump_verilog_pb_type_ports(fp, subckt_port_prefix, 0, cur_pb_type, TRUE);*/

    /* Definition ends*/
    /* Quote all child pb_types */
    for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
      /* Each child may exist multiple times in the hierarchy*/
      for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
        /* we should make sure this placement index == child_pb_type[jpb]*/
        assert(jpb == cur_pb_graph_node->child_pb_graph_nodes[mode_index][ipb][jpb].placement_index);
        /* <formatted_subckt_prefix>mode[<mode_name>]_<child_pb_type_name>[<ipb>]
         */
        /* Find the pb_type_children mode */
        if ((NULL != cur_pb->child_pbs[ipb])&&(NULL != cur_pb->child_pbs[ipb][jpb].name)) {
          child_mode_index = cur_pb->child_pbs[ipb][jpb].mode; 
          child_pb_num_conf_bits = cur_pb->child_pbs[ipb][jpb].num_conf_bits;
          child_pb_num_inpads = cur_pb->child_pbs[ipb][jpb].num_inpads;
          child_pb_num_outpads = cur_pb->child_pbs[ipb][jpb].num_outpads;
        } else /*if (NULL == cur_pb_type->modes[mode_index].pb_type_children[ipb].spice_model)*/ { /* Find the idle_mode_index, if this is not a leaf node  */
          child_mode_index = find_pb_type_idle_mode_index(cur_pb_type->modes[mode_index].pb_type_children[ipb]);
          child_pb_num_conf_bits = cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_conf_bits;
          child_pb_num_inpads = cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_inpads;
          child_pb_num_outpads = cur_pb_type->modes[mode_index].pb_type_children[ipb].default_mode_num_outpads;
        }
        /* If the pb_type_children is a leaf node, we don't use the mode to name it,
         * else we can use the mode to name it 
         */
        if (NULL == cur_pb_type->modes[mode_index].pb_type_children[ipb].spice_model) { /* Not a leaf node*/
          fprintf(fp, "%s_%s_%d__mode_%s_ ",
                  subckt_name, cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb, 
                  cur_pb_type->modes[mode_index].pb_type_children[ipb].modes[child_mode_index].name);
        } else { /* Have a verilog model definition, this is a leaf node*/
          fprintf(fp, "%s_%s_%d_ ",
                  subckt_name, cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb); 
        }
        fprintf(fp, "%s_%d_ (", cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        /* Pass the SPICE mode prefix on, 
         * <subckt_name>mode[<mode_name>]_<child_pb_type_name>[<jpb>]
         */
        /*
        child_pb_type_prefix = (char*)my_malloc(sizeof(char)*
                               (strlen(subckt_name) + 1 
                                + strlen(cur_pb_type->modes[mode_index].pb_type_children[ipb].name) + 1 
                                + strlen(my_itoa(jpb)) + 1 + 1));
        sprintf(child_pb_type_prefix, "%s_%s[%d]", subckt_name,
                cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        */
        /* Simplify the prefix! Make the SPICE netlist readable*/
        child_pb_type_prefix = (char*)my_malloc(sizeof(char)*
                               (strlen(cur_pb_type->modes[mode_index].pb_type_children[ipb].name) + 1 
                                + strlen(my_itoa(jpb)) + 1 + 1));
        sprintf(child_pb_type_prefix, "%s_%d_", 
                cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        /* Print inputs, outputs, inouts, clocks
         * NO SRAMs !!! They have already been fixed in the bottom level
         */
        dump_verilog_pb_type_ports(fp, child_pb_type_prefix, 0, &(cur_pb_type->modes[mode_index].pb_type_children[ipb]), FALSE, FALSE);
        /* Print input and output pads */
        assert(!(0 > child_pb_num_inpads));
        if (0 < child_pb_num_inpads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_input_prefix,
                  inpad_verilog_model->prefix, 
                  stamped_inpad_cnt,
                  stamped_inpad_cnt + child_pb_num_inpads - 1);
          /* update stamped inpad counter */
          stamped_inpad_cnt += child_pb_num_inpads;
        }
        assert(!(0 > child_pb_num_outpads));
        if (0 < child_pb_num_outpads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_output_prefix,
                  outpad_verilog_model->prefix, 
                  stamped_outpad_cnt,
                  stamped_outpad_cnt + child_pb_num_outpads - 1);
          /* update stamped outpad counter */
          stamped_outpad_cnt += child_pb_num_outpads;
        }
        /* Print configuration ports */
        assert(!(0 > child_pb_num_conf_bits));
        if (0 < child_pb_num_conf_bits) {
          fprintf(fp, ",");
          fprintf(fp, "  %s_out[%d:%d], \n", 
                  sram_verilog_model->prefix, 
                  stamped_sram_cnt + child_pb_num_conf_bits - 1,
                  stamped_sram_cnt);
          /* inverted output of each configuration bit */
          fprintf(fp, "  %s_outb[%d:%d] \n", 
                  sram_verilog_model->prefix, 
                  stamped_sram_cnt + child_pb_num_conf_bits - 1,
                  stamped_sram_cnt);
          /* update stamped sram counter */
          stamped_sram_cnt += child_pb_num_conf_bits; 
        }

        fprintf(fp, "); \n"); /* Local vdd and gnd*/
        my_free(child_pb_type_prefix);
      }
    }
    /* Print interconnections, set is_idle as TRUE*/
    dump_verilog_pb_graph_interc(fp, subckt_name, cur_pb_graph_node, cur_pb, mode_index, 0);
    /* Check each pins of pb_graph_node */ 
    /* Check and update stamped_sram_cnt */
    assert(!(stamped_sram_cnt > (stamped_sram_lsb + num_conf_bits)));
    stamped_sram_cnt = stamped_sram_lsb + num_conf_bits;
    /* End the subckt */
    fprintf(fp, "endmodule\n");
    /* Comment lines */
    fprintf(fp, "//----- END Programmable logic block Verilog module %s -----\n\n", subckt_name);
    /* Free subckt name*/
    my_free(subckt_name);
  }

  if ((stamped_sram_cnt) != sram_verilog_model->cnt) {
  assert(stamped_sram_cnt == sram_verilog_model->cnt); 
  }
  assert(stamped_inpad_cnt == inpad_verilog_model->cnt); 
  assert(stamped_outpad_cnt == outpad_verilog_model->cnt); 

  return;
}

/* Print physical mode of pb_types and configure it to the idle pb_types recursively
 * search the idle_mode until we reach the leaf node
 */
void dump_verilog_phy_pb_graph_node_rec(FILE* fp,
                                        char* subckt_prefix,
                                        t_pb_graph_node* cur_pb_graph_node,
                                        int pb_type_index) {
  int mode_index, ipb, jpb, child_mode_index;
  t_pb_type* cur_pb_type = NULL;
  char* subckt_name = NULL;
  char* formatted_subckt_prefix = format_spice_node_prefix(subckt_prefix); /* Complete a "_" at the end if needed*/
  char* pass_on_prefix = NULL;
  char* child_pb_type_prefix = NULL;
  char* subckt_port_prefix = NULL;
  
  int num_conf_bits = 0;
  int stamped_sram_cnt = sram_verilog_model->cnt;
  int stamped_sram_lsb = sram_verilog_model->cnt;

  int stamped_inpad_cnt = inpad_verilog_model->cnt;
  int stamped_outpad_cnt = outpad_verilog_model->cnt;
  int stamped_iopad_cnt = iopad_verilog_model->cnt;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_graph_node*/
  if (NULL == cur_pb_graph_node) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_graph_node.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  cur_pb_type = cur_pb_graph_node->pb_type;

  /* Recursively finish all the child pb_types*/
  if (NULL == cur_pb_type->spice_model) { 
    /* Find the mode that define_idle_mode*/
    mode_index = find_pb_type_physical_mode_index((*cur_pb_type));
    for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
      for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
        /* Pass the SPICE mode prefix on, 
         * <subckt_name>mode[<mode_name>]_
         */
        pass_on_prefix = (char*)my_malloc(sizeof(char)*
                           (strlen(formatted_subckt_prefix) + strlen(cur_pb_type->name) + 1 
                            + strlen(my_itoa(pb_type_index)) + 7 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1 + 1));
        sprintf(pass_on_prefix, "%s%s_%d__mode_%s__", 
                formatted_subckt_prefix, cur_pb_type->name, pb_type_index, cur_pb_type->modes[mode_index].name);
        /* Recursive*/
        dump_verilog_phy_pb_graph_node_rec(fp, pass_on_prefix,
                                          &(cur_pb_graph_node->child_pb_graph_nodes[mode_index][ipb][jpb]), jpb);
        /* Free */
        my_free(pass_on_prefix);
      }
    }
  }

  /* Check if this has defined a spice_model*/
  if (NULL != cur_pb_type->spice_model) {
    switch (cur_pb_type->class_type) {
    case LUT_CLASS: 
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                              NULL, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 1);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb_type->physical_mode_num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb_type->physical_mode_num_outpads;
      /* update stamped iopad counter */
      stamped_iopad_cnt += cur_pb_type->physical_mode_num_iopads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb_type->physical_mode_num_conf_bits;
      break;
    case LATCH_CLASS:
      assert(0 == cur_pb_type->num_modes);
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                      NULL, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 1);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb_type->physical_mode_num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb_type->physical_mode_num_outpads;
      /* update stamped iopad counter */
      stamped_iopad_cnt += cur_pb_type->physical_mode_num_iopads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb_type->physical_mode_num_conf_bits;
      break;
    case UNKNOWN_CLASS:
    case MEMORY_CLASS:
      /* Consider the num_pb, create all the subckts*/
      dump_verilog_pb_primitive_verilog_model(fp, formatted_subckt_prefix, 
                                      NULL, cur_pb_graph_node, pb_type_index, cur_pb_type->spice_model, 1);
      /* update the number of SRAM, I/O pads */
      /* update stamped inpad counter */
      stamped_inpad_cnt += cur_pb_type->physical_mode_num_inpads;
      /* update stamped outpad counter */
      stamped_outpad_cnt += cur_pb_type->physical_mode_num_outpads;
      /* update stamped iopad counter */
      stamped_iopad_cnt += cur_pb_type->physical_mode_num_iopads;
      /* update stamped sram counter */
      stamped_sram_cnt += cur_pb_type->physical_mode_num_conf_bits;
      break;  
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Unknown class type of pb_type(%s)!\n",
                 __FILE__, __LINE__, cur_pb_type->name);
      exit(1);
    }

  } else {
    /* Find the mode that define_idle_mode*/
    mode_index = find_pb_type_physical_mode_index((*cur_pb_type));
    /* Create a new subckt */
    /* <formatted_subckt_prefix>mode[<mode_name>]
     */
    subckt_name = (char*)my_malloc(sizeof(char)*
                  (strlen(formatted_subckt_prefix) + strlen(cur_pb_type->name) + 1 
                  + strlen(my_itoa(pb_type_index)) + 7 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1)); 
    /* Definition*/
    sprintf(subckt_name, "%s%s_%d__mode_%s_", 
            formatted_subckt_prefix, cur_pb_type->name, pb_type_index, cur_pb_type->modes[mode_index].name);
    /* Comment lines */
    fprintf(fp, "//----- Idle programmable logic block Verilog module %s -----\n", subckt_name);
    fprintf(fp, "module %s (", subckt_name);
    /* Inputs, outputs, inouts, clocks */
    subckt_port_prefix = (char*)my_malloc(sizeof(char)*
                                         (5 + strlen(cur_pb_type->modes[mode_index].name) + 1 + 1));
    sprintf(subckt_port_prefix, "mode_%s_", cur_pb_type->modes[mode_index].name);
    /*
    dump_verilog_pb_type_ports(fp, subckt_name, 0, cur_pb_type);
    */
    /* Print global set and reset */
    fprintf(fp, "input wire greset,\n");
    fprintf(fp, "input wire gset,\n");
    /* Simplify the port prefix, make SPICE netlist readable */
    dump_verilog_pb_type_ports(fp, subckt_port_prefix, 0, cur_pb_type, TRUE, FALSE);
    /* Print Input Pad and Output Pad */
    if (0 < (inpad_verilog_model->cnt - stamped_inpad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  input [%d:%d] %s%s \n", 
              inpad_verilog_model->cnt - 1, 
              stamped_inpad_cnt,
              gio_input_prefix,
              inpad_verilog_model->prefix); 
    }
    if (0 < (outpad_verilog_model->cnt - stamped_outpad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  output [%d:%d] %s%s \n", 
              outpad_verilog_model->cnt - 1, 
              stamped_outpad_cnt,
              gio_output_prefix,
              outpad_verilog_model->prefix);
    }
    if (0 < (iopad_verilog_model->cnt - stamped_iopad_cnt)) {
      fprintf(fp, ",");
      fprintf(fp, "  inout [%d:%d] %s%s \n", 
              iopad_verilog_model->cnt - 1, 
              stamped_iopad_cnt,
              gio_inout_prefix,
              iopad_verilog_model->prefix);
    }
    /* Print Configuration ports */
    /* sram_verilog_model->cnt should be updated because all the child pbs have been dumped
     * stamped_sram_cnt remains the old sram_verilog_model->cnt before all the child pbs are dumped
     * Note by far, sram_verilog_model->cnt could be smaller than num_conf_bits
     * because the interconnection of current pb_type/pb has not yet dumped, which may contain
     * a few configuration bits. 
     */
    num_conf_bits = cur_pb_type->physical_mode_num_conf_bits;
    if (0 < num_conf_bits) {
      fprintf(fp, ",");
      fprintf(fp, "  input [%d:%d] %s_out, \n", 
              stamped_sram_cnt + num_conf_bits - 1, 
              stamped_sram_cnt,
              sram_verilog_model->prefix); 
      /* inverted output of each configuration bit */
      fprintf(fp, "  input [%d:%d] %s_outb \n", 
              stamped_sram_cnt + num_conf_bits - 1, 
              stamped_sram_cnt,
              sram_verilog_model->prefix);
    }
    /* Finish with local vdd and gnd */
    fprintf(fp, ");\n");

    /* Definition ends*/
    /* Quote all child pb_types */
    for (ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
      /* Each child may exist multiple times in the hierarchy*/
      for (jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++) {
        /* we should make sure this placement index == child_pb_type[jpb]*/
        assert(jpb == cur_pb_graph_node->child_pb_graph_nodes[mode_index][ipb][jpb].placement_index);
        /* If the pb_type_children is a leaf node, we don't use the mode to name it,
         * else we can use the mode to name it 
         */
        if (NULL == cur_pb_type->modes[mode_index].pb_type_children[ipb].spice_model) { /* Not a leaf node*/
          child_mode_index = find_pb_type_idle_mode_index(cur_pb_type->modes[mode_index].pb_type_children[ipb]);
          fprintf(fp, "%s_%s_%d__mode_%s_ ",
                  subckt_name, cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb, 
                  cur_pb_type->modes[mode_index].pb_type_children[ipb].modes[child_mode_index].name);
        } else { /* Have a verilog model definition, this is a leaf node*/
          fprintf(fp, "%s_%s_%d_ ",
                  subckt_name, cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb); 
        }
        /* <formatted_subckt_prefix>mode[<mode_name>]_<child_pb_type_name>[<ipb>]
         */
        fprintf(fp, "%s_%d_ (", cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        /* Pass the SPICE mode prefix on, 
         * <subckt_name>mode[<mode_name>]_<child_pb_type_name>[<jpb>]
         * <child_pb_type_name>[<jpb>]
         */
        /*
        child_pb_type_prefix = (char*)my_malloc(sizeof(char)*
                                 (strlen(subckt_name) + 1 
                                  + strlen(cur_pb_type->modes[imode].pb_type_children[ipb].name) + 1 
                                  + strlen(my_itoa(jpb)) + 1 + 1));
        sprintf(child_pb_type_prefix, "%s_%s[%d]", subckt_name,
        cur_pb_type->modes[imode].pb_type_children[ipb].name, jpb);
        */
        /* Simplify the prefix! */
        child_pb_type_prefix = (char*)my_malloc(sizeof(char)* 
                                  (strlen(cur_pb_type->modes[mode_index].pb_type_children[ipb].name) + 1 
                                   + strlen(my_itoa(jpb)) + 1 + 1));
        sprintf(child_pb_type_prefix, "%s_%d_",
                cur_pb_type->modes[mode_index].pb_type_children[ipb].name, jpb);
        /* Print inputs, outputs, inouts, clocks
         * NO SRAMs !!! They have already been fixed in the bottom level
         */
        dump_verilog_pb_type_ports(fp, child_pb_type_prefix, 0, &(cur_pb_type->modes[mode_index].pb_type_children[ipb]),FALSE, FALSE);
        /* Print input and output pads */
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_inpads));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_inpads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_input_prefix,
                  inpad_verilog_model->prefix, 
                  stamped_inpad_cnt,
                  stamped_inpad_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_inpads - 1);
          /* update stamped inpad counter */
          stamped_inpad_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_inpads;
        }
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_outpads));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_outpads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_output_prefix,
                  outpad_verilog_model->prefix, 
                  stamped_outpad_cnt,
                  stamped_outpad_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_outpads - 1);
          /* update stamped outpad counter */
          stamped_outpad_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_outpads;
        }
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_iopads));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_iopads) {
          fprintf(fp, ",");
          fprintf(fp, "  %s%s[%d:%d] \n", 
                  gio_inout_prefix,
                  iopad_verilog_model->prefix, 
                  stamped_iopad_cnt,
                  stamped_iopad_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_iopads - 1);
          /* update stamped outpad counter */
          stamped_iopad_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_iopads;
        }
        /* Print configuration ports */
        assert(!(0 > cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_conf_bits));
        if (0 < cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_conf_bits) {
          fprintf(fp, ",");
          fprintf(fp, "  %s_out[%d:%d], \n", 
                  sram_verilog_model->prefix, 
                  stamped_sram_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_conf_bits - 1,
                  stamped_sram_cnt);
          /* inverted output of each configuration bit */
          fprintf(fp, "  %s_outb[%d:%d] \n", 
                  sram_verilog_model->prefix, 
                  stamped_sram_cnt + cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_conf_bits - 1,
                  stamped_sram_cnt);
          /* update stamped sram counter */
          stamped_sram_cnt += cur_pb_type->modes[mode_index].pb_type_children[ipb].physical_mode_num_conf_bits; 
        }
        fprintf(fp, ");\n"); /* Local vdd and gnd*/

        /* Find the pb_type_children mode */
        if (NULL == cur_pb_type->modes[mode_index].pb_type_children[ipb].spice_model) { /* Find the idle_mode_index, if this is not a leaf node  */
          child_mode_index = find_pb_type_idle_mode_index(cur_pb_type->modes[mode_index].pb_type_children[ipb]);
        }
        my_free(child_pb_type_prefix);
      }
    }
    /* Print interconnections, set is_idle as TRUE*/
    dump_verilog_pb_graph_interc(fp, subckt_name, cur_pb_graph_node, NULL, mode_index, 1);
    /* Check each pins of pb_graph_node */ 
    /* Check and update stamped_sram_cnt */
    assert(!(stamped_sram_cnt > (stamped_sram_lsb + num_conf_bits)));
    stamped_sram_cnt = stamped_sram_lsb + num_conf_bits;
    /* End the subckt */
    fprintf(fp, "endmodule\n");
    /* Comment lines */
    fprintf(fp, "//----- END Idle programmable logic block Verilog module %s -----\n\n", subckt_name);
    /* Free subckt name*/
    my_free(subckt_name);
  }

  if (stamped_sram_cnt != sram_verilog_model->cnt) {
  assert(stamped_sram_cnt == sram_verilog_model->cnt);
  }
  assert(stamped_inpad_cnt == inpad_verilog_model->cnt); 
  assert(stamped_outpad_cnt == outpad_verilog_model->cnt);

  return;
}

/* Print the SPICE netlist of a block that has been mapped */
void dump_verilog_block(FILE* fp,
                        char* subckt_name, 
                        int x,
                        int y,
                        int z,
                        t_type_ptr type_descriptor,
                        t_block* mapped_block) {
  t_pb* top_pb = NULL; 
  t_pb_graph_node* top_pb_graph_node = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* check */
  assert(x == mapped_block->x);
  assert(y == mapped_block->y);
  assert(type_descriptor == mapped_block->type);
  assert(NULL != type_descriptor);

  /* Print SPICE netlist according to the hierachy of type descriptor recursively*/
  /* Go for the pb_types*/
  top_pb_graph_node = type_descriptor->pb_graph_head;
  assert(NULL != top_pb_graph_node);
  top_pb = mapped_block->pb; 
  assert(NULL != top_pb);

  /* Recursively find all mode and print netlist*/
  /* IMPORTANT: type_descriptor just say we have a block that in global view, how it connects to global routing arch.
   * Inside the type_descripor, there is a top_pb_graph_node(pb_graph_head), describe the top pb_type defined.
   * The index of such top pb_type is always 0. 
   */
  dump_verilog_pb_graph_node_rec(fp, subckt_name, top_pb, top_pb_graph_node, z);

  return;
}


/* Print an idle logic block
 * Find the idle_mode in arch files,
 * And print the verilog netlist into file
 */
void dump_verilog_idle_block(FILE* fp,
                             char* subckt_name, 
                             int x,
                             int y,
                             int z,
                             t_type_ptr type_descriptor) {
  t_pb_graph_node* top_pb_graph_node = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Ensure we have a valid type_descriptor*/ 
  assert(NULL != type_descriptor);

  /* Go for the pb_types*/
  top_pb_graph_node = type_descriptor->pb_graph_head;
  assert(NULL != top_pb_graph_node);

  /* Recursively find all idle mode and print netlist*/
  dump_verilog_idle_pb_graph_node_rec(fp, subckt_name, top_pb_graph_node, z);

  return;
}

/* Print an physical logic block
 * Find the physical_mode in arch files,
 * And print the verilog netlist into file
 */
void dump_verilog_physical_block(FILE* fp,
                                 char* subckt_name, 
                                 int x,
                                 int y,
                                 int z,
                                 t_type_ptr type_descriptor) {
  t_pb_graph_node* top_pb_graph_node = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Ensure we have a valid type_descriptor*/ 
  assert(NULL != type_descriptor);

  /* Go for the pb_types*/
  top_pb_graph_node = type_descriptor->pb_graph_head;
  assert(NULL != top_pb_graph_node);

  /* Recursively find all idle mode and print netlist*/
  dump_verilog_phy_pb_graph_node_rec(fp, subckt_name, top_pb_graph_node, z);

  return;
}

/* We print all the pins of a type descriptor in the following sequence 
 * TOP, RIGHT, BOTTOM, LEFT
 */
void dump_verilog_grid_pins(FILE* fp,
                      int x,
                      int y,
                      int top_level,
                      boolean dump_port_type,
                      boolean dump_last_comma) {
  int iheight, side, ipin, class_id; 
  int side_pin_index;
  t_type_ptr type_descriptor = grid[x][y].type;
  int capacity = grid[x][y].type->capacity;
  int num_dumped_port = 0;
  int first_dump = 1;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 
  assert(NULL != type_descriptor);
  assert(0 < capacity);

  for (side = 0; side < 4; side++) {
    /* Count the number of pins */
    side_pin_index = 0;
    //for (iz = 0; iz < capacity; iz++) {
      for (iheight = 0; iheight < type_descriptor->height; iheight++) {
        for (ipin = 0; ipin < type_descriptor->num_pins; ipin++) {
          if (1 == type_descriptor->pinloc[iheight][side][ipin]) {
            /* Add comma if needed */
            if (1 == first_dump) {
              first_dump = 0;
            } else { 
              if (TRUE == dump_port_type) {
                fprintf(fp, ",\n");
              } else {
                fprintf(fp, ",\n");
             }
            }
            if (TRUE == dump_port_type) {
              /* Determine this pin is an input or output */
              class_id = type_descriptor->pin_class[ipin];
              switch (type_descriptor->class_inf[class_id].type) {
              case RECEIVER:
                fprintf(fp, "input ");
                break;
              case DRIVER:
                fprintf(fp, "output ");
                break;
              default:
                vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid pin_class_type!\n",
                           __FILE__, __LINE__);
                exit(1);
              }
            }
            /* This pin appear at this side! */
            if (1 == top_level) {
              fprintf(fp, " grid_%d__%d__pin_%d__%d__%d_", x, y,
                      iheight, side, ipin);
            } else {
              fprintf(fp, " %s_height_%d__pin_%d_", 
                      convert_side_index_to_string(side), iheight, ipin);
            }
            /* Update counter */
            num_dumped_port++;
            side_pin_index++;
          }
        }
      }  
    //}
  }

  if ((0 < num_dumped_port)&&(TRUE == dump_last_comma)) {
    if (TRUE == dump_port_type) {
      fprintf(fp, ",\n");
    } else {
      fprintf(fp, ",\n");
    }
  }

  return;
} 

/* Special for I/O grid, we need only part of the ports
 * i.e., grid[0][0..ny] only need the right side ports.
 */
/* We print all the pins of a type descriptor in the following sequence 
 * TOP, RIGHT, BOTTOM, LEFT
 */
void dump_verilog_io_grid_pins(FILE* fp,
                               int x, int y,
                               int top_level,
                               boolean dump_port_type,
                               boolean dump_last_comma) {
  int iheight, side, ipin; 
  int side_pin_index;
  t_type_ptr type_descriptor = grid[x][y].type;
  int capacity = grid[x][y].type->capacity;
  int class_id = -1;
  int num_dumped_port = 0;
  int first_dump = 1;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 
  assert(NULL != type_descriptor);
  assert(0 < capacity);
  /* Make sure this is IO */
  assert(IO_TYPE == type_descriptor);

  /* identify the location of IO grid and 
   * decide which side of ports we need
   */
  side = determine_io_grid_side(x,y);
 
  /* Count the number of pins */
  side_pin_index = 0;
  //for (iz = 0; iz < capacity; iz++) {
    for (iheight = 0; iheight < type_descriptor->height; iheight++) {
      for (ipin = 0; ipin < type_descriptor->num_pins; ipin++) {
        if (1 == type_descriptor->pinloc[iheight][side][ipin]) {
          /* Add comma if needed */
          if (1 == first_dump) {
            first_dump = 0;
          } else { 
            if (TRUE == dump_port_type) {
              fprintf(fp, ",\n");
            } else {
              fprintf(fp, ",\n");
            }
          }
          /* Determine this pin is an input or output */
          if (TRUE == dump_port_type) {
            class_id = type_descriptor->pin_class[ipin];
            switch (type_descriptor->class_inf[class_id].type) {
            case RECEIVER:
              fprintf(fp, "input ");
              break;
            case DRIVER:
              fprintf(fp, "output ");
              break;
            default:
              vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid pin_class_type!\n",
                         __FILE__, __LINE__);
              exit(1);
            }
          }
          /* This pin appear at this side! */
          if (1 == top_level) {
            fprintf(fp, " grid_%d__%d__pin_%d__%d__%d_", x, y,
                    iheight, side, ipin);
          } else {
            fprintf(fp, " %s_height_%d__pin_%d_", 
                    convert_side_index_to_string(side), iheight, ipin);
          }
          /* Update counter */
          num_dumped_port++;
          side_pin_index++;
        }
      }  
    }
  //}
  
  if ((0 < num_dumped_port)&&(TRUE == dump_last_comma)) {
    if (TRUE == dump_port_type) {
      fprintf(fp, ",\n");
    } else {
      fprintf(fp, ",\n");
    }
  }

  return;
} 

char* verilog_get_grid_block_subckt_name(int x, int y, int z,
                                         char* subckt_prefix,
                                         t_block* mapped_block) {
  char* ret = NULL;
  int imode; 
  t_type_ptr type_descriptor = NULL;
  char* formatted_subckt_prefix = format_spice_node_prefix(subckt_prefix);
  int num_idle_mode = 0;

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 

  type_descriptor = grid[x][y].type;
  assert(NULL != type_descriptor);

  if (NULL == mapped_block) {
    /* This a NULL logic block... Find the idle mode*/
    for (imode = 0; imode < type_descriptor->pb_type->num_modes; imode++) {
      if (1 == type_descriptor->pb_type->modes[imode].define_idle_mode) {
        num_idle_mode++;
      }
    } 
    assert(1 == num_idle_mode);
    for (imode = 0; imode < type_descriptor->pb_type->num_modes; imode++) {
      if (1 == type_descriptor->pb_type->modes[imode].define_idle_mode) {
        ret = (char*)my_malloc(sizeof(char)* 
               (strlen(formatted_subckt_prefix) + strlen(type_descriptor->name) + 1
                + strlen(my_itoa(z)) + 7 + strlen(type_descriptor->pb_type->modes[imode].name) + 1 + 1)); 
        sprintf(ret, "%s%s_%d__mode_%s_", formatted_subckt_prefix,
                type_descriptor->name, z, type_descriptor->pb_type->modes[imode].name);
        break;
      }
    } 
  } else {
    /* This is a logic block with specific configurations*/ 
    assert(NULL != mapped_block->pb);
    imode = mapped_block->pb->mode;
    ret = (char*)my_malloc(sizeof(char)* 
           (strlen(formatted_subckt_prefix) + strlen(type_descriptor->name) + 1
            + strlen(my_itoa(z)) + 7 + strlen(type_descriptor->pb_type->modes[imode].name) + 1 + 1)); 
    sprintf(ret, "%s%s_%d__mode_%s_", formatted_subckt_prefix,
            type_descriptor->name, z, type_descriptor->pb_type->modes[imode].name);
  }

  return ret;
}                        

/* Physical mode subckt name */
char* verilog_get_grid_phy_block_subckt_name(int x, int y, int z,
                                             char* subckt_prefix,
                                             t_block* mapped_block) {
  char* ret = NULL;
  int imode; 
  t_type_ptr type_descriptor = NULL;
  char* formatted_subckt_prefix = format_spice_node_prefix(subckt_prefix);
  int num_physical_mode = 0;

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 

  type_descriptor = grid[x][y].type;
  assert(NULL != type_descriptor);

  if (NULL == mapped_block) {
    /* This a NULL logic block... Find the idle mode*/
    for (imode = 0; imode < type_descriptor->pb_type->num_modes; imode++) {
      if (1 == type_descriptor->pb_type->modes[imode].define_physical_mode) {
        num_physical_mode++;
      }
    } 
    assert(1 == num_physical_mode);
    for (imode = 0; imode < type_descriptor->pb_type->num_modes; imode++) {
      if (1 == type_descriptor->pb_type->modes[imode].define_physical_mode) {
        ret = (char*)my_malloc(sizeof(char)* 
               (strlen(formatted_subckt_prefix) + strlen(type_descriptor->name) + 1
                + strlen(my_itoa(z)) + 7 + strlen(type_descriptor->pb_type->modes[imode].name) + 1 + 1)); 
        sprintf(ret, "%s%s_%d__mode_%s_", formatted_subckt_prefix,
                type_descriptor->name, z, type_descriptor->pb_type->modes[imode].name);
        break;
      }
    } 
  } else {
    /* This is a logic block with specific configurations*/ 
    assert(NULL != mapped_block->pb);
    imode = mapped_block->pb->mode;
    ret = (char*)my_malloc(sizeof(char)* 
           (strlen(formatted_subckt_prefix) + strlen(type_descriptor->name) + 1
            + strlen(my_itoa(z)) + 7 + strlen(type_descriptor->pb_type->modes[imode].name) + 1 + 1)); 
    sprintf(ret, "%s%s_%d__mode_%s_", formatted_subckt_prefix,
            type_descriptor->name, z, type_descriptor->pb_type->modes[imode].name);
  }

  return ret;
}                        


/* Print the pins of grid subblocks */
void dump_verilog_grid_block_subckt_pins(FILE* fp,
                                   int z,
                                   t_type_ptr type_descriptor) {
  int iport, ipin, side, dump_pin_cnt;
  int grid_pin_index, pin_height, side_pin_index;
  t_pb_graph_node* top_pb_graph_node = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check */
  assert(NULL != type_descriptor);
  top_pb_graph_node = type_descriptor->pb_graph_head;
  assert(NULL != top_pb_graph_node); 

  dump_pin_cnt = 0;

  for (iport = 0; iport < top_pb_graph_node->num_input_ports; iport++) {
    for (ipin = 0; ipin < top_pb_graph_node->num_input_pins[iport]; ipin++) {
      grid_pin_index = top_pb_graph_node->input_pins[iport][ipin].pin_count_in_cluster 
                     + z * type_descriptor->num_pins / type_descriptor->capacity;
      /* num_pins/capacity = the number of pins that each type_descriptor has.
       * Capacity defines the number of type_descriptors in each grid
       * so the pin index at grid level = pin_index_in_type_descriptor 
       *                                + type_descriptor_index_in_capacity * num_pins_per_type_descriptor
       */
      pin_height = type_descriptor->pin_height[grid_pin_index];
      for (side = 0; side < 4; side++) {
        if (1 == type_descriptor->pinloc[pin_height][side][grid_pin_index]) {
          /* This pin appear at this side! */
          if (0 < dump_pin_cnt) {
            fprintf(fp, ",\n");
          }
          fprintf(fp, "%s_height_%d__pin_%d_ ", 
                  convert_side_index_to_string(side), pin_height, grid_pin_index);
          dump_pin_cnt++;
          side_pin_index++;
        }
      }
    }
  }

  for (iport = 0; iport < top_pb_graph_node->num_output_ports; iport++) {
    for (ipin = 0; ipin < top_pb_graph_node->num_output_pins[iport]; ipin++) {
      grid_pin_index = top_pb_graph_node->output_pins[iport][ipin].pin_count_in_cluster 
                     + z * type_descriptor->num_pins / type_descriptor->capacity;
      /* num_pins/capacity = the number of pins that each type_descriptor has.
       * Capacity defines the number of type_descriptors in each grid
       * so the pin index at grid level = pin_index_in_type_descriptor 
       *                                + type_descriptor_index_in_capacity * num_pins_per_type_descriptor
       */
      pin_height = type_descriptor->pin_height[grid_pin_index];
      for (side = 0; side < 4; side++) {
        if (1 == type_descriptor->pinloc[pin_height][side][grid_pin_index]) {
          /* This pin appear at this side! */
          if (0 < dump_pin_cnt) {
            fprintf(fp, ",\n");
          }
          fprintf(fp, "%s_height_%d__pin_%d_ ", 
                  convert_side_index_to_string(side), pin_height, grid_pin_index);
          dump_pin_cnt++;
          side_pin_index++;
        }
      }
    }
  }

  for (iport = 0; iport < top_pb_graph_node->num_clock_ports; iport++) {
    for (ipin = 0; ipin < top_pb_graph_node->num_clock_pins[iport]; ipin++) {
      grid_pin_index = top_pb_graph_node->clock_pins[iport][ipin].pin_count_in_cluster 
                     + z * type_descriptor->num_pins / type_descriptor->capacity;
      /* num_pins/capacity = the number of pins that each type_descriptor has.
       * Capacity defines the number of type_descriptors in each grid
       * so the pin index at grid level = pin_index_in_type_descriptor 
       *                                + type_descriptor_index_in_capacity * num_pins_per_type_descriptor
       */
      pin_height = type_descriptor->pin_height[grid_pin_index];
      for (side = 0; side < 4; side++) {
        if (1 == type_descriptor->pinloc[pin_height][side][grid_pin_index]) {
          /* This pin appear at this side! */
          if (0 < dump_pin_cnt) {
            fprintf(fp, ",\n");
          }
          fprintf(fp, "%s_height_%d__pin_%d_ ", 
                  convert_side_index_to_string(side), pin_height, grid_pin_index);
          dump_pin_cnt++;
          side_pin_index++;
        }
      }
    }
  }

  return;
}


/* Print the pins of grid subblocks */
void dump_verilog_io_grid_block_subckt_pins(FILE* fp,
                                      int x,
                                      int y,
                                      int z,
                                      t_type_ptr type_descriptor) {
  int iport, ipin, side, dump_pin_cnt;
  int grid_pin_index, pin_height, side_pin_index;
  t_pb_graph_node* top_pb_graph_node = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check */
  assert(NULL != type_descriptor);
  top_pb_graph_node = type_descriptor->pb_graph_head;
  assert(NULL != top_pb_graph_node); 

  /* Make sure this is IO */
  assert(IO_TYPE == type_descriptor);

  /* identify the location of IO grid and 
   * decide which side of ports we need
   */
  if (0 == x) {
    /* Left side */
    assert((0 < y)&&(y < (ny + 1)));
    /* Print Right side ports*/
    side = RIGHT;
  } else if ((nx + 1) == x) {
    /* Right side */
    assert((0 < y)&&(y < (ny + 1)));
    /* Print Left side ports*/
    side = LEFT;
  } else if (0 == y) {
    /* Bottom Side */
    assert((0 < x)&&(x < (nx + 1)));
    /* Print TOP side ports */
    side = TOP;
  } else if ((ny + 1) == y) {
    /* TOP Side */
    assert((0 < x)&&(x < (nx + 1)));
    /* Print BOTTOM side ports */
    side = BOTTOM;
  } else {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid co-ordinators(x=%d, y=%d) for I/O grid!\n",
               __FILE__, __LINE__, x, y);
    exit(1);
  }  

  dump_pin_cnt = 0;

  for (iport = 0; iport < top_pb_graph_node->num_input_ports; iport++) {
    for (ipin = 0; ipin < top_pb_graph_node->num_input_pins[iport]; ipin++) {
      grid_pin_index = top_pb_graph_node->input_pins[iport][ipin].pin_count_in_cluster 
                     + z * type_descriptor->num_pins / type_descriptor->capacity;
      /* num_pins/capacity = the number of pins that each type_descriptor has.
       * Capacity defines the number of type_descriptors in each grid
       * so the pin index at grid level = pin_index_in_type_descriptor 
       *                                + type_descriptor_index_in_capacity * num_pins_per_type_descriptor
       */
      pin_height = type_descriptor->pin_height[grid_pin_index];
      if (1 == type_descriptor->pinloc[pin_height][side][grid_pin_index]) {
        /* This pin appear at this side! */
        if (0 < dump_pin_cnt) {
          fprintf(fp, ",\n");
        }
        fprintf(fp, "%s_height_%d__pin_%d_", 
                convert_side_index_to_string(side), pin_height, grid_pin_index);
        side_pin_index++;
        dump_pin_cnt++;
      }
    }
  }

  for (iport = 0; iport < top_pb_graph_node->num_output_ports; iport++) {
    for (ipin = 0; ipin < top_pb_graph_node->num_output_pins[iport]; ipin++) {
      grid_pin_index = top_pb_graph_node->output_pins[iport][ipin].pin_count_in_cluster 
                     + z * type_descriptor->num_pins / type_descriptor->capacity;
      /* num_pins/capacity = the number of pins that each type_descriptor has.
       * Capacity defines the number of type_descriptors in each grid
       * so the pin index at grid level = pin_index_in_type_descriptor 
       *                                + type_descriptor_index_in_capacity * num_pins_per_type_descriptor
       */
      pin_height = type_descriptor->pin_height[grid_pin_index];
      if (1 == type_descriptor->pinloc[pin_height][side][grid_pin_index]) {
        /* This pin appear at this side! */
        if (0 < dump_pin_cnt) {
          fprintf(fp, ",\n");
        }
        fprintf(fp, "%s_height_%d__pin_%d_", 
                convert_side_index_to_string(side), pin_height, grid_pin_index);
        side_pin_index++;
        dump_pin_cnt++;
      }
    }
  }

  for (iport = 0; iport < top_pb_graph_node->num_clock_ports; iport++) {
    for (ipin = 0; ipin < top_pb_graph_node->num_clock_pins[iport]; ipin++) {
      grid_pin_index = top_pb_graph_node->clock_pins[iport][ipin].pin_count_in_cluster 
                     + z * type_descriptor->num_pins / type_descriptor->capacity;
      /* num_pins/capacity = the number of pins that each type_descriptor has.
       * Capacity defines the number of type_descriptors in each grid
       * so the pin index at grid level = pin_index_in_type_descriptor 
       *                                + type_descriptor_index_in_capacity * num_pins_per_type_descriptor
       */
      pin_height = type_descriptor->pin_height[grid_pin_index];
      if (1 == type_descriptor->pinloc[pin_height][side][grid_pin_index]) {
        /* This pin appear at this side! */
        if (0 < dump_pin_cnt) {
          fprintf(fp, ",\n");
        }
        fprintf(fp, "%s_height_%d__pin_%d_", 
                convert_side_index_to_string(side), pin_height, grid_pin_index);
        side_pin_index++;
        dump_pin_cnt++;
      }
    }
  }

  return;
}

/* Print the SPICE netlist for a grid blocks */
void dump_verilog_grid_blocks(FILE* fp,
                        int ix,
                        int iy,
                        t_arch* arch) {
  int subckt_name_str_len = 0;
  char* subckt_name = NULL;
  t_block* mapped_block = NULL;
  int iz;
  int cur_block_index = 0;
  int capacity; 
  int temp_sram_lsb, temp_sram_msb;
  int temp_inpad_lsb, temp_inpad_msb;
  int temp_outpad_lsb, temp_outpad_msb;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check */
  assert((!(0 > ix))&&(!(ix > (nx + 1)))); 
  assert((!(0 > iy))&&(!(iy > (ny + 1)))); 

  /* Update the grid_index_low for each spice_model */
  update_spice_models_grid_index_low(ix, iy, arch->spice->num_spice_model, arch->spice->spice_models);

  /* generate_grid_subckt, type_descriptor of each grid defines the capacity,
   * for example, each grid may contains more than one top-level pb_types, such as I/O
   */
  if ((NULL == grid[ix][iy].type)||(0 != grid[ix][iy].offset)) {
    /* Update the grid_index_high for each spice_model */
    update_spice_models_grid_index_high(ix, iy, arch->spice->num_spice_model, arch->spice->spice_models);
    return; 
  }
  capacity= grid[ix][iy].type->capacity;
  assert(0 < capacity);

  /* Make the sub-circuit name*/
  /* Name format: grid[<ix>][<iy>]_*/ 
  subckt_name_str_len = 4 + 1 + strlen(my_itoa(ix)) + 2 
                        + strlen(my_itoa(iy)) + 1 + 1 + 1; /* Plus '0' at the end of string*/
  subckt_name = (char*)my_malloc(sizeof(char)*subckt_name_str_len);
  sprintf(subckt_name, "grid_%d__%d__", ix, iy);

  cur_block_index = 0;
  /* check capacity and if this has been mapped */
  for (iz = 0; iz < capacity; iz++) {
    /* Check in all the blocks(clustered logic block), there is a match x,y,z*/
    mapped_block = search_mapped_block(ix, iy, iz); 
    /* Comments: Grid [x][y]*/
    fprintf(fp, "//----- Grid[%d][%d] type_descriptor: %s[%d] -----\n", ix, iy, grid[ix][iy].type->name, iz);
    if (NULL == mapped_block) {
      /* Print a NULL logic block...*/
      dump_verilog_idle_block(fp, subckt_name, ix, iy, iz, grid[ix][iy].type);
    } else {
      if (iz == mapped_block->z) {
        // assert(mapped_block == &(block[grid[ix][iy].blocks[cur_block_index]]));
        cur_block_index++;
      }
      /* Print a logic block with specific configurations*/ 
      dump_verilog_block(fp, subckt_name, ix, iy, iz, grid[ix][iy].type, mapped_block);
    }
    fprintf(fp, "//----- END -----\n\n");
  } 
  assert(cur_block_index == grid[ix][iy].usage);

  /* Update the grid_index_high for each spice_model */
  update_spice_models_grid_index_high(ix, iy, arch->spice->num_spice_model, arch->spice->spice_models);

  /* Print grid[x][y] top-level module */
  fprintf(fp, "//----- Grid[%d][%d], Capactity: %d -----\n", ix, iy, capacity);
  fprintf(fp, "//----- Top Protocol -----\n");
  /* Definition */
  fprintf(fp, "module grid_%d__%d_( \n", ix, iy);
  /* Print global set and reset */
  fprintf(fp, "input wire greset,\n");
  fprintf(fp, "input wire gset,\n");
  /* Pins */
  /* Special Care for I/O grid */
  if (IO_TYPE == grid[ix][iy].type) {
    dump_verilog_io_grid_pins(fp, ix, iy, 0, TRUE, FALSE);
  } else {
    dump_verilog_grid_pins(fp, ix, iy, 0, TRUE, FALSE);
  }
  /* Print Input Pad and Output Pad */
  if (0 < (inpad_verilog_model->grid_index_high[ix][iy] - inpad_verilog_model->grid_index_low[ix][iy])) {
    fprintf(fp, ",\n");
    fprintf(fp, "  input [%d:%d] %s%s ", 
            inpad_verilog_model->grid_index_high[ix][iy] - 1, 
            inpad_verilog_model->grid_index_low[ix][iy],
            gio_input_prefix,
            inpad_verilog_model->prefix); 
  }
  if (0 < (outpad_verilog_model->grid_index_high[ix][iy] - outpad_verilog_model->grid_index_low[ix][iy])) {
    fprintf(fp, ",\n");
    fprintf(fp, "  output [%d:%d] %s%s ", 
            outpad_verilog_model->grid_index_high[ix][iy] - 1, 
            outpad_verilog_model->grid_index_low[ix][iy],
            gio_output_prefix,
            outpad_verilog_model->prefix); 
  }
  /* Print configuration ports */
  if (0 < (sram_verilog_model->grid_index_high[ix][iy] - sram_verilog_model->grid_index_low[ix][iy])) {
    fprintf(fp, ",\n");
    fprintf(fp, "  input [%d:%d] %s_out, \n", 
            sram_verilog_model->grid_index_high[ix][iy] - 1, 
            sram_verilog_model->grid_index_low[ix][iy],
            sram_verilog_model->prefix); 
    /* inverted output of each configuration bit */
    fprintf(fp, "  input [%d:%d] %s_outb \n", 
            sram_verilog_model->grid_index_high[ix][iy] - 1, 
            sram_verilog_model->grid_index_low[ix][iy],
            sram_verilog_model->prefix);
  }
  fprintf(fp, ");\n");

  temp_sram_lsb = sram_verilog_model->grid_index_low[ix][iy];
  temp_sram_msb = sram_verilog_model->grid_index_high[ix][iy] - 1;

  temp_inpad_lsb = inpad_verilog_model->grid_index_low[ix][iy];
  temp_inpad_msb = inpad_verilog_model->grid_index_high[ix][iy] - 1;

  temp_outpad_lsb = outpad_verilog_model->grid_index_low[ix][iy];
  temp_outpad_msb = outpad_verilog_model->grid_index_high[ix][iy] - 1;

  /* Quote all the sub blocks*/
  for (iz = 0; iz < capacity; iz++) {
    /* Check in all the blocks(clustered logic block), there is a match x,y,z*/
    mapped_block = search_mapped_block(ix, iy, iz); 
    /* Local Vdd and Gnd, subckt name*/
    fprintf(fp, "%s ", verilog_get_grid_block_subckt_name(ix, iy, iz, subckt_name, mapped_block));
    fprintf(fp, " grid_%d__%d__%d_ (", ix, iy, iz);
    /* Print global set and reset */
    fprintf(fp, "greset,\n");
    fprintf(fp, "gset,\n");
    /* Print all the pins */
    /* Special Care for I/O grid */
    if (IO_TYPE == grid[ix][iy].type) {
      dump_verilog_io_grid_block_subckt_pins(fp, ix, iy, iz, grid[ix][iy].type);
    } else {
      dump_verilog_grid_block_subckt_pins(fp, iz, grid[ix][iy].type);
    }
    /* Print configuration ports */
    if (NULL == mapped_block) {
      temp_sram_msb = temp_sram_lsb + grid[ix][iy].type->pb_type->default_mode_num_conf_bits;
      temp_inpad_msb = temp_inpad_lsb + grid[ix][iy].type->pb_type->default_mode_num_inpads;
      temp_outpad_msb = temp_outpad_lsb + grid[ix][iy].type->pb_type->default_mode_num_outpads;
    } else {
      temp_sram_msb = temp_sram_lsb + mapped_block->pb->num_conf_bits;
      temp_inpad_msb = temp_inpad_lsb + mapped_block->pb->num_inpads;
      temp_outpad_msb = temp_outpad_lsb + mapped_block->pb->num_outpads;
    }
    /* Print Input Pad and Output Pad */
    if (0 < (temp_inpad_msb - temp_inpad_lsb)) {
      fprintf(fp, "  %s%s[%d:%d], \n", 
              gio_input_prefix,
              inpad_verilog_model->prefix, 
              temp_inpad_msb - 1,
              temp_inpad_lsb); 
    }
    if (0 < (temp_outpad_msb - temp_outpad_lsb)) {
      fprintf(fp, "  %s%s[%d:%d] \n", 
              gio_output_prefix,
              outpad_verilog_model->prefix, 
              temp_outpad_msb - 1,
              temp_outpad_lsb); 
    }
    assert(!(0 > temp_sram_msb - temp_sram_lsb));
    if (0 < temp_sram_msb - temp_sram_lsb) {
      fprintf(fp, ",");
      fprintf(fp, "  %s_out[%d:%d], \n", 
              sram_verilog_model->prefix, 
              temp_sram_msb - 1,
              temp_sram_lsb);
      /* inverted output of each configuration bit */
      fprintf(fp, "  %s_outb[%d:%d] \n", 
              sram_verilog_model->prefix, 
              temp_sram_msb - 1,
              temp_sram_lsb);
    }
    /* Update temp_sram_lsb */
    temp_sram_lsb = temp_sram_msb;
    temp_inpad_lsb = temp_inpad_msb;
    temp_outpad_lsb = temp_outpad_msb;
    fprintf(fp, ");\n");
  }

  fprintf(fp, "endmodule\n");
  fprintf(fp, "//----- END Top Protocol -----\n");
  fprintf(fp, "//----- END Grid[%d][%d], Capactity: %d -----\n\n", ix, iy, capacity);

  assert(temp_sram_msb == sram_verilog_model->grid_index_high[ix][iy]);
  assert(temp_inpad_msb == inpad_verilog_model->grid_index_high[ix][iy]);
  assert(temp_outpad_msb == outpad_verilog_model->grid_index_high[ix][iy]);

  /* Free */
  my_free(subckt_name);

  return;
}

/* Print the SPICE netlist for a I/O grid blocks */
void dump_verilog_physical_grid_blocks(FILE* fp,
                                       int ix,
                                       int iy,
                                       t_arch* arch) {
  int subckt_name_str_len = 0;
  char* subckt_name = NULL;
  t_block* mapped_block = NULL;
  int iz;
  int cur_block_index = 0;
  int capacity; 
  int temp_sram_lsb, temp_sram_msb;
  int temp_inpad_lsb, temp_inpad_msb;
  int temp_outpad_lsb, temp_outpad_msb;
  int temp_iopad_lsb, temp_iopad_msb;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check */
  assert((!(0 > ix))&&(!(ix > (nx + 1)))); 
  assert((!(0 > iy))&&(!(iy > (ny + 1)))); 

  /* Update the grid_index_low for each spice_model */
  update_spice_models_grid_index_low(ix, iy, arch->spice->num_spice_model, arch->spice->spice_models);

  /* generate_grid_subckt, type_descriptor of each grid defines the capacity,
   * for example, each grid may contains more than one top-level pb_types, such as I/O
   */
  if ((NULL == grid[ix][iy].type)||(0 != grid[ix][iy].offset)) {
    /* Update the grid_index_high for each spice_model */
    update_spice_models_grid_index_high(ix, iy, arch->spice->num_spice_model, arch->spice->spice_models);
    return; 
  }
  capacity= grid[ix][iy].type->capacity;
  assert(0 < capacity);

  /* Make the sub-circuit name*/
  /* Name format: grid[<ix>][<iy>]_*/ 
  subckt_name_str_len = 4 + 1 + strlen(my_itoa(ix)) + 2 
                        + strlen(my_itoa(iy)) + 1 + 1 + 1; /* Plus '0' at the end of string*/
  subckt_name = (char*)my_malloc(sizeof(char)*subckt_name_str_len);
  sprintf(subckt_name, "grid_%d__%d__", ix, iy);

  cur_block_index = 0;
  /* check capacity and if this has been mapped */
  for (iz = 0; iz < capacity; iz++) {
    /* Check in all the blocks(clustered logic block), there is a match x,y,z*/
    mapped_block = search_mapped_block(ix, iy, iz); 
    /* Comments: Grid [x][y]*/
    fprintf(fp, "//----- Grid[%d][%d] type_descriptor: %s[%d] -----\n", ix, iy, grid[ix][iy].type->name, iz);
    /* Print a NULL logic block...*/
    dump_verilog_physical_block(fp, subckt_name, ix, iy, iz, grid[ix][iy].type);
    fprintf(fp, "//----- END -----\n\n");
  } 

  /* Update the grid_index_high for each spice_model */
  update_spice_models_grid_index_high(ix, iy, arch->spice->num_spice_model, arch->spice->spice_models);

  /* Print grid[x][y] top-level module */
  fprintf(fp, "//----- Grid[%d][%d], Capactity: %d -----\n", ix, iy, capacity);
  fprintf(fp, "//----- Top Protocol -----\n");
  /* Definition */
  fprintf(fp, "module grid_%d__%d_( \n", ix, iy);
  /* Print global set and reset */
  fprintf(fp, "input wire greset,\n");
  fprintf(fp, "input wire gset,\n");
  /* Pins */
  /* Special Care for I/O grid */
  if (IO_TYPE == grid[ix][iy].type) {
    dump_verilog_io_grid_pins(fp, ix, iy, 0, TRUE, FALSE);
  } else {
    dump_verilog_grid_pins(fp, ix, iy, 0, TRUE, FALSE);
  }
  /* Print Input Pad and Output Pad */
  if ((NULL != inpad_verilog_model)
     &&(0 < (inpad_verilog_model->grid_index_high[ix][iy] - inpad_verilog_model->grid_index_low[ix][iy]))) {
    fprintf(fp, ",\n");
    fprintf(fp, "  input [%d:%d] %s%s ", 
            inpad_verilog_model->grid_index_high[ix][iy] - 1, 
            inpad_verilog_model->grid_index_low[ix][iy],
            gio_input_prefix,
            inpad_verilog_model->prefix); 
  }
  if ((NULL != outpad_verilog_model)
    &&(0 < (outpad_verilog_model->grid_index_high[ix][iy] - outpad_verilog_model->grid_index_low[ix][iy]))) {
    fprintf(fp, ",\n");
    fprintf(fp, "  output [%d:%d] %s%s ", 
            outpad_verilog_model->grid_index_high[ix][iy] - 1, 
            outpad_verilog_model->grid_index_low[ix][iy],
            gio_output_prefix,
            outpad_verilog_model->prefix); 
  }
  /* IO PAD */
  if ((NULL != iopad_verilog_model)
    &&(0 < (iopad_verilog_model->grid_index_high[ix][iy] - iopad_verilog_model->grid_index_low[ix][iy]))) {
    fprintf(fp, ",\n");
    fprintf(fp, "  inout [%d:%d] %s%s ", 
            iopad_verilog_model->grid_index_high[ix][iy] - 1, 
            iopad_verilog_model->grid_index_low[ix][iy],
            gio_inout_prefix,
            iopad_verilog_model->prefix); 
  }
  /* Print configuration ports */
  if (0 < (sram_verilog_model->grid_index_high[ix][iy] - sram_verilog_model->grid_index_low[ix][iy])) {
    fprintf(fp, ",\n");
    fprintf(fp, "  input [%d:%d] %s_out, \n", 
            sram_verilog_model->grid_index_high[ix][iy] - 1, 
            sram_verilog_model->grid_index_low[ix][iy],
            sram_verilog_model->prefix); 
    /* inverted output of each configuration bit */
    fprintf(fp, "  input [%d:%d] %s_outb \n", 
            sram_verilog_model->grid_index_high[ix][iy] - 1, 
            sram_verilog_model->grid_index_low[ix][iy],
            sram_verilog_model->prefix);
  }
  fprintf(fp, ");\n");

  temp_sram_lsb = sram_verilog_model->grid_index_low[ix][iy];
  temp_sram_msb = sram_verilog_model->grid_index_high[ix][iy] - 1;

  temp_inpad_lsb = inpad_verilog_model->grid_index_low[ix][iy];
  temp_inpad_msb = inpad_verilog_model->grid_index_high[ix][iy] - 1;

  temp_outpad_lsb = outpad_verilog_model->grid_index_low[ix][iy];
  temp_outpad_msb = outpad_verilog_model->grid_index_high[ix][iy] - 1;

  temp_iopad_lsb = iopad_verilog_model->grid_index_low[ix][iy];
  temp_iopad_msb = iopad_verilog_model->grid_index_high[ix][iy] - 1;

  /* Quote all the sub blocks*/
  for (iz = 0; iz < capacity; iz++) {
    /* Check in all the blocks(clustered logic block), there is a match x,y,z*/
    mapped_block = search_mapped_block(ix, iy, iz); 
    /* Local Vdd and Gnd, subckt name*/
    fprintf(fp, "%s ", verilog_get_grid_phy_block_subckt_name(ix, iy, iz, subckt_name, NULL));
    fprintf(fp, " grid_%d__%d__%d_ (", ix, iy, iz);
    /* Print global set and reset */
    fprintf(fp, "greset,\n");
    fprintf(fp, "gset,\n");
    /* Print all the pins */
    /* Special Care for I/O grid */
    if (IO_TYPE == grid[ix][iy].type) {
      dump_verilog_io_grid_block_subckt_pins(fp, ix, iy, iz, grid[ix][iy].type);
    } else {
      dump_verilog_grid_block_subckt_pins(fp, iz, grid[ix][iy].type);
    }
    /* Print configuration ports */
    temp_sram_msb = temp_sram_lsb + grid[ix][iy].type->pb_type->physical_mode_num_conf_bits;
    temp_inpad_msb = temp_inpad_lsb + grid[ix][iy].type->pb_type->physical_mode_num_inpads;
    temp_outpad_msb = temp_outpad_lsb + grid[ix][iy].type->pb_type->physical_mode_num_outpads;
    temp_iopad_msb = temp_iopad_lsb + grid[ix][iy].type->pb_type->physical_mode_num_iopads;
    /* Print Input Pad and Output Pad */
    if (0 < (temp_inpad_msb - temp_inpad_lsb)) {
      fprintf(fp, "  %s%s[%d:%d] ", 
              gio_input_prefix,
              inpad_verilog_model->prefix, 
              temp_inpad_msb - 1,
              temp_inpad_lsb); 
    }
    if (0 < (temp_outpad_msb - temp_outpad_lsb)) {
      fprintf(fp, ", //---- INPAD ----\n");
      fprintf(fp, " %s%s[%d:%d] ", 
              gio_output_prefix,
              outpad_verilog_model->prefix, 
              temp_outpad_msb - 1,
              temp_outpad_lsb); 
    }
    if (0 < (temp_iopad_msb - temp_iopad_lsb)) {
      fprintf(fp, ", //---- OUTPAD ----\n");
      fprintf(fp, " %s%s[%d:%d] ", 
              gio_inout_prefix,
              iopad_verilog_model->prefix, 
              temp_iopad_msb - 1,
              temp_iopad_lsb); 
    }
    assert(!(0 > temp_sram_msb - temp_sram_lsb));
    if (0 < temp_sram_msb - temp_sram_lsb) {
      fprintf(fp, ", //---- IOPAD ----\n");
      fprintf(fp, "  %s_out[%d:%d], \n", 
              sram_verilog_model->prefix, 
              temp_sram_msb - 1,
              temp_sram_lsb);
      /* inverted output of each configuration bit */
      fprintf(fp, "  %s_outb[%d:%d] \n", 
              sram_verilog_model->prefix, 
              temp_sram_msb - 1,
              temp_sram_lsb);
      fprintf(fp, "//---- SRAM ----\n");
    }
    /* Update temp_sram_lsb */
    temp_sram_lsb = temp_sram_msb;
    temp_inpad_lsb = temp_inpad_msb;
    temp_outpad_lsb = temp_outpad_msb;
    temp_iopad_lsb = temp_iopad_msb;
    fprintf(fp, ");\n");
  }

  fprintf(fp, "endmodule\n");
  fprintf(fp, "//----- END Top Protocol -----\n");
  fprintf(fp, "//----- END Grid[%d][%d], Capactity: %d -----\n\n", ix, iy, capacity);

  assert(temp_sram_msb == sram_verilog_model->grid_index_high[ix][iy]);
  assert(temp_inpad_msb == inpad_verilog_model->grid_index_high[ix][iy]);
  assert(temp_outpad_msb == outpad_verilog_model->grid_index_high[ix][iy]);
  assert(temp_iopad_msb == iopad_verilog_model->grid_index_high[ix][iy]);

  /* Free */
  my_free(subckt_name);

  return;
}

/* Print all logic blocks SPICE models 
 * Each logic blocks in the grid that allocated for the FPGA
 * will be printed. May have an additional option that only
 * output the used logic blocks 
 */
void dump_verilog_logic_blocks(char* subckt_dir,
                                 t_arch* arch) {
  /* Create file names */
  char* sp_name = my_strcat(subckt_dir, logic_block_verilog_file_name);
  FILE* fp = NULL;
  int ix, iy; 
  
  /* Check the grid*/
  if ((0 == nx)||(0 == ny)) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid grid size (nx=%d, ny=%d)!\n", __FILE__, __LINE__, nx, ny);
    return;    
  }
  vpr_printf(TIO_MESSAGE_INFO,"Grid size of FPGA: nx=%d ny=%d\n", nx + 1, ny + 1);
  assert(NULL != grid);
 
  /* Create a file*/
  fp = fopen(sp_name, "w");
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Failure in create verilog netlist %s",__FILE__, __LINE__, sp_name); 
    exit(1);
  } 
  /* Generate the descriptions*/
  dump_verilog_file_header(fp,"Logic Blocks in FPGA");
 
  /* Print the core logic block one by one
   * Note ix=0 and ix = nx + 1 are IO pads. They surround the core logic blocks
   */
  vpr_printf(TIO_MESSAGE_INFO,"Generating core grids...\n");
  for (ix = 1; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      /* Ensure this is not a io */
      assert(IO_TYPE != grid[ix][iy].type);
      /* Ensure a valid usage */
      assert((0 == grid[ix][iy].usage)||(0 < grid[ix][iy].usage));
      dump_verilog_grid_blocks(fp, ix, iy, arch); 
    }
  }

  vpr_printf(TIO_MESSAGE_INFO,"Generating IO grids...\n");
  /* Print the IO pads */
  /* Left side: x = 0, y = 1 .. ny*/
  ix = 0;
  for (iy = 1; iy < (ny + 1); iy++) {
    /* Ensure this is a io */
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_physical_grid_blocks(fp, ix, iy, arch); 
  }
  /* Right side : x = nx + 1, y = 1 .. ny*/
  ix = nx + 1;
  for (iy = 1; iy < (ny + 1); iy++) {
    /* Ensure this is a io */
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_physical_grid_blocks(fp, ix, iy, arch); 
  }
  /* Bottom  side : x = 1 .. nx + 1, y = 0 */
  iy = 0;
  for (ix = 1; ix < (nx + 1); ix++) {
    /* Ensure this is a io */
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_physical_grid_blocks(fp, ix, iy, arch); 
  }
  /* Top side : x = 1 .. nx + 1, y = nx + 1  */
  iy = ny + 1;
  for (ix = 1; ix < (nx + 1); ix++) {
    /* Ensure this is a io */
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_physical_grid_blocks(fp, ix, iy, arch); 
  }


  /* Close the file */
  fclose(fp);

  /* Free */
  my_free(sp_name);
   
  return; 
}
