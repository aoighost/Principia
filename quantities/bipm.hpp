﻿#pragma once

#include "quantities/named_quantities.hpp"
#include "quantities/numbers.hpp"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"

namespace principia {
// This namespace contains the other non-SI units listed in the BIPM's
// SI brochure 8, section 4.1, table 8,
// http://www.bipm.org/en/si/si_brochure/chapter4/table8.html.
namespace bipm {
quantities::Pressure const Bar                 = 1e5 * si::Pascal;
quantities::Pressure const MillimetreOfMercury = 133.322 * si::Pascal;
quantities::Length   const Ångström            = 1e-10 * si::Metre;
quantities::Length   const NauticalMile        = 1852 * si::Metre;
quantities::Speed    const Knot                = 1 * NauticalMile / si::Hour;
quantities::Area const Barn =
    1e-28 * quantities::Pow<2>(si::Metre);
}  // namespace bipm
}  // namespace principia
