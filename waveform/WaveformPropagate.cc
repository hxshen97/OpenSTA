#include "WaveformPropagate.hh"

#include "sta/Sta.hh"
#include "sta/Network.hh"
#include "sta/Graph.hh"
#include "sta/Liberty.hh"
#include "sta/ArcDelayCalc.hh"
#include "sta/DcalcAnalysisPt.hh"
#include "sta/Corner.hh"
#include "sta/Report.hh"

namespace sta {

WaveformPropagator::WaveformPropagator(StaState *sta) :
  StaState(sta),
  dcalc_ap_(nullptr),
  events_processed_(0),
  vcd_seeded_pins_(0),
  propagated_driver_events_(0)
{
}


////////////////////////////////////////////////////////////////

void
WaveformPropagator::propagate(const Corner *corner,
                              const MinMax *min_max)
{
  dcalc_ap_ = corner->findDcalcAnalysisPt(min_max);
  if (!dcalc_ap_) {
    report()->warn(1473, "No delay calculation analysis point for corner %s.",
                   corner->name());
    return;
  }

  SignalWaveformStore *store = waveformStore();
  if (!store || store->getSignalWaveformPins().empty()) {
    report()->warn(1470, "No waveform data loaded. Call read_waveform first.");
    return;
  }

  report()->reportLine("Waveform propagation: seeding from VCD data...");
  seedFromWaveforms();

  report()->reportLine("Waveform propagation: processing %zu seed events...",
                       event_queue_.size());
  processEvents();

  report()->reportLine("Waveform propagation complete:");
  report()->reportLine("  Events processed: %zu", events_processed_);
  report()->reportLine("  VCD seeded pins: %zu", vcd_seeded_pins_);
  report()->reportLine("  Propagated driver events: %zu", propagated_driver_events_);
  report()->reportLine("  Clock edges: %zu", clock_edges_.size());
}

void
WaveformPropagator::seedFromWaveforms()
{
  SignalWaveformStore *store = waveformStore();
  auto pins = store->getSignalWaveformPins();
  vcd_seeded_pins_ = pins.size();

  for (const Pin *pin : pins) {
    const SignalWaveform *wf = store->getSignalWaveform(pin);
    if (!wf) continue;
    for (const auto &t : wf->allTransitions()) {
      event_queue_.push({pin, t.time_, t.from_value_, t.to_value_,
                         nullptr, 0});
    }
  }
}

////////////////////////////////////////////////////////////////
// Helper methods
////////////////////////////////////////////////////////////////

bool
WaveformPropagator::isSequentialCellOutput(const Pin *pin) const
{
  const LibertyPort *lib_port = network()->libertyPort(pin);
  return lib_port && lib_port->isRegOutput();
}

char
WaveformPropagator::valueAtDriverPin(const Pin *pin, VcdTime time) const
{
  PinSet *drivers = network()->drivers(pin);
  if (!drivers || drivers->empty())
    return 'X';
  const Pin *drvr = *drivers->begin();
  const SignalWaveform *wf = waveformStore()->getSignalWaveform(drvr);
  if (!wf) return 'X';
  return wf->getValueAt(time);
}

const Aig&
WaveformPropagator::getOrCreateAig(const FuncExpr *expr)
{
  auto it = aig_cache_.find(expr);
  if (it != aig_cache_.end())
    return it->second;
  Aig aig = buildAig(expr);
  auto result = aig_cache_.emplace(expr, std::move(aig));
  return result.first->second;
}

////////////////////////////////////////////////////////////////
// arcDelay
////////////////////////////////////////////////////////////////

ArcDelay
WaveformPropagator::arcDelay(const TimingArc *arc,
                             const Pin *in_pin,
                             const Pin *drvr_pin) const
{
  if (!arc) return ArcDelay(0);

  const Vertex *in_vertex = graph()->pinDrvrVertex(in_pin);
  if (!in_vertex) in_vertex = graph()->pinLoadVertex(in_pin);
  const RiseFall *in_rf = arc->fromEdge()->asRiseFall();
  if (!in_rf) return ArcDelay(0);
  Slew in_slew = graph()->slew(in_vertex, in_rf, dcalc_ap_->index());

  Parasitic *parasitic = arcDelayCalc()->findParasitic(drvr_pin, in_rf, dcalc_ap_);
  float load_cap = 0.0F;

  LoadPinIndexMap load_pin_index_map{PinIdLess(network())};
  ArcDcalcResult result = arcDelayCalc()->gateDelay(
    drvr_pin, arc, in_slew, load_cap, parasitic,
    load_pin_index_map, dcalc_ap_);
  return result.gateDelay();
}

////////////////////////////////////////////////////////////////
// processEvents
////////////////////////////////////////////////////////////////

void
WaveformPropagator::processEvents()
{
  visited_insts_.clear();

  while (!event_queue_.empty()) {
    WaveformEvent event = event_queue_.top();
    event_queue_.pop();
    events_processed_++;

    if (network()->isDriver(event.pin)) {
      SignalWaveform *wf = waveformStore()->findOrCreateSignalWaveform(event.pin);
      wf->addTransition(event.time, event.from_value, event.to_value,
                        event.cause_pin, event.cause_time);
      propagated_driver_events_++;
    }

    propagateEvent(event);
  }
}

////////////////////////////////////////////////////////////////
// propagateEvent
////////////////////////////////////////////////////////////////

void
WaveformPropagator::propagateEvent(const WaveformEvent &event)
{
  const Net *net = network()->net(event.pin);
  if (!net) return;

  NetConnectedPinIterator *pin_iter = network()->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *load_pin = pin_iter->next();
    if (load_pin == event.pin) continue;
    if (!network()->isLoad(load_pin)) continue;

    const Instance *inst = network()->instance(load_pin);
    if (!network()->isLeaf(inst)) continue;

    evaluateInstance(inst, event);
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////
// evaluateInstance
////////////////////////////////////////////////////////////////

void
WaveformPropagator::evaluateInstance(const Instance *inst,
                                     const WaveformEvent &event)
{
  if (visited_insts_.count(inst)) {
    report()->warn(1471, "Combinational loop detected at instance %s.",
                   network()->pathName(inst));
    return;
  }
  visited_insts_.insert(inst);

  std::unordered_map<const LibertyPort*, char> input_values;
  InstancePinIterator *pin_iter = network()->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network()->isLoad(pin)) {
      const LibertyPort *lib_port = network()->libertyPort(pin);
      if (lib_port)
        input_values[lib_port] = valueAtDriverPin(pin, event.time);
    }
  }
  delete pin_iter;

