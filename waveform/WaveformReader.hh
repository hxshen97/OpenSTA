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

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "SignalWaveformClass.hh"
#include "StaState.hh"

namespace sta {

class Sta;

// Per-bit state tracked for each VCD variable ID.
struct VcdIdState
{
  const Pin *pin;
  char prev_value;
};

// VCD variable ID -> per-bit states (width=1 for scalar, width=N for bus).
typedef std::vector<VcdIdState> VcdIdStates;
typedef std::unordered_map<std::string, VcdIdStates> VcdIdStateMap;

// Reads a VCD file and populates a SignalWaveformStore with all transitions.
class VcdWaveformReader : public VcdReader
{
public:
  VcdWaveformReader(const char *scope,
                    Network *sdc_network,
                    SignalWaveformStore *store,
                    Report *report,
                    Debug *debug);
  ~VcdWaveformReader() override {}

  VcdTime timeMin() const { return time_min_; }
  VcdTime timeMax() const { return time_max_; }
  double timeScale() const { return time_scale_; }

  // VcdReader callbacks.
  void setDate(const std::string &date) override;
  void setComment(const std::string &comment) override;
  void setVersion(const std::string &version) override;
  void setTimeUnit(const std::string &time_unit,
                   double time_unit_scale,
                   double time_scale) override;
  void setTimeMin(VcdTime time) override;
  void setTimeMax(VcdTime time) override;
  void varMinDeltaTime(VcdTime min_delta_time) override;
  bool varIdValid(const std::string &id) override;
  void makeVar(const VcdScope &scope,
               const std::string &name,
               VcdVarType type,
               size_t width,
               const std::string &id) override;
  void varAppendValue(const std::string &id,
                      VcdTime time,
                      char value) override;
  void varAppendBusValue(const std::string &id,
                         VcdTime time,
                         const std::string &bus_value) override;

private:
  void addVarPin(const std::string &pin_name,
                 const std::string &id,
                 size_t width,
                 size_t bit_idx);

  const char *scope_;
  Network *sdc_network_;
  SignalWaveformStore *store_;
  Report *report_;
  Debug *debug_;

  double time_scale_;
  VcdTime time_min_;
  VcdTime time_max_;
  VcdIdStateMap id_state_map_;
};

// Entry point: read VCD waveform file into Sta's waveform store.
void
readWaveformFile(const char *filename,
                 const char *scope,
                 Sta *sta);

} // namespace sta
