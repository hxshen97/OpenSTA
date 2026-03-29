# VCD Waveform Parsing Design

## Overview

Add VCD waveform parsing to OpenSTA that reads raw VCD files and stores all signal transitions in memory. Provide both C++ API and TCL command interface for querying waveform data.

## Architecture

```
VCD File
  |
  v
VcdParse (existing, power/VcdParse.cc)
  | callbacks
  v
VcdWaveformReader (new, waveform/WaveformReader.cc)
  | extends VcdReader abstract interface
  | implements makeVar / varAppendValue / varAppendBusValue
  v
SignalWaveformStore (existing, waveform/SignalWaveformClass.hh)
  | Pin* -> SignalWaveform mapping
  v
+----------------+----------------+
|  C++ API       |  TCL Interface |
| (headers)      | (SWIG .i)      |
+----------------+----------------+
```

Key decisions:
- Reuse `VcdParse` + `VcdReader` callback architecture (same pattern as `VcdCountReader` in `power/VcdReader.cc`)
- `SignalWaveformStore` is a member of `Sta`, accessed via `StaState`
- `VcdWaveformReader` is a one-shot reader, not a persistent `Sta` component

## Requirements

- **Scope filtering**: Only load signals under specified VCD scope (like existing `read_vcd_file`)
- **Bus signal support**: Multi-bit buses split per-bit, each bit has its own `SignalWaveform`
- **Scale**: Medium scale (<1M transitions), correctness over performance

## Data Structures

### SignalWaveformTransition (existing, unchanged)
- `time_`, `from_value_`, `to_value_`, `cause_pin_`, `cause_time_`

### SignalWaveform (existing, unchanged)
- `pin_`, `initial_value_`, `transitions_` (sorted by time)
- Methods: `getValueAt()`, `getTransitions()`, `allTransitions()`, `transitionCount()`

### SignalWaveformStore (existing, minor enhancement)
- `Pin* -> SignalWaveform` map, `start_time_`, `end_time_`
- New: `getTransitionCount()` returns total transitions across all signals

### Bus handling
Each bus bit maps to an independent `Pin*` with its own `SignalWaveform`. Matches `VcdCountReader` bus processing pattern (VcdReader.cc:250-275).

## VcdWaveformReader Implementation

Class members:
- `scope_` - scope filter string
- `network_` - for `findPin` lookups
- `store_` - target `SignalWaveformStore` to populate
- `time_min_`, `time_max_` - time range tracking
- `time_scale_` - VCD time unit scale
- `id_map_` - maps VCD ID to `(Pin*, width, prev_value)` for tracking state

VcdReader callback implementations:
- `makeVar()` - scope filtering + pin lookup via `network_->findPin()` + build ID map
- `varAppendValue()` - compare with prev_value, if changed call `SignalWaveform::addTransition()`
- `varAppendBusValue()` - per-bit independent transition recording
- `setDate/setComment/setVersion` - no-op
- `setTimeUnit/setTimeMin/setTimeMax` - record time info

Pin lookup logic (reused from VcdCountReader):
1. Build path from scope + var name
2. Strip scope prefix
3. Convert Verilog naming to STA naming via `netVerilogToSta()`
4. For buses, parse bus name and iterate bits
5. `findPin()` in `sdc_network_`, skip hierarchical/internal/power-ground pins

Entry function:
```cpp
void readWaveformFile(const char *filename, const char *scope, Sta *sta);
// 1. Create VcdWaveformReader(scope, network, store)
// 2. Create VcdParse(report, debug)
// 3. parse.read(filename, &reader)
// 4. Store owned by Sta
```

## Sta Integration

- Add `SignalWaveformStore *waveform_store_` to `StaState`
- Add accessor `signalWaveformStore()` / `setSignalWaveformStore()`
- Create in `Sta::makeComponents()`, destroy in `Sta::~Sta()`

## TCL Commands

| Command | Purpose |
|---------|---------|
| `read_waveform_file -file <path> [-scope <scope>]` | Load VCD, populate waveform data |
| `report_waveform -pin <pin_name> [-from <time>] [-to <time>]` | Report transitions for pin |
| `report_waveform_summary` | Summary statistics for all signals |
| `get_waveform_value -pin <pin_name> -time <time>` | Query signal value at time |
| `print_waveform -pin <pin_name> [-from <time>] [-to <time>]` | ASCII waveform display |
| `remove_waveform` | Clear loaded waveform data |

## Error Handling

- File not found / unreadable: reuse `VcdParse::FileNotReadable` exception
- Invalid VCD format: reuse `VcdParse` error reporting (line number + error code)
- Pin not found in network: skip signal with warning, do not abort parsing
- Multiple `read_waveform_file` calls: clear previous data before loading new

## File Changes

| File | Action |
|------|--------|
| `waveform/SignalWaveformClass.hh` | Keep (minor: simplify cause_pin_/cause_time_) |
| `waveform/SignalWaveform.cc` | Keep as-is |
| `waveform/WaveformReader.hh` | Rewrite (extend VcdReader) |
| `waveform/WaveformReader.cc` | Rewrite (full VCD parsing via VcdParse) |
| `waveform/Waveform.i` | Rewrite (full TCL commands) |
| `include/sta/StaState.hh` | Modify (add waveform_store_) |
| `CMakeLists.txt` | Modify (add waveform sources) |

## Testing

- Unit test: create simple VCD file, verify parsed transitions match expected
- Coverage: single-bit signals, bus signals, scope filtering, time range queries
- Location: `test/` directory, TCL scripts following existing regression pattern
