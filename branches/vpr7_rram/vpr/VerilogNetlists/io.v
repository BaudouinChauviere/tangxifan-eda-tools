//------ Module: iopad -----//
//------ Verilog file: io.v -----//
//------ Author: Xifan TANG -----//
module iopad(
inout pad, // bi-directional pad
output din, // Data input
input dout, // Data output
input direction // enable signal to control direction of iopad
);
  //----- when EN enabled, the direction is input
  assign din = direction ? pad : 1'bz;
  assign dout = direction ? 1'bz : pad;
endmodule