  const Net *net = network()->net(event.pin);
  if (net) {
    NetConnectedPinIterator *cp_iter = network()->connectedPinIterator(net);
    while (cp_iter->hasNext()) {
      const Pin *cp = cp_iter->next();
      if (network()->instance(cp) == inst && network()->isLoad(cp)) {
        const LibertyPort *lp = network()->libertyPort(cp);
        if (lp) input_values[lp] = event.to_value;
      }
    }
    delete cp_iter;
  }

  evaluateInstanceOutputs(inst, event, input_values);
  recordClockEdge(inst, event);

  visited_insts_.erase(inst);
}

////////////////////////////////////////////////////////////////
// evaluateInstanceOutputs
////////////////////////////////////////////////////////////////

void
WaveformPropagator::evaluateInstanceOutputs(
    const Instance *inst,
    const WaveformEvent &event,
    const std::unordered_map<const LibertyPort*, char> &input_values)
{
  InstancePinIterator *pin_iter = network()->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *drvr_pin = pin_iter->next();
    if (!network()->isDriver(drvr_pin)) continue;
    if (isSequentialCellOutput(drvr_pin)) continue;

    const LibertyPort *drvr_port = network()->libertyPort(drvr_pin);
    if (!drvr_port) continue;

    FuncExpr *func = drvr_port->function();
    if (!func) continue;

    const Aig &aig = getOrCreateAig(func);
    char out_value = aig.evaluate(input_values);

    const SignalWaveform *wf = waveformStore()->getSignalWaveform(drvr_pin);
    char current_value = wf ? wf->getValueAt(event.time) : 'X';

    if (out_value == current_value) continue;

    // Determine rise/fall for timing arc lookup.
    // Default to rise for ambiguous X transitions.
    const RiseFall *in_rf = (event.to_value == '1')
      ? RiseFall::rise() : RiseFall::fall();
    const RiseFall *drvr_rf = (out_value == '1')
      ? RiseFall::rise() : RiseFall::fall();

    Edge *edge = nullptr;
    const TimingArc *arc = nullptr;
    graph()->gateEdgeArc(event.pin, in_rf, drvr_pin, drvr_rf, edge, arc);

    ArcDelay delay(0);
    if (arc)
      delay = arcDelay(arc, event.pin, drvr_pin);

    scheduleEvent({drvr_pin,
                   event.time + static_cast<VcdTime>(delayAsFloat(delay)),
                   current_value,
                   out_value,
                   event.pin,
                   event.time});
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////
// recordClockEdge
////////////////////////////////////////////////////////////////

void
WaveformPropagator::recordClockEdge(const Instance *inst,
                                    const WaveformEvent &event)
{
  const LibertyPort *event_port = network()->libertyPort(event.pin);
  if (!event_port || !event_port->isRegClk()) return;

  int cycle = 0;
  for (const auto &ce : clock_edges_) {
    if (ce.ff == inst)
      cycle = std::max(cycle, ce.cycle_index + 1);
  }
  clock_edges_.push_back({inst, event.pin, event.time, cycle});
}

////////////////////////////////////////////////////////////////
// scheduleEvent
////////////////////////////////////////////////////////////////

void
WaveformPropagator::scheduleEvent(const WaveformEvent &event)
{
  event_queue_.push(event);
}

////////////////////////////////////////////////////////////////
// cycleIndexOf
////////////////////////////////////////////////////////////////

int
WaveformPropagator::cycleIndexOf(const Instance *ff, VcdTime time) const
{
  int cycle = -1;
  for (const auto &ce : clock_edges_) {
    if (ce.ff == ff && ce.edge_time <= time)
      cycle = std::max(cycle, ce.cycle_index);
  }
  return cycle;
}

////////////////////////////////////////////////////////////////
// Entry point
////////////////////////////////////////////////////////////////

void
propagateWaveform(const Corner *corner, const MinMax *min_max, Sta *sta)
{
  sta->ensureLibLinked();
  WaveformPropagator propagator(sta);
  propagator.propagate(corner, min_max);
}

} // namespace sta
