﻿#pragma once

#include "quantities/named_quantities.hpp"
#include "quantities/numbers.hpp"
#include "quantities/quantities.hpp"

namespace principia {
// This namespace contains the units and prefixes of the SI (except the
// Becquerel, Gray and Sievert), as well as the Non-SI units accepted for use
// with the SI.
namespace si {
// Prefixes
template<typename D> quantities::Quantity<D> Yotta(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Zetta(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Exa(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Peta(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Tera(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Giga(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Mega(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Kilo(quantities::Quantity<D>);

template<typename D> quantities::Quantity<D> Hecto(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Deca(quantities::Quantity<D>);

template<typename D> quantities::Quantity<D> Deci(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Centi(quantities::Quantity<D>);

template<typename D> quantities::Quantity<D> Milli(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Micro(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Nano(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Pico(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Femto(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Atto(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Zepto(quantities::Quantity<D>);
template<typename D> quantities::Quantity<D> Yocto(quantities::Quantity<D>);

// SI base units
// From the BIPM's SI brochure 8, section 2.1.2, table 1,
// http://www.bipm.org/en/si/si_brochure/chapter2/2-1/.
quantities::Length const Metre   = quantities::SIUnit<quantities::Length>();
quantities::Mass const Kilogram  = quantities::SIUnit<quantities::Mass>();
quantities::Time const Second    = quantities::SIUnit<quantities::Time>();
quantities::Current const Ampere = quantities::SIUnit<quantities::Current>();
quantities::Temperature const Kelvin =
    quantities::SIUnit<quantities::Temperature>();
quantities::Amount const Mole = quantities::SIUnit<quantities::Amount>();
quantities::LuminousIntensity const Candela =
    quantities::SIUnit<quantities::LuminousIntensity>();
// Nonstandard.
quantities::Winding const Cycle =
    quantities::SIUnit<quantities::Winding>();
// Not base units in the SI. We make these quantities rather than units as they
// are natural.
quantities::Angle const Radian = quantities::SIUnit<quantities::Angle>();
quantities::SolidAngle const Steradian =
    quantities::SIUnit<quantities::SolidAngle>();

// Gram, for use with prefixes.
quantities::Mass const Gram = 1e-3 * Kilogram;

// Coherent derived units in the SI with special names and symbols
// From the BIPM's SI brochure 8, section 2.2.2, table 3,
// http://www.bipm.org/en/si/si_brochure/chapter2/2-2/table3.html.
// We exclude the Becquerel, Gray and Sievert as they are weakly typed.
// The Celsius only really makes sense as an affine temperature and is not taken
// care of here.
// Note the nonstandard definition of the Hertz, with a dimensionful cycle.

// The uno was proposed but never accepted.
double                          const Uno     = 1;
quantities::Frequency           const Hertz   = Cycle / Second;
quantities::Force               const Newton  = Metre * Kilogram /
                                                (Second * Second);
quantities::Pressure            const Pascal  = Newton / (Metre * Metre);
quantities::Energy              const Joule   = Newton * Metre;
quantities::Power               const Watt    = Joule / Second;
quantities::Charge              const Coulomb = Ampere * Second;
quantities::Voltage             const Volt    = Watt / Ampere;
quantities::Capacitance         const Farad   = Coulomb / Volt;
quantities::Resistance          const Ohm     = Volt / Ampere;
quantities::Conductance         const Siemens = Ampere / Volt;
quantities::MagneticFlux        const Weber   = Volt * Second;
quantities::MagneticFluxDensity const Tesla   = Weber / (Metre * Metre);
quantities::Inductance          const Henry   = Weber / Ampere;
quantities::LuminousFlux        const Lumen   = Candela * Steradian;
quantities::CatalyticActivity   const Katal   = Mole / Second;

// Non-SI units accepted for use with the SI
// From the BIPM's SI brochure 8, section 4.1, table 6,
// http://www.bipm.org/en/si/si_brochure/chapter4/table6.html
quantities::Time const Minute = 60 * Second;
quantities::Time const Hour = 60 * Minute;
quantities::Time const Day = 24 * Hour;

quantities::Angle  const Degree    = π / 180 * Radian;
quantities::Angle  const ArcMinute = π / 10800 * Radian;
quantities::Angle  const ArcSecond = π / 648000 * Radian;
quantities::Area   const Hectare   = 1e4 * Metre * Metre;
quantities::Volume const Litre     = quantities::Pow<3>(Deci(Metre));
quantities::Mass   const Tonne     = 1e3 * Kilogram;

// Non-SI units whose values must be obtained experimentally
// From the BIPM's SI brochure 8, section 4.1, table 7,
// Units accepted for use with the SI.
quantities::Energy const ElectronVolt     = 1.602176565e-19 * Joule;
quantities::Mass   const Dalton           = 1.660538921e-27 * Kilogram;
quantities::Length const AstronomicalUnit = 149597870700 * si::Metre;
}  // namespace si
}  // namespace principia

#include "quantities/si_body.hpp"
