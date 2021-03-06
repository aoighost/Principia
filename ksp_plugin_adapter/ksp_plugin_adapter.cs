﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace principia {
namespace ksp_plugin_adapter {

[KSPAddon(startup : KSPAddon.Startup.MainMenu, once : false)]
public partial class PluginAdapter : UnityEngine.MonoBehaviour {
  // This constant can be at most 32766, since Vectrosity imposes a maximum of
  // 65534 vertices, where there are 2 vertices per point on discrete lines.  We
  // want this to be even since we have two points per line segment.
  // NOTE(egg): Things are fairly slow with the maximum number of points.  We
  // have to do with fewer.  10000 is mostly ok, even fewer would be better.
  // TODO(egg): At the moment we store points in the history every
  // 10 n seconds, where n is maximal such that 10 n seconds is less than the
  // length of a |FixedUpdate|. This means we sometimes have very large gaps.
  // We should store *all* points of the history, then decimate for rendering.
  // This means splines are not needed, since 10 s is small enough to give the
  // illusion of continuity on the scales we are dealing with, and cubics are
  // rendered by Vectrosity as line segments, so that a cubic rendered as
  // 10 segments counts 20 towards |kLinePoints| (and probably takes as long to
  // render as 10 segments from the actual data, with extra overhead for
  // the evaluation of the cubic).
  private const int kLinePoints = 10000;

  private const int kGUIQueueSpot = 3;

  private UnityEngine.Rect main_window_rectangle_;
  private IntPtr plugin_ = IntPtr.Zero;
  // TODO(egg): rendering only one trajectory at the moment.
  private VectorLine rendered_trajectory_;
  private IntPtr transforms_ = IntPtr.Zero;
  private int first_selected_celestial_ = 0;
  private int second_selected_celestial_ = 0;

  private bool show_logging_settings_ = false;
  private bool show_reference_frame_selection_ = true;

  private bool time_is_advancing_;

  private DateTime last_plugin_reset_;

  private Krakensbane krakensbane_;

  private static bool an_instance_is_loaded_;

  PluginAdapter() {
    // We create this directory here so we do not need to worry about cross-
    // platform problems in C++.
    System.IO.Directory.CreateDirectory("glog/Principia");
    Log.InitGoogleLogging();
  }

  ~PluginAdapter() {
    Cleanup();
  }

  private bool PluginRunning() {
    return plugin_ != IntPtr.Zero;
  }

  private delegate void BodyProcessor(CelestialBody body);
  private delegate void VesselProcessor(Vessel vessel);

  // Applies |process_body| to all bodies but the sun in the tree of celestial
  // bodies, in topological order.
  private void ApplyToBodyTree(BodyProcessor process_body) {
    // Tree traversal (DFS, not that it matters).
    Stack<CelestialBody> stack = new Stack<CelestialBody>();
    foreach (CelestialBody child in Planetarium.fetch.Sun.orbitingBodies) {
      stack.Push(child);
    }
    CelestialBody body;
    while (stack.Count > 0) {
      body = stack.Pop();
      process_body(body);
      foreach (CelestialBody child in body.orbitingBodies) {
        stack.Push(child);
      }
    }
  }

  // Applies |process_vessel| to all vessels in space.
  private void ApplyToVesselsOnRailsOrInInertialPhysicsBubbleInSpace(
      VesselProcessor process_vessel) {
    var vessels = from vessel in FlightGlobals.Vessels
                  where is_on_rails_in_space(vessel) ||
                        is_in_inertial_physics_bubble_in_space(vessel)
                  select vessel;
    foreach (Vessel vessel in vessels) {
      process_vessel(vessel);
    }
  }

  private void ApplyToVesselsInPhysicsBubble(VesselProcessor process_vessel) {
    var vessels = from vessel in FlightGlobals.Vessels
                  where is_in_physics_bubble(vessel)
                  select vessel;
    foreach (Vessel vessel in vessels) {
      process_vessel(vessel);
    }
  }

  private void UpdateBody(CelestialBody body, double universal_time) {
    UpdateCelestialHierarchy(plugin_,
                             body.flightGlobalsIndex,
                             body.orbit.referenceBody.flightGlobalsIndex);
    QP from_parent = CelestialFromParent(plugin_, body.flightGlobalsIndex);
    // TODO(egg): Some of this might be be superfluous and redundant.
    Orbit original = body.orbit;
    Orbit copy = new Orbit(original.inclination, original.eccentricity,
                           original.semiMajorAxis, original.LAN,
                           original.argumentOfPeriapsis,
                           original.meanAnomalyAtEpoch, original.epoch,
                           original.referenceBody);
    copy.UpdateFromStateVectors((Vector3d)from_parent.q,
                                (Vector3d)from_parent.p,
                                copy.referenceBody,
                                universal_time);
    body.orbit.inclination = copy.inclination;
    body.orbit.eccentricity = copy.eccentricity;
    body.orbit.semiMajorAxis = copy.semiMajorAxis;
    body.orbit.LAN = copy.LAN;
    body.orbit.argumentOfPeriapsis = copy.argumentOfPeriapsis;
    body.orbit.meanAnomalyAtEpoch = copy.meanAnomalyAtEpoch;
    body.orbit.epoch = copy.epoch;
    body.orbit.referenceBody = copy.referenceBody;
    body.orbit.Init();
    body.orbit.UpdateFromUT(universal_time);
    body.CBUpdate();
    body.orbit.UpdateFromStateVectors((Vector3d)from_parent.q,
                                      (Vector3d)from_parent.p,
                                      copy.referenceBody,
                                      universal_time);
  }

  private void UpdateVessel(Vessel vessel, double universal_time) {
    bool inserted = InsertOrKeepVessel(
        plugin_,
        vessel.id.ToString(),
        vessel.orbit.referenceBody.flightGlobalsIndex);
    if (inserted) {
      SetVesselStateOffset(plugin      : plugin_,
                           vessel_guid : vessel.id.ToString(),
                           from_parent : new QP{q = (XYZ)vessel.orbit.pos,
                                                p = (XYZ)vessel.orbit.vel});
    }
    QP from_parent = VesselFromParent(plugin_, vessel.id.ToString());
    // NOTE(egg): Here we work around a KSP bug: |Orbit.pos| for a vessel
    // corresponds to the position one timestep in the future.  This is not
    // the case for celestial bodies.
    vessel.orbit.UpdateFromStateVectors(
        pos     : (Vector3d)from_parent.q +
                  (Vector3d)from_parent.p * UnityEngine.Time.deltaTime,
        vel     : (Vector3d)from_parent.p,
        refBody : vessel.orbit.referenceBody,
        UT      : universal_time);
  }

  private void AddToPhysicsBubble(Vessel vessel) {
    Vector3d gravity =
        FlightGlobals.getGeeForceAtPosition(vessel.findWorldCenterOfMass());
    Vector3d kraken_velocity = Krakensbane.GetFrameVelocity();
    KSPPart[] parts =
        (from part in vessel.parts
         where part.rb != null  // Physicsless parts have no rigid body.
         select new KSPPart {
             world_position = (XYZ)(Vector3d)part.rb.worldCenterOfMass,
             world_velocity = (XYZ)(kraken_velocity + part.rb.velocity),
             mass = (double)part.mass + (double)part.GetResourceMass(),
             gravitational_acceleration_to_be_applied_by_ksp = (XYZ)gravity,
             id = part.flightID}).ToArray();
    if (parts.Count() > 0) {
      bool inserted = InsertOrKeepVessel(
          plugin_,
          vessel.id.ToString(),
          vessel.orbit.referenceBody.flightGlobalsIndex);
      if (inserted) {
        // NOTE(egg): this is only used when a (plugin-managed) physics bubble
        // appears with a new vessel (e.g. when exiting the atmosphere).
        // TODO(egg): these degrees of freedom are off by one Δt and we don't
        // compensate for the pos/vel synchronization bug.
        SetVesselStateOffset(plugin      : plugin_,
                             vessel_guid : vessel.id.ToString(),
                             from_parent : new QP{q = (XYZ)vessel.orbit.pos,
                                                  p = (XYZ)vessel.orbit.vel});
      }
      AddVesselToNextPhysicsBubble(plugin      : plugin_,
                                   vessel_guid : vessel.id.ToString(),
                                   parts       : parts,
                                   count       : parts.Count());
    }
  }

  private bool is_in_space(Vessel vessel) {
    return vessel.state != Vessel.State.DEAD &&
           (vessel.situation == Vessel.Situations.SUB_ORBITAL ||
            vessel.situation == Vessel.Situations.ORBITING ||
            vessel.situation == Vessel.Situations.ESCAPING);
  }

  private bool is_on_rails_in_space(Vessel vessel) {
    return vessel.packed && is_in_space(vessel);
  }

  private bool is_in_physics_bubble(Vessel vessel) {
    return !vessel.packed && vessel.loaded;
  }

  private bool is_in_inertial_physics_bubble_in_space(Vessel vessel) {
    return is_in_physics_bubble(vessel) &&
           !Planetarium.FrameIsRotating() &&
           is_in_space(vessel);
  }

  private bool has_inertial_physics_bubble_in_space() {
    Vessel active_vessel = FlightGlobals.ActiveVessel;
    return active_vessel != null &&
           is_in_inertial_physics_bubble_in_space(active_vessel);
  }

  #region Unity Lifecycle
  // See the Unity manual on execution order for more information on |Start()|,
  // |OnDestroy()| and |FixedUpdate()|.
  // http://docs.unity3d.com/Manual/ExecutionOrder.html

  // Awake is called once.
  private void Awake() {
    Log.Info("principia.ksp_plugin_adapter.PluginAdapter.Awake()");
    if (an_instance_is_loaded_) {
      Log.Info("an instance was loaded");
      UnityEngine.Object.Destroy(gameObject);
    } else {
      UnityEngine.Object.DontDestroyOnLoad(gameObject);
      an_instance_is_loaded_ = true;
    }
    GameEvents.onGameStateLoad.Add(InitializeOnGameStateLoad);
    main_window_rectangle_ = new UnityEngine.Rect(
        left   : UnityEngine.Screen.width / 2.0f,
        top    : UnityEngine.Screen.height / 3.0f,
        width  : 10,
        height : 10);
  }

  private void OnGUI() {
    UnityEngine.GUI.skin = HighLogic.Skin;
    main_window_rectangle_ = UnityEngine.GUILayout.Window(
        id         : 1,
        screenRect : main_window_rectangle_,
        func       : DrawMainWindow,
        text       : "Traces of Various Descriptions",
        options    : UnityEngine.GUILayout.MinWidth(500));
  }

  private void FixedUpdate() {
    if (PluginRunning()) {
      double universal_time = Planetarium.GetUniversalTime();
      double plugin_time = current_time(plugin_);
      if (plugin_time > universal_time) {
        Log.Fatal("Closed Timelike Curve");
      } else if (plugin_time == universal_time) {
        time_is_advancing_ = false;
        return;
      }
      time_is_advancing_ = true;
      if (has_inertial_physics_bubble_in_space()) {
        ApplyToVesselsInPhysicsBubble(AddToPhysicsBubble);
      }
      AdvanceTime(plugin_, universal_time, Planetarium.InverseRotAngle);
      ApplyToBodyTree(body => UpdateBody(body, universal_time));
      ApplyToVesselsOnRailsOrInInertialPhysicsBubbleInSpace(
          vessel => UpdateVessel(vessel, universal_time));
      Vessel active_vessel = FlightGlobals.ActiveVessel;
      if (!PhysicsBubbleIsEmpty(plugin_)) {
        Vector3d displacement_offset =
            (Vector3d)BubbleDisplacementCorrection(
                          plugin_,
                          (XYZ)Planetarium.fetch.Sun.position);
        Vector3d velocity_offset =
            (Vector3d)BubbleVelocityCorrection(
                          plugin_,
                          active_vessel.orbit.referenceBody.flightGlobalsIndex);
        if (krakensbane_ == null) {
          krakensbane_ = (Krakensbane)FindObjectOfType(typeof(Krakensbane));
        }
        krakensbane_.setOffset(displacement_offset);
        krakensbane_.FrameVel += velocity_offset;
      }
      if (MapView.MapIsEnabled &&
          active_vessel != null &&
          (is_on_rails_in_space(active_vessel) ||
           is_in_inertial_physics_bubble_in_space(active_vessel))) {
        if (active_vessel.orbitDriver.Renderer.drawMode !=
                OrbitRenderer.DrawMode.OFF ||
            active_vessel.orbitDriver.Renderer.drawIcons !=
                OrbitRenderer.DrawIcons.OBJ ||
            active_vessel.patchedConicRenderer != null) {
          Log.Info("Removing orbit rendering for the active vessel");
          active_vessel.orbitDriver.Renderer.drawMode =
              OrbitRenderer.DrawMode.OFF;
          active_vessel.orbitDriver.Renderer.drawIcons =
              OrbitRenderer.DrawIcons.OBJ;
          active_vessel.DetachPatchedConicsSolver();
          active_vessel.patchedConicRenderer = null;
        }
        IntPtr trajectory_iterator = IntPtr.Zero;
        try {
          trajectory_iterator = RenderedVesselTrajectory(
              plugin_,
              active_vessel.id.ToString(),
              transforms_,
              (XYZ)Planetarium.fetch.Sun.position);

          LineSegment segment;
          int index_in_line_points = kLinePoints -
              NumberOfSegments(trajectory_iterator) * 2;
          while (index_in_line_points < 0) {
            FetchAndIncrement(trajectory_iterator);
            index_in_line_points += 2;
          }
          if (rendered_trajectory_ == null) {
            ResetRenderedTrajectory();
          }
          while (!AtEnd(trajectory_iterator)) {
            segment = FetchAndIncrement(trajectory_iterator);
            // TODO(egg): should we do the |LocalToScaledSpace| conversion in
            // native code?
            // TODO(egg): could we directly assign to
            // |rendered_trajectory_.points3| from C++ using unsafe code and
            // something like the following?
            // |fixed (UnityEngine.Vector3* pts = rendered_trajectory_.points3)|
            rendered_trajectory_.points3[index_in_line_points++] =
                ScaledSpace.LocalToScaledSpace((Vector3d)segment.begin);
            rendered_trajectory_.points3[index_in_line_points++] =
                ScaledSpace.LocalToScaledSpace((Vector3d)segment.end);
          }
        } finally {
          DeleteLineAndIterator(ref trajectory_iterator);
        }
        if (MapView.Draw3DLines) {
          Vector.DrawLine3D(rendered_trajectory_);
        } else {
          Vector.DrawLine(rendered_trajectory_);
        }
      } else {
        DestroyRenderedTrajectory();
      }
    }
  }

  #endregion

  private void ResetRenderedTrajectory() {
    DestroyRenderedTrajectory();
    rendered_trajectory_ = new VectorLine(
        lineName     : "rendered_trajectory_",
        linePoints   : new UnityEngine.Vector3[kLinePoints],
        lineMaterial : MapView.OrbitLinesMaterial,
        color        : XKCDColors.AcidGreen,
        width        : 5,
        lineType     : LineType.Discrete);
    rendered_trajectory_.vectorObject.transform.parent =
        ScaledSpace.Instance.transform;
    rendered_trajectory_.vectorObject.renderer.castShadows = false;
    rendered_trajectory_.vectorObject.renderer.receiveShadows = false;
    rendered_trajectory_.layer = 31;
  }

  private void DestroyRenderedTrajectory() {
    if (rendered_trajectory_ != null) {
      Vector.DestroyLine(ref rendered_trajectory_);
    }
  }

  private void Cleanup() {
    DeletePlugin(ref plugin_);
    DeleteTransforms(ref transforms_);
    DestroyRenderedTrajectory();
  }

  private void InitializeOnGameStateLoad(ConfigNode node) {
    // TODO(egg): Here loading of the persisted game state should occur, or
    // initialization should be scheduled.  Without persistence, we get plugin
    // resets at every scene change or vessel switch, so we do nothing.
  }

  private void DrawMainWindow(int window_id) {
    UnityEngine.GUILayout.BeginVertical();
    String plugin_state;
    if (PluginRunning()) {
      if (UnityEngine.GUILayout.Button(text : "Stop Plugin")) {
        Cleanup();
      }
    } else {
      if (UnityEngine.GUILayout.Button(text : "Start Plugin")) {
        ResetPlugin();
      }
    }
    if (!PluginRunning()) {
      plugin_state = "not started";
    } else if (!time_is_advancing_) {
      plugin_state = "holding";
    } else if (PhysicsBubbleIsEmpty(plugin_)) {
      plugin_state = "running";
    } else {
      plugin_state = "managing physics bubble";
    }
    UnityEngine.GUILayout.TextArea(text : "Plugin is " + plugin_state);
    String last_reset_information;
    if (!PluginRunning()) {
      last_reset_information = "";
    } else {
      last_reset_information =
          "Plugin was started at " +
          last_plugin_reset_.ToUniversalTime().ToString("O");
    }
    UnityEngine.GUILayout.TextArea(last_reset_information);
    ToggleableSection(name   : "Reference Frame Selection",
                      show   : ref show_reference_frame_selection_,
                      render : ReferenceFrameSelection);
    ToggleableSection(name   : "Logging Settings",
                      show   : ref show_logging_settings_,
                      render : LoggingSettings);
    UnityEngine.GUILayout.EndVertical();
    UnityEngine.GUI.DragWindow(
        position : new UnityEngine.Rect(left : 0f, top : 0f, width : 10000f,
                                        height : 10000f));
  }

  delegate void GUIRenderer();

  private void ToggleableSection(String name,
                                 ref bool show,
                                 GUIRenderer render) {
    String toggle = show ? "↑ " + name + " ↑"
                         : "↓ " + name + " ↓";
    if (UnityEngine.GUILayout.Button(toggle)) {
      show = !show;
      if (!show) {
        ShrinkMainWindow();
      }
    }
    if (show) {
      render();
    }
  }

  private void ReferenceFrameSelection() {
    bool barycentric_rotating =
        first_selected_celestial_ != second_selected_celestial_;
    String reference_frame_description =
        "The trajectory of the active vessel is plotted in ";
    if (barycentric_rotating) {
      reference_frame_description +=
          "the reference frame fixing the barycentre of " +
          FlightGlobals.Bodies[first_selected_celestial_].theName + " and " +
          FlightGlobals.Bodies[second_selected_celestial_].theName + ", " +
          "the line through them, and the plane in which they move about the " +
          "barycentre.";
    } else {
      reference_frame_description +=
          "the nonrotating reference frame fixing the centre of " +
          FlightGlobals.Bodies[first_selected_celestial_].theName + ".";
    }
    UnityEngine.GUILayout.TextArea(
        text    : reference_frame_description,
        options : UnityEngine.GUILayout.Height(100));
    foreach (CelestialBody celestial in FlightGlobals.Bodies) {
      bool changed_rendering = false;
      UnityEngine.GUILayout.BeginHorizontal();
      if (UnityEngine.GUILayout.Toggle(
              value : first_selected_celestial_ == celestial.flightGlobalsIndex,
              text  : "") &&
          first_selected_celestial_ != celestial.flightGlobalsIndex) {
        first_selected_celestial_ = celestial.flightGlobalsIndex;
        changed_rendering = true;
      }
      if (UnityEngine.GUILayout.Toggle(
              value : second_selected_celestial_ ==
                          celestial.flightGlobalsIndex,
              text  : celestial.name) &&
          second_selected_celestial_ != celestial.flightGlobalsIndex) {
        second_selected_celestial_ = celestial.flightGlobalsIndex;
        changed_rendering = true;
      }
      UnityEngine.GUILayout.EndHorizontal();
      if (changed_rendering && PluginRunning()) {
        UpdateRenderingFrame();
      }
    }
  }

  private void LoggingSettings() {
    UnityEngine.GUILayout.BeginHorizontal();
    UnityEngine.GUILayout.Label(text : "Verbose level:");
    if (UnityEngine.GUILayout.Button(
            text    : "←",
            options : UnityEngine.GUILayout.Width(50))) {
      Log.SetVerboseLogging(Math.Max(Log.GetVerboseLogging() - 1, 0));
    }
    UnityEngine.GUILayout.TextArea(
        text    : Log.GetVerboseLogging().ToString(),
        options : UnityEngine.GUILayout.Width(50));
    if (UnityEngine.GUILayout.Button(
            text    : "→",
            options : UnityEngine.GUILayout.Width(50))) {
      Log.SetVerboseLogging(Math.Min(Log.GetVerboseLogging() + 1, 4));
    }
    UnityEngine.GUILayout.EndHorizontal();
    int column_width = 75;
    UnityEngine.GUILayout.BeginHorizontal();
    UnityEngine.GUILayout.Space(column_width);
    UnityEngine.GUILayout.Label(
        text    : "Log",
        options : UnityEngine.GUILayout.Width(column_width));
    UnityEngine.GUILayout.Label(
        text    : "stderr",
        options : UnityEngine.GUILayout.Width(column_width));
    UnityEngine.GUILayout.Label(
        text    : "Flush",
        options : UnityEngine.GUILayout.Width(column_width));
    UnityEngine.GUILayout.EndHorizontal();
    UnityEngine.GUILayout.BeginHorizontal();
    UnityEngine.GUILayout.Space(column_width);
    if (UnityEngine.GUILayout.Button(
            text    : "↑",
            options : UnityEngine.GUILayout.Width(column_width))) {
      Log.SetSuppressedLogging(Math.Max(Log.GetSuppressedLogging() - 1, 0));
    }
    if (UnityEngine.GUILayout.Button(
            text    : "↑",
            options : UnityEngine.GUILayout.Width(column_width))) {
      Log.SetStderrLogging(Math.Max(Log.GetStderrLogging() - 1, 0));
    }
    if (UnityEngine.GUILayout.Button(
            text    : "↑",
            options : UnityEngine.GUILayout.Width(column_width))) {
      Log.SetBufferedLogging(Math.Max(Log.GetBufferedLogging() - 1, -1));
    }
    UnityEngine.GUILayout.EndHorizontal();
    for (int severity = 0; severity <= 3; ++severity) {
      UnityEngine.GUILayout.BeginHorizontal();
      UnityEngine.GUILayout.Label(
          text    : Log.kSeverityNames[severity],
          options : UnityEngine.GUILayout.Width(column_width));
      UnityEngine.GUILayout.Toggle(
          value   : severity >= Log.GetSuppressedLogging(),
          text    : "",
          options : UnityEngine.GUILayout.Width(column_width));
      UnityEngine.GUILayout.Toggle(
          value   : severity >= Log.GetStderrLogging(),
          text    : "",
          options : UnityEngine.GUILayout.Width(column_width));
      UnityEngine.GUILayout.Toggle(
          value   : severity > Log.GetBufferedLogging(),
          text    : "",
          options : UnityEngine.GUILayout.Width(column_width));
      UnityEngine.GUILayout.EndHorizontal();
    }
    UnityEngine.GUILayout.BeginHorizontal();
    UnityEngine.GUILayout.Space(column_width);
    if (UnityEngine.GUILayout.Button(
            text    : "↓",
            options : UnityEngine.GUILayout.Width(column_width))) {
      Log.SetSuppressedLogging(Math.Min(Log.GetSuppressedLogging() + 1, 3));
    }
    if (UnityEngine.GUILayout.Button(
            text    : "↓",
            options : UnityEngine.GUILayout.Width(column_width))) {
      Log.SetStderrLogging(Math.Min(Log.GetStderrLogging() + 1, 3));
    }
    if (UnityEngine.GUILayout.Button(
            text    : "↓",
            options : UnityEngine.GUILayout.Width(column_width))) {
      Log.SetBufferedLogging(Math.Min(Log.GetBufferedLogging() + 1, 3));
    }
    UnityEngine.GUILayout.EndHorizontal();
  }

  private void ShrinkMainWindow() {
    main_window_rectangle_.height = 0.0f;
    main_window_rectangle_.width = 0.0f;
  }

  private void UpdateRenderingFrame() {
    DeleteTransforms(ref transforms_);
    if (first_selected_celestial_ == second_selected_celestial_) {
      transforms_ = NewBodyCentredNonRotatingTransforms(
                        plugin_,
                        first_selected_celestial_);
    } else {
      transforms_ = NewBarycentricRotatingTransforms(
                        plugin_,
                        first_selected_celestial_,
                        second_selected_celestial_);
    }
  }

  private void ResetPlugin() {
    Cleanup();
    ApplyToBodyTree(body => body.inverseRotThresholdAltitude =
                                body.timeWarpAltitudeLimits[1]);
    ResetRenderedTrajectory();
    last_plugin_reset_ = DateTime.Now;
    plugin_ = NewPlugin(Planetarium.GetUniversalTime(),
                        Planetarium.fetch.Sun.flightGlobalsIndex,
                        Planetarium.fetch.Sun.gravParameter,
                        Planetarium.InverseRotAngle);
    BodyProcessor insert_body = body => {
      Log.Info("Inserting " + body.name + "...");
      InsertCelestial(plugin_,
                      body.flightGlobalsIndex,
                      body.gravParameter,
                      body.orbit.referenceBody.flightGlobalsIndex,
                      new QP{q = (XYZ)body.orbit.pos, p = (XYZ)body.orbit.vel});
    };
    ApplyToBodyTree(insert_body);
    EndInitialization(plugin_);
    UpdateRenderingFrame();
    VesselProcessor insert_vessel = vessel => {
      Log.Info("Inserting " + vessel.name + "...");
      bool inserted =
          InsertOrKeepVessel(plugin_,
                             vessel.id.ToString(),
                             vessel.orbit.referenceBody.flightGlobalsIndex);
      if (!inserted) {
        Log.Fatal("Plugin initialization: vessel not inserted");
      } else {
        SetVesselStateOffset(plugin_,
                             vessel.id.ToString(),
                             new QP{q = (XYZ)vessel.orbit.pos,
                                    p = (XYZ)vessel.orbit.vel});
      }
    };
    ApplyToVesselsOnRailsOrInInertialPhysicsBubbleInSpace(insert_vessel);
  }
}

}  // namespace ksp_plugin_adapter
}  // namespace principia
