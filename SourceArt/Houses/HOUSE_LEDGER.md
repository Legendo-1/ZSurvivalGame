# House ledger: 30 Meshy houses rebuilt as sharp, enterable buildings

This file holds all state for the house rebuild. Every task starts by reading this file and ends by updating it.
Chat history and the session scratchpad are gone after `/clear`, so anything the next task needs must be written here.

**Task prompt, to paste after `/clear`:**

> Read SourceArt/Houses/HOUSE_LEDGER.md and do the next incomplete house.

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

## 1b. Per-house routine
1. **Read** this ledger: the house's row and details, its plot group (section 8), the conventions and the kit index. Set the house's status to `in progress`.
2. **Study the template.** Look at `ref_*.png`, then load `model.glb` in Blender and take section drawings with `SourceArt/Cathedral/v3/tools/template_slices.py`, adapted to the GLB. Decide the scale (section 3), then measure storey heights, bay widths, opening sizes, cornices and the roofline.
3. **Fit the plot.** Find the slot's exact area. Check the 5 m rule and which side is the street, and list the old actors that overlap. Decide the real depth (section 8, depth rule).
4. **Write `specs/<Folder>.json`:** scale, footprint, storeys, bays, openings, roof, materials, palette and room layout. Measure the palette for each surface by sampling `model_textures/base_color.png` at UVs picked on each surface, not from the raw k-means clusters.
5. **Kit.** Add any new pieces to `house_kit.py` (additive only, rule 4) and list them in section 5.
6. **Build** with `build_house.py --spec ... --audit --renders ...`. The audit must report 0 see-through rays. Compare the clay renders with `ref_front.png` and fix any proportion that is off.
7. **Materials.** Create or update the `MI_House_<Folder>_*` instances, tinted to the palette.
8. **Import** into `/Game/Buildings/Houses/<Folder>/`. Fix Nanite and placeholder materials (section 4), add collision, and make `BP_House_<Folder>`.
9. **Place it.** Hide the old actors, place the BP and run the 5 m check against the cathedral components.
10. **Captures** to `Saved/HouseCheck/<Folder>/`: front and three-quarter views at eye height, close-ups at 1–2 m, an interior view on each floor, and a `SCS_BASE_COLOR` view matched to `ref_front.png`. Compare the colour of each surface, and adjust the tints until they're within about 5%.
11. **Playability.** Check that the player capsule fits every door and stair (trace or test). Rebuild the NavMesh and confirm it covers each floor.
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
  - Meshes `SM_House_<Folder>_Ext` (exterior), `SM_House_<Folder>_Int` (interior: floors, inner walls, stairs) and `SM_House_<Folder>_Col` (collision only, if needed).
  - Blueprint `BP_House_<Folder>`. Level actor label `House_<Folder>`.
  - Materials `MI_House_<Folder>_<Surface>`, with Surface one of Brick, Plaster, Stone, Trim, Timber, Shutter, Door, Roof, Metal, Copper, Glass.
- **Source files:** `SourceArt/Houses/specs/<Folder>.json` is the house spec. `SourceArt/Houses/build/<Folder>/` holds the `.blend` and `.fbx` (committed, like the cathedral v3 files).
- **Blender house space:** metres, origin at the **front-left corner at street level**. +X runs along the frontage (to the right when you face the front door), the front face is at y = 0 and the house extends to +Y. Z is up, with z = 0 at the street in front of the main door.
- **To the level:** the FBX export (axis_forward −Z, up Y, as `build_common.py` does) gives UE mesh y = −Blender y. **House #1 must verify this on import and record the exact actor transform formula here.**
- **Playability:**
  - Doorways at least 1.2 m wide × 2.3 m high.
  - About 3.4 m floor to floor, ground floor 3.6–4.0 m. Inner wall 0.15 m, outer wall 0.35–0.5 m.
  - Rooms ideally at least 4 × 4 m. The third-person camera boom needs space.
  - Stairs: 0.18 m rise, 0.28 m tread, at least 1.2 m wide, with a landing at each floor. Collision is a ramp.
  - Collision: simple boxes or UCX per wall run, slab and stair ramp. Leave door openings open. Windows are blocked at the glass.
  - NavMesh must cover every floor, so zombies can follow players in.
