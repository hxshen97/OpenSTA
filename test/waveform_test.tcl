# Waveform feature test
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog waveform_test.v
link_design tb

# Test 1: read_waveform - load VCD file
read_waveform waveform_test.vcd

# Test 2: report_waveform_summary
report_waveform_summary

# Test 3: report_waveform for clk pin
report_waveform -pin clk

# Test 4: report_waveform for a pin
report_waveform -pin a

# Test 5: report_waveform for b pin
report_waveform -pin b

# Test 6: report_waveform with time range
report_waveform -pin clk -from 0 -to 30

# Test 7: print_waveform
print_waveform -pin clk

# Test 8: get_waveform_value
set val [get_waveform_value -pin clk -time 15]
puts "clk at t=15: $val"

set val [get_waveform_value -pin a -time 5]
puts "a at t=5: $val"

set val [get_waveform_value -pin a -time 25]
puts "a at t=25: $val"

# Test 9: report_waveform for non-existent pin (should warn)
report_waveform -pin nonexist

# Test 10: remove_waveform
remove_waveform

# Test 11: summary after remove (should show empty)
report_waveform_summary

exit
