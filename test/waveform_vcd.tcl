# Waveform VCD read and query test
read_liberty asap7_small.lib.gz
read_verilog waveform_test.v
link_design tb

read_waveform waveform_test.vcd

report_waveform_summary

report_waveform -pin clk
report_waveform -pin a
report_waveform -pin b

report_waveform -pin clk -from 0 -to 30

print_waveform -pin clk

set val [get_waveform_value -pin clk -time 15]
puts "clk@15=$val"
set val [get_waveform_value -pin a -time 5]
puts "a@5=$val"
set val [get_waveform_value -pin a -time 25]
puts "a@25=$val"

report_waveform -pin nonexist

remove_waveform
report_waveform_summary