- **The 5 m rule:** every house wall is at least 5 m from the cathedral's walls and buttresses. Check it against the `BP_GironaCathedral_V3` components, not its bounding box: the box includes the stairs and front terrace.
- **Materials:** tiling textures on the mesh's own UVs at 2 m per repeat, as in the cathedral. Master `/Game/CityArt/Materials/M_Cathedral_Stone`, with vertex colour R = block tone, G = crevice dirt, B = streaks, A = macro. The CC0 sets in `/Game/CityArt/Textures/CC0/` are ashlar, brick, cobble, plaster, slate and timber. There is no roof-tile, rubble-stone or copper set yet: if a house needs one, download a CC0 set (for example from ambientCG) only after asking the user, then record it here. Make the per-house instances with `make_house_materials_ue.py` (house #1 creates it), tinted to the palette.
- **Colour match target:** under `SCS_BASE_COLOR`, the average of each surface on the new house should be within about 5% of the same surface on the Meshy model from the same view.
- **Replacing old houses:** hide the old actors with `set_is_temporarily_hidden_in_editor` plus Hidden in Game and NoCollision, and **never delete** them. Record their labels in the house row so the change can be undone.

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
- **Captures:** `SourceArt/Cathedral/v3/tools/ncap.py` is a template (`__OUT__`, `__MODE__`, `__VIEWS__`, `__FILLS__`). It was filled in by a scratchpad wrapper (`nshoot.sh`) that no longer exists. **House #1 must add a repo runner** `SourceArt/Houses/tools/house_capture.py` (fill the template, then run spawn, tick about 30 times over separate remote calls, export, done) and record its usage here. Nanite pages stream in over editor ticks, so a single-call capture looks badly simplified.
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
| _(none yet)_ | | |

## 6. Status
- **LOOK APPROVED:** no. House #1 (the pilot) must be shown to the user, and the user says yes.
- **Fix list:** none.
- **Open questions for the user:** see section 8.

## 7. House table
Build order = row order. Status is one of `todo`, `in progress`, `waiting for look approval` (the pilot only), `done`, `blocked`.
"Est. size" is width × depth × height in metres, estimated from storey count. Width is the street frontage. With the depth rule (section 8) the depth follows the plot, not the Meshy model.

| # | Status | Folder | Plot (section 8) | Est. size W×D×H | Front faces |
|---|---|---|---|---|---|
| 1 | todo | ClockmakersNarrowTownhouse | G1 slot 1 | 6.0 × 7.9 × 20 | −Y |
| 2 | todo | GuildmastersRowhouse | G1 slot 2 | 8.0 × 7.0 × 17.5 | −Y |
| 3 | todo | SilversmithsOrnateAtelier | G1 slot 4 | 9.0 × 8.8 × 22.6 | −Y |
| 4 | todo | AlchemistsMansardTenement | G1 slot 6 | 10.8 × 7.7 × 15.3 | −Y |
| 5 | todo | BookbindersMansardAtelier | G1 slot 8 | 10.4 × 8.9 × 16.3 | −Y |
| 6 | todo | GrandApothecaryHerbalistShop | G1 slot 3 | 8.4 × 8.2 × 18.6 | −Y |
| 7 | todo | TextilePrintersDryingLoft | G1 slot 7 | 6.9 × 8.4 × 15 | −Y |
| 8 | todo | GlassblowersHighDraftFurnace | G1 slot 5 | 7.7 × 7.8 × 18 | −Y |
| 9 | todo | HighSpireTavern | G1 slot 9 | 9.4 × 11.5 × 22 | −Y |
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
- Result: _not built_.

**#2 GuildmastersRowhouse** (`9909a371-bb89-460e-8771-045ff7cebee2`)
- Pieces: 4 storeys of cream ashlar with channelled ground-floor rustication. 3 bays of tall sash windows with moulded surrounds. A central double timber door with a fanlight and two wall lanterns. A heavy cornice, a flat roof with 2 skylights and one chimney.
- Palette: #c7b7a2:18 #afa08d:16 #e0d0b9:13; door/timber #785d4a:12.
- Result: _not built_.

**#3 SilversmithsOrnateAtelier** (`469e6db6-6948-4dfa-9972-b66795d3a463`)
- Pieces: 5 storeys of cream ashlar with 3 bays. Cast-iron balconies with ornate railings on the 2nd and 3rd floors, and a roof gallery with a railing. A small square **clock cupola** on top with a clock face and a spire finial. A shopfront ground floor.
- Palette: #c2b6a4:15 #b1a696:14 #d1c5b3:9; iron #393938:16 #545250:14.
- Result: _not built_.

