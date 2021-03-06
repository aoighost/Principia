﻿#pragma once

#include "testing_utilities/solar_system.hpp"

#include <string>
#include <vector>

#include "base/macros.hpp"
#include "base/not_null.hpp"
#include "geometry/epoch.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/point.hpp"
#include "physics/massive_body.hpp"
#include "physics/n_body_system.hpp"
#include "physics/oblate_body.hpp"
#include "physics/trajectory.hpp"
#include "quantities/named_quantities.hpp"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"

using principia::base::check_not_null;
using principia::base::make_not_null_unique;
using principia::geometry::Bivector;
using principia::geometry::Displacement;
using principia::geometry::Instant;
using principia::geometry::JulianDate;
using principia::geometry::Point;
using principia::geometry::Rotation;
using principia::geometry::Vector;
using principia::physics::MassiveBody;
using principia::physics::NBodySystem;
using principia::physics::OblateBody;
using principia::physics::Trajectory;
using principia::quantities::Angle;
using principia::quantities::GravitationalParameter;
using principia::quantities::Pow;
using principia::quantities::SIUnit;
using principia::quantities::Time;
using principia::si::Day;
using principia::si::Degree;
using principia::si::Kilo;
using principia::si::Kilogram;
using principia::si::Metre;
using principia::si::Second;

