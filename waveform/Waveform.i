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

%{
#include "Sta.hh"
#include "waveform/SignalWaveformClass.hh"
#include "power/VcdParse.hh"
#include "waveform/WaveformReader.hh"

using namespace sta;

%}

typedef long long VcdTime;

%inline %{

void
read_waveform_file(const char *filename,
                   const char *scope)
{
  Sta *sta = Sta::sta();
  sta->ensureLibLinked();
  readWaveformFile(filename, scope, sta);
}

void
remove_waveform_data()
{
  Sta *sta = Sta::sta();
  SignalWaveformStore *store = sta->waveformStore();
  store->clear();
}

void
report_waveform_transitions(const char *pin_name,
                            VcdTime from_time,
                            VcdTime to_time)
{
  Sta *sta = Sta::sta();
  Network *network = sta->sdcNetwork();
  const Pin *pin = network->findPin(pin_name);
  if (!pin) {
    sta->report()->warn(1460, "pin %s not found.", pin_name);
    return;
  }
  SignalWaveformStore *store = sta->waveformStore();
  const SignalWaveform *wf = store->getSignalWaveform(pin);
  if (!wf) {
    sta->report()->warn(1461, "no waveform data for pin %s.", pin_name);
    return;
  }
  Report *report = sta->report();
  std::vector<SignalWaveformTransition> transitions;
  if (from_time == 0 && to_time == 0)
    transitions = wf->allTransitions();
  else
    transitions = wf->getTransitions(from_time, to_time);

  report->reportLine("Pin %s: %zu transitions",
                     network->pathName(pin), transitions.size());
  for (const auto &t : transitions) {
    report->reportLine("  time=%lld %c->%c",
                       (long long)t.time_, t.from_value_, t.to_value_);
  }
}

void
report_waveform_summary_cmd()
{
  Sta *sta = Sta::sta();
  SignalWaveformStore *store = sta->waveformStore();
  Report *report = sta->report();
  Network *network = sta->sdcNetwork();

  report->reportLine("Waveform Summary:");
  report->reportLine("  Time range: %lld - %lld",
                     (long long)store->startTime(), (long long)store->endTime());
  auto pins = store->getSignalWaveformPins();
  report->reportLine("  Pins: %zu", pins.size());
  size_t total_transitions = 0;
  for (const Pin *pin : pins) {
    const SignalWaveform *wf = store->getSignalWaveform(pin);
    if (wf) {
      size_t count = wf->transitionCount();
      total_transitions += count;
      report->reportLine("    %-40s %zu transitions",
                         network->pathName(pin), count);
    }
  }
  report->reportLine("  Total transitions: %zu", total_transitions);
}

void
print_waveform_cmd(const char *pin_name,
                   VcdTime from_time,
                   VcdTime to_time)
{
  Sta *sta = Sta::sta();
  Network *network = sta->sdcNetwork();
  const Pin *pin = network->findPin(pin_name);
  if (!pin) {
    sta->report()->warn(1460, "pin %s not found.", pin_name);
    return;
  }
  SignalWaveformStore *store = sta->waveformStore();
  const SignalWaveform *wf = store->getSignalWaveform(pin);
  if (!wf) {
    sta->report()->warn(1461, "no waveform data for pin %s.", pin_name);
    return;
  }

  Report *report = sta->report();
  std::vector<SignalWaveformTransition> transitions;
  if (from_time == 0 && to_time == 0)
    transitions = wf->allTransitions();
  else
    transitions = wf->getTransitions(from_time, to_time);

  report->reportLine("Waveform for %s:", network->pathName(pin));
  for (const auto &t : transitions) {
    report->reportLine("  %lld: %c -> %c",
                       (long long)t.time_, t.from_value_, t.to_value_);
  }
}

const char *
get_waveform_value_cmd(const char *pin_name,
                       VcdTime time)
{
  Sta *sta = Sta::sta();
  Network *network = sta->sdcNetwork();
  const Pin *pin = network->findPin(pin_name);
  if (!pin) {
    sta->report()->warn(1460, "pin %s not found.", pin_name);
    return "";
  }
  SignalWaveformStore *store = sta->waveformStore();
  const SignalWaveform *wf = store->getSignalWaveform(pin);
  if (!wf) {
    sta->report()->warn(1461, "no waveform data for pin %s.", pin_name);
    return "";
  }
  char value = wf->getValueAt(time);
  static char result[2];
  result[0] = value;
  result[1] = '\0';
  return result;
}

%} // inline