**#4 AlchemistsMansardTenement** (`743eb1f3-78ed-425b-b031-4ce6c728ab18`)
- Pieces: rough grey stone with heavy soot streaks (use the `bake_weather` streaks, or a darker vertex tint). 3 storeys plus a slate mansard with 3 gabled dormers. A shopfront ground floor (dark timber, multi-pane). 3 bays.
- Palette: stone #5a5752:15 #787065:9 #9a8f80:7; slate #41454b:20 #35373a:17.
- Result: _not built_.

**#5 BookbindersMansardAtelier** (`dc67b13b-d46d-47ff-b7a5-68c24d8a51b5`)
- Pieces: cream ashlar, 3 storeys plus a blue-grey metal or slate mansard with 2 dormers and iron roof cresting. 3 bays of sash windows. An arched front door with steps.
- Palette: #ddccb5:14 #c8b8a2:13; mansard #595f66:14 #74777b:12; iron #2d2e2f:18.
- Result: _not built_.

**#6 GrandApothecaryHerbalistShop** (`dfbeef57-3b33-4ca5-8106-b7dd2ee53649`)
- Pieces: a stone ground floor with a big round-arched shopfront (green-painted timber, glazed). 3 half-timbered storeys above (dark timber frame, cream infill) with green shutters. A steep slate gable roof with a dormer, and a hanging sign bracket.
- Palette: infill #beaa90:10 #dfc8a9:9; timber #544c3a:13; stone #9e8e79:12; slate #454c56:9; green shutters (sample them; they're small).
- Result: _not built_.

**#7 TextilePrintersDryingLoft** (`a6db1738-7e72-4931-8f8c-962d040a354e`)
- Pieces: a stone ground floor with a plank door. 2 tall storeys clad in dark vertical timber louvres (the drying loft). A slate gable roof with a timber-framed gable end and a hoist opening.
- Palette: timber #4a423b:17 #2b2623:15 #5f564e:14; stone #797169:11 #96928c:7.
- Result: _not built_.

**#8 GlassblowersHighDraftFurnace** (`92f0f8a1-4830-4246-a6e1-902d00486af4`)
- Pieces: a cream ashlar gable house, 3 storeys, with a slate roof. A very tall **brick chimney stack** with iron bands rising beside the facade. Small windows and a plank door.
- Palette: stone #d7ccb8:21 #bdb19e:13; brick #7e4d35:13 #633421:9; slate #515254:15.
- Result: _not built_.

**#9 HighSpireTavern** (`eaef5756-feaf-469a-9c07-9204c614b5f6`)
- Pieces: a half-timbered gable house (black frame, white infill), 3 storeys, with a dark shopfront ground floor. A stone **tower with a slate spire** and finial on one side. Slate roof.
- Palette: slate #383c41:20 #494d51:16; infill #d0c4b2:7 #ada395:10; stone #8e867c:9.
- Result: _not built_.

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

The slots are built out of x order (see the build order). Keep each slot's left edge fixed and leave any slack at its right end. If a built house turns out wider than its slot, shift only the slots that aren't built yet and record the change here.

The survey data for every plot (bounds, open sides, street z) is in `Saved/HouseCheck/district_plots.json`. Saved/ is not in git, so re-run `SourceArt/Houses/tools/survey_plots_ue.py` if it's missing.

## 9. Open questions and notes for the user
- **Missing texture sets:** clay roof tiles (Tanners #27), copper sheet (Dockside #29, Distillers #30, maybe Scholars' dome #20) and rough rubble stone. Ask before downloading CC0 sets.
- **Old open question from the cathedral work, unchanged:** lamp posts `Plaza_GasLamp*` stand 0.6 m in front of the stair terraces.
- **G1 backs** look over a 10 m drop to the lower town. Any back door or balcony there needs a railing.

## 10. Log
- 2026-11-13: Task 0 done. Downloaded the 30 textured GLBs (the textured one is the OLDER task of each Meshy pair; the newer is an untextured companion). Rendered references, palettes and contact sheets, surveyed the district, wrote the plot plan, and created this ledger. The user approved: kit rebuild, empty interiors (rooms and stairs), replace district houses, one house per task with `/clear` between, ring + next streets, facade exact with depth fitted to the plot.
