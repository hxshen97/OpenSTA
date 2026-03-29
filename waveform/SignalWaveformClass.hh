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

#include <vector>
#include <unordered_map>
#include "../power/VcdParse.hh"

namespace sta {

class Pin;

// Single transition event representing a signal value change.
struct SignalWaveformTransition
{
  VcdTime time_;
  char from_value_;
  char to_value_;
  const Pin *cause_pin_;
  VcdTime cause_time_;

  SignalWaveformTransition(VcdTime time, char from_value, char to_value,
                       const Pin *cause_pin = nullptr, VcdTime cause_time = 0)
      : time_(time), from_value_(from_value), to_value_(to_value),
        cause_pin_(cause_pin), cause_time_(cause_time) {}

  VcdTime time() const { return time_; }
  char fromValue() const { return from_value_; }
  char toValue() const { return to_value_; }
  const Pin* causePin() const { return cause_pin_; }
  VcdTime causeTime() const { return cause_time_; }
};

// SignalWaveform for a single signal pin.
class SignalWaveform
{
public:
  explicit SignalWaveform(const Pin *pin);
  SignalWaveform(const SignalWaveform& other);  // Copy constructor
  void addTransition(VcdTime time,
                    char from_value,
                    char to_value,
                    const Pin *cause_pin = nullptr,
                    VcdTime cause_time = 0);
  void setInitialValue(char value);

  char getValueAt(VcdTime time) const;
  std::vector<SignalWaveformTransition> getTransitions(VcdTime start,
                                                  VcdTime end) const;
  const std::vector<SignalWaveformTransition>& allTransitions() const;

  const Pin *pin() const;
  VcdTime firstTransitionTime() const;
  VcdTime lastTransitionTime() const;
  size_t transitionCount() const;

private:
  const Pin *pin_;
  char initial_value_;
  std::vector<SignalWaveformTransition> transitions_;
};

// Manager for storing and querying signal waveforms.
class SignalWaveformStore
{
public:
  SignalWaveformStore();
  ~SignalWaveformStore();

  void addSignalWaveform(const Pin *pin, const SignalWaveform &waveform);
  const SignalWaveform* getSignalWaveform(const Pin *pin) const;
  SignalWaveform* findOrCreateSignalWaveform(const Pin *pin);
  std::vector<const Pin*> getSignalWaveformPins() const;

  VcdTime startTime() const;
  VcdTime endTime() const;
  void setStartTime(VcdTime time);
  void setEndTime(VcdTime time);
  void clear();

private:
  std::unordered_map<const Pin*, SignalWaveform> waveforms_;
  VcdTime start_time_;
  VcdTime end_time_;
};

} // namespace sta