namespace principia {
namespace testing_utilities {

namespace {

not_null<std::unique_ptr<MassiveBody>> NewBody(
    SolarSystem::Accuracy const accuracy,
    GravitationalParameter const& gravitational_parameter,
    double const j2,
    Length const& radius,
    Vector<double, ICRFJ2000Ecliptic> const& axis) {
  switch (accuracy) {
    case SolarSystem::Accuracy::kMajorBodiesOnly:
    case SolarSystem::Accuracy::kMinorAndMajorBodies:
      return make_not_null_unique<MassiveBody>(gravitational_parameter);
    case SolarSystem::Accuracy::kAllBodiesAndOblateness:
      return make_not_null_unique<OblateBody<ICRFJ2000Ecliptic>>(
          gravitational_parameter, j2, radius, axis);
    default:
      LOG(FATAL) << __FUNCSIG__ << "Unexpected accuracy "
                 << static_cast<int>(accuracy);
      base::noreturn();
  }
}

// Returns a unit vector pointing in the direction defined by |right_ascension|
// and |declination|.
Vector<double, ICRFJ2000Equator> Direction(Angle const& right_ascension,
                                           Angle const& declination) {
  // Positive angles map {1, 0, 0} to the positive z hemisphere, which is north.
  // An angle of 0 keeps {1, 0, 0} on the equator.
  auto const decline = Rotation<ICRFJ2000Equator, ICRFJ2000Equator>(
                           declination,
                           Bivector<double, ICRFJ2000Equator>({0, -1, 0}));
  // Rotate counterclockwise around {0, 0, 1} (north), i.e., eastward.
  auto const ascend = Rotation<ICRFJ2000Equator, ICRFJ2000Equator>(
                          right_ascension,
                          Bivector<double, ICRFJ2000Equator>({0, 0, 1}));
  return ascend(decline(Vector<double, ICRFJ2000Equator>({1, 0, 0})));
}

}  // namespace

not_null<std::unique_ptr<SolarSystem>> SolarSystem::AtСпутник1Launch(
    Accuracy const accuracy) {
  // Number of days since the JD epoch. JD2436116.3115 is the time of the launch
  // of Простейший Спутник-1.
  Instant const kСпутник1LaunchTime = JulianDate(2436116.3115);

  // Can't use make_unique here.
  not_null<std::unique_ptr<SolarSystem>> solar_system(
      check_not_null(new SolarSystem(accuracy)));

  // All data is from the Jet Propulsion Laboratory's HORIZONS system.

  // Star.
  auto sun_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                            solar_system->massive_bodies_[kSun].get());
  sun_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.138350928138014E+06 * Kilo(Metre),
             6.177753685036716E+05 * Kilo(Metre),
            -3.770941657504326E+04 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-5.067456621846211E-03 * Kilo(Metre) / Second,
             1.259599196445122E-02 * Kilo(Metre) / Second,
             9.778588606052481E-05 * Kilo(Metre) / Second})});

  // Planets.

  // Gas giants.
  auto jupiter_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kJupiter].get());
  jupiter_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.950209667306620E+08 * Kilo(Metre),
            -1.784285526424396E+08 * Kilo(Metre),
             1.853825132237791E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 2.709330231918198E+00 * Kilo(Metre) / Second,
            -1.213073724288562E+01 * Kilo(Metre) / Second,
            -1.088748435062713E-02 * Kilo(Metre) / Second})});
  auto saturn_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kSaturn].get());
  saturn_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.774715321901159E+08 * Kilo(Metre),
            -1.451892263379818E+09 * Kilo(Metre),
             4.040621083792380E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 8.817029873536633E+00 * Kilo(Metre) / Second,
            -2.466058486223613E+00 * Kilo(Metre) / Second,
            -3.068419809533604E-01 * Kilo(Metre) / Second})});
  auto neptune_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kNeptune].get());
  neptune_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.810689792831146E+09 * Kilo(Metre),
            -2.456423858579051E+09 * Kilo(Metre),
             1.383694320077938E+08 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 2.913267720085410E+00 * Kilo(Metre) / Second,
            -4.535247383721019E+00 * Kilo(Metre) / Second,
             2.589759251085161E-02 * Kilo(Metre) / Second})});
  auto uranus_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kUranus].get());
  uranus_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-1.729995609344851E+09 * Kilo(Metre),
             2.159967050539728E+09 * Kilo(Metre),
             3.048735047038063E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-5.366539669972795E+00 * Kilo(Metre) / Second,
            -4.575802196749351E+00 * Kilo(Metre) / Second,
             5.261322980347850E-02 * Kilo(Metre) / Second})});

  // Telluric planets.
  auto earth_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kEarth].get());
  earth_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.475150112055673E+08 * Kilo(Metre),
             3.144435102288270E+07 * Kilo(Metre),
            -3.391764309344300E+04 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-6.635753510543799E+00 * Kilo(Metre) / Second,
             2.904321639216012E+01 * Kilo(Metre) / Second,
             3.125252418990812E-03 * Kilo(Metre) / Second})});
  auto venus_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                              solar_system->massive_bodies_[kVenus].get());
  venus_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 6.084974577091119E+07 * Kilo(Metre),
            -9.037413730207849E+07 * Kilo(Metre),
            -4.719158908401959E+06 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 2.903958257174759E+01 * Kilo(Metre) / Second,
             1.910383147602264E+01 * Kilo(Metre) / Second,
            -1.418780340302349E+00 * Kilo(Metre) / Second})});
  auto mars_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                             solar_system->massive_bodies_[kMars].get());
  mars_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-2.440047184660406E+08 * Kilo(Metre),
            -2.002994580992744E+07 * Kilo(Metre),
             5.577600092368793E+06 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 2.940381268511949E+00 * Kilo(Metre) / Second,
            -2.206625841382794E+01 * Kilo(Metre) / Second,
            -5.348179460834037E-01 * Kilo(Metre) / Second})});
  auto mercury_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kMercury].get());
  mercury_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.013851560892715E+07 * Kilo(Metre),
             3.823388939456400E+07 * Kilo(Metre),
             5.907240907643730E+06 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-4.731017449071709E+01 * Kilo(Metre) / Second,
            -2.918747853895398E+01 * Kilo(Metre) / Second,
             1.963450229872517E+00 * Kilo(Metre) / Second})});

  // End of planets.

  // Satellite of Jupiter.
  auto ganymede_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kGanymede].get());
  ganymede_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.942681422941415E+08 * Kilo(Metre),
            -1.776681035234876E+08 * Kilo(Metre),
             1.857215495334835E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-5.026319376504355E+00 * Kilo(Metre) / Second,
            -4.481735740234995E+00 * Kilo(Metre) / Second,
             1.326192167761359E-01 * Kilo(Metre) / Second})});

  // Satellite of Saturn.
  auto titan_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kTitan].get());
  titan_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.771930512714775E+08 * Kilo(Metre),
            -1.452931696594699E+09 * Kilo(Metre),
             4.091643033375849E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 1.433381483669744E+01 * Kilo(Metre) / Second,
            -1.422590492527597E+00 * Kilo(Metre) / Second,
            -1.375826555026097E+00 * Kilo(Metre) / Second})});

  // Satellites of Jupiter.
  auto callisto_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kCallisto].get());
  callisto_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.951805452047400E+08 * Kilo(Metre),
            -1.802957437059298E+08 * Kilo(Metre),
             1.847154088070625E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 1.091928199422218E+01 * Kilo(Metre) / Second,
            -1.278098875182818E+01 * Kilo(Metre) / Second,
             5.878649120351949E-02 * Kilo(Metre) / Second})});
  auto io_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                           solar_system->massive_bodies_[kIo].get());
  io_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.946073188298367E+08 * Kilo(Metre),
            -1.783491436977172E+08 * Kilo(Metre),
             1.854699192614355E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-5.049684272040893E-01 * Kilo(Metre) / Second,
             4.916473261567652E+00 * Kilo(Metre) / Second,
             5.469177855959977E-01 * Kilo(Metre) / Second})});

  // Satellite of Earth.
  auto moon_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                             solar_system->massive_bodies_[kMoon].get());
  moon_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.478545271460863E+08 * Kilo(Metre),
             3.122566749814625E+07 * Kilo(Metre),
             1.500491219719345E+03 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-6.099833968412930E+00 * Kilo(Metre) / Second,
             2.985006033154299E+01 * Kilo(Metre) / Second,
            -1.952438319420470E-02 * Kilo(Metre) / Second})});

  // Satellite of Jupiter.
  auto europa_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kEuropa].get());
  europa_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.944180333947762E+08 * Kilo(Metre),
            -1.787346439588362E+08 * Kilo(Metre),
             1.853675837527557E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 8.811255547505889E+00 * Kilo(Metre) / Second,
             5.018147960240774E-02 * Kilo(Metre) / Second,
             6.162195631257494E-01 * Kilo(Metre) / Second})});

  // Satellite of Neptune.
  auto triton_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                               solar_system->massive_bodies_[kTriton].get());
  triton_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.810797098554279E+09 * Kilo(Metre),
            -2.456691608348630E+09 * Kilo(Metre),
             1.381629136719314E+08 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-1.047462448797063E+00 * Kilo(Metre) / Second,
            -4.404556713303486E+00 * Kilo(Metre) / Second,
             1.914469843538767E+00 * Kilo(Metre) / Second})});

  // Dwarf planet (scattered disc object).
  auto eris_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                             solar_system->massive_bodies_[kEris].get());
  eris_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.317390066862979E+10 * Kilo(Metre),
             2.221403321600002E+09 * Kilo(Metre),
            -5.736076877456254E+09 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 4.161883594267296E-01 * Kilo(Metre) / Second,
             1.872714752602233E+00 * Kilo(Metre) / Second,
             1.227093842948539E+00 * Kilo(Metre) / Second})});

  // Dwarf planet (Kuiper belt object).
  auto pluto_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kPluto].get());
  pluto_trajectory->Append(
      kСпутник1LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-4.406985590968750E+09 * Kilo(Metre),
             2.448731153209013E+09 * Kilo(Metre),
             1.012525975599311E+09 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-1.319871918266467E+00 * Kilo(Metre) / Second,
            -5.172112237151897E+00 * Kilo(Metre) / Second,
             9.407707128142039E-01 * Kilo(Metre) / Second})});

  solar_system->trajectories_.emplace_back(std::move(sun_trajectory));
  solar_system->trajectories_.emplace_back(std::move(jupiter_trajectory));
  solar_system->trajectories_.emplace_back(std::move(saturn_trajectory));
  solar_system->trajectories_.emplace_back(std::move(neptune_trajectory));
  solar_system->trajectories_.emplace_back(std::move(uranus_trajectory));
  solar_system->trajectories_.emplace_back(std::move(earth_trajectory));
  solar_system->trajectories_.emplace_back(std::move(venus_trajectory));
  solar_system->trajectories_.emplace_back(std::move(mars_trajectory));
  solar_system->trajectories_.emplace_back(std::move(mercury_trajectory));
  solar_system->trajectories_.emplace_back(std::move(ganymede_trajectory));
  solar_system->trajectories_.emplace_back(std::move(titan_trajectory));
  solar_system->trajectories_.emplace_back(std::move(callisto_trajectory));
  solar_system->trajectories_.emplace_back(std::move(io_trajectory));
  solar_system->trajectories_.emplace_back(std::move(moon_trajectory));
  solar_system->trajectories_.emplace_back(std::move(europa_trajectory));
  solar_system->trajectories_.emplace_back(std::move(triton_trajectory));
  solar_system->trajectories_.emplace_back(std::move(eris_trajectory));
  solar_system->trajectories_.emplace_back(std::move(pluto_trajectory));

  if (accuracy > Accuracy::kMajorBodiesOnly) {
    // Satellites of Uranus.
    auto titania_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kTitania].get());
    titania_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.729595658924435E+09 * Kilo(Metre),
               2.159860356365425E+09 * Kilo(Metre),
               3.035141077516359E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-6.591633969110592E+00 * Kilo(Metre) / Second,
              -4.794586046464699E+00 * Kilo(Metre) / Second,
              -3.377964153317895E+00 * Kilo(Metre) / Second})});
    auto oberon_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kOberon].get());
    oberon_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.730563623290436E+09 * Kilo(Metre),
               2.160079664472153E+09 * Kilo(Metre),
               3.041037690361578E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-5.685242261484572E+00 * Kilo(Metre) / Second,
              -4.073586348304020E+00 * Kilo(Metre) / Second,
               3.143404489724676E+00 * Kilo(Metre) / Second})});

    // Satellites of Saturn.
    auto rhea_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kRhea].get());
    rhea_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.772933911553755E+08 * Kilo(Metre),
              -1.451461170080230E+09 * Kilo(Metre),
               4.016028653663339E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 8.698485920139012E-01 * Kilo(Metre) / Second,
               3.972546870497955E-01 * Kilo(Metre) / Second,
              -1.060361115947588E+00 * Kilo(Metre) / Second})});
    auto iapetus_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kIapetus].get());
    iapetus_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.751029285588183E+08 * Kilo(Metre),
              -1.449565401910516E+09 * Kilo(Metre),
               3.935332456093812E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 6.497053388258254E+00 * Kilo(Metre) / Second,
              -7.325141764921950E-02 * Kilo(Metre) / Second,
              -4.351376438069059E-01 * Kilo(Metre) / Second})});

    // Satellite of Pluto.
    auto charon_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kCharon].get());
    charon_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-4.406983470848657E+09 * Kilo(Metre),
               2.448743066982903E+09 * Kilo(Metre),
               1.012541389091277E+09 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-1.157714657718270E+00 * Kilo(Metre) / Second,
              -5.062468891990006E+00 * Kilo(Metre) / Second,
               8.337034401124047E-01 * Kilo(Metre) / Second})});

    // Satellites of Uranus.
    auto ariel_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kAriel].get());
    ariel_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.730181133162941E+09 * Kilo(Metre),
               2.160003751339937E+09 * Kilo(Metre),
               3.045891238850706E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-6.019957829322824E+00 * Kilo(Metre) / Second,
              -3.682977487897364E+00 * Kilo(Metre) / Second,
               5.440031145983448E+00 * Kilo(Metre) / Second})});
    auto umbriel_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kUmbriel].get());
    umbriel_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.729930175425741E+09 * Kilo(Metre),
               2.159917273779030E+09 * Kilo(Metre),
               3.023539524396962E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-9.791806936609191E+00 * Kilo(Metre) / Second,
              -3.786140307785084E+00 * Kilo(Metre) / Second,
              -1.264397874774153E+00 * Kilo(Metre) / Second})});

    // Satellites of Saturn.
    auto dione_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kDione].get());
    dione_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.777740468280008E+08 * Kilo(Metre),
              -1.452078913407227E+09 * Kilo(Metre),
               4.053308808094668E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 1.473536505664190E+01 * Kilo(Metre) / Second,
              -9.857333255400615E+00 * Kilo(Metre) / Second,
               2.994635825207214E+00 * Kilo(Metre) / Second})});
    auto tethys_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kTethys].get());
    tethys_trajectory->Append(
        kСпутник1LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.772002485884590E+08 * Kilo(Metre),
              -1.451803185519638E+09 * Kilo(Metre),
               4.033334240953118E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 4.474028082408450E+00 * Kilo(Metre) / Second,
               6.894343190912965E+00 * Kilo(Metre) / Second,
              -5.036010312221701E+00 * Kilo(Metre) / Second})});

    solar_system->trajectories_.emplace_back(std::move(titania_trajectory));
    solar_system->trajectories_.emplace_back(std::move(oberon_trajectory));
    solar_system->trajectories_.emplace_back(std::move(rhea_trajectory));
    solar_system->trajectories_.emplace_back(std::move(iapetus_trajectory));
    solar_system->trajectories_.emplace_back(std::move(charon_trajectory));
    solar_system->trajectories_.emplace_back(std::move(ariel_trajectory));
    solar_system->trajectories_.emplace_back(std::move(umbriel_trajectory));
    solar_system->trajectories_.emplace_back(std::move(dione_trajectory));
    solar_system->trajectories_.emplace_back(std::move(tethys_trajectory));
  }

  return std::move(solar_system);
}

