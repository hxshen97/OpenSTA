# test/waveform_propagation_test.tcl
# Smoke test for waveform propagation.
# Usage: sta -e "source test/waveform_propagation_test.tcl"
# Replace paths with actual test files.

# read_liberty <path_to_lib>
# read_verilog <path_to_netlist>
# link_design <top>
# create_clock clk -period 10
# update_timing
# read_waveform <path_to_vcd>
# propagate_waveform
# report_waveform_summary
