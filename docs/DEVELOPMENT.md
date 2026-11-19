# Development log — MX Dirt Mod Alt

*[Versión en español](DESARROLLO.md)*

How kadenball's MX Dirt Mod 1.7.0 became this alternative version. Each entry says what was seen (in game or in the
logs), what was found in the code and what was done. The numbers come from the mod's real logs (`mx_dirt.jsonl`,
`mx_dirt_render.log`), the Windows error log and the tests.

---

## 1. Understanding the mod (2026-11-15)

The mod is two MX Bikes plugins written in C:

- **`mx_dirt.dlo` (core):** hooks the game's wheel-force function (every physics step) and its per-frame scheduler.
  From what it reads of each wheel (position, load, slip, material) it runs a soil simulation (Bekker sinkage, mass
  conservation, berms, roost, moisture, bumps) and publishes the heights into the game's own deformation grid, so the
  bike **really** rides the ruts.
- **`mx_dirt_render.dlo` (render):** draws a GLSL pass over the terrain the game has just drawn: relief, loose
  dirt layer, clods.

Everything is tied to one exact `mxbikes.exe` (SHA-256 and fixed offsets). Overview diagram:
[`diagrams/pipeline.html`](diagrams/pipeline.html).

## 2. Wider ruts on some tracks

**Seen:** on tracks with different meshes the rut came out wider.
**In the code:** the minimum half width was `0.7 × cell`: the mesh, not the tyre, set the width.
**Done:** `rut_width_cm` — width in metres, with a floor of 0.6 cells (the least the grid shows without "teeth"). A
`rut_width` log line per track gives the cell size and whether the mesh limits it.

Found on the way: the simulation is **always 2048×2048**; on a 4097-vertex track (STF, 800 m) that means 39 cm cells
and 47 cm ruts. Documented, not changed.

## 3. The renderer drew nothing with Graphics Overhaul

**Log:** `slot_check ... reason=4`, `drawn=0`. Overhaul had already replaced the `glDrawRangeElementsBaseVertex`
pointer and the renderer switched itself off by design.
**Done:** chain: if the pointer belongs to another plugin, the dirt mod's wrapper calls it. On exit it is restored only
if the slot still holds ours. It re-hooks on the next load.
**Result:** `reason=7` (chained), the renderer draws; ~2.2 ms GPU at 2560×1440.

## 4. The bike "stuck" and the "log" hit

**Felt:** leaned over, touch an old rut, fall.
**In the code:** packed walls held `repose_firm × cohesion` ≈ **59°** at the default moisture.
**Done:** `wall_angle_deg` (40° cap) and a 36 cm default rut. Also found: the game's "Deformation" slider at 100 %
**doubles** the mod's depth (25 % reference).

## 5. Detail pop-in and camera switches

**In the code:** the soft layer lived in a 51 m window centred 10 m ahead of the camera: detail popped in ~35 m ahead
and jumped on camera switches.
**Done:** a 128×128-tile window (102 m) centred 25 m ahead; the size lives in macros shared by C and GLSL, guarded by a
`_Static_assert`.

## 6. In-game menu (F8)

Built on MX Bikes' **official HUD API** (`DrawInit`/`Draw`), the one HUD plugins use: no new hooks. Tabs, keyboard and
mouse (own cursor), double confirmation for destructive actions, written straight to the `.ini` (the mod already reloads
it live).

**Our own bug:** the first menu reserved 17 strings and drew 31; it overwrote core memory. Symptoms: the simulation
stopped and the game crashed when leaving the track (`mx_dirt.dlo+0x338bd`, in `mxb_transport_pause`). The address was
traced by rebuilding the `.dlo`'s function order and matching it to `.pdata`. Fixed and bounded.

## 7. Live reset, smooth and watering

**Trap found:** the simulation stores heights *relative to the track as it was loaded*
(`target = initial DFM − (sim − ref)`). Zeroing the simulation went back to the saved wear.
**Done:** `mxdirt_height_field_rebase` replaces the reference (`0` or `×0.5`) and the whole track is resent through the
pre-wear path. Test: `tests/test_rebase.c`. Diagram: [`diagrams/nuevas-funciones.html`](diagrams/nuevas-funciones.html).

## 8. Presets, per-track profiles, moisture

- 6 editable built-in presets, and "save as…" with a typed name.
- Per-track profile: applied before the session is created; back on a track without one, the general settings return
  (`_global.ini`).
- Moisture: follows the game's "wet track" and, optionally, `OverhaulTrackWetness()` — a function Overhaul exports for
  other plugins (its machine code was checked: `float(void)`, read-only). The mod **never** depends on Overhaul.
- **Hypothesis dropped:** the game's weather was assumed to sit next to the deformation scale in memory (same order as
  `profile.ini`). The log showed `temperature` = 0 with 20 °C in the profile: it doesn't. Removed from the menu.

## 9. Stutter when terrain is handed to the game

**Overhaul's log:** `FREEZE ... at mx_dirt.dlo+0x316d3` (2.5 s). Traced to `private_range_exact`: a safety check that walks
the whole grid (16 MB) **about 5 times per hand-over**. After a reset ~1000 tiles are resent → seconds of freeze.
**First fix (this fork):** a full check at most every 250 ms.
**Final fix (kadenball, 1.7.1):** check 1/64 of the grid per hand-over, rotating — better, it spreads the cost and leaves
no periodic spike. This version uses his.

## 10. Lean erosion

Lean comes from the wheel axle and the ground normal (`lean = −axle·normal`), independent of which way the axle points.
The rut floor tilts toward the inside of the turn and the cut soil is shoved into the outside berm. `tests/test_lean.c`:
at 0.6 lean, inside floor −6.1 cm vs outside −4.4 cm; outside berm 9.5 mm vs 5.9 mm.

## 11. Real tyres and telemetry

- **Unpacked bikes:** `.cfg` → tyre set → `.tyre`: width (`2×TorusRadius`), `KnobsFactor` (roost) and `SlipSinkage`.
  Tested on real files: 450XF = 11/15 cm.
- **OEM bikes (encrypted):** the width is read from the wheel definition in memory, right after the radius the mod
  already read. Confirmed in game (OEM Honda CRF250R): `0.3415 · 0.0481 · 70.8 · 0.0001` — radius, torus, width and
  `radiusRPS`, the same order as a `.tyre` → 9.6/14.5 cm.
- **Official telemetry:** brakes and crash. Used only if the size and the wheel materials match what the mod reads.
  Still to be verified in game.
- The mod **already** measured real load, slip and lean from memory, so telemetry only adds what was missing (brake
  intent, crash).

## 12. The crash inside the game's code

`mxbikes.exe+0x2469bb`, render thread, with the mod's renderer off and Overhaul active. The executable is encrypted on
disk, so that function can't be inspected. Left unattributed; logs are now saved every 10 s so a next case leaves a
trail.

## 13. Merging kadenball's 1.7.1

Three-way merge (base 1.7.0, this fork, 1.7.1) with no conflicts, after normalising line endings. In: the sliced checks,
the fix for the **square of different ground following the bike**, the edge fades, `soft_lift_percent`, clods by
opacity, `terrain_timing` and hand-over pacing. Checked that his shader did not bring back the 64 window size.

## 14. Publishing

Public repo with the original's full history plus this fork's changes, GPL-3.0, `NOTICE.md` with the modification
notice, docs in Spanish and English and a bilingual menu. To collaborate: Discord **`fran8035`**.