not_null<std::unique_ptr<SolarSystem>> SolarSystem::AtСпутник2Launch(
    Accuracy const accuracy) {
  // Number of days since the JD epoch. JD2436145.60417 is the time of the
  // launch of Простейший Спутник-2.
  Instant const kСпутник2LaunchTime = JulianDate(2436145.60417);

  // Can't use make_unique here.
  not_null<std::unique_ptr<SolarSystem>> solar_system(
      check_not_null(new SolarSystem(accuracy)));

  // All data is from the Jet Propulsion Laboratory's HORIZONS system.

  // Star.
  auto sun_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                            solar_system->massive_bodies_[kSun].get());
  sun_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.125014268938588E+06 * Kilo(Metre),
             6.494303112314661E+05 * Kilo(Metre),
            -3.744891854948698E+04 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-5.465237566098069E-03 * Kilo(Metre) / Second,
             1.242259254161160E-02 * Kilo(Metre) / Second,
             1.073185551299655E-04 * Kilo(Metre) / Second})});

  // Planets.

  // Gas giants.
  auto jupiter_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kJupiter].get());
  jupiter_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.875437547145393E+08 * Kilo(Metre),
            -2.089781394713737E+08 * Kilo(Metre),
             1.849633128369343E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 3.199399521413006E+00 * Kilo(Metre) / Second,
            -1.200823909873311E+01 * Kilo(Metre) / Second,
            -2.224995144931441E-02 * Kilo(Metre) / Second})});
  auto saturn_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kSaturn].get());
  saturn_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.551066003300792E+08 * Kilo(Metre),
            -1.457950211244599E+09 * Kilo(Metre),
             3.962394173262903E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 8.853737336923556E+00 * Kilo(Metre) / Second,
            -2.320374389288883E+00 * Kilo(Metre) / Second,
            -3.114483488133248E-01 * Kilo(Metre) / Second})});
  auto neptune_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kNeptune].get());
  neptune_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.803301375557554E+09 * Kilo(Metre),
            -2.467890768105946E+09 * Kilo(Metre),
             1.384353457950279E+08 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 2.927010084845599E+00 * Kilo(Metre) / Second,
            -4.526307194625022E+00 * Kilo(Metre) / Second,
             2.545268709706176E-02 * Kilo(Metre) / Second})});
  auto uranus_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kUranus].get());
  uranus_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-1.743543206484359E+09 * Kilo(Metre),
             2.148343005727444E+09 * Kilo(Metre),
             3.061995217929694E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-5.339030319622795E+00 * Kilo(Metre) / Second,
            -4.609984321394619E+00 * Kilo(Metre) / Second,
             5.202604125767743E-02 * Kilo(Metre) / Second})});

  // Telluric planets.
  auto earth_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kEarth].get());
  earth_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.131005469241166E+08 * Kilo(Metre),
             9.799962736944504E+07 * Kilo(Metre),
            -2.743948682505761E+04 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-2.003713675265280E+01 * Kilo(Metre) / Second,
             2.237299122930724E+01 * Kilo(Metre) / Second,
             2.796170626009044E-03 * Kilo(Metre) / Second})});
  auto venus_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                              solar_system->massive_bodies_[kVenus].get());
  venus_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.079589109069277E+08 * Kilo(Metre),
            -1.883185527327590E+07 * Kilo(Metre),
            -6.471728962310291E+06 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 6.105839497257745E+00 * Kilo(Metre) / Second,
             3.430628991145717E+01 * Kilo(Metre) / Second,
             1.117436366138174E-01 * Kilo(Metre) / Second})});
  auto mars_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                             solar_system->massive_bodies_[kMars].get());
  mars_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-2.295899280109642E+08 * Kilo(Metre),
            -7.474408961700515E+07 * Kilo(Metre),
             4.075745516046084E+06 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 8.432679541838580E+00 * Kilo(Metre) / Second,
            -2.095334664935562E+01 * Kilo(Metre) / Second,
            -6.470034479976146E-01 * Kilo(Metre) / Second})});
  auto mercury_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kMercury].get());
  mercury_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.280770775568475E+07 * Kilo(Metre),
            -5.947158605939089E+07 * Kilo(Metre),
            -1.827172250582807E+06 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 3.259843531566923E+01 * Kilo(Metre) / Second,
            -2.157557185030672E+01 * Kilo(Metre) / Second,
            -4.758347584450094E+00 * Kilo(Metre) / Second})});

  // End of planets.

  // Satellite of Jupiter.
  auto ganymede_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kGanymede].get());
  ganymede_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.873440767588949E+08 * Kilo(Metre),
            -2.079266562514496E+08 * Kilo(Metre),
             1.853235266265094E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-7.484803642517669E+00 * Kilo(Metre) / Second,
            -9.979889365339663E+00 * Kilo(Metre) / Second,
            -9.540419435645386E-02 * Kilo(Metre) / Second})});

  // Satellite of Saturn.
  auto titan_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kTitan].get());
  titan_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.559932418834351E+08 * Kilo(Metre),
            -1.458657870294226E+09 * Kilo(Metre),
             4.007469245438983E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 1.277329830321889E+01 * Kilo(Metre) / Second,
            -5.987335332263677E+00 * Kilo(Metre) / Second,
             1.206347481985469E+00 * Kilo(Metre) / Second})});

  // Satellites of Jupiter.
  auto callisto_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kCallisto].get());
  callisto_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.894358442189863E+08 * Kilo(Metre),
            -2.088864854947591E+08 * Kilo(Metre),
             1.847824600878225E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 2.841233795859193E+00 * Kilo(Metre) / Second,
            -2.014928300738163E+01 * Kilo(Metre) / Second,
            -3.092683314888902E-01 * Kilo(Metre) / Second})});
  auto io_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                           solar_system->massive_bodies_[kIo].get());
  io_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.879055114756504E+08 * Kilo(Metre),
            -2.091931053457293E+08 * Kilo(Metre),
             1.848354122950428E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 1.213574915656921E+01 * Kilo(Metre) / Second,
            -2.689611236410145E+01 * Kilo(Metre) / Second,
            -4.221293967140784E-01 * Kilo(Metre) / Second})});

  // Satellite of Earth.
  auto moon_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                             solar_system->massive_bodies_[kMoon].get());
  moon_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.134993352573264E+08 * Kilo(Metre),
             9.793594458884758E+07 * Kilo(Metre),
             1.300882839548027E+03 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-1.988824948390998E+01 * Kilo(Metre) / Second,
             2.332896066382083E+01 * Kilo(Metre) / Second,
            -5.471933119303941E-02 * Kilo(Metre) / Second})});

  // Satellite of Jupiter.
  auto europa_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kEuropa].get());
  europa_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-7.872380045561892E+08 * Kilo(Metre),
            -2.083874295273294E+08 * Kilo(Metre),
             1.852692606438262E+07 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-9.132729339507982E+00 * Kilo(Metre) / Second,
            -5.706657631633117E+00 * Kilo(Metre) / Second,
             8.154101985062136E-03 * Kilo(Metre) / Second})});

  // Satellite of Neptune.
  auto triton_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kTriton].get());
  triton_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-3.803376889526241E+09 * Kilo(Metre),
            -2.468158270187521E+09 * Kilo(Metre),
             1.382149037665635E+08 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-1.144130125366116E+00 * Kilo(Metre) / Second,
            -4.720828265121008E+00 * Kilo(Metre) / Second,
             1.656135195284262E+00 * Kilo(Metre) / Second})});

  // Dwarf planet (scattered disc object).
  auto eris_trajectory = make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
                             solar_system->massive_bodies_[kEris].get());
  eris_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           { 1.317496754534689E+10 * Kilo(Metre),
             2.226129564084833E+09 * Kilo(Metre),
            -5.732978102633001E+09 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           { 4.147613028977510E-01 * Kilo(Metre) / Second,
             1.872488324095242E+00 * Kilo(Metre) / Second,
             1.227720820942441E+00 * Kilo(Metre) / Second})});

  // Dwarf planet (Kuiper belt object).
  auto pluto_trajectory =
      make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
          solar_system->massive_bodies_[kPluto].get());
  pluto_trajectory->Append(
      kСпутник2LaunchTime,
      {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
           {-4.410266464068357E+09 * Kilo(Metre),
             2.435666526837864E+09 * Kilo(Metre),
             1.014876954733593E+09 * Kilo(Metre)}),
       Velocity<ICRFJ2000Ecliptic>(
           {-1.277624218981084E+00 * Kilo(Metre) / Second,
            -5.163643781275358E+00 * Kilo(Metre) / Second,
             9.068668780280327E-01 * Kilo(Metre) / Second})});

  solar_system->trajectories_.emplace_back(std::move(sun_trajectory));
  solar_system->trajectories_.emplace_back(std::move(jupiter_trajectory));
  solar_system->trajectories_.emplace_back(std::move(saturn_trajectory));
  solar_system->trajectories_.emplace_back(std::move(neptune_trajectory));
  solar_system->trajectories_.emplace_back(std::move(uranus_trajectory));
  solar_system->trajectories_.emplace_back(std::move(earth_trajectory));
  solar_system->trajectories_.emplace_back(std::move(venus_trajectory));
  solar_system->trajectories_.emplace_back(std::move(mars_trajectory));
  solar_system->trajectories_.emplace_back(std::move(mercury_trajectory));
  solar_system->trajectories_.emplace_back(std::move(ganymede_trajectory));
  solar_system->trajectories_.emplace_back(std::move(titan_trajectory));
  solar_system->trajectories_.emplace_back(std::move(callisto_trajectory));
  solar_system->trajectories_.emplace_back(std::move(io_trajectory));
  solar_system->trajectories_.emplace_back(std::move(moon_trajectory));
  solar_system->trajectories_.emplace_back(std::move(europa_trajectory));
  solar_system->trajectories_.emplace_back(std::move(triton_trajectory));
  solar_system->trajectories_.emplace_back(std::move(eris_trajectory));
  solar_system->trajectories_.emplace_back(std::move(pluto_trajectory));

  if (accuracy > Accuracy::kMajorBodiesOnly) {
    // Satellites of Uranus.
    auto titania_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kTitania].get());
    titania_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.743918818421802E+09 * Kilo(Metre),
               2.148394286698188E+09 * Kilo(Metre),
               3.040267774304451E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-7.036501356327411E+00 * Kilo(Metre) / Second,
              -3.797291350751153E+00 * Kilo(Metre) / Second,
               3.166248684554561E+00 * Kilo(Metre) / Second})});
    auto oberon_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kOberon].get());
    oberon_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.743851406035026E+09 * Kilo(Metre),
               2.148476767320335E+09 * Kilo(Metre),
               3.109684580610486E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-2.745718567351011E+00 * Kilo(Metre) / Second,
              -4.926321308326997E+00 * Kilo(Metre) / Second,
               1.815423517306933E+00 * Kilo(Metre) / Second})});

    // Satellites of Saturn.
    auto rhea_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kRhea].get());
    rhea_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.553342718565885E+08 * Kilo(Metre),
              -1.458360446041042E+09 * Kilo(Metre),
               3.986359566173195E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 1.647814913327736E+01 * Kilo(Metre) / Second,
              -5.870586279416220E+00 * Kilo(Metre) / Second,
               8.369964139554196E-01 * Kilo(Metre) / Second})});
    auto iapetus_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kIapetus].get());
    iapetus_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.586954722831523E+08 * Kilo(Metre),
              -1.457628919841799E+09 * Kilo(Metre),
               4.026455668743709E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 8.707383466174113E+00 * Kilo(Metre) / Second,
              -5.392253140156209E+00 * Kilo(Metre) / Second,
               4.807764918652989E-01 * Kilo(Metre) / Second})});

    // Satellite of Pluto.
    auto charon_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kCharon].get());
    charon_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-4.410275620814398E+09 * Kilo(Metre),
               2.435651353388658E+09 * Kilo(Metre),
               1.014868590806160E+09 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-1.404153365129384E+00 * Kilo(Metre) / Second,
              -5.187717357379291E+00 * Kilo(Metre) / Second,
               1.089041178376519E+00 * Kilo(Metre) / Second})});

    // Satellites of Uranus.
    auto ariel_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kAriel].get());
    ariel_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.743394693015613E+09 * Kilo(Metre),
               2.148295228037889E+09 * Kilo(Metre),
               3.051049859822118E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-8.590134956934145E+00 * Kilo(Metre) / Second,
              -4.517951101991714E+00 * Kilo(Metre) / Second,
              -4.406982500749494E+00 * Kilo(Metre) / Second})});
    auto umbriel_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kUmbriel].get());
    umbriel_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-1.743588311968474E+09 * Kilo(Metre),
               2.148316432062827E+09 * Kilo(Metre),
               3.035987024560333E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-9.843503117910014E+00 * Kilo(Metre) / Second,
              -3.525745217265672E+00 * Kilo(Metre) / Second,
               7.092444771525036E-01 * Kilo(Metre) / Second})});

    // Satellites of Saturn.
    auto dione_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kDione].get());
    dione_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.552295721012846E+08 * Kilo(Metre),
              -1.457630098290271E+09 * Kilo(Metre),
               3.946811133174797E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             {-6.030846229930553E-01 * Kilo(Metre) / Second,
              -4.868620315848084E+00 * Kilo(Metre) / Second,
               1.933469386798133E+00 * Kilo(Metre) / Second})});
    auto tethys_trajectory =
        make_not_null_unique<Trajectory<ICRFJ2000Ecliptic>>(
            solar_system->massive_bodies_[kTethys].get());
    tethys_trajectory->Append(
        kСпутник2LaunchTime,
        {kSolarSystemBarycentre + Displacement<ICRFJ2000Ecliptic>(
             {-3.553644729603329E+08 * Kilo(Metre),
              -1.458064034431594E+09 * Kilo(Metre),
               3.970978147111944E+07 * Kilo(Metre)}),
         Velocity<ICRFJ2000Ecliptic>(
             { 1.427192911372915E+01 * Kilo(Metre) / Second,
              -1.127052555342930E+01 * Kilo(Metre) / Second,
               4.094008639209452E+00 * Kilo(Metre) / Second})});

    solar_system->trajectories_.emplace_back(std::move(titania_trajectory));
    solar_system->trajectories_.emplace_back(std::move(oberon_trajectory));
    solar_system->trajectories_.emplace_back(std::move(rhea_trajectory));
    solar_system->trajectories_.emplace_back(std::move(iapetus_trajectory));
    solar_system->trajectories_.emplace_back(std::move(charon_trajectory));
    solar_system->trajectories_.emplace_back(std::move(ariel_trajectory));
    solar_system->trajectories_.emplace_back(std::move(umbriel_trajectory));
    solar_system->trajectories_.emplace_back(std::move(dione_trajectory));
    solar_system->trajectories_.emplace_back(std::move(tethys_trajectory));
  }

  return std::move(solar_system);
}

