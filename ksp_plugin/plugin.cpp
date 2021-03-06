﻿#include "ksp_plugin/plugin.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <set>

#include "base/not_null.hpp"
#include "base/unique_ptr_logging.hpp"
#include "geometry/affine_map.hpp"
#include "geometry/barycentre_calculator.hpp"
#include "geometry/identity.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/permutation.hpp"
#include "glog/logging.h"
#include "glog/stl_logging.h"

namespace principia {
namespace ksp_plugin {

using base::make_not_null_unique;
using geometry::AffineMap;
using geometry::AngularVelocity;
using geometry::BarycentreCalculator;
using geometry::Bivector;
using geometry::Identity;
using geometry::Permutation;
using quantities::Force;
using si::Radian;

namespace {

// The map between the vector spaces of |World| and |AliceWorld|.
Permutation<World, AliceWorld> const kWorldLookingGlass(
    Permutation<World, AliceWorld>::CoordinatePermutation::XZY);

// The map between the vector spaces of |WorldSun| and |AliceSun|.
Permutation<WorldSun, AliceSun> const kSunLookingGlass(
    Permutation<WorldSun, AliceSun>::CoordinatePermutation::XZY);

}  // namespace

Plugin::Plugin(GUIDToOwnedVessel vessels,
               IndexToOwnedCelestial celestials,
               std::set<not_null<Vessel*> const> dirty_vessels,
               not_null<std::unique_ptr<PhysicsBubble>> bubble,
               Angle planetarium_rotation,
               Instant current_time,
               Index sun_index)
    : vessels_(std::move(vessels)),
      celestials_(std::move(celestials)),
      dirty_vessels_(std::move(dirty_vessels)),
      bubble_(std::move(bubble)),
      n_body_system_(make_not_null_unique<NBodySystem<Barycentric>>()),
      planetarium_rotation_(planetarium_rotation),
      current_time_(current_time),
      // TODO(egg): don't use |find|, use |FindOrDie|.
      sun_(celestials_.find(sun_index)->second.get()) {
  for (auto const& guid_vessel : vessels_) {
    auto const& vessel = guid_vessel.second;
    if (!vessel->is_synchronized()) {
      unsynchronized_vessels_.emplace(vessel.get());
    }
  }
  history_integrator_.Initialize(history_integrator_.Order5Optimal());
  // NOTE(egg): perhaps a lower order would be appropriate.
  prolongation_integrator_.Initialize(history_integrator_.Order5Optimal());
  EndInitialization();
}

not_null<std::unique_ptr<Vessel>> const& Plugin::find_vessel_by_guid_or_die(
    GUID const& vessel_guid) const {
  VLOG(1) << __FUNCTION__ << '\n' << NAMED(vessel_guid);
  auto const it = vessels_.find(vessel_guid);
  CHECK(it != vessels_.end()) << "No vessel with GUID " << vessel_guid;
  VLOG_AND_RETURN(1, it->second);
}

Instant Plugin::current_time() const {
  return current_time_;
}

bool Plugin::has_dirty_vessels() const {
  return !dirty_vessels_.empty();
}

bool Plugin::has_unsynchronized_vessels() const {
  return !unsynchronized_vessels_.empty();
}

bool Plugin::is_dirty(not_null<Vessel*> const vessel) const {
  return dirty_vessels_.count(vessel) > 0;
}

// The map between the vector spaces of |Barycentric| and |WorldSun| at
// |current_time_|.
Rotation<Barycentric, WorldSun> Plugin::PlanetariumRotation() const {
  return Rotation<Barycentric, WorldSun>(
      planetarium_rotation_,
      Bivector<double, Barycentric>({0, 1, 0}));
}

void Plugin::CheckVesselInvariants(
    GUIDToOwnedVessel::const_iterator const it) const {
  not_null<std::unique_ptr<Vessel>> const& vessel = it->second;
  CHECK(vessel->is_initialized()) << "Vessel with GUID " << it->first
                                  << " was not given an initial state";
  // TODO(egg): At the moment, if a vessel is inserted when
  // |current_time_ == HistoryTime()| (that only happens before the first call
  // to |AdvanceTime|) its first step is unsynchronized. This is convenient to
  // test code paths, but it means the invariant is GE, rather than GT.
  CHECK_GE(vessel->prolongation().last().time(), HistoryTime());
  if (unsynchronized_vessels_.count(vessel.get()) > 0) {
    CHECK(!vessel->is_synchronized());
  } else {
    CHECK(vessel->is_synchronized());
    CHECK_EQ(vessel->history().last().time(), HistoryTime());
  }
}

void Plugin::CleanUpVessels() {
  VLOG(1) <<  __FUNCTION__;
  // Remove the vessels which were not updated since last time.
  for (auto it = vessels_.cbegin(); it != vessels_.cend();) {
    // While we're going over the vessels, check invariants.
    CheckVesselInvariants(it);
    not_null<Vessel*> const vessel = it->second.get();
    // Now do the cleanup.
    if (kept_vessels_.erase(vessel)) {
      ++it;
    } else {
      LOG(INFO) << "Removing vessel with GUID " << it->first;
      // Since we are going to delete the vessel, we must remove it from
      // |new_vessels| if it's there.
      if (unsynchronized_vessels_.erase(vessel)) {
        LOG(INFO) << "Vessel had not been synchronized";
      }
      if (dirty_vessels_.erase(vessel)) {
        LOG(INFO) << "Vessel was dirty";
      }
      // |std::map::erase| invalidates its parameter so we post-increment.
      vessels_.erase(it++);
    }
  }
}

void Plugin::EvolveHistories(Instant const& t) {
  VLOG(1) << __FUNCTION__ << '\n' << NAMED(t);
  // Integration with a constant step.
  NBodySystem<Barycentric>::Trajectories trajectories;
  // NOTE(egg): This may be too large, vessels that are not new and in the
  // physics bubble or dirty will not be added.
  trajectories.reserve(vessels_.size() - unsynchronized_vessels_.size() +
                       celestials_.size());
  for (auto const& pair : celestials_) {
    not_null<std::unique_ptr<Celestial>> const& celestial = pair.second;
    trajectories.push_back(celestial->mutable_history());
  }
  for (auto const& pair : vessels_) {
    not_null<Vessel*> const vessel = pair.second.get();
    if (vessel->is_synchronized() &&
        !bubble_->contains(vessel) &&
        !is_dirty(vessel)) {
      trajectories.push_back(vessel->mutable_history());
    }
  }
  VLOG(1) << "Starting the evolution of the histories" << '\n'
          << "from : " << HistoryTime();
  n_body_system_->Integrate(history_integrator_,  // integrator
                            t,                    // tmax
                            Δt_,                  // Δt
                            0,                    // sampling_period
                            false,                // tmax_is_exact
                            trajectories);        // trajectories
  CHECK_GE(HistoryTime(), current_time_);
  VLOG(1) << "Evolved the histories" << '\n'
          << "to   : " << HistoryTime();
}

void Plugin::SynchronizeNewVesselsAndCleanDirtyVessels() {
  VLOG(1) << __FUNCTION__;
  NBodySystem<Barycentric>::Trajectories trajectories;
  trajectories.reserve(celestials_.size() + unsynchronized_vessels_.size() +
                       bubble_->size());
  for (auto const& pair : celestials_) {
    not_null<std::unique_ptr<Celestial>> const& celestial = pair.second;
    trajectories.push_back(celestial->mutable_prolongation());
  }
  for (not_null<Vessel*> const vessel : unsynchronized_vessels_) {
    if (!bubble_->contains(vessel)) {
      trajectories.push_back(vessel->mutable_prolongation());
    }
  }
  for (not_null<Vessel*> const vessel : dirty_vessels_) {
    if (!bubble_->contains(vessel) && vessel->is_synchronized()) {
      trajectories.push_back(vessel->mutable_prolongation());
    }
  }
  if (!bubble_->empty()) {
    trajectories.push_back(bubble_->mutable_centre_of_mass_trajectory());
  }
  VLOG(1) << "Starting the synchronization of the new vessels"
          << (bubble_->empty() ? "" : " and of the bubble");
  n_body_system_->Integrate(prolongation_integrator_,  // integrator
                            HistoryTime(),             // tmax
                            Δt_,                       // Δt
                            0,                         // sampling_period
                            true,                      // tmax_is_exact
                            trajectories);             // trajectories
  if (!bubble_->empty()) {
    SynchronizeBubbleHistories();
  }
  for (not_null<Vessel*> const vessel : unsynchronized_vessels_) {
    CHECK(!bubble_->contains(vessel));
    vessel->CreateHistoryAndForkProlongation(
        HistoryTime(),
        vessel->prolongation().last().degrees_of_freedom());
    dirty_vessels_.erase(vessel);
  }
  unsynchronized_vessels_.clear();
  for (not_null<Vessel*> const vessel : dirty_vessels_) {
    CHECK(!bubble_->contains(vessel));
    vessel->mutable_history()->Append(
        HistoryTime(),
        vessel->prolongation().last().degrees_of_freedom());
  }
  dirty_vessels_.clear();
  VLOG(1) << "Synchronized the new vessels"
          << (bubble_->empty() ? "" : " and the bubble");
}

void Plugin::SynchronizeBubbleHistories() {
  VLOG(1) << __FUNCTION__;
  DegreesOfFreedom<Barycentric> const& centre_of_mass =
      bubble_->centre_of_mass_trajectory().last().degrees_of_freedom();
  for (not_null<Vessel*> const vessel : bubble_->vessels()) {
    RelativeDegreesOfFreedom<Barycentric> const& from_centre_of_mass =
        bubble_->from_centre_of_mass(vessel);
    if (vessel->is_synchronized()) {
      vessel->mutable_history()->Append(
          HistoryTime(),
          centre_of_mass + from_centre_of_mass);
    } else {
      vessel->CreateHistoryAndForkProlongation(
          HistoryTime(),
          centre_of_mass + from_centre_of_mass);
      CHECK(unsynchronized_vessels_.erase(vessel));
    }
    CHECK(dirty_vessels_.erase(vessel));
  }
}

void Plugin::ResetProlongations() {
  VLOG(1) << __FUNCTION__;
  for (auto const& pair : vessels_) {
    not_null<std::unique_ptr<Vessel>> const& vessel = pair.second;
    vessel->ResetProlongation(HistoryTime());
  }
  for (auto const& pair : celestials_) {
    not_null<std::unique_ptr<Celestial>> const& celestial = pair.second;
    celestial->ResetProlongation(HistoryTime());
  }
  VLOG(1) << "Prolongations have been reset";
}

void Plugin::EvolveProlongationsAndBubble(Instant const& t) {
  VLOG(1) << __FUNCTION__ << '\n' << NAMED(t);
  NBodySystem<Barycentric>::Trajectories trajectories;
  trajectories.reserve(vessels_.size() + celestials_.size() -
                       bubble_->number_of_vessels() + bubble_->size());
  for (auto const& pair : celestials_) {
    not_null<std::unique_ptr<Celestial>> const& celestial = pair.second;
    trajectories.push_back(celestial->mutable_prolongation());
  }
  for (auto const& pair : vessels_) {
    not_null<Vessel*> const vessel = pair.second.get();
    if (!bubble_->contains(vessel)) {
      trajectories.push_back(vessel->mutable_prolongation());
    }
  }
  if (!bubble_->empty()) {
    trajectories.push_back(bubble_->mutable_centre_of_mass_trajectory());
  }
  VLOG(1) << "Evolving prolongations"
          << (bubble_->empty() ? "" : " and bubble") << '\n'
          << "from : " << trajectories.front()->last().time() << '\n'
          << "to   : " << t;
  n_body_system_->Integrate(prolongation_integrator_,  // integrator
                            t,                         // tmax
                            Δt_,                       // Δt
                            0,                         // sampling_period
                            true,                      // tmax_is_exact
                            trajectories);             // trajectories
  if (!bubble_->empty()) {
    DegreesOfFreedom<Barycentric> const& centre_of_mass =
        bubble_->centre_of_mass_trajectory().last().degrees_of_freedom();
    for (not_null<Vessel*> vessel : bubble_->vessels()) {
      RelativeDegreesOfFreedom<Barycentric> const& from_centre_of_mass =
          bubble_->from_centre_of_mass(vessel);
      vessel->mutable_prolongation()->Append(
          t,
          centre_of_mass + from_centre_of_mass);
    }
  }
}

Instant const& Plugin::HistoryTime() const {
  return sun_->history().last().time();
}

Plugin::Plugin(Instant const& initial_time,
               Index const sun_index,
               GravitationalParameter const& sun_gravitational_parameter,
               Angle const& planetarium_rotation)
    : bubble_(make_not_null_unique<PhysicsBubble>()),
      n_body_system_(make_not_null_unique<NBodySystem<Barycentric>>()),
      planetarium_rotation_(planetarium_rotation),
      current_time_(initial_time),
      sun_(celestials_.emplace(sun_index,
                               make_not_null_unique<Celestial>(
                                   make_not_null_unique<MassiveBody>(
                                       sun_gravitational_parameter))).
               first->second.get()) {
  sun_->CreateHistoryAndForkProlongation(
      current_time_,
      {Position<Barycentric>(), Velocity<Barycentric>()});
  history_integrator_.Initialize(history_integrator_.Order5Optimal());
  // NOTE(egg): perhaps a lower order would be appropriate.
  prolongation_integrator_.Initialize(history_integrator_.Order5Optimal());
}

void Plugin::InsertCelestial(
    Index const celestial_index,
    GravitationalParameter const& gravitational_parameter,
    Index const parent_index,
    RelativeDegreesOfFreedom<AliceSun> const& from_parent) {
  CHECK(initializing_) << "Celestial bodies should be inserted before the end "
                       << "of initialization";
  auto const it = celestials_.find(parent_index);
  CHECK(it != celestials_.end()) << "No body at index " << parent_index;
  not_null<Celestial const*> parent = it->second.get();
  auto const inserted = celestials_.emplace(
      celestial_index,
      make_not_null_unique<Celestial>(
          make_not_null_unique<MassiveBody>(gravitational_parameter)));
  CHECK(inserted.second) << "Body already exists at index " << celestial_index;
  LOG(INFO) << "Initial |{orbit.pos, orbit.vel}| for celestial at index "
            << celestial_index << ": " << from_parent;
  auto const relative =
      PlanetariumRotation().Inverse()(
          kSunLookingGlass.Inverse()(from_parent));
  LOG(INFO) << "In barycentric coordinates: " << relative;
  not_null<Celestial*> const celestial = inserted.first->second.get();
  celestial->set_parent(parent);
  celestial->CreateHistoryAndForkProlongation(
      current_time_,
      parent->history().last().degrees_of_freedom() + relative);
}

void Plugin::EndInitialization() {
  initializing_.Flop();
}

void Plugin::UpdateCelestialHierarchy(Index const celestial_index,
                                      Index const parent_index) const {
  VLOG(1) << __FUNCTION__ << '\n'
          << NAMED(celestial_index) << '\n' << NAMED(parent_index);
  CHECK(!initializing_);
  auto const it = celestials_.find(celestial_index);
  CHECK(it != celestials_.end()) << "No body at index " << celestial_index;
  auto const it_parent = celestials_.find(parent_index);
  CHECK(it_parent != celestials_.end()) << "No body at index " << parent_index;
  it->second->set_parent(it_parent->second.get());
}

bool Plugin::InsertOrKeepVessel(GUID const& vessel_guid,
                                Index const parent_index) {
  VLOG(1) << __FUNCTION__ << '\n'
          << NAMED(vessel_guid) << '\n' << NAMED(parent_index);
  CHECK(!initializing_);
  auto const it = celestials_.find(parent_index);
  CHECK(it != celestials_.end()) << "No body at index " << parent_index;
  not_null<Celestial const*> parent = it->second.get();
  auto inserted = vessels_.emplace(vessel_guid,
                                   make_not_null_unique<Vessel>(parent));
  not_null<Vessel*> const vessel = inserted.first->second.get();
  kept_vessels_.emplace(vessel);
  vessel->set_parent(parent);
  LOG_IF(INFO, inserted.second) << "Inserted vessel with GUID " << vessel_guid
                                << " at " << vessel;
  VLOG(1) << "Parent of vessel with GUID " << vessel_guid <<" is at index "
          << parent_index;
  return inserted.second;
}

void Plugin::SetVesselStateOffset(
    GUID const& vessel_guid,
    RelativeDegreesOfFreedom<AliceSun> const& from_parent) {
  VLOG(1) << __FUNCTION__ << '\n'
          << NAMED(vessel_guid) << '\n' << NAMED(from_parent);
  CHECK(!initializing_);
  not_null<std::unique_ptr<Vessel>> const& vessel =
      find_vessel_by_guid_or_die(vessel_guid);
  CHECK(!vessel->is_initialized())
      << "Vessel with GUID " << vessel_guid << " already has a trajectory";
  LOG(INFO) << "Initial |{orbit.pos, orbit.vel}| for vessel with GUID "
            << vessel_guid << ": " << from_parent;
  RelativeDegreesOfFreedom<Barycentric> const relative =
      PlanetariumRotation().Inverse()(
          kSunLookingGlass.Inverse()(from_parent));
  LOG(INFO) << "In barycentric coordinates: " << relative;
  vessel->CreateProlongation(
      current_time_,
      vessel->parent().prolongation().last().degrees_of_freedom() + relative);
  auto const inserted = unsynchronized_vessels_.emplace(vessel.get());
  CHECK(inserted.second);
}

void Plugin::AdvanceTime(Instant const& t, Angle const& planetarium_rotation) {
  VLOG(1) << __FUNCTION__ << '\n'
          << NAMED(t) << '\n' << NAMED(planetarium_rotation);
  CHECK(!initializing_);
  CHECK_GT(t, current_time_);
  CleanUpVessels();
  bubble_->Prepare(PlanetariumRotation(), current_time_, t);
  if (HistoryTime() + Δt_ < t) {
    // The histories are far enough behind that we can advance them at least one
    // step and reset the prolongations.
    EvolveHistories(t);
    // TODO(egg): I think |!bubble_->empty()| => |has_dirty_vessels()|.
    if (has_unsynchronized_vessels() ||
        has_dirty_vessels() ||
        !bubble_->empty()) {
      SynchronizeNewVesselsAndCleanDirtyVessels();
    }
    ResetProlongations();
  }
  EvolveProlongationsAndBubble(t);
  VLOG(1) << "Time has been advanced" << '\n'
          << "from : " << current_time_ << '\n'
          << "to   : " << t;
  current_time_ = t;
  planetarium_rotation_ = planetarium_rotation;
}

RelativeDegreesOfFreedom<AliceSun> Plugin::VesselFromParent(
    GUID const& vessel_guid) const {
  CHECK(!initializing_);
  not_null<std::unique_ptr<Vessel>> const& vessel =
      find_vessel_by_guid_or_die(vessel_guid);
  CHECK(vessel->is_initialized()) << "Vessel with GUID " << vessel_guid
                                  << " was not given an initial state";
  RelativeDegreesOfFreedom<Barycentric> const barycentric_result =
      vessel->prolongation().last().degrees_of_freedom() -
      vessel->parent().prolongation().last().degrees_of_freedom();
  RelativeDegreesOfFreedom<AliceSun> const result =
      kSunLookingGlass(PlanetariumRotation()(barycentric_result));
  VLOG(1) << "Vessel with GUID " << vessel_guid
          << " is at parent degrees of freedom + " << barycentric_result
          << " Barycentre (" << result << " AliceSun)";
  return result;
}

RelativeDegreesOfFreedom<AliceSun> Plugin::CelestialFromParent(
    Index const celestial_index) const {
  CHECK(!initializing_);
  auto const it = celestials_.find(celestial_index);
  CHECK(it != celestials_.end()) << "No body at index " << celestial_index;
  Celestial const& celestial = *it->second;
  CHECK(celestial.has_parent())
      << "Body at index " << celestial_index << " is the sun";
  RelativeDegreesOfFreedom<Barycentric> const barycentric_result =
      celestial.prolongation().last().degrees_of_freedom() -
      celestial.parent().prolongation().last().degrees_of_freedom();
  RelativeDegreesOfFreedom<AliceSun> const result =
      kSunLookingGlass(PlanetariumRotation()(barycentric_result));
  VLOG(1) << "Celestial at index " << celestial_index
          << " is at parent degrees of freedom + " << barycentric_result
          << " Barycentre (" << result << " AliceSun)";
  return result;
}

RenderedTrajectory<World> Plugin::RenderedVesselTrajectory(
    GUID const& vessel_guid,
    not_null<Transforms<Barycentric, Rendering, Barycentric>*> const transforms,
    Position<World> const& sun_world_position) const {
  CHECK(!initializing_);
  auto const to_world =
      AffineMap<Barycentric, World, Length, Rotation>(
          sun_->prolongation().last().degrees_of_freedom().position(),
          sun_world_position,
          Rotation<WorldSun, World>::Identity() * PlanetariumRotation());
  not_null<std::unique_ptr<Vessel>> const& vessel =
      find_vessel_by_guid_or_die(vessel_guid);
  CHECK(vessel->is_initialized());
  VLOG(1) << "Rendering a trajectory for the vessel with GUID " << vessel_guid;
  RenderedTrajectory<World> result;
  if (!vessel->is_synchronized()) {
    // TODO(egg): We render neither unsynchronized histories nor prolongations
    // at the moment.
    VLOG(1) << "Returning an empty trajectory";
    return result;
  }

  // Compute the apparent trajectory using the given |transforms|.
  Trajectory<Barycentric> const& actual_trajectory = vessel->history();

  // First build the trajectory resulting from the first transform.
  Trajectory<Rendering> intermediate_trajectory(actual_trajectory.body<Body>());
  for (auto actual_it = transforms->first(actual_trajectory);
       !actual_it.at_end();
       ++actual_it) {
    intermediate_trajectory.Append(actual_it.time(),
                                   actual_it.degrees_of_freedom());
  }

  // Then build the apparent trajectory using the second transform.
  auto apparent_trajectory = make_not_null_unique<Trajectory<Barycentric>>(
                                 actual_trajectory.body<Body>());
  for (auto intermediate_it = transforms->second(intermediate_trajectory);
       !intermediate_it.at_end();
       ++intermediate_it) {
    apparent_trajectory->Append(intermediate_it.time(),
                                intermediate_it.degrees_of_freedom());
  }

  // Finally use the apparent trajectory to build the result.
  auto initial_it = apparent_trajectory->first();
  if (!initial_it.at_end()) {
    for (auto final_it = initial_it;
         ++final_it, !final_it.at_end();
         initial_it = final_it) {
      result.emplace_back(to_world(initial_it.degrees_of_freedom().position()),
                          to_world(final_it.degrees_of_freedom().position()));
    }
  }
  VLOG(1) << "Returning a " << result.size() << "-segment trajectory";
  return result;
}

not_null<std::unique_ptr<Transforms<Barycentric, Rendering, Barycentric>>>
Plugin::NewBodyCentredNonRotatingTransforms(
    Index const reference_body_index) const {
  auto const it = celestials_.find(reference_body_index);
  CHECK(it != celestials_.end());
  not_null<Celestial const*> reference_body = it->second.get();
  Transforms<Barycentric, Rendering, Barycentric>::
      LazyTrajectory<Barycentric> const reference_body_prolongation =
          std::bind(&Celestial::prolongation, reference_body);
  return Transforms<Barycentric, Rendering, Barycentric>::
             BodyCentredNonRotating(reference_body_prolongation,
                                    reference_body_prolongation);
}

not_null<std::unique_ptr<Transforms<Barycentric, Rendering, Barycentric>>>
Plugin::NewBarycentricRotatingTransforms(Index const primary_index,
                                         Index const secondary_index) const {
  auto const primary_it = celestials_.find(primary_index);
  CHECK(primary_it != celestials_.end());
  not_null<Celestial const*> primary = primary_it->second.get();
  auto const secondary_it = celestials_.find(secondary_index);
  CHECK(secondary_it != celestials_.end());
  not_null<Celestial const*> secondary = secondary_it->second.get();
  Transforms<Barycentric, Rendering, Barycentric>::
      LazyTrajectory<Barycentric> const primary_prolongation =
          std::bind(&Celestial::prolongation, primary);
  Transforms<Barycentric, Rendering, Barycentric>::
      LazyTrajectory<Barycentric> const secondary_prolongation =
          std::bind(&Celestial::prolongation, secondary);
  return Transforms<Barycentric, Rendering, Barycentric>::BarycentricRotating(
             primary_prolongation,
             primary_prolongation,
             secondary_prolongation,
             secondary_prolongation);
}

Position<World> Plugin::VesselWorldPosition(
    GUID const& vessel_guid,
    Position<World> const& parent_world_position) const {
  not_null<std::unique_ptr<Vessel>> const& vessel =
      find_vessel_by_guid_or_die(vessel_guid);
  CHECK(vessel->is_initialized()) << "Vessel with GUID " << vessel_guid
                                 << " was not given an initial state";
  auto const to_world =
      AffineMap<Barycentric, World, Length, Rotation>(
          vessel->parent().
              prolongation().last().degrees_of_freedom().position(),
          parent_world_position,
          Rotation<WorldSun, World>::Identity() * PlanetariumRotation());
  return to_world(
      vessel->prolongation().last().degrees_of_freedom().position());
}

Velocity<World> Plugin::VesselWorldVelocity(
      GUID const& vessel_guid,
      Velocity<World> const& parent_world_velocity,
      Time const& parent_rotation_period) const {
  not_null<std::unique_ptr<Vessel>> const& vessel =
      find_vessel_by_guid_or_die(vessel_guid);
  CHECK(vessel->is_initialized()) << "Vessel with GUID " << vessel_guid
                                  << " was not given an initial state";
  Rotation<Barycentric, World> to_world =
      Rotation<WorldSun, World>::Identity() * PlanetariumRotation();
  RelativeDegreesOfFreedom<Barycentric> const relative_to_parent =
      vessel->prolongation().last().degrees_of_freedom() -
      vessel->parent().prolongation().last().degrees_of_freedom();
  AngularVelocity<Barycentric> const world_frame_angular_velocity =
      AngularVelocity<Barycentric>({0 * Radian / Second,
                                    2 * π * Radian / parent_rotation_period,
                                    0 * Radian / Second});
  return to_world(
      (world_frame_angular_velocity *
       relative_to_parent.displacement()) / Radian +
      relative_to_parent.velocity()) + parent_world_velocity;
}

void Plugin::AddVesselToNextPhysicsBubble(
    GUID const& vessel_guid,
    std::vector<IdAndOwnedPart> parts) {
  VLOG(1) << __FUNCTION__ << '\n' << NAMED(vessel_guid) << '\n' << NAMED(parts);
  not_null<std::unique_ptr<Vessel>> const& vessel =
      find_vessel_by_guid_or_die(vessel_guid);
  dirty_vessels_.insert(vessel.get());
  bubble_->AddVesselToNext(vessel.get(), std::move(parts));
}

Displacement<World> Plugin::BubbleDisplacementCorrection(
    Position<World> const& sun_world_position) const {
  VLOG(1) << __FUNCTION__ << '\n' << NAMED(sun_world_position);
  VLOG_AND_RETURN(1, bubble_->DisplacementCorrection(PlanetariumRotation(),
                                                    *sun_,
                                                    sun_world_position));
}

bool Plugin::PhysicsBubbleIsEmpty() const {
  VLOG(1) << __FUNCTION__;
  VLOG_AND_RETURN(1, bubble_->empty());
}

Velocity<World> Plugin::BubbleVelocityCorrection(
    Index const reference_body_index) const {
  VLOG(1) << __FUNCTION__ << '\n' << NAMED(reference_body_index);
  auto const found = celestials_.find(reference_body_index);
  CHECK(found != celestials_.end());
  Celestial const& reference_body = *found->second;
  VLOG_AND_RETURN(1, bubble_->VelocityCorrection(PlanetariumRotation(),
                                                reference_body));
}

void Plugin::WriteToMessage(
    not_null<serialization::Plugin*> const message) const {
  CHECK(!initializing_);
  std::map<not_null<Celestial const*>, Index const> celestial_to_index;
  for (auto const& index_celestial : celestials_) {
    celestial_to_index.emplace(index_celestial.second.get(),
                               index_celestial.first);
  }
  for (auto const& index_celestial : celestials_) {
    Index const index = index_celestial.first;
    not_null<Celestial const*> const celestial = index_celestial.second.get();
    auto const celestial_message = message->add_celestial();
    celestial_message->set_index(index);
    celestial->WriteToMessage(celestial_message->mutable_celestial());
    if (celestial->has_parent()) {
      auto const it = celestial_to_index.find(&celestial->parent());
      CHECK(it != celestial_to_index.end());
      Index const parent_index = it->second;
      celestial_message->set_parent_index(parent_index);
    }
  }
  std::map<not_null<Vessel const*>, GUID const> vessel_to_guid;
  for (auto const& guid_vessel : vessels_) {
    std::string const& guid = guid_vessel.first;
    not_null<Vessel*> const vessel = guid_vessel.second.get();
    vessel_to_guid.emplace(vessel, guid);
    auto* const vessel_message = message->add_vessel();
    vessel_message->set_guid(guid);
    vessel->WriteToMessage(vessel_message->mutable_vessel());
    auto const it = celestial_to_index.find(&vessel->parent());
    CHECK(it != celestial_to_index.end());
    Index const parent_index = it->second;
    vessel_message->set_parent_index(parent_index);
    vessel_message->set_dirty(is_dirty(vessel));
  }

  bubble_->WriteToMessage(
      [&vessel_to_guid](not_null<Vessel const*> const vessel) -> GUID {
        auto const it = vessel_to_guid.find(vessel);
        CHECK(it != vessel_to_guid.end());
        return it->second;
      },
      message->mutable_bubble());

  planetarium_rotation_.WriteToMessage(message->mutable_planetarium_rotation());
  current_time_.WriteToMessage(message->mutable_current_time());
  auto const it = celestial_to_index.find(sun_);
  CHECK(it != celestial_to_index.end());
  Index const sun_index = it->second;
  message->set_sun_index(sun_index);
}

std::unique_ptr<Plugin> Plugin::ReadFromMessage(
    serialization::Plugin const& message) {
  IndexToOwnedCelestial celestials;
  for (auto const& celestial_message : message.celestial()) {
    celestials.emplace(
        celestial_message.index(),
        Celestial::ReadFromMessage(celestial_message.celestial()));
  }
  for (auto const& celestial_message : message.celestial()) {
    if (celestial_message.has_parent_index()) {
      auto const it = celestials.find(celestial_message.index());
      CHECK(it != celestials.end());
      not_null<std::unique_ptr<Celestial>> const& celestial = it->second;
      auto const parent_it = celestials.find(celestial_message.parent_index());
      CHECK(parent_it != celestials.end());
      not_null<Celestial const*> const parent = parent_it->second.get();
      celestial->set_parent(parent);
    }
  }
  GUIDToOwnedVessel vessels;
  std::set<not_null<Vessel*> const> dirty_vessels;
  for (auto const& vessel_message : message.vessel()) {
    auto const parent_it = celestials.find(vessel_message.parent_index());
    CHECK(parent_it != celestials.end());
    not_null<Celestial const*> const parent = parent_it->second.get();
    not_null<std::unique_ptr<Vessel>> vessel =
        Vessel::ReadFromMessage(vessel_message.vessel(), parent);
    if (vessel_message.dirty()) {
      dirty_vessels.emplace(vessel.get());
    }
    auto const inserted =
        vessels.emplace(vessel_message.guid(), std::move(vessel));
    CHECK(inserted.second);
  }
  not_null<std::unique_ptr<PhysicsBubble>> bubble =
      PhysicsBubble::ReadFromMessage(
          [&vessels](GUID guid) -> not_null<Vessel*> {
            auto const it = vessels.find(guid);
            CHECK(it != vessels.end());
            return it->second.get();
          },
          message.bubble());
  // Can't use |make_unique| here without implementation-dependent friendships.
  return std::unique_ptr<Plugin>(
      new Plugin(std::move(vessels),
                 std::move(celestials),
                 std::move(dirty_vessels),
                 std::move(bubble),
                 Angle::ReadFromMessage(message.planetarium_rotation()),
                 Instant::ReadFromMessage(message.current_time()),
                 message.sun_index()));
}

}  // namespace ksp_plugin
}  // namespace principia
