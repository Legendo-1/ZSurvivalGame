import unreal, json
OUT = r"__OUT__"
MODE = "__MODE__"
VIEWS = json.loads(r'''__VIEWS__''')
FILLS = json.loads(r'''__FILLS__''')
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
tmp = [a for a in eas.get_all_level_actors() if a.get_actor_label().startswith("TMP_N")]
caps = {a.get_actor_label()[9:]: a for a in tmp if a.get_actor_label().startswith("TMP_NCap_")}
if MODE == "spawn":
    for a in tmp: eas.destroy_actor(a)
    for p in FILLS:
        la = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(p[0], p[1], p[2]), unreal.Rotator(0, 0, 0))
        la.set_actor_label("TMP_NFill")
        lc = la.get_component_by_class(unreal.PointLightComponent)
        lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        lc.set_intensity(p[3]); lc.set_attenuation_radius(6000.0); lc.set_light_color(unreal.LinearColor(*(p[4:7] if len(p) > 4 else (1.0, 0.96, 0.9)), 1.0))
        lc.set_editor_property("source_radius", 60.0)
    for v in VIEWS:
        pos = unreal.Vector(*v["pos"])
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, pos, unreal.MathLibrary.find_look_at_rotation(pos, unreal.Vector(*v["at"])))
        cap.set_actor_label("TMP_NCap_" + v["name"])
        cc = cap.get_component_by_class(unreal.SceneCaptureComponent2D)
        cc.set_editor_property("texture_target", unreal.RenderingLibrary.create_render_target2d(world, v.get("w", 1600), v.get("h", 1000), unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB))
        cc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_BASE_COLOR if v.get("base") else unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        cc.set_editor_property("fov_angle", v.get("fov", 70))
        cc.set_editor_property("capture_every_frame", False); cc.set_editor_property("capture_on_movement", False)
        cc.set_editor_property("always_persist_rendering_state", True)
elif MODE == "tick":
    for a in caps.values(): a.get_component_by_class(unreal.SceneCaptureComponent2D).capture_scene()
elif MODE == "export":
    for name, a in caps.items():
        cc = a.get_component_by_class(unreal.SceneCaptureComponent2D); cc.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, cc.get_editor_property("texture_target"), OUT, name + ".png")
elif MODE == "done":
    for a in tmp: eas.destroy_actor(a)
unreal.log(f"[NCap] {MODE}: {len(caps)} captures, {len(tmp)} temporary actors")