SolarSystem::SolarSystem(Accuracy const accuracy) {
  // All data is from the Jet Propulsion Laboratory's HORIZONS system unless
  // otherwise specified.

  // Star.
  auto sun = make_not_null_unique<MassiveBody>(
                 1.3271244004193938E+11 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // Planets.

  // Gas giants.
  // Gravitational characteristics from
  // http://ssd.jpl.nasa.gov/?gravity_fields_op.  See also "Interior Models of
  // Uranus and Neptune", Helled et al.,
  // http://www.astrouw.edu.pl/~nalezyty/semistud/Artykuly/1010.5546v1.pdf
  // and "Jupiter’s Moment of Inertia: A Possible Determination by JUNO", Helled
  // et al., http://arxiv.org/pdf/1109.1627.pdf.
  // Axis directions from "Report of the IAU Working Group on Cartographic
  // Coordinates and Rotational Elements: 2009", Archinal et al.,
  // http://astropedia.astrogeology.usgs.gov/download/Docs/WGCCRE/WGCCRE2009reprint.pdf.

  not_null<std::unique_ptr<MassiveBody>> jupiter(
      NewBody(accuracy,
              126686535 * Pow<3>(Kilo(Metre)) / Pow<2>(Second),
              14696.43E-6,
              71492 * Kilo(Metre),
              kEquatorialToEcliptic(Direction(268.056595 * Degree,
                                              64.495303 * Degree))));
  not_null<std::unique_ptr<MassiveBody>> saturn(
      NewBody(accuracy,
              37931208 * Pow<3>(Kilo(Metre)) / Pow<2>(Second),
              16290.71E-6,
              60330 * Kilo(Metre),
              kEquatorialToEcliptic(Direction(40.589 * Degree,
                                              83.537 * Degree))));
  not_null<std::unique_ptr<MassiveBody>> neptune(
      NewBody(accuracy,
              6835100 * Pow<3>(Kilo(Metre)) / Pow<2>(Second),
              3408.43E-6,
              25225 * Kilo(Metre),
              kEquatorialToEcliptic(Direction(299.36 * Degree,
                                              43.46 * Degree))));
  not_null<std::unique_ptr<MassiveBody>> uranus(
      NewBody(accuracy,
              5793964 * Pow<3>(Kilo(Metre)) / Pow<2>(Second),
              3341.29E-6,
              26200 * Kilo(Metre),
              kEquatorialToEcliptic(Direction(257.311 * Degree,
                                              -15.175 * Degree))));

  // Telluric planets.
  auto earth = make_not_null_unique<MassiveBody>(
                   398600.440 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));
  auto venus = make_not_null_unique<MassiveBody>(
                   324858.63 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));
  auto mars = make_not_null_unique<MassiveBody>(
                  42828.3 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));
  auto mercury = make_not_null_unique<MassiveBody>(
                     22032.09 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // End of planets.

  // Satellite of Jupiter.
  auto ganymede = make_not_null_unique<MassiveBody>(1482E20 * Kilogram);

  // Satellite of Saturn.
  auto titan = make_not_null_unique<MassiveBody>(
                   8978.13 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // Satellites of Jupiter.
  auto callisto = make_not_null_unique<MassiveBody>(1076E20 * Kilogram);
  auto io = make_not_null_unique<MassiveBody>(893.3E20 * Kilogram);

  // Satellite of Earth.
  auto moon = make_not_null_unique<MassiveBody>(
                  4902.798 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // Satellite of Jupiter.
  auto europa = make_not_null_unique<MassiveBody>(479.7E20 * Kilogram);

  // Satellite of Neptune.
  auto triton = make_not_null_unique<MassiveBody>(214.7E20 * Kilogram);

  // Dwarf planet (scattered disc object).
  // Mass from Brown, Michael E.; Schaller, Emily L. (15 June 2007).
  // "The Mass of Dwarf Planet Eris", in Science, through Wikipedia.
  auto eris = make_not_null_unique<MassiveBody>(1.67E22 * Kilogram);

  // Dwarf planet (Kuiper belt object).
  auto pluto = make_not_null_unique<MassiveBody>(
                   872.4 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // Satellites of Uranus.
  auto titania = make_not_null_unique<MassiveBody>(35.27E20 * Kilogram);
  auto oberon = make_not_null_unique<MassiveBody>(30.14E20 * Kilogram);

  // Satellites of Saturn.
  auto rhea = make_not_null_unique<MassiveBody>(
                  153.94 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));
  auto iapetus = make_not_null_unique<MassiveBody>(
                     120.51 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // Satellite of Pluto.
  // The masses reported by HORIZONS have very few significant digits. Instead
  // we subtract Pluto's gravitational parameter from the one given for the
  // Charon-Pluto system.
  auto charon =
      make_not_null_unique<MassiveBody>(
          9.7549380662106296E2 * Pow<3>(Kilo(Metre)) / Pow<2>(Second) -
              pluto->gravitational_parameter());

  // Satellites of Uranus.
  auto ariel = make_not_null_unique<MassiveBody>(13.53E20 * Kilogram);
  auto umbriel = make_not_null_unique<MassiveBody>(11.72E20 * Kilogram);

  // Satellites of Saturn.
  auto dione = make_not_null_unique<MassiveBody>(
                   73.113 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));
  auto tethys = make_not_null_unique<MassiveBody>(
                    41.21 * Pow<3>(Kilo(Metre)) / Pow<2>(Second));

  // End of celestial bodies.

  massive_bodies_.emplace_back(std::move(sun));
  massive_bodies_.emplace_back(std::move(jupiter));
  massive_bodies_.emplace_back(std::move(saturn));
  massive_bodies_.emplace_back(std::move(neptune));
  massive_bodies_.emplace_back(std::move(uranus));
  massive_bodies_.emplace_back(std::move(earth));
  massive_bodies_.emplace_back(std::move(venus));
  massive_bodies_.emplace_back(std::move(mars));
  massive_bodies_.emplace_back(std::move(mercury));
  massive_bodies_.emplace_back(std::move(ganymede));
  massive_bodies_.emplace_back(std::move(titan));
  massive_bodies_.emplace_back(std::move(callisto));
  massive_bodies_.emplace_back(std::move(io));
  massive_bodies_.emplace_back(std::move(moon));
  massive_bodies_.emplace_back(std::move(europa));
  massive_bodies_.emplace_back(std::move(triton));
  massive_bodies_.emplace_back(std::move(eris));
  massive_bodies_.emplace_back(std::move(pluto));
  if (accuracy > Accuracy::kMajorBodiesOnly) {
    massive_bodies_.emplace_back(std::move(titania));
    massive_bodies_.emplace_back(std::move(oberon));
    massive_bodies_.emplace_back(std::move(rhea));
    massive_bodies_.emplace_back(std::move(iapetus));
    massive_bodies_.emplace_back(std::move(charon));
    massive_bodies_.emplace_back(std::move(ariel));
    massive_bodies_.emplace_back(std::move(umbriel));
    massive_bodies_.emplace_back(std::move(dione));
    massive_bodies_.emplace_back(std::move(tethys));
  }
}

SolarSystem::Bodies SolarSystem::massive_bodies() {
  return std::move(massive_bodies_);
}

physics::NBodySystem<ICRFJ2000Ecliptic>::Trajectories
SolarSystem::trajectories() const {
  physics::NBodySystem<ICRFJ2000Ecliptic>::Trajectories result;
  for (auto const& trajectory : trajectories_) {
    result.push_back(trajectory.get());
  }
  return result;
}

int SolarSystem::parent(int const index) {
  switch (index) {
    case kSun:
      LOG(FATAL) << __FUNCSIG__ << "The Sun has no parent";
      base::noreturn();
    case kJupiter:
    case kSaturn:
    case kNeptune:
    case kUranus:
    case kEarth:
    case kVenus:
    case kMars:
    case kMercury:
    case kEris:
    case kPluto:
      return kSun;
    case kGanymede:
    case kCallisto:
    case kIo:
    case kEuropa:
      return kJupiter;
    case kTitan:
    case kRhea:
    case kIapetus:
    case kDione:
    case kTethys:
      return kSaturn;
    case kMoon:
      return kEarth;
    case kTriton:
      return kNeptune;
    case kTitania:
    case kOberon:
    case kAriel:
    case kUmbriel:
      return kUranus;
    case kCharon:
      return kPluto;
    default:
      LOG(FATAL) << __FUNCSIG__ << "Undefined index";
      base::noreturn();
  }
}

std::string SolarSystem::name(int const index) {
#define BODY_NAME(name) case k##name: return #name
  switch (index) {
    BODY_NAME(Sun);
    BODY_NAME(Jupiter);
    BODY_NAME(Saturn);
    BODY_NAME(Neptune);
    BODY_NAME(Uranus);
    BODY_NAME(Earth);
    BODY_NAME(Venus);
    BODY_NAME(Mars);
    BODY_NAME(Mercury);
    BODY_NAME(Ganymede);
    BODY_NAME(Titan);
    BODY_NAME(Callisto);
    BODY_NAME(Io);
    BODY_NAME(Moon);
    BODY_NAME(Europa);
    BODY_NAME(Triton);
    BODY_NAME(Eris);
    BODY_NAME(Pluto);
    BODY_NAME(Titania);
    BODY_NAME(Oberon);
    BODY_NAME(Rhea);
    BODY_NAME(Iapetus);
    BODY_NAME(Charon);
    BODY_NAME(Ariel);
    BODY_NAME(Umbriel);
    BODY_NAME(Dione);
    BODY_NAME(Tethys);
    default:
      LOG(FATAL) << __FUNCSIG__ << "Undefined index";
      base::noreturn();
  }
#undef BODY_NAME
}

}  // namespace testing_utilities
}  // namespace principia
