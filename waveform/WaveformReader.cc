// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

#include "WaveformReader.hh"

#include <inttypes.h>
#include <string>
#include <vector>

#include "power/VcdParse.hh"
#include "Sta.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "ParseBus.hh"
#include "VerilogNamespace.hh"
#include "Stats.hh"
#include "Report.hh"

namespace sta {

using std::string;
using std::to_string;
using std::vector;

////////////////////////////////////////////////////////////////

VcdWaveformReader::VcdWaveformReader(const char *scope,
                                     Network *sdc_network,
                                     SignalWaveformStore *store,
                                     Report *report,
                                     Debug *debug) :
  scope_(scope),
  sdc_network_(sdc_network),
  store_(store),
  report_(report),
  debug_(debug),
  time_scale_(1.0),
  time_min_(0),
  time_max_(0)
{
}

void
VcdWaveformReader::setDate(const string &)
{
}

void
VcdWaveformReader::setComment(const string &)
{
}

void
VcdWaveformReader::setVersion(const string &)
{
}

void
VcdWaveformReader::setTimeUnit(const string &,
                               double time_unit_scale,
                               double time_scale)
{
  time_scale_ = time_scale * time_unit_scale;
}

void
VcdWaveformReader::setTimeMin(VcdTime time)
{
  time_min_ = time;
}

void
VcdWaveformReader::setTimeMax(VcdTime time)
{
  time_max_ = time;
}

void
VcdWaveformReader::varMinDeltaTime(VcdTime)
{
}

bool
VcdWaveformReader::varIdValid(const string &)
{
  return true;
}

void
VcdWaveformReader::makeVar(const VcdScope &scope,
                           const string &name,
                           VcdVarType type,
                           size_t width,
                           const string &id)
{
  if (type == VcdVarType::wire || type == VcdVarType::reg) {
    string path_name;
    bool first = true;
    for (const string &context : scope) {
      if (!first)
        path_name += '/';
      path_name += context;
      first = false;
    }
    size_t scope_length = strlen(scope_);
    if (scope_length == 0
        || path_name.substr(0, scope_length) == scope_) {
      path_name += '/';
      path_name += name;
      string var_scoped;
      if (scope_length == 0) {
        // No scope filter: use only the variable name.
        var_scoped = name;
      }
      else
        var_scoped = path_name.substr(scope_length + 1);
      if (width == 1) {
        string pin_name = netVerilogToSta(&var_scoped);
        addVarPin(pin_name, id, width, 0);
      }
      else {
        bool is_bus, is_range, subscript_wild;
        string bus_name;
        int from, to;
        parseBusName(var_scoped.c_str(), '[', ']', '\\',
                     is_bus, is_range, bus_name, from, to, subscript_wild);
        if (is_bus) {
          string sta_bus_name = netVerilogToSta(&bus_name);
          int bit_idx = 0;
          if (to < from) {
            for (int bus_bit = to; bus_bit <= from; bus_bit++) {
              string pin_name = sta_bus_name;
              pin_name += '[';
              pin_name += to_string(bus_bit);
              pin_name += ']';
              addVarPin(pin_name, id, width, bit_idx);
              bit_idx++;
            }
          }
          else {
            for (int bus_bit = to; bus_bit >= from; bus_bit--) {
              string pin_name = sta_bus_name;
              pin_name += '[';
              pin_name += to_string(bus_bit);
              pin_name += ']';
              addVarPin(pin_name, id, width, bit_idx);
              bit_idx++;
            }
          }
        }
        else
          report_->warn(1451, "problem parsing bus %s.", var_scoped.c_str());
      }
    }
  }
}

void
VcdWaveformReader::addVarPin(const string &pin_name,
                             const string &id,
                             size_t width,
                             size_t bit_idx)
{
  const Pin *pin = sdc_network_->findPin(pin_name.c_str());
  LibertyPort *liberty_port = pin ? sdc_network_->libertyPort(pin) : nullptr;
  if (pin
      && !sdc_network_->isHierarchical(pin)
      && !sdc_network_->direction(pin)->isInternal()
      && !sdc_network_->direction(pin)->isPowerGround()
      && !(liberty_port && liberty_port->isPwrGnd())) {
    VcdIdStates &states = id_state_map_[id];
    states.resize(width);
    states[bit_idx].pin = pin;
    states[bit_idx].prev_value = '\0';
  }
}

void
VcdWaveformReader::varAppendValue(const string &id,
                                  VcdTime time,
                                  char value)
{
  const auto &itr = id_state_map_.find(id);
  if (itr != id_state_map_.end()) {
    VcdIdStates &states = itr->second;
    for (size_t bit_idx = 0; bit_idx < states.size(); bit_idx++) {
      VcdIdState &state = states[bit_idx];
      if (state.pin) {
        char prev = state.prev_value;
        state.prev_value = value;
        if (prev != '\0' && prev != value) {
          SignalWaveform *wf = store_->findOrCreateSignalWaveform(state.pin);
          wf->addTransition(time, prev, value);
        }
        else if (prev == '\0') {
          SignalWaveform *wf = store_->findOrCreateSignalWaveform(state.pin);
          wf->setInitialValue(value);
        }
      }
    }
  }
}

void
VcdWaveformReader::varAppendBusValue(const string &id,
                                     VcdTime time,
                                     const string &bus_value)
{
  const auto &itr = id_state_map_.find(id);
  if (itr != id_state_map_.end()) {
    VcdIdStates &states = itr->second;
    for (size_t bit_idx = 0; bit_idx < states.size(); bit_idx++) {
      VcdIdState &state = states[bit_idx];
      if (state.pin) {
        char bit_value;
        if (bus_value.size() == 1)
          bit_value = bus_value[0];
        else if (bit_idx < bus_value.size())
          bit_value = bus_value[bit_idx];
        else
          bit_value = '0';

        char prev = state.prev_value;
        state.prev_value = bit_value;
        if (prev != '\0' && prev != bit_value) {
          SignalWaveform *wf = store_->findOrCreateSignalWaveform(state.pin);
          wf->addTransition(time, prev, bit_value);
        }
        else if (prev == '\0') {
          SignalWaveform *wf = store_->findOrCreateSignalWaveform(state.pin);
          wf->setInitialValue(bit_value);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
readWaveformFile(const char *filename,
                 const char *scope,
                 Sta *sta)
{
  SignalWaveformStore *store = sta->waveformStore();
  store->clear();

  VcdWaveformReader reader(scope, sta->sdcNetwork(), store,
                           sta->report(), sta->debug());
  VcdParse vcd_parse(sta->report(), sta->debug());

  Stats stats(sta->debug(), sta->report());
  vcd_parse.read(filename, &reader);
  stats.report("Read waveform VCD");

  store->setStartTime(reader.timeMin());
  store->setEndTime(reader.timeMax());

  sta->report()->reportLine("Loaded %zu pin waveforms.",
                             store->getSignalWaveformPins().size());
}

} // namespace sta
