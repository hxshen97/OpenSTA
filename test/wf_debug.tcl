# Waveform debug test - step by step
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog waveform_test.v
link_design tb
puts "Design loaded OK"

# Try calling the SWIG command directly
puts "About to call sta::read_waveform_file..."
sta::read_waveform_file waveform_test.vcd ""
puts "read_waveform_file done"

exit
