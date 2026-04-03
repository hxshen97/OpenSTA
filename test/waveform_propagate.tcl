# Waveform propagation test using reg1_asap7 design
# Tests AIG boolean evaluation through BUF and AND2 gates
# Design: in1→r1(DFF)→r1/Q→u2(AND2.A)
#         in2→r2(DFF)→r2/Q→u1(BUF)→u1/Y→u2(AND2.B)
#         u2/Y→r3(DFF)→out

read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top

create_clock -name clk1 -period 1000 {clk1 clk2 clk3}
set_input_delay -clock clk1 0 {in1 in2}
report_checks

read_waveform reg1_asap7.vcd

propagate_waveform

report_waveform_summary
