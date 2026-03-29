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

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "SignalWaveformClass.hh"
#include "sta/Network.hh"
#include "sta/Debug.hh"

namespace sta {

using std::min;
using std::max;

////////////////////////////////////////////////////////////////

SignalWaveform::SignalWaveform(const Pin *pin) :
  pin_(pin),
  initial_value_('\0')
{
}

SignalWaveform::SignalWaveform(const SignalWaveform& other) :
  pin_(other.pin_),
  initial_value_(other.initial_value_),
  transitions_(other.transitions_)
{
}

void
SignalWaveform::setInitialValue(char value)
{
  initial_value_ = value;
}

void
SignalWaveform::addTransition(VcdTime time,
                           char from_value,
                           char to_value,
                           const Pin *cause_pin,
                           VcdTime cause_time)
{
  transitions_.emplace_back(time, from_value, to_value,
                            cause_pin, cause_time);
  // Keep transitions sorted by time.
  if (transitions_.size() > 1) {
    size_t idx = transitions_.size() - 1;
    while (idx > 0 && transitions_[idx].time_ < transitions_[idx - 1].time_) {
      std::swap(transitions_[idx], transitions_[idx - 1]);
      idx--;
    }
  }
}

const std::vector<SignalWaveformTransition>&
SignalWaveform::allTransitions() const
{
  return transitions_;
}

char
SignalWaveform::getValueAt(VcdTime time) const
{
  if (transitions_.empty())
    return initial_value_;

  // If time is before first transition, return initial value
  if (time < transitions_[0].time_)
    return initial_value_;

  // Binary search for the transition at or before time
  size_t low = 0;
  size_t high = transitions_.size();

  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (transitions_[mid].time_ < time)
      low = mid + 1;
    else
      high = mid;
  }

  // Return the value at the found position
  // low is now the index of first transition with time >= target time
  if (low == transitions_.size())
    return transitions_[low - 1].toValue();
  else if (transitions_[low].time_ == time)
    return transitions_[low].toValue();
  else
    return transitions_[low].fromValue();
}

std::vector<SignalWaveformTransition>
SignalWaveform::getTransitions(VcdTime start, VcdTime end) const
{
  std::vector<SignalWaveformTransition> result;

  // Validate time range
  if (start > end)
    return result;

  // Binary search for first transition >= start
  size_t low = 0;
  size_t high = transitions_.size();
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (transitions_[mid].time_ < start)
      low = mid + 1;
    else
      high = mid;
  }

  // Collect transitions until end time
  for (size_t i = low; i < transitions_.size(); ++i) {
    if (transitions_[i].time_ > end)
      break;
    result.push_back(transitions_[i]);
  }

  return result;
}

const Pin*
SignalWaveform::pin() const
{
  return pin_;
}

VcdTime
SignalWaveform::firstTransitionTime() const
{
  if (transitions_.empty())
    return std::numeric_limits<VcdTime>::max();
  return transitions_[0].time_;
}

VcdTime
SignalWaveform::lastTransitionTime() const
{
  if (transitions_.empty())
    return std::numeric_limits<VcdTime>::min();
  return transitions_.back().time_;
}

size_t
SignalWaveform::transitionCount() const
{
  return transitions_.size();
}

////////////////////////////////////////////////////////////////

SignalWaveformStore::SignalWaveformStore() :
  start_time_(std::numeric_limits<VcdTime>::max()),
  end_time_(std::numeric_limits<VcdTime>::min())
{
}

SignalWaveformStore::~SignalWaveformStore()
{
  clear();
}

void
SignalWaveformStore::addSignalWaveform(const Pin *pin,
                                     const SignalWaveform &waveform)
{
  auto result = waveforms_.emplace(pin, waveform);
  if (!result.second) {
    // Key already exists, update the value
    result.first->second = waveform;
  }
}

const SignalWaveform*
SignalWaveformStore::getSignalWaveform(const Pin *pin) const
{
  const auto &itr = waveforms_.find(pin);
  if (itr != waveforms_.end())
    return &itr->second;
  return nullptr;
}

SignalWaveform*
SignalWaveformStore::findOrCreateSignalWaveform(const Pin *pin)
{
  auto itr = waveforms_.find(pin);
  if (itr != waveforms_.end())
    return &itr->second;
  // Create new waveform for this pin.
  auto result = waveforms_.emplace(pin, SignalWaveform(pin));
  return &result.first->second;
}

std::vector<const Pin*>
SignalWaveformStore::getSignalWaveformPins() const
{
  std::vector<const Pin*> pins;
  pins.reserve(waveforms_.size());

  for (const auto &entry : waveforms_)
    pins.push_back(entry.first);

  return pins;
}

VcdTime
SignalWaveformStore::startTime() const
{
  return start_time_;
}

VcdTime
SignalWaveformStore::endTime() const
{
  return end_time_;
}

void
SignalWaveformStore::setStartTime(VcdTime time)
{
  start_time_ = time;
}

void
SignalWaveformStore::setEndTime(VcdTime time)
{
  end_time_ = time;
}

void
SignalWaveformStore::clear()
{
  waveforms_.clear();
  start_time_ = std::numeric_limits<VcdTime>::max();
  end_time_ = std::numeric_limits<VcdTime>::min();
}

} // namespace sta
