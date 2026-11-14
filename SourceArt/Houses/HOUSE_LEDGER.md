# House ledger: 30 Meshy houses rebuilt as sharp buildings (outside first)

This file holds all state for the house rebuild. Every task starts by reading this file and ends by updating it.
Chat history and the session scratchpad are gone after `/clear`, so anything the next task needs must be written here.

**Task prompt, to paste after `/clear`:**

> Read SourceArt/Houses/HOUSE_LEDGER.md (scope, rules and lessons first) and do the next incomplete house.

**Scope (the user, 2026-11-13; applies from house #2 on):** only the **outside** matters: how the house looks. Inside, leave **one empty space**: no floors, stairs, rooms, guards or NavMesh. **Every house has its door(s)**: every door the Meshy model shows is built, **closed** and in place, so that from the outside **the house looks identical to the Meshy reference**. Each leaf is a separate piece that **can be made to open later** (section 3, "Inside and door"). **House #1 keeps its full interior** (it was built before this decision); its front door is now closed like the others.

## 0. Strict rules for every house (the user, 2026-11-13; they override anything below that disagrees)
The user's words: "Use the Meshy concept model purely as a visual and color guide, exactly as we did for the ClockmakersNarrowTownhouse. Execute the Blender Python scripts (build_house.py, house_kit.py) to generate the procedural kit geometry for this specific house. Adjust the material parameters in make_house_materials_ue.py to match the color palette of this house's Meshy reference. Do NOT take any viewport screenshots. Do NOT run lighting, exposure, or material tests. I have verified the master material logic. Once the UE5 import and assembly scripts run successfully, mark the house complete in the ledger and stop immediately."
1. **The Meshy model is a visual and colour guide only**, used as for house #1: measure it in Blender (`measure_meshy.py`, `raycast_meshy.py`, `profile_meshy.py`, `color_match.py` on the Meshy pictures), then build the house from the kit.
2. **Geometry** comes from `build_house.py` + `house_kit.py` and the house's spec.
3. **Materials:** set the parameters of each slot in the spec's `"materials"` (read by `make_house_materials_ue.py`) to the Meshy palette, measured on the Meshy pictures or texture. The gains are not tuned afterwards.
4. **No viewport screenshots and no Unreal captures** (`house_capture.py`, `ncap.py`, high-res screenshots). Clay renders in headless Blender (`--renders`) are allowed for checking proportions.
5. **No lighting, exposure or material tests**, and no colour match against captures. The master material is verified.
6. **No 5 m rule checks** for now (the user, 2026-11-13: "dont worry about the 5m rule … i will worry about all that later"). Keep the old houses' front line.
7. **Done =** materials, import (`import_house_ue.py`: meshes plus `BP_House_<Folder>`) and placement (`place_house_ue.py`) ran without errors. Then mark the house `done` in the ledger, commit (rule 6 below) and **stop immediately**. No extra checks after that.

## 1. Rules (read first)
1. **One house per task.** Take the first row in the house table (section 7) whose status is `in progress` (an interrupted task: resume it, after checking which files and assets already exist), or else the first `todo` row, in order. Build it, update the ledger, commit, and **stop**.
2. **The pilot gate.** House #1 is the pilot. Until section 6 says `LOOK APPROVED: yes`, the next task is to finish or fix house #1, never to start #2. If #1's status is `waiting for look approval`, start by asking the user whether the look is approved, and what to change if not. Record the answer in section 6. Then either fix #1 or, once it's approved, set #1 to `done` and start #2.
3. **Never touch a `done` house** unless the "Fix list" in section 6 names it.
4. **Kit changes are additive.** New `house_kit.py` parameters must default to the old behaviour, so earlier houses rebuild identically. Never rename or remove a kit function. Add every new function to the kit index (section 5).
5. **Write things down before stopping.** Fill in the house's row: status, files, actor names, location and yaw, materials, gotchas. Add any new convention to section 3 and any new tool or command to section 4.
6. **Commit at the end of each task** straight to `main` (never make a branch), with a message like `feat(houses): #N <Name>`. Never add a Co-Authored-By or AI attribution line. Don't commit the Meshy downloads; they live outside the repo.
7. **Never spend Meshy credits** without asking the user first. Downloading and checking status are free.
8. **Run the editor scripts yourself** through remote execution (section 4). Never ask the user to paste logs. Never start `UnrealEditor-Cmd.exe`.
9. If something blocks the house, set its status to `blocked`, say why in its notes, and stop. Don't skip ahead silently.

## 1a. Lessons: time lost on house #1 (read before every house, and add to it)
Each line is a mistake that cost real time, and what to do instead. **When something costs you more than two attempts, add it here before you stop.**

**Scope and attention**
1. **Interior work.** Stairs, guards, floor collision and a NavMesh hunt that never succeeded took most of house #1's time. Now: the inside is one empty space (scope note). Never build stairs or floors, and never debug the NavMesh.
2. **Doors left open.** House #1's door was modelled standing open, so the facade no longer looked like the Meshy model, and the user had to point it out. Now: every Meshy door is closed and in place (`"leaf": {"separate": true}`), and the outside must look identical to the reference.
3. **Debugging without a budget.** If a check fails and two or three focused tries don't find the cause, write it down under the house as "open", tell the user, and move on. No long hunts.
4. **Ledger written only at the end.** Update the house's row and details after each major step (spec, build, import, place), so an interruption loses nothing.

**Captures**
5. **Too many captures.** The user asked me to stop. Take only what the routine needs: one base-colour telephoto front (for the colour match), one three-quarter view, and close-ups of the door and one window. No interior views. Don't re-shoot to tune lighting.
6. **Lit captures at night.** They are blown out or blue, whatever the fills (6000 cd and even 300 cd were too much). Judge colour and shape only on `SCS_BASE_COLOR` captures.
7. **Black base-colour captures.** They were never black: they have alpha 0, and OpenImageIO premultiplies. `color_match.py` and `crop.py` handle it now. Don't read those PNGs with other code without `oiio:UnassociatedAlpha`.
8. **Orthographic captures.** They come out empty in UE 5.8. Use the telephoto: 400 m away, `"only": ["House_<Folder>"]`, fov = 2·atan(half width / distance) (see `Saved/HouseCheck/ClockmakersNarrowTownhouse/views_o.json`).

**Materials and colour**
9. **Wrong texture means.** The CC0 PNG exports hold linear 16-bit values, so brick came out 3× too bright. The right means are in `make_house_materials_ue.py`. Trust them; don't re-measure.
10. **Two colours on one slot.** The clock plate shared "Metal" with the window frames. Before building, sample every distinct colour area of the Meshy facade (`color_match.py` on `m_front.png`) and give each its own slot (append new slots at the end).
11. **Colour samples on the wrong surface.** Sample boxes landed on the brass bosses and the open door. Always write `--overlay` and look at it once before trusting the numbers.

**Geometry and the audit**
12. **Audit leaks**, all fixed in the kit; use the kit and don't hand-build walls or windows:
   - the joint slit where a block zone meets a flat zone (`rubble_face` seals it);
   - hollow walls (`wall_core`);
   - coincident two-sided glass (`sash_window` sets the two sides 2 mm apart);
   - reveals that skipped the frame zone (reveal from `rec` to `t`);
   - grid points exactly on edges (the audit grid is offset).
13. **Dirt from corner vertices.** Big flat quads take crevice dirt from their corner vertices. Exterior flat walls use `cell=0.6`; interior faces get no dirt (`clear_dirt`).

**Measuring and the site**
14. **Measuring at Meshy scale first.** Run `measure_meshy.py` once at the estimated scale with `--ppm 100`: grid lines are then whole metres and crops are 1 px = 1 cm.
15. **Unknown level geometry in the plot.** House #1 found the stepped-street gate wall late. In step 3, map the plot with downward traces (house #1's probe: a 20 cm grid of `line_trace_single`, labelling each hit actor) before deciding the footprint.

**Roofs**
20. **One see-through ray at a verge** (house #7): a grazing ray from the lower town slid along the slate courses and in at the seam where the courses meet the roof's verge edge face, then out through the open underside into the attic. A ridge cap did not help; bargeboards over the verges (`"barge"` in the roof spec) closed it. Try bargeboards first.

21. **Holes stacked in a gable** (house #8): `outline_face` used to keep only the last hole over a strip, so a door, transom, window and slit above each other came out as plain wall (the audit still read 0: glass was there behind the uncut face). Fixed in the kit; if a facade opening is missing in the clay render, probe the mesh with a ray (`scene.ray_cast` in the saved `.blend`) before changing the spec.

**Editor and tools**
16. **Waiting on the NavMesh build.** The poll grepped "True" from "navigation system True". If you ever wait on a build, grep "being built: False". You shouldn't need to, now that the scope has no NavMesh.
17. **Editing collision from Python.** Editing `agg_geom` convex elements on a duplicate didn't take effect, so the test proved nothing. Change collision in the build, then re-import.
18. **`hidden_actors` and `set_editor_property`.** That property can't be set on a capture component. Use `hide_actor_components(actor)` / `show_only_actor_components(actor)` (`ncap.py` does).
19. **Bash heredocs with long Python** (house #3) failed twice with "unexpected EOF while looking for matching `''", even quoted. Write the script to a file (Write tool, scratchpad) and run that, or use Edit for code changes. House #4 hit it again with a ledger-edit script full of backticks and apostrophes: **never** pass long text through a heredoc.

## 1b. Per-house routine
**Section 0 overrides this routine:** skip the 5 m checks (steps 3 and 9), the captures and colour tuning (step 10), and the capture-based colour check of the door (step 11). The task ends as soon as step 9's placement has run.

0. **Commit leftover material resaves.** Run `git status`. If the only changes under `Content/Buildings/Houses/` are `MI_House_*` files of houses already marked `done`, commit them first as `chore(houses): resave material instances`. Unreal adds the same 124 bytes of cached data to each material instance once the house has been drawn in the viewport, and they show up after the user's Save All. Never commit `ZSurvivalGame.uproject`: its `EngineAssociation` was changed on this PC.
1. **Read** this ledger: the scope note, the rules, the lessons (section 1a), the house's row and details, its plot group (section 8), the conventions and the kit index. Set the house's status to `in progress`.
2. **Study the template.** Look at `ref_*.png`, then load `model.glb` in Blender and take section drawings with `SourceArt/Cathedral/v3/tools/template_slices.py`, adapted to the GLB. Decide the scale (section 3), then measure storey heights, bay widths, opening sizes, cornices and the roofline.
3. **Fit the plot.** Find the slot's exact area. Check the 5 m rule and which side is the street, and list the old actors that overlap. Decide the real depth (section 8, depth rule).
4. **Write `specs/<Folder>.json`:** scale, footprint, storeys, bays, openings, **every door of the Meshy model** (`"leaf": {"separate": true}`), roof, materials and palette (no room layout). Measure the palette for each surface by sampling `model_textures/base_color.png` at UVs picked on each surface, not from the raw k-means clusters.
5. **Kit.** Add any new pieces to `house_kit.py` (additive only, rule 4) and list them in section 5.
6. **Build** with `build_house.py --spec ... --audit --renders ...`. The audit must report 0 see-through rays from the street, plaza, lower-town and roof eye points (outside eyes only: with the door closed, nobody stands inside). Compare the clay renders with `ref_front.png` and fix any proportion that is off.
7. **Materials.** Create or update the `MI_House_<Folder>_*` instances, tinted to the palette.
8. **Import** into `/Game/Buildings/Houses/<Folder>/`. Fix Nanite and placeholder materials (section 4), add collision (outer walls, roof, the closed door leaf), and make `BP_House_<Folder>` with the components `Ext` and `Door`.
9. **Place it.** Hide the old actors, place the BP and run the 5 m check against the cathedral components.
10. **Captures** to `Saved/HouseCheck/<Folder>/`: front and three-quarter views at eye height, close-ups at 1–2 m (door, a window, the wall material, any hero feature), and a `SCS_BASE_COLOR` view matched to `ref_front.png`. Compare the colour of each surface, and adjust the tints until they're within about 5%.
11. **Door check.** Every door is there and closed. Each leaf is its own mesh (listed in `build/<Folder>/doors.json`) with its origin on the hinge axis, sits in a real doorway (at least 1.2 × 2.3 m) with the empty inside behind it, and matches the Meshy door's panels and colour (the `Door` area in `specs/<Folder>_colors.json`, within about 5%). No capsule route or NavMesh checks (scope note above).
12. **Finish.** Save the level. Fill in the house's details (scale, real size, transform, actors hidden, materials, files, gotchas) and set its status to `done`. For the pilot, set it to `waiting for look approval`. Add a line to the log (section 10), commit, and stop. Tell the user what was built and where to look.

## 2. Sources
- **Meshy downloads** are in `C:\Users\Lenovo\Downloads\MeshyHouses\<Folder>\`. These are untrusted files, so scripts only read them by path. Each folder holds:
  - `model.glb`: the **textured** model, which is the **older** task of each Meshy pair.
  - `model_textures/{base_color,metallic,roughness,normal}.png` at 4096².
  - `ref_{front,right,back,left,iso}.png`: unlit base-colour renders, orthographic. "front" looks from Blender −Y toward +Y.
  - `palette.json` and `palette.png`: 8 k-means colour clusters of the base-colour texture (share, sRGB hex, linear RGB). They're raw: cluster 1 is often dark window glass or seams. Use them as a starting point, then measure each surface properly (routine step 4).
  - `_index.json` lists folder, name, `textured_task` and `untextured_companion`. `_contact_sheet.png` shows iso views and `_contact_sheet_front.png` shows front views, in index order, 6 per row.
- **Re-download** if the files are missing: `meshy_download_model(task_id=<textured_task>, format="glb", save_to=<folder>\model.glb)`. It's free.
- **Scale:** Meshy normalises every model to about 1.9 units on its longest side. The real scale is set per house from the storey count (section 3). The table's "est. size" values are estimates; the house task measures them properly.
- **District survey**, read-only, from 2026-11-13: `Saved/HouseCheck/district_survey.json` (houses within 60 m of the cathedral) and `Saved/HouseCheck/district_plots.json` (bounds, open sides and street height for each candidate plot). Re-run `SourceArt/Houses/tools/survey_*_ue.py` if the level changes.

## 3. Conventions
- **Names:**
  - Folder `/Game/Buildings/Houses/<Folder>/`.
  - Meshes `SM_House_<Folder>_Ext` (the outside, plus the empty shell inside: inner wall faces, ground floor, ceiling) and `SM_House_<Folder>_Door` (the main door leaf, origin on the hinge axis at its foot). House #1 also has `SM_House_<Folder>_Int` (floors, stair). Several doors: `_Door`, `_Door_1`, `_Door_2` … (BP components `Door`, `Door_1` …).
  - Blueprint `BP_House_<Folder>` with components `Ext` and `Door` (the Door component sits at the hinge, rotation 0 = closed). Level actor label `House_<Folder>`.
  - Materials `MI_House_<Folder>_<Surface>` in `/Game/Buildings/Houses/<Folder>/Materials/`. Surface = an FBX slot of `house_kit.MATERIALS`: Stone, Brick, Gauged (gauged-brick arches, chimney caps), Trim, Door, Metal (dark paint: window frames, clock marks), Glass, Brass, Dial, Clay (chimney pots), Roof, Plaster, Timber, **Bronze** (added by house #1 for the clock plate; it is now a permanent slot). Add new slots **only at the end** of the list (Shutter, Copper … when a house needs them), so earlier houses keep their slot order.
- **Source files:** `SourceArt/Houses/specs/<Folder>.json` is the house spec. `SourceArt/Houses/build/<Folder>/` holds the `.blend` and `.fbx` (committed, like the cathedral v3 files).
- **Blender house space:** metres, origin at the **front-left corner at street level**. +X runs along the frontage (to the right when you face the front door), the front face is at y = 0 and the house extends to +Y. Z is up, with z = 0 at the street in front of the main door.
- **To the level (verified by house #1):** UE mesh = (100·bx, −100·by, 100·bz) cm. Actor location = world position of the house origin (front-left corner, street level). **Yaw = (direction the front faces) − 90°**: front −Y → yaw 180, +Y → 0, +X → −90, −X → 90. With yaw 180: world = (Lx − 100·bx, Ly + 100·by, Lz + 100·bz). Note that the "left" of a facade (viewer facing the door) is world +X when the front faces −Y.
- **Inside and door (from house #2 on, the user's scope of 2026-11-13):**
  - The inside is one empty space: the inner faces of the outer walls (plain Plaster), a ground floor at the door's threshold height (Timber) and one ceiling under the roof. No upper floors, stairs, rooms, guards or NavMesh. Window glass stays two-sided and dark, so the missing floors don't show from the street.
  - **Doors (built and in use since house #1's fix):** every door of the Meshy model is a real doorway (the main one at least 1.2 m wide × 2.3 m high) with reveals through the wall, and a **closed** leaf matching the model (panels, transom, colour). In the spec: `"leaf": {"separate": true, "hinge": "left"|"right", "cols": 2, "rows": 3}`. `build_house.py` then writes the leaf as `SM_House_<Folder>_Door.fbx` in hinge space (origin on the hinge axis at the foot of the leaf, 6 cm thick, in the frame at the window-frame depth) with its own UCX box. It also writes `doors.json` (mesh, component, location in cm, yaw, `open_yaw_delta`). `import_house_ue.py` imports it and adds the BP component at the hinge. **To open a door later:** add `open_yaw_delta` (−90 for a left hinge, +90 for a right one) to the Door component's yaw. It swings inwards into the doorway without cutting the reveal.
  - Collision: UCX boxes for the outer walls (the door opening cut out), the roof and the ground floor. The door leaf has its own box collision, so it blocks while closed and moves with the component when it opens. Windows are blocked at the glass.
  - Storey heights: take them from the Meshy facade; there are no stair rules to satisfy. The walls stay 0.35–0.5 m thick.
  - `"interior": "shell"` (added by house #2) builds the empty room into the Ext mesh: inner wall faces, floor and ceiling are joined in after the dirt bake (kept clean), and no Int mesh is written. Use it on every house from #2 on.
  - Double doors (house #2): `build_house.separate_leaf()` writes each leaf as its own mesh (`_Door`, `_Door_1`), hinged at its jamb. The leaves meet in the middle with no gap.
- **Builder types:** `townhouse_flat` (house #1: flat roof, zones on one plane) and `townhouse_gable` (house #2: gable roof between coped side walls; front zones each at their own depth `y`; `gaps` between zones closed by swept `mouldings` with optional `breaks`; `window_style` + `windows`; a `door` bay with steps, pedestals, pilasters, consoles, architrave, hood, lattice transom and two leaves; `lanterns`; `sides`, `back`, `roof`, `chimneys`). The spec format is in `specs/GuildmastersRowhouse.json`. A house with a different roof or door bay extends `townhouse_gable` with optional keys, or adds a new type.
- **Slots used for other colours (house #2):** the slot name is only a name. Each house's MI sets its colour. The meaning per house is in its details row. For example, house #2 uses Gauged for its plinth and steps and Bronze for its black iron.
- **House #1 only (kept for reference):** doorways ≥ 1.2 × 2.3 m, floors about 3.3–4.1 m apart, a 1.2 m dog-leg stair with ramp collision, guards with full-height collision.
- **The 5 m rule:** every house wall is at least 5 m from the cathedral's walls and buttresses. Check it against the `BP_GironaCathedral_V3` components, not its bounding box: the box includes the stairs and front terrace.
- **Materials:** tiling textures on the mesh's own UVs at 2 m per repeat, as in the cathedral. Master `/Game/CityArt/Materials/M_Cathedral_Stone`, with vertex colour R = block tone, G = crevice dirt, B = streaks, A = macro. The CC0 sets in `/Game/CityArt/Textures/CC0/` are ashlar, brick, cobble, plaster, slate and timber. There is no roof-tile, rubble-stone or copper set yet: if a house needs one, download a CC0 set (for example from ambientCG) only after asking the user, then record it here. Make the per-house instances with `make_house_materials_ue.py` (house #1 creates it), tinted to the palette.
- **Colour match target:** under `SCS_BASE_COLOR`, the average of each surface on the new house should be within about 5% of the same surface on the Meshy model from the same view.
- **Replacing old houses:** hide the old actors with `set_is_temporarily_hidden_in_editor` plus Hidden in Game, NoCollision and `can_ever_affect_navigation` off (`place_house_ue.py` does all four), and **never delete** them. Record their labels in the house row so the change can be undone.

## 4. Commands (Windows, Git Bash)
```
BL="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"            # Blender 5.2
PY="/c/Program Files/Blender Foundation/Blender 5.2/5.2/python/bin/python.exe"   # the only Python on this PC
"$BL" -b --factory-startup --python <script.py> -- <args>                     # headless Blender
"$PY" -I SourceArt/Cathedral/v3/tools/ue_run.py "C:/Users/Lenovo/Documents/Unreal Projects/ZSurvivalGame/<script>_ue.py"
                                                                               # runs a script in the OPEN editor; prints its log
"$PY" -I SourceArt/Houses/tools/glb_info.py <model.glb>                        # what a GLB holds (textured or not)
"$BL" -b --factory-startup --python SourceArt/Houses/tools/render_meshy_refs.py -- [--only Folder] [--sheet front]
```
- The editor keeps Python globals between remote runs, so give script switches unique names (`HOUSE_NAME = ...`).
- A remote run longer than the 600 s tool timeout keeps running, so wait for it before sending another.
- **House pipeline (added by house #1):**
  ```
  "$BL" -b --factory-startup --python SourceArt/Houses/tools/measure_meshy.py -- <Folder> <out> --scale S --ppm 100   # m_front/back/left/right(_grid).png, m_info.json
  "$PY" -I SourceArt/Houses/tools/color_match.py <rects.json> <picA> col0,row0,ppm,sign [<picB> map] [--overlay out.png]
  "$BL" -b --factory-startup --python SourceArt/Houses/build_house.py -- --spec SourceArt/Houses/specs/<Folder>.json --audit [--renders <dir>]
  "$PY" -I SourceArt/Houses/tools/ue_house.py SourceArt/Houses/make_house_materials_ue.py <Folder>
  "$PY" -I SourceArt/Houses/tools/ue_house.py SourceArt/Houses/import_house_ue.py <Folder>     # meshes + BP_House_<Folder>
  "$PY" -I SourceArt/Houses/tools/ue_house.py SourceArt/Houses/place_house_ue.py <Folder>      # hide old, place, NavBounds, save level
  "$BL" -b --factory-startup --python SourceArt/Houses/tools/check_5m.py -- x0 x1 y0 y1 z0 z1 [--blend <house.blend> --place Lx Ly Lz yaw]
  "$PY" -I SourceArt/Houses/tools/house_capture.py <views.json> <out dir> [--ticks 30]
  ```
- **Measuring tools (added by house #2):**
  ```
  "$BL" -b --factory-startup --python SourceArt/Houses/tools/raycast_meshy.py -- <Folder> --scale S --depthmap <out.npy> --res 0.02 --xmax 8.06 --z1 17.6
        # front depth map of the Meshy model in house space (hit y per x, z): wall planes, surrounds, hoods, steps as numbers
  "$BL" -b --factory-startup --python SourceArt/Houses/tools/raycast_meshy.py -- <Folder> --scale S --x 0.3,4.0 [--step 0.05]
        # the same along chosen x columns, printed
  "$PY" -I SourceArt/Houses/tools/profile_meshy.py <m_info.json> right [--u0 -1 --u1 1.6]     # silhouette of a side picture
  ```
  Reading the depth map in runs (`np.load`, then group equal depths along a row or column) gave every measurement of house #2 in a few calls.
  House #4 made that a tool: `"$PY" -I SourceArt/Houses/tools/depth_runs.py <depth.npy> col|row <value> [tol] [minlen]` (map made with `--res 0.02 --z1 16.0`; for a map made with another `--z1`, set `DEPTH_Z1=<z1>` in the environment, added by house #6).
  `raycast_meshy.py --along x` (side faces) no longer stops at the first ray that misses (fixed by house #6): a `-` in its output is a real opening.
  House #7: `--depthmap <out.npy> --along x` makes a **side** map (col = Meshy-house y, value = hit x of rays along +x from the left; `--from back` shoots from the right). `depth_runs.py` reads it the same way (`col <y>` / `row <z>`). Pass `measure_meshy.py` an **absolute** output folder: with a relative one the pictures are written somewhere else.
- **Captures:** `house_capture.py` fills `ncap.py` and runs spawn, ticks, export, done over one connection. View keys: `name pos at fov w h base`, plus optional `hide` / `only` (actor labels) and `ortho`. **Ortho captures come out empty in 5.8**; for a front matched to `m_front.png` use a telephoto 400 m away with `"only": ["House_<Folder>"]` (fov = 2·atan(half width / distance)). The night level's auto exposure blows out fills: keep fills ≤ 300 cd outside, ≤ 60 cd inside, or use base-colour views.
- **Playability (house #1 only; not part of the routine now):** `"$PY" -I SourceArt/Houses/tools/ue_house.py SourceArt/Houses/check_playability_ue.py <Folder> HOUSE_STEP=traces|negative|build|nav`. `traces` sweeps the player capsule along the route, `negative` runs sweeps that must be blocked, `build` starts a NavMesh rebuild, and `nav` reports coverage and a street-to-top-floor path. Note that `nav` prints "navigation system True", so don't grep its output for "True" to wait on the build.
- **Git:** `SourceArt/Houses/build/` matches the `Build/` rule in `.gitignore` (case-insensitive on Windows), so add the `.blend` and `.fbx` with `git add -f` (they go through LFS). Leave out `.blend1`.
- **Reading captures:** base-colour PNGs have alpha 0, and OpenImageIO premultiplies, so they read as black unless you set `oiio:UnassociatedAlpha` (done in `color_match.py` and `crop.py`).
- **Texture means:** Unreal exports the CC0 albedos as 16-bit PNGs holding **linear** values (take the raw mean), but plaster as an 8-bit sRGB TGA. The correct means are in `make_house_materials_ue.py`.
- **Night level:** for readable captures, use `SCS_BASE_COLOR` views (`"base": true`) or temporary PointLight fills.
- **Known editor quirks:** scripted FBX import turns Nanite on and makes placeholder materials, so fix both after import. Set `recompute_normals` and `compute_weighted_normals` off, which makes a build take about 20 s instead of 165 s. A new mesh needs `set_material(i, mat)` per slot. Count UCX hulls with `get_convex_collision_count`. Patterns to copy are in `SourceArt/Cathedral/v3/import_cathedral_v3_ue.py` and `make_cathedral_v3_bp_ue.py`.

## 5. Kit index (`SourceArt/Houses/house_kit.py`)
House #1 creates `house_kit.py`, which imports `SourceArt/Cathedral/v3/gothic_kit.py` (path insert). Reusable from `gothic_kit`:
- `Builder` (`add`, `grid`, `quad`, `box`, `sweep`, `lathe`, `tube`, `to_object`)
- `block_wall(B, x0, x1, z0, z1, mat, holes=...)` (coursed stone with holes), `seal_inside_corner`, `corner_post`
- `slab_floor`, `glazed_window`, `opening`, `arch_through`, `clock_face(B, cx, cz, r, stone, dial, iron)`
- `balustrade`, `tile_facet` (roof tiles), `stepped_dome`, `entablature`, `course` (string course), `urn`, `ball`
- `bake_cavity`, `bake_weather`, `audit_plane` (the see-through check)

`SourceArt/Cathedral/v3/build_common.py` is the cathedral's CLI tail. Its asset names are cathedral-specific, so house #1 writes `SourceArt/Houses/build_house.py` with the same flow (build, cavity and weather bake, save `.blend`, export FBX, `--audit`, `--renders`).

| Function | Signature (short) | Added by house |
|---|---|---|
| `MATERIALS`, `COLORS` | FBX slot list (append only; **Bronze** added permanently by #1) | 1 |
| `facade_frame` | `(side, W, D) -> (matrix, length)`; side front/right/back/left | 1 |
| `rect`, `block`, `flat_face` | quad with a fixed UV offset; box; wall face with holes (`inward`, `cell` grid) | 1 |
| `reveal` | `(B, hole, y0, y1, mat, sides="lrtb")` | 1 |
| `rubble_face` | `block_wall` with rounded blocks plus joint-plate seals above and below the zone | 1 |
| `wall_core` | `(B, L, z0, z1, t, holes)` hidden mid-wall plates and lids (keeps the audit at 0) | 1 |
| `sash_window` | `(B, x0, x1, z0, z1, frame, glass, cols, rows, y, depth)`, two-sided glass 2 mm apart | 1 |
| `sill`, `lintel`, `gauged_arch` | stone sill, flat lintel, gauged-brick flat arch | 1 |
| `panel_leaf` | panelled door leaf in its own frame (hinge at x = 0) | 1 |
| `clock_dial` | `(B, cx, cz, half, r, plate, ring, dial, marks, brass)` Meshy-style clock | 1 |
| `plan_loop`, `band_profile`, `band_loop` | string courses round the house | 1 |
| `cornice_loop` | entablature round the house plus the parapet coping | 1 |
| `chimney` | stack, cap and clay pots; returns a hull | 1 |
| `box_pts`, `slab_rects`, `flight`, `prism_bar`, `balustrade`, `balustrade_y` | slabs, a straight flight (ramp hull), guards | 1 |
| `build_house.py` door option | `"leaf": {"separate": true, "hinge": …}` → closed leaf as its own mesh + `doors.json` (importer adds the BP component) | 1 (fix) |
| `dogleg_stair` | `(B, C, floors, risers, x_left, x_half, x_land, x_right, y_back, …)`: back-wall dog-leg, spine, guards, hulls (house #1 only for now) | 1 |
| `MATERIALS` + **Lamp** | new slot at the end: lit lantern glass (a glass MI with emissive) | 2 |
| `moulding_run` | `(B, x0, x1, z, prof, mat, y, caps, hub_v)`: straight swept moulding (cornice, band, hood, plinth cap) with capped ends | 2 |
| `frame_run` | `(B, x0, x1, z0, z1, prof, mat, y)`: architrave up both jambs and over the head | 2 |
| `studded_leaf` | `(B, w, h, frame_mat, panel_mat, rows, t, stile, sunk, stud, pitch, ring)`: iron-framed timber leaf, one panel column, studs, ring knocker | 2 |
| `wall_lantern` | `(B, cx, y_wall, z_foot, out, iron, glass)`: back plate, curved arm, six-sided lantern | 2 |
| `console` | `(B, xc, y_wall, z_top, mat, out, height, width)`: S-scroll console under a hood | 2 |
| `gable_coping` | `(B, x0, x1, outline, h, mat, inner_want)`: coping along a roof outline, upright steps closed | 2 |
| `slope_rib` | `(B, xc, w, a, b, h, mat)`: rib lying on a roof slope | 2 |
| `lattice_bars` | `(B, x0, x1, z0, z1, pitch, width, y_back, y_front, mat)`: diamond lattice in a rectangle | 2 |
| `build_house.separate_leaf`, `gable_outline` | one hinged leaf as its own mesh (DOORS); the side-wall roof outline | 2 |
| `clock_dial` options | `rosette=` (corner bosses) and `centre=` (centre disc and its ring) materials; both default to `brass` as before | 3 |
| `both_faces` | `(B, y, fn)`: runs fn twice, the second time mirrored about the plane y (gk.bar iron gets a back) | 3 |
| `lace_panel` | `(B, x0, x1, z0, z1, y, mat, pitch)`: cast-iron balcony panel: rails, stiles, mid rail, two-sided diamond lattice | 3 |
| `iron_railing` | `(B, x0, x1, y, z0, h, mat, pitch, spear)`: roof railing: four rails, spear-headed bars, an oval ring per bay | 3 |
| `cast_column`, `newel` | `(B, cx, cy, z0, z1, mat, r)` turned iron column; `(B, cx, cy, z0, z1, mat, w)` square post with cap and ball | 3 |
| `spandrel` | `(B, xa, xb, z_spring, z_crown, z_top, y, mat)`: segmental iron arch under a beam, filled with uprights and rings | 3 |
| `blank_dial` | `(B, cx, cz, half, r, plate, ring, disc)`: clock_dial's plate and ring round a plain disc | 3 |
| `pyramid_roof` | `(B, cx, cy, half, z0, apex, mat, pitch)`: square tiled pyramid (gk.tile_facet) | 3 |
| `arched_block` | `(B, cx, w, z0, z_spring, y0, y1, mat)`: round-topped block (dormer, gablet) | 3 |
| `iron_finial` | `(B, cx, cy, z0, top, mat)`: turned iron spire with two tiers of curling leaves | 3 |
| `build_house.terrace_ground`, `terrace_windows`, `terrace_pilasters`, `terrace_balcony`, `terrace_cupola` | the parts of `townhouse_terrace` | 3 |
| `outline_at`, `outline_face` | `(B, outline, z0, mat, holes, y, inward)`: upright face from z0 up to a sloping top outline `[(x, z), ...]`, with rectangular holes (gables) | 4 |
| `verge_band` | `(B, outline, h, proud, mat)`: flat band hanging under a top outline (a gable's verge moulding) | 4 |
| `glazed_leaf` | `(B, w, h, mat, glass, bar_mat, t, frame, light, grid, panel_z)`: shop door leaf, a barred glass light over two sunk panels | 4 |
| `quoin_corner` | `(B, z0, z1, course, long, short, proud, mat, start_long)`: alternating quoins round the corner at a facade frame's x = 0 (call once per side) | 4 |
| `build_house.mansard_profile`, `roof_z`, `mansard_windows`, `mansard_shop`, `mansard_roof`, `mansard_dormer` | the parts of `townhouse_mansard` | 4 |
| `panel_leaf` option | `weights=` the panel rows' heights from the bottom up (default: as before) | 5 |
| `arc_pts`, `arch_intrados`, `arch_ring`, `arch_spandrels` | round arch: arc points; soffit half cylinder; archivolt front face and extrados; wall faces filling the rectangle outside the arch | 5 |
| `fanlight` | `(B, xc, zs, r, y, glass, iron, spokes)`: half-round two-sided glass with an iron sunburst (hub, spokes, half ring, small rings) | 5 |
| `vent_grille` | `(B, x0, x1, z0, z1, y, depth, mat, iron, back)`: basement vent: reveals, dark back plate, framed upright bars | 5 |
| `seam_facet` | `(B, mat, a, b, c, d, pitch, w, h, ribs)`: standing-seam metal sheet with ribs up the slope | 5 |
| `mansard_roof` option | `"cover": "seam"` (+ `seam_pitch`, `seam_w`, `seam_h`) in the spec's roof: metal sheets instead of slates, ribs on the steep slopes only (default: slates as before) | 5 |
| `build_house.zinc_windows`, `zinc_plinth`, `zinc_arch_door`, `zinc_square_door`, `zinc_dormer`, `shots_zinc` | the parts of `townhouse_zinc` | 5 |
| `timber_members` | `(B, wl, z0, top)`: half-timbering proud of a wall's infill plane from a wall spec: `plates` [x0, x1, z0, z1, proud], `posts` [x0, x1] (full height, clipped under a top outline), `rails` [x0, x1, z0, z1] (rails and studs), `braces` [xa, za, xb, zb]; each kind at its own depth | 6 |
| `plank_shutter` | `(B, x0, x1, z0, z1, y, mat, iron, hinge)`: open plank shutter against a wall: board, joints, two ledgers, iron strap hinges | 6 |
| `barred_window` | `(B, x0, x1, z0, z1, depth, stone, frame, glass, iron, cols, rows)`: small window with a flat frame, reveals, dark glass and an iron bar grid | 6 |
| `hanging_sign` | `(B, x, y_wall, z_arm, reach, board, mat)`: wrought-iron shop sign: wall plate, arm with a ball end, curved stay, hangers, shield board with curls | 6 |
| `mansard_roof` / `zinc_dormer` options | `"gutter"` may be left out (no gutters); dormer `"trim"` (material of the architrave, sill, head, raking cornice and front soffit; default: the face's `mat`) and `"posts"` [[x0, x1], ...] (blocks either side of the window) | 6 |
| `build_house.jetty_mirror`, `jetty_wall`, `jetty_storey`, `jetty_ground`, `audit_setup_jetty`, `shots_jetty` | the parts of `townhouse_jetty` | 6 |
| `louvres` | `(B, x0, x1, z0, z1, mat, back_mat, depth, pitch, slat)`: fixed louvre boards in a hole of a wall face: reveals, a back board that closes the wall, sloping slats | 7 |
| `plank_leaf` | `(B, w, h, mat, iron, t, planks, straps, strap_h, hinge, ring)`: ledged plank door leaf in leaf space, full-width iron straps with rivets, optional ring pull | 7 |
| `hoist_crane`, `AXIS_X` | `(B, xc, y_wall, y_tip, z_beam, beam_h, foot, wheel_r, block_c, block_r, hook_z, iron, rope)`: gable hoist (beam, cheeks, pulley wheel, strut, ropes, lower block, hook); `AXIS_X` turns a lathe about x | 7 |
| `jetty_wall` options | wall keys `"infill"` (outer face material, default Plaster), `"louvres"` [[x0, x1, z0, z1], ...] (louvre boards; core and inner face stay whole), `"holes"` [[x0, x1, z0, z1], ...] (openings through the wall that the caller fills) | 7 |
| `build_house.loft_roof_frame`, `loft_ground`, `audit_setup_loft`, `shots_loft` | the parts of `townhouse_loft`; roof options `"ridge_cap"` [half width, height] and `"barge"` [proud, over the slates, under the underside] | 7 |
| `oct_ring`, `rect_ring`, `ring_skin` | `(cx, cy, r, z)` the 8 corners of an octagon (apothem r, flats facing ±x, ±y and the diagonals); `(cx, cy, hx, hy, c, z)` 8 matching points on a chamfered rectangle; `(B, lo, hi, mat)` the faces between two such rings (a taper from a pier to an octagon) | 8 |
| `octagon_profile`, `octagon_band` | `(B, cx, cy, [(z, apothem), ...], mat)` an octagonal shaft turned from a profile (steps where z repeats: corbelled caps, flues); `(B, cx, cy, z0, z1, r0, r1, proud, mat, rivets)` an iron band with rivets round a tapering octagon | 8 |
| `axis_frame`, `anchor_plate`, `tie_rod` | `(c, n)` a frame whose local z is n; `(B, c, n, mat, r, ...)` a round tie-rod anchor plate with a boss, nut and bolt heads on a wall with outward normal n; `(B, pts, mat, r)` a round rod along a polyline with square knuckles at its corners | 8 |
| `outline_face` fix | a strip under **several stacked holes** now cuts them all (it kept only the last one, so a gable with a door, transom, window and slit above each other lost all but one); with one hole or none per strip the faces are the same as before (houses #6 and #7 rebuilt identically: 3545 and 5323 faces) | 8 |
| `jetty_wall` options | window style `"reveal"` (reveal material, default Timber), wall key `"louvre_mat"` (default Timber); `window_style["shutter"]` may now be left out | 8 |
| `build_house.loft_roof` | the roof part of `townhouse_loft` split out (mansard_roof in the turned frame, ridge cap, bargeboards); `townhouse_stack` calls it with `frame.inset` 0 | 8 |
| `build_house.stack_chimney`, `shots_stack` | the chimney stack of `townhouse_stack`; audit planes from `audit_setup_loft` | 8 |
| `arched_opening` | `(B, I, xc, z0, zs, r, t, rec, stone, trim, ring_w, proud, sill_h, inner_from)`: a round-headed opening cut as a rectangle in a wall face: spandrels over the arch, jamb reveals and soffit back to the inner reveal, inner reveals, and with `trim` a surround (jamb blocks, arch ring, sill); the caller fills it at `rec` (sash + `fanlight`, or a door leaf) | 9 |
| `spike_finial` | `(B, cx, cy, z0, top, mat, r)`: turned spike finial (collar, neck, ball, collar, long spike) | 9 |
| `spire_rings` | `(B, x0, x1, y0, y1, rings, mat, soffit_z, pitch, step)`: hipped (bell-cast) spire over a rectangle from rings `[(z, iy, ix), ...]` (inset on the front/back faces and on the sides; negative = eave overhang), slate facets, flat top, fascia and soffit; returns the ring corners | 9 |
| `build_house.shop_wall`, `tavern_shop`, `tavern_oriel`, `tavern_roof`, `tavern_tower`, `tavern_spire`, `tavern_stair`, `audit_setup_tavern`, `shots_tavern` | the parts of `townhouse_tavern` (the spire's dormers are `mansard_dormer` in each face's frame) | 9 |

`build_house.py` builder types: `townhouse_flat` (house #1, spec `specs/ClockmakersNarrowTownhouse.json`) , `townhouse_gable` (house #2, spec `specs/GuildmastersRowhouse.json`) and `townhouse_terrace` (house #3, spec `specs/SilversmithsOrnateAtelier.json`: a flat roof behind a swept cornice with dentils, a roof railing with corner finials, an optional clock `cupola`; per side a plinth, a rusticated ground floor with `shop` windows and a recessed `door` (pilasters, steps, transom, separate leaf), `fascia`, `corbels`; ashlar upper floors with corner pilasters, `bands` and windows on `storeys`; an optional front `balcony` of two cast-iron storeys) and `townhouse_mansard` (house #4, spec `specs/AlchemistsMansardTenement.json`: flat walls on one plane (`Stone`, holes for windows), `quoins` on all corners, `bands` (optional `sides`), a gambrel `roof` given as its front `profile` points plus a `ridge` (mirrored to the back, ridge at D/2) with `overhang` past the gables, slate courses, soffits, verge and eave edges and `gutter`s; stone gables up to the roof's underside with a `verge` band and a `gable_window`; casement windows per `storeys` (panes per storey) with architrave, sill and `lintel`; a timber `shop` front (pilasters, shop windows on stallrisers, a glazed door bay, fascia, consoles, cornice, pier caps); gabled `dormers` on the front slope) and `townhouse_zinc` (house #5, spec `specs/BookbindersMansardAtelier.json`: ashlar walls (`stone` slot) over a `plinth` with vent `grilles`, rusticated corner `pilasters` (quoin_corner per zone), `bands` and a crowning `cornice` sweep right round; per side `ground` and `upper` windows in `ground_style` / `window_style` (optional architrave, sill, `brackets`, `hood`), an `arch_door` (steps, pedestals, archivolt, keystone, spandrels, deep reveals with a timber lining, fanlight, two leaves) or a `square_door` (sill, transom light, two leaves); a mansard_roof with `"cover": "seam"` whose profile ends flat (ridge = break height), stone gables up to its underside, an iron `railing` with corner finials round the flat top, pedimented `dormers` on the front and back slopes; audit planes from `townhouse_mansard`) and `townhouse_jetty` (house #6, spec `specs/GrandApothecaryHerbalistShop.json`: a stone `ground` floor (plinth, ashlar faces, a deep round-arched porch: archivolt on imposts, soffit back to a tympanum with a barred window and a fascia, a porch ceiling; a timber `shop` front on its own plane with shop windows on stallrisers, door posts, a glazed door leaf and transom; arched `tunnel` passages through both piers into the porch); timber-framed `storeys`, each its own box (`x`, `front`, `back`, `z`, `t`) so it can jetty out, with per-side `walls` (`timber_members` keys, `windows` [x0, x1, z0, z1, cols, rows], `shutters`, or `{"mirror": "right"}`) and `"top": "roof"` for walls framed up to the roof's underside (gables); `soffits` under the jetties and `corbels`; a gable roof from a free `profile` (eave, kick points, ridge anywhere, back eave) through `mansard_roof`; timber `dormers` through `zinc_dormer`; an optional `sign`; `inside` floor and ceiling rectangles). and `townhouse_loft` (house #7, spec `specs/TextilePrintersDryingLoft.json`: a rubble `ground` floor (`rubble` course, length, chamfer, jitter) with a plank `door` between dressed `jamb`s under a `lintel` (separate leaf, `plank_leaf`); timber `storeys` round a box inset by `frame.inset` from the stone, each side a `jetty_wall` spec (posts, rails, `louvres`, `infill`, `"mirror"`), `side_top` for side walls carried up to the eaves; `bands` (band_loop at the stone line) with `ledges` back to the timber; `gables` front and back (`z0`, principal `rafter` [h, proud], a `jetty_wall` spec each, loft doors from `holes` + `leaves`, or `windows`); a gable roof from a free `profile` across x (eave, ridge, eave) with the ridge running front to back, `ridge_cap`, `barge`; an optional `hoist`) and `townhouse_stack` (house #8, spec `specs/GlassblowersHighDraftFurnace.json`: four plain walls `wall` thick, each a `jetty_wall` with a `"Stone"` infill under `walls.<side>` (`windows` [x0, x1, z0, z1, cols, rows] in `window_style` with `"reveal"`, `louvres` + `louvre_mat` for vent slits, `blocks` [[x0, x1, y0, y1, z0, z1, mat], ...] for proud trim in wall coordinates: surrounds, sills, heads); front and back are gables up to the roof's underside, the sides stop under the eaves; a `door` (side, opening `x`, `z1`, stone `reveal`, timber `frame` {w, y, head, mat}, a separate `plank_leaf`); the roof as `townhouse_loft`'s (`profile` across x, `ridge_cap`, `barge`); a `stack`: chimney centre `cx`, `cy`, a stone `pier` box against a side wall (optional `window` on its outer face), a `taper` from the pier to the octagon, the `shaft` (`z0`/`z1` as [z, apothem]), a corbelled `cap` profile, a `flue`, iron `bands`, `ties` (rods along the wall at `x`, optional `corners` round to anchor plates on the front and back at that x, wall `plates`)). and `townhouse_tavern` (house #9, spec `specs/HighSpireTavern.json`: a `shop` round the front block (`walls.front|right|left` each with a `span`, `pilasters`, `plaster` panels, `windows` [x0, x1, cols, rows], `consoles`, the front `door`; shared heights `plinth`, `sill`, `glass`, `head`, `fascia`, `frieze`, `beam`; `corners` as blocks; `y1` = where the front block ends); a jettied `first` floor (`x`, `front`, `z`, `t`, `walls` as `jetty_wall` specs, `"holes"` for the oriel, corner `consoles`); an `oriel` (`plan` of four points, `glass`, `shelf`, `top`, `lights`, `panes`, `corbels`); a framed front `gable` over a `bressummer` with a verge `rafter`; a gable `roof` across x (`profile`, `front`/`back` edges, `cut` = the tower's right face and front face, where the left slope stops, `ridge_cap`, `barge`, `finial`); a stone `back_block` (right side under the eave and the back gable, `jetty_wall` with Stone); a `tower` (`box`, `quoins`, per face round-headed `windows`, `slits`, a round-headed `door`); a `spire` (`rings`, a `dormer` on every face, `finials`); a `stair` (steps and a sloping stone wall)). `place_house_ue.py` takes `"nav_bounds": false` (no NavBounds volume; from house #2 on).

## 6. Status
- **LOOK APPROVED:** yes (2026-11-13). The user, on the pilot: "The ClockmakersNarrowTownhouse geometry, material generation, and lighting tests are successful … mark House 1 as 100% complete." The next task starts house #2.
- **Fix list:** #1's front door, closed as a separate leaf like the Meshy model: **done 2026-11-13**. Nothing else is open. The user (2026-11-13): leave the rest of house #1 as it is, and don't worry about the NavMesh inside.
- **Open questions for the user:** see section 8.

## 7. House table
Build order = row order. Status is one of `todo`, `in progress`, `waiting for look approval` (the pilot only), `done`, `blocked`.
"Est. size" is width × depth × height in metres, estimated from storey count. Width is the street frontage. With the depth rule (section 8) the depth follows the plot, not the Meshy model.

| # | Status | Folder | Plot (section 8) | Est. size W×D×H | Front faces |
|---|---|---|---|---|---|
| 1 | done (100%) | ClockmakersNarrowTownhouse | G1 slot 1 | 6.0 × 6.64 × 19.0 (21.0 to the pots) | −Y |
| 2 | done | GuildmastersRowhouse | G1 slot 2 | 8.0 × 6.64 × 16.5 (17.5 to the pots) | −Y |
| 3 | done | SilversmithsOrnateAtelier | G1 slot 4 | 8.05 × 6.64 × 12.5 (21.9 to the finial) | −Y |
| 4 | done | AlchemistsMansardTenement | G1 slot 6 | 10.6 × 6.64 × 15.96 | −Y |
| 5 | done | BookbindersMansardAtelier | G1 slot 8 | 8.56 × 6.64 × 12.72 (14.22 to the corner finials) | −Y |
| 6 | done | GrandApothecaryHerbalistShop | G1 slot 3 | 7.14 × 6.64 × 18.27 (jetties to 7.38 wide, roof 7.94) | −Y |
| 7 | done | TextilePrintersDryingLoft | G1 slot 7 | 6.28 × 6.64 × 15.04 (roof 6.90 wide; hoist 2.56 m out over the lane) | −Y |
| 8 | done | GlassblowersHighDraftFurnace | G1 slot 5 | 5.11 × 6.64 × 14.28 (7.75 wide with the stack, 18.02 to its top) | −Y |
| 9 | done | HighSpireTavern | G1 slot 9 | 6.24 × 6.64 × 12.69 (8.46 wide with the tower and stair; 19.98 to the spire finials) | −Y |
| 10 | todo | WaterWardensGatehouse | G1 slot 10 | 13.2 × 8.1 × 14 | −Y |
| 11 | todo | ClockworkEngineersClockTower | G0 | 5.8 × 5.8 × 25 | −Y |
| 12 | todo | MagistratesOfficialTenement | G4d | 9.8 × 11.8 × 12.7 | +Y |
| 13 | todo | BookbindersDormerAtelier | G4b | 9.9 × 14.4 × 15.8 | +Y |
| 14 | todo | PottersKilnHouse | G4c | 11.8 × 7.2 × 14 | +Y |
| 15 | todo | MapmakersSkyObservatory | G4a | 12.6 × 13.2 × 21 | +Y |
| 16 | todo | GuildsMeetingHall | G7 slot 1 | 15.0 × 10.6 × 15 | +Y |
| 17 | todo | ShipwrightsMoldLoft | G7 slot 2 | 8.3 × 15.2 × 12 | +Y |
| 18 | todo | BankVaultGuardhouse | G2 slot 1 | 13.1 × 14.0 × 13 | −Y (check) |
| 19 | todo | AssayersIronBalconiedVilla | G2 slot 2 | 12.7 × 13.0 × 13.5 | −Y (check) |
| 20 | todo | ScholarsCornerLibrary | G3 plot 33 | 10.7 × 10.8 × 14 | −X |
| 21 | todo | CanalSideMerchantHouse | G3 plot 31 | 12.5 × 11.4 × 15.5 | +X |
| 22 | todo | BlacksmithsSteamWorkshop | G3 plot 34 | 13.8 × 11.9 × 15 | −X |
| 23 | todo | BellFoundersCastingHouse | G3 plot 32 | 13.4 × 16.3 × 12.5 | +X |
| 24 | todo | BakersGabledBakery | G5a | 15.1 × 11.3 × 16 | −Y |
| 25 | todo | RopeMakersIndustrialShed | G5b | 14.9 × 10.1 × 8 | −Y |
| 26 | todo | CooperageBarrelStorehouse | G6 slot 1 | 14.5 × 8.4 × 11 | check |
| 27 | todo | TannersTiledTenement | G6 slot 2 | 15.6 × 10.2 × 15 | check |
| 28 | todo | CoalMerchantsSteppedManor | G6 slot 3 | 14.1 × 15.8 × 14 | check |
| 29 | todo | DocksideTollhouse | G6 slot 4 | 11.4 × 7.4 × 13 | check |
| 30 | todo | DistillersCopperDomeShed | G6 slot 5 | 11.5 × 10.0 × 12 | check |

### Per-house details
Palette lists the dominant clusters from `palette.json` (sRGB hex, % of texels), with the very dark window and seam clusters left out. Pieces come from the reference renders. Each house task re-reads the renders and replaces "est." values with measurements.

**#1 ClockmakersNarrowTownhouse** (Meshy textured task `4b2ecae8-b61c-43fc-90ab-0350f80191b1`). PILOT.
- Pieces: 5 storeys, 2 bays wide. Coursed pale rubble/ashlar stone for the ground and 1st floor. Red-brown brick for the 2nd–4th floors, with stone string courses at the brick base and between the 3rd and 4th floors. A flat roof behind a stone parapet with a moulded cornice, and a brick chimney with 2 clay pots. Dark 6-over-6 sash windows with stone sills and brick flat-arch heads. A round **clock face** (gold dial, Roman numerals) centred on the 3rd floor between two narrow windows (`gothic_kit.clock_face`). A panelled timber front door in a stone surround, with a transom, plus one small window beside it.
- Palette: stone #a79f91:16 #8f8779:14 #c0b8a8:11; brick #42332a:12 #67483a:4; dark trim #564f46:13.
- Rooms: one room per floor, plus the stair along the side wall at the back. The plot is only about 6.6 m deep and 6 m wide, so the inside is about 5.2 × 5.8 m. Stair core about 1.2 × 5 m. The roof is reachable through a hatch (optional).
- **Result: DONE (100%), 2026-11-13; look approved by the user.**
  - Scale 11.05 (the Meshy wall is 0.543 wide, which gives 6.0 m). Size 6.0 × 6.64 m (depth cut to the plot), cornice top 18.98 m, pots 21.03 m. Floors 0.18 / 4.20 / 8.25 / 11.60 / 14.90, ceiling 17.75, roof deck 18.05.
  - Dog-leg stair against the back wall (x_half 1.55, x_land 4.40, 1.2 m flights, 24/24/20/20 risers), guards with full-height collision. No roof access.
  - Front door (fixed 2026-11-13, the user: "every house should have a door"): closed six-panel leaf `SM_House_ClockmakersNarrowTownhouse_Door`, BP component `Door` at the hinge (283, −18, 18) cm. Opening it means yaw −90 on that component. Door colour against the Meshy door: 1.00 (gain [1.47, 1.51, 1.59]). The player can't walk in until it's opened.
  - Placed as `House_ClockmakersNarrowTownhouse` at (18572, 5756, 1300), yaw 180. `House_ClockmakersNarrowTownhouse_NavBounds` was added. Hidden (not deleted): `Cathedral_RowHouse_00171`.
  - Files: `specs/ClockmakersNarrowTownhouse.json`, `specs/ClockmakersNarrowTownhouse_colors.json`, `build/ClockmakersNarrowTownhouse/` (`.blend`, Ext/Int FBX). Assets are in `/Game/Buildings/Houses/ClockmakersNarrowTownhouse/` (14 MIs, including Bronze).
  - Checks: audit 0 see-through rays; 45 UCX hulls; 5 m rule 5.31 m (cornice corner to the nave buttresses; walls 5.68 m). Colour match against the Meshy front (B/A luminance): stone 1.01, brick 1.04, trim 1.00, gauged 0.95, dial 0.96, glass 0.88 (gain then raised to 1.14).
  - Gotchas: the stepped-street gate wall (`Girona_SteppedStreet_00030`, x 17920–18000, y 5580–6220, top z 2037) pokes 0.28 m into the plot's right edge. It sits inside the wall thickness, and its front end stands 1.8 m out into the lane.
  - Playability (`check_playability_ue.py`): the player capsule (radius 35, half height 90) passes all 46 route segments, from the street through the door, up every flight and across every landing, to the top room. The negative self-test is blocked by the front wall, a stair-well guard, the back wall and a ceiling, as it should be.
  - **Not fixed (the user's decision, 2026-11-13: leave house #1 as it is): NavMesh does not cover the stair.** Every room has NavMesh, but the flights, half landings and most landings don't, so AI can't climb past the ground floor. The collision is right: 45 hulls in the right places, complete triangle lists, consistent winding, and the physics traces agree. Which stair parts get NavMesh changes with the tile size (10 m vs 5 m) and with placement (copies in empty sky at yaw 0 and 180 showed different gaps). Also ruled out: the hidden old house, the Int mesh, nav modifiers, and simple-as-complex collision. Every experiment was undone: tile size is back to 10 m and the Ext collision flag to default. Next ideas: draw the NavMesh in a debug view, try the region and layer partitioning settings, or add NavLinkProxies between floors as a workaround.
- Result: _not built_.

**#2 GuildmastersRowhouse** (`9909a371-bb89-460e-8771-045ff7cebee2`)
- Pieces: 4 storeys of cream ashlar with channelled ground-floor rustication. 3 bays of tall sash windows with moulded surrounds. A central double timber door with a fanlight and two wall lanterns. A heavy cornice, a flat roof with 2 skylights and one chimney.
- Palette: #c7b7a2:18 #afa08d:16 #e0d0b9:13; door/timber #785d4a:12.
- **Result: DONE, 2026-11-13** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: 3 storeys, not 4. A tall rusticated ground floor (plinth 0–1.14 m, channelled blocks on 0.465 m courses, set 0.11–0.14 m **behind** the upper facade, quoins at the corners), a band at 6.86–7.30 m, two floors of ashlar with 3 bays of 2×4 sash windows (moulded architraves, sills on brackets, cornice hoods), and a cornice at 14.38–15.36 m (0.49 m proud). The roof is a **gable roof** between coped side walls, not a flat roof. A gutter runs behind the cornice. The front slope (z 15.36→16.08) carries 8 light ribs (the "skylights"), then comes an upright stone step to the ridge (16.48) and a long back slope at 16° to the back eave. The chimney (3 clay pots) is on the left. The door bay has steps, pedestals, pilasters with sunk panels, scroll consoles, an architrave and a hood 0.72 m proud. The double door is iron-framed timber with studs and ring knockers, under a lattice transom. Two iron wall lanterns.
  - Scale 9.2 (the Meshy facade 0.870 → 8.0 m). Size 8.0 × 6.64 m (depth stretched from the Meshy 6.21 m to the plot), cornice top 15.36, ridge 16.48, back eave 15.15, pots 17.48. Ground floor inside at 0.48 (threshold), ceiling 15.0.
  - Doors: two closed leaves, `SM_House_GuildmastersRowhouse_Door` (left hinge, BP component `Door` at (268.5, −27, 48) cm, opens with yaw −90) and `…_Door_1` (right hinge, `Door_1` at (531.5, −27, 48) cm, opens with yaw +90). Each is 1.315 × 3.805 m with its own box collision. The doorway is 2.64 × 5.12 m (threshold 0.48 to 5.60, transom from 4.29).
  - Placed as `House_GuildmastersRowhouse` at (19372, 5756, 1300), yaw 180: world x 18572–19372, butting onto house #1 with no gap (section 8). Nothing was hidden: the old row houses 00172–00183 are no longer in the level. No NavBounds (`"nav_bounds": false`).
  - Materials: 15 `MI_House_GuildmastersRowhouse_*` (Lamp is new). Targets measured on the Meshy front (`specs/GuildmastersRowhouse_colors.json`, overlay checked). Slot use on this house: Stone = ashlar walls; Trim = cornice, band, architraves, sills, hoods, door surround; Gauged = plinth, steps, chimney stack, roof ribs; Metal = window frames (brown-grey paint); Bronze = black iron (door frame and studs, transom, lanterns); Door = door panels; Lamp = lantern glass (emissive 1.5); Roof = slate. Brick, Brass and Dial are unused (house #1 values).
  - Checks: audit 0 see-through rays (street, both sides, lower town, above); Ext 10715 faces, 16 UCX hulls, Nanite on.
  - Files: `specs/GuildmastersRowhouse.json`, `specs/GuildmastersRowhouse_colors.json`, `build/GuildmastersRowhouse/` (`.blend`, Ext + Door + Door_1 FBX, `doors.json`). Measuring pictures and clay renders: `Saved/HouseCheck/GuildmastersRowhouse/{measure,study,clay}` (not in git).
  - Gotchas: the plinth cap ran across the doorway until mouldings got `"breaks"`. The audit planes over the roof must stop below the coping top, or rays aimed over the wall edge count as "see-through" while they really pass outside the house.

**#3 SilversmithsOrnateAtelier** (`469e6db6-6948-4dfa-9972-b66795d3a463`)
- Pieces: 5 storeys of cream ashlar with 3 bays. Cast-iron balconies with ornate railings on the 2nd and 3rd floors, and a roof gallery with a railing. A small square **clock cupola** on top with a clock face and a spire finial. A shopfront ground floor.
- Palette: #c2b6a4:15 #b1a696:14 #d1c5b3:9; iron #393938:16 #545250:14.
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: 3 storeys under a flat roof, not 5. A rusticated ground floor 0.08 m proud of the upper wall (plinth 0–1.0 m, 0.15 proud; blocks 1.08–4.02; frieze to 4.38) with two shop windows (stallriser, sill, 2×2 lights under a 3-light transom), a recessed door between pilasters (steps, transom, 2×4-panel leaf), a fascia and four corbels. Two ashlar floors with corner pilasters (sunk panels) and 2×4 windows with architraves, sills and hoods: 3 bays front and back, 2 on each side. A two-storey cast-iron balcony across the front, 1.37 m deep: slabs at 4.38–4.64 and 8.08–8.38, lace railings to 5.56 and 9.38, returns to the wall at both ends, four cast columns with arched spandrels, and newels with balls. A cornice with dentils at 11.78–12.52 (0.37 proud), an iron railing round the roof terrace with four finial posts (to 14.32), and a square clock cupola (2.2 m, 12.52–16.66, cornice to 17.04) with the clock on the front and plain dials on the other three faces, iron pinnacles, a tiled pyramid to 18.80 with an arched dormer, and an iron finial to 21.86. The back repeats the front without the balcony, fascia and corbels; its door leaf sits only 0.28 m deep.
  - Scale 11.5 (the upper wall, 0.700 Meshy units wide, gives 8.05 m; the model is 1.90 tall, 21.88 m). Size 8.05 × 6.64 m (the Meshy body is 6.77 m deep; shortened to the plot). Floor inside at 0.26 (threshold), ceiling 12.2, roof deck 12.52.
  - Doors: `SM_House_SilversmithsOrnateAtelier_Door` (front, left hinge, BP component `Door` at (323.5, −46, 26) cm, opens with yaw −90) and `…_Door_1` (back, left hinge as seen from the back, `Door_1` at (471.5, −638, 26) cm, yaw −180, opens with a further −90). Front leaf 1.58 × 2.64 m, back leaf 1.38 × 2.64 m; both doorways are 1.79 × 3.64 m. **The back door opens onto the 10 m drop**: give it a landing and railing before it is ever opened (section 9).
  - Placed as `House_SilversmithsOrnateAtelier` at (21114, 5756, 1300), yaw 180: walls at world x 20309–21114, cornice 20272–21151 (slot 4's left edge), balcony out to y 5619. Nothing hidden (the plot was empty apart from the town plinth). No NavBounds.
  - Materials: 15 `MI_House_SilversmithsOrnateAtelier_*`. Targets measured on the Meshy front (`specs/SilversmithsOrnateAtelier_colors.json`, overlay checked). Slot use on this house: Stone = upper ashlar; Gauged = plinth, rusticated ground floor, door pilasters, steps; Trim = cornice, bands, pilasters, frieze, fascia, corbels, architraves, sills, hoods, balcony slabs, and the cupola's base, band, cornice, dial plates and dormer; **Brick = the cupola's walls and piers** (a greyer stone); Metal = window frames; Door = leaves and door frames (near black); Bronze = all the black iron (balcony, railings, posts, pinnacles, finial, clock ring, numerals, hands); Dial = clock dial and the plain discs; Roof = deck and cupola pyramid. Brass, Clay and Lamp are unused.
  - Checks: audit 0 see-through rays (street, both sides, lower town, above); Ext 44593 faces, 22 UCX hulls, Nanite on.
  - Files: `specs/SilversmithsOrnateAtelier.json`, `specs/SilversmithsOrnateAtelier_colors.json`, `build/SilversmithsOrnateAtelier/` (`.blend`, Ext + Door + Door_1 FBX, `doors.json`). Measuring pictures, depth maps and clay renders: `Saved/HouseCheck/SilversmithsOrnateAtelier/{measure,study,clay}` (not in git).
  - Gotchas: house space starts at the upper wall, not at the Meshy bounds (the balcony and cornice stand in front of it): house x = Meshy x − 0.33, house y = Meshy y − 1.374 at scale 11.5. clock_dial's brass centre disc would have come out black (Bronze), so this dial passes `centre="Dial"`.

**#4 AlchemistsMansardTenement** (`743eb1f3-78ed-425b-b031-4ce6c728ab18`)
- Pieces: rough grey stone with heavy soot streaks (use the `bake_weather` streaks, or a darker vertex tint). 3 storeys plus a slate mansard with 3 gabled dormers. A shopfront ground floor (dark timber, multi-pane). 3 bays.
- Palette: stone #5a5752:15 #787065:9 #9a8f80:7; slate #41454b:20 #35373a:17.
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: 3 storeys under a **gambrel (mansard) roof between two stone gables**, not a mansard on four sides. Random rubble walls with heavy soot streaks, ashlar quoins at all four corners (alternating 0.45 / 0.30 m on 0.33 m courses), a band at 6.56–6.74 all round and one at 3.24–3.46 on the sides and back (the shop cornice takes its place on the front). A timber shopfront across the ground floor: six pilasters, two 4×4 and two 2×4 shop windows on panelled stallrisers, a glazed shop door in the middle bay under a flush head panel, a fascia (2.64–3.14) with two consoles and a cornice 0.49 m proud (3.14–3.46), stone pier caps at the ends. Upper floors front and back: 3 casement windows each (4×4 on the 1st floor, 4×3 on the 2nd) with flat stone architraves, sills and lintels; the sides are blank. A pale gutter at the wall head (9.85–10.02). The roof: lower slopes flared at the eave (eave y −0.50 at 10.04) and steep (about 71°) to a break at y 1.46 / z 14.88, upper slopes at about 30° to the ridge at 15.96, 0.33 m past both gables. Each gable: stone up to the roof's underside, a verge band, one 2×4 window (11.0–12.58) under a lintel. Two gabled stone dormers on the front slope (centres 2.82 and 7.78, 1.54 m wide, 2×4 windows 11.0–12.56, slate roofs to a ridge at 13.92). No chimneys, no back door.
  - Scale 8.4 (the model is 1.90 tall: 15.98 m to the ridge; the wall, 1.263 Meshy units wide, gives 10.60 m). Size 10.60 × 6.64 m (the Meshy body is 7.02 m deep; the lower roof slopes keep their run from the walls, the upper slopes got a little steeper). Floor inside at 0.04 (threshold), ceiling 9.5, wall head 9.85.
  - Door: `SM_House_AlchemistsMansardTenement_Door` (front, left hinge, BP component `Door` at (475.5, −41, 4) cm, opens with yaw −90). A glazed shop door (6×7 lattice light over two panels), 1.09 × 2.28 m, in a 1.20 m doorway (the whole bay between the door pilasters). **The Meshy door is only about 0.9 m wide; it was widened to the bay to meet the 1.2 m doorway rule**, with thinner jambs (0.05) than the model's.
  - Placed as `House_AlchemistsMansardTenement` at (23075, 5756, 1300), yaw 180: walls at world x 22015–23075, roof 21982–23108 (slot 6's left edge is the roof's edge), shop cornice out to y 5707, back eave to y 6470 (over the drop). Nothing hidden (the old row houses are gone). No NavBounds.
  - Materials: 15 `MI_House_AlchemistsMansardTenement_*`. Targets measured on the Meshy front (`specs/AlchemistsMansardTenement_colors.json`, overlay checked). Slot use on this house: **Stone = the rubble walls and gables, on the `cobble` texture** (rounded stones, the closest CC0 set to random rubble), with high streak (0.8) and dirt for the soot; Trim = quoins, bands, architraves, sills, lintels, pier caps, gutters, verge bands, dormer sills and head bands (`ashlar` texture); Gauged = dormer fronts and cheeks (paler stone); Timber = the shopfront; Door = the leaf; Metal = all window frames and shop glazing bars; Roof = slate (main roof and dormers). Brick, Brass, Dial, Clay, Bronze and Lamp are unused.
  - Checks: audit 0 see-through rays (street, both sides, lower town, both gables, the attic from above); Ext 7164 faces, 12 UCX hulls (walls, gables, roof, dormers, floor), Nanite on.
  - Files: `specs/AlchemistsMansardTenement.json`, `specs/AlchemistsMansardTenement_colors.json`, `build/AlchemistsMansardTenement/` (`.blend`, Ext + Door FBX, `doors.json`). Measuring pictures, depth map and clay renders: `Saved/HouseCheck/AlchemistsMansardTenement/{measure,study,clay}` (not in git).
  - Gotchas: the house is wider than its slot by its roof overhang: the roof ends at world x 23108, 0.26 m into slot 7 (section 8). The dormer cheeks' feet follow the main roof's own profile points (5 cm under), so they never show above the slates; the dormer roofs simply run back into the main roof (to y 1.25), which stays whole underneath.

**#5 BookbindersMansardAtelier** (`dc67b13b-d46d-47ff-b7a5-68c24d8a51b5`)
- Pieces: cream ashlar, 3 storeys plus a blue-grey metal or slate mansard with 2 dormers and iron roof cresting. 3 bays of sash windows. An arched front door with steps.
- Palette: #ddccb5:14 #c8b8a2:13; mansard #595f66:14 #74777b:12; iron #2d2e2f:18.
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: 2 storeys of cream ashlar under a **standing-seam metal mansard with a flat top** between two stone gables (no mansard on the sides), not 3 storeys. A plinth (0–0.94, 0.12 proud) with basement vent grilles (2 front, 3 back), rusticated corner pilasters (0.54 wide, 0.06 proud, 0.27 courses) on both floors, a band at 4.54–4.76 (0.15 proud) and a crowning cornice at 8.38–8.76 (0.34 proud) right round, a grey gutter on it. Front ground floor: an **arched door** (opening 1.66 wide, spring 3.11, crown 3.94) in an archivolt (0.30 wide, 0.15 proud) on pedestals with a keystone, a sunburst fanlight over a transom rail, two panelled leaves 0.84 m deep in the opening, three steps; two 3×4 windows with plain sills. Upper floor front and back: three 2×4 windows (5.44–7.58) with architraves, sills on two brackets and cornice hoods. Back ground floor: two 3×4 windows and a **square double door** (1.32 wide, sill at 1.36, 6-light transom). The mansard: an apron from the gutter, then a steep slope (about 68°) from (0.10, 9.06) to the break at (1.52, 12.56), a curb to the flat top at 12.72, standing seams about every 0.78 m. Two pedimented stone dormers on the front **and** the back slope (2×3 windows 9.44–10.94, architrave, sill, entablature, pediment to 11.90, metal roof). An iron railing round the flat top (0.66 high, spear heads, rings) with a finial post at each corner (to 14.22). No chimneys.
  - Scale 7.5 (the model is 1.90 tall: 14.27 m; the main walls, 1.141 Meshy units apart, give 8.56 m). Size 8.56 × 6.64 m (the Meshy body is 6.87 m deep; the flat top is 0.23 m shorter, the slopes keep their run). Floor inside at 0.50 (the threshold), ceiling 8.30, wall head 8.76.
  - Doors: front `SM_House_BookbindersMansardAtelier_Door` (left hinge, BP component `Door` at (113.5, −90, 50) cm, opens with yaw −90) and `…_Door_1` (right hinge, `Door_1` at (278.5, −90, 50) cm, yaw +90), each 0.825 × 2.495 m; back `…_Door_2` (`Door_2` at (260.5, −624, 136) cm, yaw −180, opens with a further −90) and `…_Door_3` (`Door_3` at (129.5, −624, 136) cm, a further +90), each 0.655 × 1.675 m. **The front leaves sit 0.84 m deep, deeper than the 0.45 m wall**, as in the Meshy model: the reveals and soffit run on into the room (a short timber-lined tunnel). **The back door opens onto the 10 m drop** and its sill is 1.36 m up: give it a landing and railing before it is ever opened (section 9).
  - Placed as `House_BookbindersMansardAtelier` at (24728, 5756, 1300), yaw 180: walls at world x 23872–24728, cornice and finials 23831–24769. Nothing hidden (the old row houses are gone). No NavBounds.
  - Materials: 15 `MI_House_BookbindersMansardAtelier_*`. Targets measured on the Meshy front (`specs/BookbindersMansardAtelier_colors.json`, overlay checked). Slot use on this house: Stone = ashlar walls and gables (`ashlar` texture); Trim = pilasters, band, cornice, architraves, sills, brackets, hoods, archivolt, pedestals, keystone, top step, dormers; Gauged = plinth and lower steps; Roof = the zinc mansard, gutter and dormer roofs (`slate` texture, metal 0.3); Bronze = black iron (railing, finials, grilles, fanlight); Metal = window frames; Door = leaves, transom rails and the door lining. Brick, Brass, Dial, Clay and Lamp are unused.
  - Checks: audit 0 see-through rays (street, both sides, lower town, both gables, the attic from above); Ext 11724 faces, 17 UCX hulls, Nanite on.
  - Files: `specs/BookbindersMansardAtelier.json`, `specs/BookbindersMansardAtelier_colors.json`, `build/BookbindersMansardAtelier/` (`.blend`, Ext + Door … Door_3 FBX, `doors.json`). Measuring pictures, depth maps and clay renders: `Saved/HouseCheck/BookbindersMansardAtelier/{measure,study,clay}` (not in git).
  - Gotchas: house x = Meshy-house x − 0.27 (the side walls' main face; the corner pilasters stand 0.06 proud of it), house y = Meshy-house y − 0.46. The back-depth map (`--from back`) keeps house x (not mirrored); the back facade's wall x is W − house x. The corner finials are the kit's leafy `iron_finial`, a bit heavier than the Meshy's thin spikes.

**#6 GrandApothecaryHerbalistShop** (`dfbeef57-3b33-4ca5-8106-b7dd2ee53649`)
- Pieces: a stone ground floor with a big round-arched shopfront (green-painted timber, glazed). 3 half-timbered storeys above (dark timber frame, cream infill) with green shutters. A steep slate gable roof with a dormer, and a hanging sign bracket.
- Palette: infill #beaa90:10 #dfc8a9:9; timber #544c3a:13; stone #9e8e79:12; slate #454c56:9; green shutters (sample them; they're small).
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: a stone ground floor (0–6.38, ashlar on a plinth to 0.6) whose front is one **deep round-arched porch**: opening 4.80 wide (spring 3.30, crown 5.70) under an archivolt (r 2.40–3.00, 0.10 proud) on imposts that wrap the corners; a stone soffit 1.48 m deep back to a tympanum with a barred 4×4 window and a green fascia, then a green **timber shopfront** 1.83 m behind the face (3×4 and 4×4 shop windows on panelled stallrisers, a glazed door with a 2-light transom). **Narrow arched passages** run through both piers into the porch (y 0.98–1.78). Above it two **jettied half-timbered storeys**: the first 0.34 m out (6.38–10.28, a bressummer 0.24 proud on six corbels), the second a further 0.63 m (10.28–14.43, bressummer on nine joist ends). Cream plaster infill, dark posts, rails, studs and corner braces; 2×4 casements with **open green plank shutters** on the front (2 per storey), one unshuttered 2×4 window per storey on each side, none at the back. A gable roof with the ridge towards the back: a kick at the eave (−1.47, 14.42) → (−0.57, 14.94), 42° to the ridge (3.08, 18.27), a short back slope to (7.07, 16.95); framed gables; one timber-and-plaster dormer off-centre (x 2.35–3.67, 2×3 window, pediment to 17.70). A wrought-iron hanging sign on the right corner of the first floor, 2.9 m out over the street. No chimneys, no back door.
  - Scale 9.6 (the model is 1.90 tall: 18.27 m; the ledger's estimate). House space starts at the stone ground floor: house x = Meshy-house x − 0.48, house y = Meshy-house y − 3.37. Size 7.14 × 6.64 m (the Meshy ground floor is only 4.18 m deep; stretched to the plot, the back slope flattened to about 18° like house #2's). Floor inside at 0.02, ceiling 14.1.
  - Door: `SM_House_GrandApothecaryHerbalistShop_Door` (the shop door, left hinge, BP component `Door` at (270.5, −192, 2) cm, opens with yaw −90): a glazed leaf 1.19 × 2.54 m. **The Meshy door is 1.06 m wide; widened to 1.20 m** (thinner door posts) for the doorway rule. It sits 1.83 m back inside the porch.
  - Placed as `House_GrandApothecaryHerbalistShop` at (20126, 5756, 1300), yaw 180: stone walls at world x 19412–20126, jettied storeys 19400–20138, roof 19372–20166 (its right-hand overhang touches house #2's side wall line at 19372); jetties to y 5638, roof eave 5609, sign to 5408. 1.06 m free to house #3's cornice (20272). Nothing hidden. No NavBounds.
  - Materials: 15 `MI_House_GrandApothecaryHerbalistShop_*`. Targets measured on the Meshy front (`specs/GrandApothecaryHerbalistShop_colors.json`, overlay checked). Slot use on this house: Stone = ashlar ground floor, porch, soffit, tympanum, passages (`ashlar`); Gauged = plinth; Trim = archivolt and imposts; **Plaster = the cream infill** (and the dormer face); Timber = all framing, corbels, soffits under the jetties, dormer posts and bargeboards, tympanum window frame; **Door = the green paint: shopfront, door leaf, fascia, shutters**; Metal = casement frames (dark brown, `timber` texture); Bronze = iron (sign, window bars, shutter straps); Roof = slate. Brick, Brass, Dial, Clay and Lamp are unused.
  - Checks: audit 0 see-through rays (street into the porch, both side passages, both sides, the lower town, the attic from above); Ext 3545 faces, 32 UCX hulls, Nanite on.
  - Files: `specs/GrandApothecaryHerbalistShop.json`, `specs/GrandApothecaryHerbalistShop_colors.json`, `build/GrandApothecaryHerbalistShop/` (`.blend`, Ext + Door FBX, `doors.json`). Measuring pictures, depth maps and clay renders: `Saved/HouseCheck/GrandApothecaryHerbalistShop/{measure,study,clay}` (not in git).
  - Gotchas: a straight roof line from the measured slope put the eave 0.3 m too low and hid the eave plate; the Meshy roof flattens at the eave, so the profile has a kick point. The passages straddle the shopfront plane in the Meshy model; here they end just in front of it (y 0.98–1.78, crown 3.10). The porch's side faces step down from 3.30 to 3.18 behind the tympanum.

**#7 TextilePrintersDryingLoft** (`a6db1738-7e72-4931-8f8c-962d040a354e`)
- Pieces: a stone ground floor with a plank door. 2 tall storeys clad in dark vertical timber louvres (the drying loft). A slate gable roof with a timber-framed gable end and a hoist opening.
- Palette: timber #4a423b:17 #2b2623:15 #5f564e:14; stone #797169:11 #96928c:7.
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: a narrow tower-like loft, 1 stone storey and 2 timber storeys, with the **gable to the front** (the ridge runs front to back). A random-rubble ground floor (0–3.80) with a ledged plank door (opening 1.22 × 2.44, the leaf 0.30 deep, two full-width iron straps) between dressed-stone jambs (0.28 wide, 0.05 proud) under a long lintel (2.06–4.16, 2.44–2.80, 0.09 proud). Timber bands 3.80–4.12 (0.05 proud) and 7.00–7.34 (0.06 proud) right round. The timber storeys (4.12–7.17 and 7.17–11.36) stand **0.14 m behind the stone face** and are clad in fixed louvres between corner posts and rails: 3 bays front and back on both storeys; on the sides plaster | louvre | plaster below and 2 louvre bays above. A tie beam (11.36–11.68) at the stone line under each gable; the gables framed with plaster infill, principal rafters, studs, a sill rail and a collar round a central opening (1.04 × 1.38): **two dark plank loft doors** in the front gable, a single-light window in the back gable. A slate roof at 45.7° from the eaves (−0.31, 11.50) to the ridge (3.14, 15.04), 0.15 m past both gables, with bargeboards. An **iron hoist** out of the front apex: a beam to a pulley wheel 2.3 m out (r 0.36, centre 14.44), a strut down to the gable's sill rail, ropes to a lower block and a hook (to 12.52). No chimneys, no back door.
  - Scale 7.9 (the model is 1.90 tall: 15.04 m; at 8.0 the roof, 7.02 m wide, would leave 1.5 cm to house #5). House space starts at the stone ground floor: house x = Meshy-house x − 0.32, house y = Meshy-house y − 2.58. Size 6.28 × 6.64 m (the Meshy stone box is 5.65 m deep; stretched to the plot: the side bays keep their member widths and move apart, the upper side louvres got wider). Floor inside at 0.02, ceiling 11.2.
  - Doors: `SM_House_TextilePrintersDryingLoft_Door` (front, left hinge, BP component `Door` at (250.5, −36, 2) cm, opens with yaw −90): a 1.21 × 2.42 m plank leaf in the 1.22 m doorway. The loft doors `…_Door_1` (left hinge, `Door_1` at (262.25, −35, 1196.5) cm, yaw −90 to open) and `…_Door_2` (right hinge, `Door_2` at (365.75, −35, 1196.5) cm, yaw +90), each 0.52 × 1.37 m, 12 m up: **give the loft a floor before they are ever opened**.
  - Placed as `House_TextilePrintersDryingLoft` at (23787, 5756, 1300), yaw 180: stone walls at world x 23159–23787, roof 23126–23818 (0.18 m after house #4's roof at 23108, 0.13 m before house #5's cornice at 23831), hoist out to y 5500 at z 2552–2780. Nothing hidden. No NavBounds. (`get_actor_bounds` reports x to 23915 and z from 1172; the components themselves are where they should be.)
  - Materials: 15 `MI_House_TextilePrintersDryingLoft_*`. Targets measured on the Meshy front, back and right pictures (`specs/TextilePrintersDryingLoft_colors.json` for the front, overlay checked). Slot use on this house: Stone = rubble (`plaster` texture, as house #1); Trim = door jambs, lintel, threshold (`ashlar`); Timber = all framing, bands, tie beams, louvres, rafters; Plaster = side panels and gable infill; Door = the plank door; **Metal = the dark loft doors** and the back window frame; Glass = the back window (pale blue-grey, as the Meshy one); Bronze = iron (door straps and ring, hoist); **Clay = the hoist ropes**; Roof = slate, ridge cap and bargeboards. Brick, Gauged, Brass, Dial and Lamp are unused.
  - Checks: audit 0 see-through rays (street with the door, louvres and loft doors, both sides, the lower town, the attic from above and from the sides); Ext 5323 faces, 18 UCX hulls, Nanite on.
  - Files: `specs/TextilePrintersDryingLoft.json`, `specs/TextilePrintersDryingLoft_colors.json`, `build/TextilePrintersDryingLoft/` (`.blend`, Ext + Door … Door_2 FBX, `doors.json`). Measuring pictures, depth maps (front, back and the new side map `depth_left.npy`) and clay renders: `Saved/HouseCheck/TextilePrintersDryingLoft/{measure,study,clay}` (not in git).
  - Gotchas: the hoist strut lands on the gable's sill rail in front of the loft doors, as in the Meshy model, so the doors open inwards past it. The roof is `mansard_roof` turned a quarter (`loft_roof_frame`); its hull comes back in the turned frame and is mapped to house space by the builder.

**#8 GlassblowersHighDraftFurnace** (`92f0f8a1-4830-4246-a6e1-902d00486af4`)
- Pieces: a cream ashlar gable house, 3 storeys, with a slate roof. A very tall **brick chimney stack** with iron bands rising beside the facade. Small windows and a plank door.
- Palette: stone #d7ccb8:21 #bdb19e:13; brick #7e4d35:13 #633421:9; slate #515254:15.
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: a narrow ashlar house (one tall room, no storey marks) with its **gable to the front** (the ridge runs front to back) and an **octagonal brick chimney stack** against the left side wall, not a 3-storey house. Front: a plank door (opening 1.76–3.30 × 0–2.64, a dark timber frame 0.12 wide, the leaf 0.37 behind the face) in a pale stone surround (jambs 0.26 wide, 0.05 proud), a shelf (2.64–2.90, 0.15 proud), a 4×2 transom (2.94–3.56) in a stone frame under a cap (3.60–3.84); a 1×4 window (2.04–3.08 × 8.28–10.06) on a sill under a head; a vent slit (2.43–2.71 × 11.58–12.28) under a small lintel. Back: the same slit only. Right side: plain. A slate roof at 54.4° from the eaves (−0.28, 10.32) to the ridge (2.555, 14.28), 0.30 m past both gables, dark bargeboards. The stack: a stone pier (2.29 out from the wall, 2.94 along it, to 3.40) with a 2×4 window on its outer face, a brick taper to the octagon at 5.00 (apothem 1.17, centre 1.045 out from the wall), the shaft tapering to 1.01 at 16.98, a corbelled cap to 18.02 with a sooty flue, five riveted iron bands (5.02, 7.16, 9.38, 12.00, 14.50; 0.24 high); iron tie rods along the left wall at 5.12 and 7.28 that turn the corners to round anchor plates on the front and back (0.58 from the corner), a third rod at 9.50 between two wall plates either side of the stack. No back door.
  - Scale 9.5 (the model is 1.90 tall: 18.07 m; the Meshy body is then 5.11 m wide, and the whole front with the stack 7.76 m, which fits slot 5). House space starts at the stone body: house x = Meshy-house x − 2.32, house y = Meshy-house y − 0.36. Size 5.11 × 6.64 m (the Meshy body is 7.08 m deep; fitted to the plot with the stack kept centred, cy 3.33). Floor inside at 0.02, ceiling 10.2, side walls to 10.51 under the eaves.
  - Door: `SM_House_GlassblowersHighDraftFurnace_Door` (front, left hinge, BP component `Door` at (188.5, −43, 2) cm, opens with yaw −90): a 1.29 × 2.32 m plank leaf with a ring pull (the Meshy has a handle there), in the 1.30 m gap of the timber frame.
  - Placed as `House_GlassblowersHighDraftFurnace` at (21721, 5756, 1300), yaw 180: walls at world x 21210–21721, roof eave 21180 (0.29 m from house #3's cornice at 21151), the stack's pier and cap to 21955 (0.27 m before house #4's roof at 21982). Nothing hidden. No NavBounds. (`get_actor_bounds` again reports y from 5628 and z from 1172, as for house #7.)
  - Materials: 15 `MI_House_GlassblowersHighDraftFurnace_*`. Targets measured on the Meshy front (`specs/GlassblowersHighDraftFurnace_colors.json`, overlay checked) and the roof on the left picture. Slot use on this house: Stone = ashlar walls and the pier (`ashlar`); Trim = door surround, shelf, transom frame and cap, window sill and head, slit lintels, the pier window's sill (paler); **Brick = the stack** (`brick` texture); **Timber = the brown window frames and the door frame**; Door = the leaf; **Metal = the vent slits and the flue** (dark grey); Glass = blue-grey; Bronze = iron (bands, rods, plates, the leaf's ring); Roof = slate, ridge cap, bargeboards. Gauged, Brass, Dial, Clay and Lamp are unused.
  - Checks: audit 0 see-through rays (street, both sides, the lower town, the attic from above and the sides); Ext 2854 faces, 10 UCX hulls (walls, roof, floor, pier, taper, shaft, cap), Nanite on.
  - Files: `specs/GlassblowersHighDraftFurnace.json`, `specs/GlassblowersHighDraftFurnace_colors.json`, `build/GlassblowersHighDraftFurnace/` (`.blend`, Ext + Door FBX, `doors.json`). Measuring pictures, depth maps (front, back, left side) and clay renders: `Saved/HouseCheck/GlassblowersHighDraftFurnace/{measure,study,clay}` (not in git).
  - Gotchas: the first build lost the door, transom and window: `outline_face` kept only the last of several holes stacked in one strip (fixed in the kit, section 5). The roof's left eave runs into the stack, as in the Meshy model. The tie rods pass through the shaft between the plates (hidden inside it).

**#9 HighSpireTavern** (`eaef5756-feaf-469a-9c07-9204c614b5f6`)
- Pieces: a half-timbered gable house (black frame, white infill), 3 storeys, with a dark shopfront ground floor. A stone **tower with a slate spire** and finial on one side. Slate roof.
- Palette: slate #383c41:20 #494d51:16; infill #d0c4b2:7 #ada395:10; stone #8e867c:9.
- **Result: DONE, 2026-11-14** (under the strict rules of section 0: no captures, no material or lighting tests, no 5 m check).
  - What the Meshy model really is: a half-timbered shop house with its **gable to the street** (the ridge runs front to back) and a **square stone tower at its back-left corner**, the tower 1.4 m wider than the house on the left. A dark (near-black, blue-grey) timber shopfront round the front and the front part of both sides: pilasters, shop windows on sunk-panel stallrisers (front 3×4 and 5×4, two 3×4 on the right, one on the left beside a plaster panel), a glazed door with a 2-light transom, fascia, frieze, carved consoles over the pilasters and a beam at 3.72–3.98. A jettied timber first floor (3.98–6.88, black frame, cream infill, corner braces) with a **canted oriel bay** (2 lights on the front, 1 on each cant) on three corbels; the gable jettied again over a bressummer (6.88–7.14), framed (posts, rails, collar, king post, struts), a two-light window, a verge rafter and heavy bargeboards, a spike finial at the apex. The back part of the house is plain stone up to the eaves and the back gable, with a slit window on the right and a downpipe. The tower: ashlar with quoins, a round-headed window high on every face (11.62–13.00), two slits and a round-headed plank door on its left face (sill 3.65) reached by a short flight of steps and a stone stair wall sloping up along the tower; a **hipped bell-cast spire** (eave 13.80, flare to the wall line at 14.20, 72° above) with a gabled dormer on each face, a short ridge along x at 18.72 and two spike finials to 19.98. No chimneys.
  - Scale 10.5 (not the ledger's 22 m guess: the door is then 2.36 m and the first floor 2.9 m; 19.98 m to the finials). House space: x 0 = the shopfront's left face (Meshy-house x 1.84), y 0 = its pilaster face (Meshy-house y 1.18). The Meshy model is 8.9 m deep; the tower keeps its square 3.42 × 3.42 m and ends on the plot line, so the **front block is shortened from 5.48 to 3.22 m** (the side shop windows and the side timber framing are re-spaced to fit). Size 6.24 × 6.64 m at the ground (first floor −0.12..6.36, jetty 0.17 m; gable face at y −0.58, roof front edge −1.18, eaves −0.36..6.62), tower x −1.42..2.00, y 3.22..6.64, stair wall to x −1.81. Floor inside at 0.14, ceiling 6.80, tower ceiling 13.30.
  - Doors: `SM_House_HighSpireTavern_Door` (the shop door, left hinge, BP component `Door` at (193.5, −26, 14) cm, opens with yaw −90): a glazed leaf 1.19 × 2.36 m (2×3 light over two panels) in a 1.20 m doorway. **The Meshy door is 1.14 m wide; widened to 1.20 m** (thinner pilasters). `…_Door_1` (the tower door on its left face, left hinge as seen from outside, `Door_1` at (−116, −538.5, 365) cm, yaw 90, opens with a further −90): a plank leaf 0.91 × 1.61 m under a fixed timber tympanum, 3.65 m up at the top of the stair wall. **The tower has no floor at that height: give it one before the door is ever opened.**
  - Placed as `House_HighSpireTavern` at (25451, 5756, 1300), yaw 180: the right eave's bargeboard at world x 24786 (0.17 m from house #5's cornice at 24769), shop walls 24827–25451, the tower 25251–25593, the stair wall to 25632 (1.8 m before slot 10's 25812). Gable and roof out to y 5638, the back on y 6420 (spire eave to 6446). Nothing hidden. No NavBounds. (`get_actor_bounds` reports z from 1172, as for houses #7 and #8.)
  - Materials: 15 `MI_House_HighSpireTavern_*`. Targets measured on the Meshy front (`specs/HighSpireTavern_colors.json`, overlay checked). Slot use on this house: Stone = the tower, the back block, steps and stair wall (`ashlar`, high dirt and streaks for the grimy Meshy stone); Trim = quoins, window and door surrounds, sills, the door step; **Door = the near-black shopfront paint** (pilasters, frames, stallrisers, fascia, frieze, consoles, the shop door); Timber = all framing, beams, oriel, corbels, brackets, dormers, the tower door; Plaster = the infill and the shop's left plaster panel; Metal = window frames and the slits, the downpipe; Glass = blue-grey; Bronze = the spike finials and the tower door's straps; Roof = slate. Brick, Gauged, Brass, Dial, Clay and Lamp are unused.
  - Checks: audit 0 see-through rays (street, the oriel, both sides, the lower town, the attic from above, through the tower's windows and door, the spire and its dormers); Ext 24777 faces, 28 UCX hulls, Nanite on.
  - Files: `specs/HighSpireTavern.json`, `specs/HighSpireTavern_colors.json`, `build/HighSpireTavern/` (`.blend`, Ext + Door + Door_1 FBX, `doors.json`). Measuring pictures, depth maps (front, back, both sides at scale 10.5) and clay renders: `Saved/HouseCheck/HighSpireTavern/{measure,study,clay}` (not in git).
  - Gotchas: the first audit found 2 rays through a 1 cm ring between a tower window's fanlight glass and the arch soffit; `arched_opening` now carries the soffit back to the inner reveal and the fanlight glass is the full radius. A rerun of `build_house.py` with `--no-bake` (to read the audit quickly) **overwrites the exported FBX without the dirt bake**: always rebuild without it before importing. The Meshy "corner turrets" on the spire seen in the front and back pictures are the side dormers seen edge-on.

**#10 WaterWardensGatehouse** (`736555e3-85f9-405a-8082-7fba02ddd74c`)
- Pieces: a stone gate block with 2 large **through-arches** (open passages) and a crenellated parapet. A timber-clad watch room on top with a slate hip roof and finials.
- Palette: stone #787773:19 #696661:14 #8c8b87:11; timber #5d4b34:17 #332b22:9; slate #3c3f42:13.
- Note: in G1 the arches open from the cathedral lane to the back terrace over the drop. Add a parapet or railing at the back edge.
- Result: _not built_.

**#11 ClockworkEngineersClockTower** (`acb0eabf-2565-40e7-b8d8-8e5565a15926`)
- Pieces: a slender square stone tower. Clock faces on several sides with exposed brass **gears** below the clock. A slate spire with 4 corner pinnacles. Enterable with a stair that winds up around the walls.
- Palette: stone #a9a39a:18 #928d85:13 #c1baae:9; brass #a59169:4; slate #3c3832:13.
- Result: _not built_.

**#12 MagistratesOfficialTenement** (`3e141de1-2ac7-4e12-ab28-cf504b393845`)
- Pieces: a grey coursed-stone block, 3 storeys, flat roof with a parapet. Small rectangular windows. An arched iron-gated door and a wall lantern.
- Palette: #6a6863:16 #787671:16 #5a5854:14 #888680:10.
- Result: _not built_.

**#13 BookbindersDormerAtelier** (`8182e2cc-962a-4ab3-8faa-03691797d910`)
- Pieces: a stone ground floor with an arched door. 2 half-timbered storeys (black frame, white infill) with a row of 3 arched windows. A steep slate gable with a dormer.
- Palette: infill #d2cab8:9; timber #302d2c:20 #413d39:15; stone #76746f:11 #97948d:8.
- Result: _not built_.

**#14 PottersKilnHouse** (`d6b0a10d-3e5c-4a41-a579-55d23cc69532`)
- Pieces: a brick **bottle kiln** (a hollow cone; enterable at ground level). Half-timbered sheds with slate roofs, a stone base, and a lean-to with barrels and pots as props.
- Palette: brick #825a40:15 #9b7351:12; stone #9b978d:15; slate #575759:14; timber #4f443b:18.
- Result: _not built_.

**#15 MapmakersSkyObservatory** (`4f56a4cf-e7a3-4547-9252-86a52b3c20b1`)
- Pieces: a crenellated stone base block with an arched door. A tall round tower with an **external spiral stair** and a gallery ring. A metal **observatory dome** with a slot and a brass telescope.
- Palette: stone #8d8982:26 #7f7c76:19 #9a958d:17; dome #b0a491:9; brass (sample it).
- Result: _not built_.

**#16 GuildsMeetingHall** (`943eb6ad-5903-4ca9-9999-c4e72d527a6f`)
- Pieces: a classical temple front in cream stone. Giant pilasters or engaged columns, an entablature and a triangular pediment with a relief. A big arched timber door and arched blind windows. A grey gable roof.
- Palette: #b2a797:22 #a4998a:19 #c0b5a5:17 #d3c8b8:8; door #625549:6.
- Reuse: `gothic_kit.entablature`, `column_shaft`, `corinthian_capital`.
- Result: _not built_.

**#17 ShipwrightsMoldLoft** (`64ead4f5-f36c-4ef2-b2c8-4e0a8b05be71`)
- Pieces: a stone ground floor with an arched door. A half-timbered upper floor with a row of arched multi-pane windows. A steep slate gable to the front.
- Palette: slate #353538:24 #282626:17; stone #93908c:7 #b1a99d:7; timber #4c4947:9.
- Result: _not built_.

**#18 BankVaultGuardhouse** (`976d1b4c-4d8e-4523-815e-88dc324fb652`)
- Pieces: a massive grey stone block with crenellations. Small **barred** windows, a huge iron double gate and drainpipes. Flat roof.
- Palette: #747069:17 #837e78:15 #65615b:14 #938f89:13; iron #2f2f30:13.
- Result: _not built_.

**#19 AssayersIronBalconiedVilla** (`e0d0e0fb-bdc5-4838-939f-9b3ca8fbda57`)
- Pieces: a cream ashlar villa, 3 storeys. A dark iron-and-glass **oriel bay** on the 1st and 2nd floors over the door. A flat roof with an ornate **iron roof railing**, and drainpipes.
- Palette: stone #ab9f91:14 #9b8f83:12 #bbafa0:8; iron #1b1c1e:26 #2c2e31:18.
- Result: _not built_.

**#20 ScholarsCornerLibrary** (`4f6b9de3-5f3e-4877-8c56-25ab5176ec8b`)
- Pieces: a 2-storey stone block with large multi-pane iron-framed windows. A round **corner turret** with a ribbed metal dome. A slate hip roof with a flat top. The turret goes on the plot's open corner.
- Palette: stone #aaa59a:11 #8d8b85:9 #cabeab:7; slate #444645:19 #52585c:15; dome #6b7175:11.
- Result: _not built_.

**#21 CanalSideMerchantHouse** (`d1f57ebe-78b9-4f79-96d0-c0d2e8685eae`)
- Pieces: a stone undercroft with arches (weathered and mossy). A terrace with an iron railing. A stone storey, then a timber-clad storey with a balcony. A slate gable roof.
- Palette: stone #8e8577:11 #aba69b:4; timber #7d705e:13 #665b4c:13; slate #514a3e:15.
- Result: _not built_.

**#22 BlacksmithsSteamWorkshop** (`aff2ebdb-9de7-4ae6-b2bd-10dbcdd5c252`)
- Pieces: a crenellated stone base with a large arched opening (the forge). A brick-and-timber upper storey. A slate hip roof with 2 tall brick chimneys.
- Palette: stone #8c887e:19 #a6a298:12; brick #774932:8 #97664c:6; slate #36383b:20.
- Result: _not built_.

**#23 BellFoundersCastingHouse** (`c7aa5cfe-e803-4577-93a9-42882c4de5a7`)
- Pieces: a stone base with 2 huge round arches (casting bays). A big half-timbered gable above (with a hoist beam and loft door). A slate roof.
- Palette: slate #3a404a:20 #505357:18 #2d323b:17; stone #8a8782:14 #aaa79f:8; timber #4f453b:6.
- Result: _not built_.

**#24 BakersGabledBakery** (`35d96bb4-1cf8-43cc-8168-aa99e30a5bdf`)
- Pieces: a stone arcade ground floor. A half-timbered upper floor (cream infill) with flower boxes. Slate roofs with dormers, and 2 very tall **brick chimneys** (oven flues). A small porch roof over the door.
- Palette: slate #48535c:17; timber #594b39:16 #3c352d:14; infill #e0d1bb:8; stone #a69d90:13; brick #7d5e47:12.
- Result: _not built_.

**#25 RopeMakersIndustrialShed** (`f87c7f7b-2230-4b24-9dbf-f8c739c39998`)
- Pieces: a long low shed with a **sawtooth roof** (5 teeth, slate). Brick and timber gable ends, a stone base, and big timber rope wheels as props.
- Palette: slate #3e3e40:24 #555353:21; brick #9d6143:12 #b57e61:4; stone #8e8b85:10.
- Result: _not built_.

**#26 CooperageBarrelStorehouse** (`b5e1f8fc-39f9-4958-a338-95d5c3f35862`)
- Pieces: a long stone barn, 2 storeys, with large plank double doors. A slate roof with a raised clerestory or vent ridge, and timber framing at the ends.
- Palette: slate #363638:28 #444241:16; stone #8d8a82:15 #a9a399:7.
- Result: _not built_.

**#27 TannersTiledTenement** (`2eed4ce4-fb05-44ad-9d66-434d97f16735`)
- Pieces: a cluster of 2–3 joined houses: a weathered stone block plus a half-timbered gable house. **Mixed red clay and slate tile roofs** with several brick chimneys.
- Palette: stone #797065:13 #968e81:10; red tile #784e33:8; slate #49423e:20.
- Needs a clay roof-tile texture set (section 3, so ask first).
- Result: _not built_.

**#28 CoalMerchantsSteppedManor** (`99e79f19-ef11-45eb-b114-3eee2ec964c8`)
- Pieces: a grey stone manor with stepped roof levels (several slate gables at different heights), coal chutes, and a tall black iron **flue pipe**.
- Palette: slate #343538:23 #424345:17; stone #828079:13 #94928a:12 #a9a69e:8.
- Result: _not built_.

**#29 DocksideTollhouse** (`58455809-aa73-40e2-9601-0a5089677a59`)
- Pieces: a round stone tower with a **copper conical roof** and finial. A lower wing with a copper lean-to roof, and wall lanterns.
- Palette: stone #89867f:20 #a5a29a:12; copper #99654a:9 #b4795d:7.
- Needs a copper material (MI_Cath_BronzeRust tinted, or a new set).
- Result: _not built_.

**#30 DistillersCopperDomeShed** (`0b1fd298-b059-43f5-8adf-8f490217df18`)
- Pieces: a stone shed with a steep brown tile or shingle gable roof. A large **copper pot still** with pipes beside the house, as a prop.
- Palette: roof #45342b:22 #574437:17; stone #807b72:11 #9c9991:8; copper #925f3d:9.
- Result: _not built_.

## 8. Plot plan
The user approved this on 2026-11-13: "ring + next streets", and "facade exact, depth fits the plot".
- **Depth rule:** keep each house's **front facade and roofline exactly** as Meshy made them, at the chosen scale. The depth follows the plot. Side walls hidden by neighbours may be plain. Back facades that can be seen (G1 backs face the lower town) get finished in the same style.
- **Scale rule:** set the scale from the storey heights (section 3). If the facade is a little wider than its slot, scale the whole house down by up to 10%. If it needs more than that, ask the user.
- **Old houses:** hide (never delete) every old house actor whose footprint overlaps the new house, and list them in the house's details.
- **Coordinates:** world cm. Street level on the upper plinth is z = 1300, and in the lower town z ≈ 300.
- **Cathedral (for the 5 m rule):** right-side chapel wall at about Y = 5130, with buttresses to about Y = 5245. Left-side chapel wall at about Y = 1730, with buttresses to about Y = 1615. Front (stairs) at X ≈ 15592, apse end at X ≈ 26590. Always check against the BP's components.

| Group | Old actors (labels) | Area (world cm) | Depth | Street z | Front | Slots |
|---|---|---|---|---|---|---|
| **G0** | Cathedral_RowHouse_00170 | x 16572–17228, y 5606–6417 | 8.1 m | 1300 | −Y | Clockwork Clock Tower |
| **G1** | Cathedral_RowHouse_00171 … 00183 | x 17972–27128. Front y ≈ 5756–5806: keep the current set-backs or more, then re-check 5 m. Back y ≈ 6420 is the edge of a 10 m drop to the lower town | 6.2–6.7 m (171–179), 7.4 m (180–181), 10.7 m (182–183) | 1300 | −Y, toward the cathedral | See the G1 slot list below |
| **G2** | Plaza_FlankHouse_00304, 00305, 00306 | x 12770–15329, y 4719–6463 | 17 m | ≈300 | −Y toward the grand stair (check which side is the street) | 1 Bank Vault (west), 2 Assayers Villa (east) |
| **G3** | Girona_StreetHouse_00031–00034 | Two columns either side of the stepped street at x 17292–17908 (z 551–1051). 32 = x 16395–17292 / y 6400–7700; 31 = same x / y 7700–9000; 34 = x 17908–19205 / y 6400–7700; 33 = same x / y 7700–9000 | 9 m (31, 32), 13 m (33, 34) | ≈300, rising with the steps | Toward the stepped street: +X for 31/32, −X for 33/34 | 32 Bell Founders; 31 Canal Side; 34 Blacksmiths; 33 Scholars Library (turret on the open +Y corner) |
| **G4** | Cathedral_LatticeHouse front and middle rows, plus Infill_InfillHouse_00033/00034. Keep the back row (y < −138) | Merge the front and middle rows to y −231 … ≤ 1105 (5 m from the left buttresses). The old front row reaches y 1342, which breaks the 5 m rule | about 13 m | 1300 (the raised court at 211/212 is 1530) | +Y, toward the cathedral lane | 4a x 15214–16436 Mapmakers (west edge drops to the plaza, z 300); 4b x 16572–17528 Bookbinders Dormer (lattice 200/201/203; keep the Cathedral_LatticePassage free or bridge it); 4c x 17664–18763 Potters Kiln (lattice 211–214, raised court); 4d x 18910–19886 Magistrates (lattice 215–218) |
| **G5** | OldTown_OldHouse_00221–00225 | 5a x 15270–17400; 5b x 18270–19843; y −1886 … −918 | 9–9.7 m | 1300 | −Y (the +Y side has only a 1.3 m gap to the lattice back row) | 5a Bakers (replaces 221–223); 5b Rope Makers (replaces 224–225) |
| **G6** | OldTown_OldHouse_00226–00234 | x 20224–27290, y −2786 … −1818 | about 9 m | 1300 | Check: the +Y side at x < 22630 is only 2 m from Church_SquareHouse | Along +X: 1 Cooperage, 2 Tanners, 3 Coal Merchants, 4 Dockside Tollhouse, 5 Distillers (67 m in 70.7 m) |
| **G7** | Church_SquareHouse_00072–00074 | x 20272–22628, y −1615 … −556 | 10.6 m | 1300 | +Y, toward the square north of the cathedral | 1 Guilds Meeting Hall (15 m), 2 Shipwrights (8.3 m) |

**G1 slots**, along +X with 0.2 m gaps (estimates):
1. Clockmakers 17972–18572
2. Guildmasters 18592–19392
3. Grand Apothecary 19412–20252
4. Silversmiths 20272–21172
5. Glassblowers 21192–21962
6. Alchemists 21982–23062
7. Textile Printers 23082–23772
8. Bookbinders Mansard 23792–24832
9. High Spire Tavern 24852–25792
10. Water Wardens 25812–27128

**Changed by house #9 (2026-11-14):** slot 9 is built against house #5: the right eave's bargeboard at world x 24786 (0.17 m from #5's cornice at 24769), the tower and its stair wall on the +X side to 25632. **Slot 10 (Water Wardens) keeps 25812–27128**; 1.8 m stays free between the stair wall and slot 10. The tower door on the tower's +X face (world) looks into that gap.

**Changed by house #8 (2026-11-14):** slot 5 is built in the gap between houses #3 and #4: roof eave at world x 21180 (0.29 m from #3's cornice), walls 21210–21721, the chimney stack on the +X side to 21955 (0.27 m before #4's roof). **The G1 row from slot 1 to slot 8 is now complete**; slots 9 and 10 remain.

**Changed by house #7 (2026-11-14):** slot 7 is built from its left edge (23128, as moved by house #4): roof at world x 23126–23818 (walls 23159–23787), 0.13 m free to house #5's cornice (23831). The G1 row from slot 1 to slot 8 is now built except slot 5.

**Changed by house #6 (2026-11-14):** slot 3 is built against house #2: roof at world x 19372–20166 (walls 19412–20126, jettied storeys 19400–20138), so 1.06 m stays free up to house #3's cornice at 20272. The G1 row from slot 1 to slot 8 is now built except slots 5 and 7.

**Changed by house #5 (2026-11-14):** slot 8 starts at **23838** (0.2 m after slot 7's estimated end, 23128 + 6.9 m = 23818): house #5's cornice runs at world x 23831–24769 (walls 23872–24728; the corner finial leaves reach 23831), so the slot is 23838–24769 and slot 9 (24852) is untouched. If Textile Printers (slot 7) measures wider than 7.1 m, it must be scaled down or its slot shifted left, not slot 8.

**Changed by house #4 (2026-11-14):** slot 6 is built from its left edge: roof (with its 0.33 m gable overhangs) at x 21982–23108, walls 22015–23075. The roof reaches 0.26 m into slot 7: **slot 7 (Textile Printers) now starts at 23128** (0.2 m gap after the roof); shift slots 8–10 only if the later houses need the room.

**Changed by house #3 (2026-11-14):** slot 4 is built from its left edge: cornice at x 20272–21151 (walls 20309–21114). Slot 3 (Grand Apothecary) now has 19372–20272 between houses #2 and #3: 9.0 m, including any cornice overhang.

**Changed by house #2 (2026-11-13):** slot 2 moved 0.2 m to x 18572–19372, so house #2 butts onto house #1 (a closed terrace, no slit between the two). The old row houses `Cathedral_RowHouse_00172` … `00183` are **no longer in the level** (only 00171 is left, hidden by house #1), so the G1 slots are empty ground and there is nothing to hide. Keep each new G1 house against its built neighbour where it can be.

The slots are built out of x order (see the build order). Keep each slot's left edge fixed and leave any slack at its right end. If a built house turns out wider than its slot, shift only the slots that aren't built yet and record the change here.

The survey data for every plot (bounds, open sides, street z) is in `Saved/HouseCheck/district_plots.json`. Saved/ is not in git, so re-run `SourceArt/Houses/tools/survey_plots_ue.py` if it's missing.

## 9. Open questions and notes for the user
- **Missing texture sets:** clay roof tiles (Tanners #27), copper sheet (Dockside #29, Distillers #30, maybe Scholars' dome #20) and rough rubble stone. Ask before downloading CC0 sets.
- **Old open question from the cathedral work, unchanged:** lamp posts `Plaza_GasLamp*` stand 0.6 m in front of the stair terraces.
- **House #9's tower door** (on the tower's side facing slot 10) is 3.65 m up at the top of the stair wall, and the tower has no floor there: it needs a floor or landing inside before it is ever opened.
- **G1 backs** look over a 10 m drop to the lower town. Any back door or balcony there needs a railing. Houses #3 and #5 have closed back doors (as in the Meshy models) on that side; #5's sill is 1.36 m above the street level.

## 10. Log
- 2026-11-13: Task 0 done. Downloaded the 30 textured GLBs (the textured one is the OLDER task of each Meshy pair; the newer is an untextured companion). Rendered references, palettes and contact sheets, surveyed the district, wrote the plot plan, and created this ledger. The user approved: kit rebuild, empty interiors (rooms and stairs), replace district houses, one house per task with `/clear` between, ring + next streets, facade exact with depth fitted to the plot.
- 2026-11-13: House #1 ClockmakersNarrowTownhouse done (the pilot). The user approved the look and marked it 100% complete. It created `house_kit.py` (with the permanent **Bronze** slot), `build_house.py`, the three `*_ue.py` editor scripts and the tools `measure_meshy.py`, `check_5m.py`, `color_match.py`, `house_capture.py` and `ue_house.py`. `ncap.py` gained `hide`, `only` and `ortho`, and `crop.py` now keeps alpha-0 captures readable.
- 2026-11-13: Scope change by the user: from house #2 on, only the outside matters. The inside is one empty space (no floors, stairs or NavMesh), and the main door is closed but built as a separate leaf so it can open later. House #1 stays as built.
- 2026-11-13: The user: "every house should have a door … the house should look identical to the Meshy reference". House #1's front door is now closed as a separate leaf (`SM_House_…_Door`, BP component `Door`, colour 1.00), and the ledger asks for every Meshy door, closed, on every house.
- 2026-11-13: Added section 1a, Lessons: everything that cost time on house #1, with what to do instead. The task prompt now says to read the scope, rules and lessons first.
- 2026-11-13: The user's strict rules are now section 0: the Meshy model is a visual and colour guide only; no screenshots or captures; no lighting, exposure or material tests; no 5 m checks for now; the house is done once the import and placement scripts run.
- 2026-11-13: House #2 GuildmastersRowhouse done: a 3-storey ashlar row house with a gable roof, a grand double door and lanterns, an empty shell inside, placed against house #1. New builder type `townhouse_gable`, `"interior": "shell"`, double separate leaves, the **Lamp** slot, kit pieces for mouldings, architraves, the studded leaf, lanterns, consoles, copings, roof ribs and lattices, and the measuring tools `raycast_meshy.py` and `profile_meshy.py`.
- 2026-11-14: House #4 AlchemistsMansardTenement done: a 3-storey sooty rubble tenement with ashlar quoins, a timber shopfront with a glazed shop door, a gambrel roof between stone gables with gable windows, and two gabled dormers. New builder type `townhouse_mansard`, kit pieces `outline_face`, `verge_band`, `glazed_leaf`, `quoin_corner`, and the tool `depth_runs.py`. Slot 7 moved to start at 23128.
- 2026-11-14: House #5 BookbindersMansardAtelier done: a 2-storey cream ashlar house with rusticated corner pilasters, an arched front door with a fanlight, a standing-seam metal mansard with a flat top between stone gables, pedimented dormers front and back, and an iron railing with corner finials; front and back double doors closed as separate leaves. New builder type `townhouse_zinc`, the roof option `"cover": "seam"`, the `panel_leaf` option `weights`, and kit pieces for round arches, fanlights, vent grilles and seam sheets. Slot 8 now starts at 23838.
- 2026-11-14: House #6 GrandApothecaryHerbalistShop done: a stone ground floor with a deep arched porch in front of a green shopfront (side passages through the piers), two jettied half-timbered storeys with open green shutters, a gable roof with its ridge towards the back, a timber dormer and a hanging iron sign; the shop door closed as a separate leaf. New builder type `townhouse_jetty`, kit pieces `timber_members`, `plank_shutter`, `barred_window`, `hanging_sign`, optional gutters and dormer trim/posts, `DEPTH_Z1` for `depth_runs.py` and a fix in `raycast_meshy.py --along x`.
- 2026-11-14: House #7 TextilePrintersDryingLoft done: a narrow drying loft with a rubble ground floor and a plank door in a dressed surround, two timber storeys clad in louvres (set back from the stone, timber bands at the joints), framed gables front and back (loft doors under an iron hoist at the front, a window at the back) under a slate roof whose ridge runs front to back; the door and both loft doors closed as separate leaves. New builder type `townhouse_loft`, kit pieces `louvres`, `plank_leaf`, `hoist_crane`, the `jetty_wall` options `infill`, `louvres` and `holes`, roof options `ridge_cap` and `barge`, and side depth maps in `raycast_meshy.py`.
- 2026-11-14: House #3 SilversmithsOrnateAtelier done: a 3-storey cream ashlar shop house with a two-storey cast-iron balcony, a railed roof terrace and a clock cupola with a tiled pyramid and iron finial; front and back doors closed as separate leaves. New builder type `townhouse_terrace`, kit pieces for iron lace, railings, cast columns, newels, spandrels, blank dials, pyramid roofs, arched blocks and finials, and the `clock_dial` options `rosette` and `centre`.
- 2026-11-14: House #8 GlassblowersHighDraftFurnace done: a narrow ashlar house with its gable to the front (door in a stone surround with a transom, a 1×4 window, vent slits front and back) and an octagonal brick chimney stack on a stone pier against its left wall (riveted iron bands, a corbelled cap, tie rods round the corners to anchor plates); the door closed as a separate leaf. New builder type `townhouse_stack`, kit pieces for octagonal shafts, bands, anchor plates and tie rods, `loft_roof` split out of `townhouse_loft`, `jetty_wall` options `reveal` and `louvre_mat`, and a fix in `outline_face` for stacked holes. The G1 row is complete from slot 1 to slot 8.
- 2026-11-14: House #9 HighSpireTavern done: a half-timbered shop house with its gable to the street (a dark shopfront round three sides, a jettied first floor with a canted oriel, a framed gable jettied again under heavy bargeboards) and a square stone tower at its back-left corner (quoins, round-headed windows, a round-headed door reached by a stone stair, a hipped bell-cast spire with four dormers and two spike finials); the shop door and the tower door closed as separate leaves. New builder type `townhouse_tavern`, kit pieces `arched_opening`, `spike_finial`, `spire_rings`. Scale 10.5 (not 22 m); the front block is shortened to fit the 6.64 m plot behind a full-size tower.
