#!/bin/sh
# Example of how to run vpr

# Pack, place, and route a heterogeneous FPGA
# Packing uses the AAPack algorithm
#./vpr sample_arch.xml or1200.latren.blif
rm -rf spice_test
./vpr ARCH/K6_N10_I33_frac_UMC180nm_1p8V.xml Circuits/s298_K6_N10_ace.blif --activity_file Circuits/s298_K6_N10_ace.act --nodisp --fpga_spice --spice_dir /home/xitang/tangxifan-eda-tools/branches/vpr7_rram/vpr/spice_test #--fpga_spice_leakage_only
#valgrind 
#rm -rf spice_test_rram
#./vpr ARCH/K6_N10_I33_frac_UMC180nm_1p2V_rram.xml Circuits/s298_K6_N10_ace.blif --activity_file Circuits/s298_K6_N10_ace.act --nodisp --fpga_spice --spice_dir /home/xitang/tangxifan-eda-tools/branches/vpr7_rram/vpr/spice_test_rram --print_spice_mux_testbench --fpga_spice_leakage_only

#rm -rf spice_test_rram_sized
#./vpr ARCH/K6_N10_I33_frac_UMC180nm_1p2V_rram_sizing.xml Circuits/s298_K6_N10_ace.blif --activity_file Circuits/s298_K6_N10_ace.act --nodisp --fpga_spice --spice_dir /home/xitang/tangxifan-eda-tools/branches/vpr7_rram/vpr/spice_test_rram_sized --print_spice_mux_testbench --fpga_spice_leakage_only

