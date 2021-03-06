﻿
// .\Release\benchmarks.exe --benchmark_repetitions=5 --benchmark_min_time=30 --benchmark_filter=HarmonicOscillator                                               // NOLINT(whitespace/line_length)
// Benchmarking on 1 X 3310 MHz CPU
// 2014/09/24-00:22:28
// Benchmark                           Time(ns)    CPU(ns) Iterations
// ------------------------------------------------------------------
// BM_SolveHarmonicOscillator        3041595356 3010819300          3                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)
// BM_SolveHarmonicOscillator        2969998496 2958818967          3                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)
// BM_SolveHarmonicOscillator        2951685882 2932818800          3                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)
// BM_SolveHarmonicOscillator        2957046406 2948418900          3                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)
// BM_SolveHarmonicOscillator        2960333642 2964019000          3                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)
// BM_SolveHarmonicOscillator_mean   2976131957 2962978993          3                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)
// BM_SolveHarmonicOscillator_stddev   33270202   26186699          0                                 1.3701886847350409e-13 m, 1.3705703238997557e-13 m kg s^-1  // NOLINT(whitespace/line_length)

#include "benchmarks/symplectic_partitioned_runge_kutta_integrator.hpp"

#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <algorithm>
#include <vector>

#include "base/not_null.hpp"
#include "quantities/elementary_functions.hpp"
#include "quantities/named_quantities.hpp"

// Must come last to avoid conflicts when defining the CHECK macros.
#include "benchmark/benchmark.h"

using principia::integrators::SPRKIntegrator;
using principia::quantities::Abs;
using principia::quantities::AngularFrequency;
using principia::quantities::Cos;
using principia::quantities::Length;
using principia::quantities::Momentum;

namespace principia {
namespace benchmarks {

void SolveHarmonicOscillatorAndComputeError(
    not_null<benchmark::State*> const state,
    not_null<Length*> const q_error,
    not_null<Momentum*> const p_error) {
  std::vector<SPRKIntegrator<Length, Momentum>::SystemState> solution;

  SolveHarmonicOscillator(&solution);

  state->PauseTiming();
  *q_error = Length();
  *p_error = Momentum();
  for (std::size_t i = 0; i < solution.size(); ++i) {
    *q_error = std::max(*q_error,
                        Abs(solution[i].positions[0].value -
                            SIUnit<Length>() *
                            Cos(solution[i].time.value *
                                SIUnit<AngularFrequency>())));
    *p_error = std::max(*p_error,
                        Abs(solution[i].momenta[0].value +
                            SIUnit<Momentum>() *
                            Sin(solution[i].time.value *
                                SIUnit<AngularFrequency>())));
  }
  state->ResumeTiming();
}

void BM_SolveHarmonicOscillator(
    benchmark::State& state) {  // NOLINT(runtime/references)
  Length   q_error;
  Momentum p_error;
  while (state.KeepRunning()) {
    SolveHarmonicOscillatorAndComputeError(&state, &q_error, &p_error);
  }
  std::stringstream ss;
  ss << q_error << ", " << p_error;
  state.SetLabel(ss.str());
}
BENCHMARK(BM_SolveHarmonicOscillator);

}  // namespace benchmarks
}  // namespace principia
