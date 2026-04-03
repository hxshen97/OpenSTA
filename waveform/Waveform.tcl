# OpenSTA, Static Timing Analyzer
# Copyright (c) 2025, Parallax Software, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

################################################################
#
# Waveform commands.
#
################################################################

namespace eval sta {

define_cmd_args "read_waveform" { [-scope scope] filename }

proc read_waveform { args } {
  parse_key_args "read_waveform" args \
    keys {-scope} flags {}

  check_argc_eq1 "read_waveform" $args
  set filename [file nativename [lindex $args 0]]
  set scope ""
  if { [info exists keys(-scope)] } {
    set scope $keys(-scope)
  }
  read_waveform_file $filename $scope
}

################################################################

define_cmd_args "report_waveform" { [-pin pin_name] [-from time] [-to time] }

proc report_waveform { args } {
  parse_key_args "report_waveform" args \
    keys {-pin -from -to} flags {}

  check_argc_eq0 "report_waveform" $args

  if { ![info exists keys(-pin)] } {
    sta_error 1462 "report_waveform requires -pin argument."
  }
  set pin_name $keys(-pin)
  set from_time 0
  set to_time 0
  if { [info exists keys(-from)] } {
    set from_time $keys(-from)
  }
  if { [info exists keys(-to)] } {
    set to_time $keys(-to)
  }
  report_waveform_transitions $pin_name $from_time $to_time
}

################################################################

define_cmd_args "report_waveform_summary" {}

proc report_waveform_summary { args } {
  check_argc_eq0 "report_waveform_summary" $args
  report_waveform_summary_cmd
}

################################################################

define_cmd_args "get_waveform_value" { [-pin pin_name] [-time time] }

proc get_waveform_value { args } {
  parse_key_args "get_waveform_value" args \
    keys {-pin -time} flags {}

  check_argc_eq0 "get_waveform_value" $args

  if { ![info exists keys(-pin)] } {
    sta_error 1463 "get_waveform_value requires -pin argument."
  }
  if { ![info exists keys(-time)] } {
    sta_error 1464 "get_waveform_value requires -time argument."
  }
  set pin_name $keys(-pin)
  set time $keys(-time)
  return [get_waveform_value_cmd $pin_name $time]
}

################################################################

define_cmd_args "print_waveform" { [-pin pin_name] [-from time] [-to time] }

proc print_waveform { args } {
  parse_key_args "print_waveform" args \
    keys {-pin -from -to} flags {}

  check_argc_eq0 "print_waveform" $args

  if { ![info exists keys(-pin)] } {
    sta_error 1465 "print_waveform requires -pin argument."
  }
  set pin_name $keys(-pin)
  set from_time 0
  set to_time 0
  if { [info exists keys(-from)] } {
    set from_time $keys(-from)
  }
  if { [info exists keys(-to)] } {
    set to_time $keys(-to)
  }
  print_waveform_cmd $pin_name $from_time $to_time
}

################################################################

define_cmd_args "remove_waveform" {}

proc remove_waveform { args } {
  check_argc_eq0 "remove_waveform" $args
  sta::remove_waveform_data
}

################################################################

define_cmd_args "propagate_waveform" { [-min_max min_max] }

proc propagate_waveform { args } {
  parse_key_args "propagate_waveform" args \
    keys {-min_max} flags {}

  check_argc_eq0 "propagate_waveform" $args

  set min_max "max"
  if { [info exists keys(-min_max)] } {
    set min_max $keys(-min_max)
  }
  propagate_waveform_cmd $min_max
}

} ; # namespace eval sta
