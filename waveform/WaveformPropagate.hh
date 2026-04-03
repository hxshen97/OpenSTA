#pragma once

#include <queue>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "StaState.hh"
#include "SignalWaveformClass.hh"
#include "power/VcdParse.hh"
#include "Aig.hh"

namespace sta {

class Sta;

struct WaveformEvent {
  const Pin *pin;
  VcdTime time;
  char from_value;
  char to_value;
  const Pin *cause_pin;
  VcdTime cause_time;

  bool operator>(const WaveformEvent &other) const {
    return time > other.time;
  }
};

class WaveformPropagator : public StaState {
public:
  explicit WaveformPropagator(StaState *sta);

  void propagate(const Corner *corner, const MinMax *min_max);
  int cycleIndexOf(const Instance *ff, VcdTime time) const;

private:
  void seedFromWaveforms();
  void processEvents();
  void propagateEvent(const WaveformEvent &event);
  void evaluateInstance(const Instance *inst, const WaveformEvent &event);
  void evaluateInstanceOutputs(const Instance *inst,
                               const WaveformEvent &event,
                               const std::unordered_map<const LibertyPort*, char> &input_values);
  void recordClockEdge(const Instance *inst, const WaveformEvent &event);
  void scheduleEvent(const WaveformEvent &event);

  bool isSequentialCellOutput(const Pin *pin) const;
  char valueAtDriverPin(const Pin *pin, VcdTime time) const;
  ArcDelay arcDelay(const TimingArc *arc,
                    const Pin *in_pin,
                    const Pin *drvr_pin) const;
  const Aig& getOrCreateAig(const FuncExpr *expr);

  std::priority_queue<WaveformEvent, std::vector<WaveformEvent>,
                      std::greater<WaveformEvent>> event_queue_;
  const DcalcAnalysisPt *dcalc_ap_;
  std::unordered_map<const FuncExpr*, Aig> aig_cache_;

  struct ClockEdge {
    const Instance *ff;
    const Pin *clock_pin;
    VcdTime edge_time;
    int cycle_index;
  };
  std::vector<ClockEdge> clock_edges_;
  std::unordered_set<const Instance*> visited_insts_;

  size_t events_processed_;
  size_t vcd_seeded_pins_;
  size_t propagated_driver_events_;
};

void propagateWaveform(const Corner *corner, const MinMax *min_max, Sta *sta);

} // namespace sta
