# Undo/redo: coverage, architecture and a visible affordance (#2214)

Research for [obs-rust/obs-studio#2214](https://github.com/obs-rust/obs-studio/issues/2214).
Every claim cites the tree as of 2026-10-24 (branch from `origin/master`).
Nothing here was reproduced at runtime; statuses marked *unknown* need a repro.

## 0. The stack in one paragraph

`undo_stack` (`frontend/utility/undo_stack.hpp:12-58`) holds two
`std::deque<undo_redo_t>` (`:28-29`); each entry is a name, two
`std::string` blobs and two `std::function` callbacks (`:19-25`). The stack
is capped at `MAX_STACK_SIZE 5000` (`undo_stack.cpp.hpp:3`, eviction
`:37-40`), clears redo on every new entry (`:56`, `:163-166`), and writes the
Edit menu text and enable state directly (`:23-27`, `:58-62`, `:78-86`,
`:102-110`). Plugins reach it through `obs_frontend_add_undo_redo_action`
(`frontend/api/obs-frontend-api.h:254`, `frontend/OBSStudioAPI.cpp.hpp:686-691`,
documented in `docs/sphinx/reference-frontend-api.rst:958-966`).

## 1. Coverage audit

### 1a. Undoable today (every `add_action` site)

| Operation | Evidence | Blob stored |
|---|---|---|
| Plugin/script action via frontend API | `frontend/OBSStudioAPI.cpp.hpp:689` | caller's strings |
| Paste transform | `frontend/widgets/OBSBasic_Clipboard.cpp.hpp:43` | transform states |
| Paste filter(s) (single / multiple / multi-dest) | `OBSBasic_Clipboard.cpp.hpp:245`, `:286` | filter JSON |
| Paste source / source ref | `OBSBasic_Clipboard.cpp.hpp:135-139` (via `CreateSceneUndoRedoAction`) | whole scene backup |
| Add source (new / existing) | `frontend/dialogs/OBSBasicSourceSelect.cpp.hpp:771`, `:820` | name only |
| Keyboard nudge (coalesced 1 s by own timer) | `frontend/widgets/OBSBasic_Preview.cpp.hpp:362` | transform states, all items |
| Mouse drag/resize/rotate in preview | `frontend/widgets/OBSBasicPreview.cpp.hpp:874` | transform states, all items |
| Deinterlace mode / order | `frontend/widgets/OBSBasic_SceneItems.cpp.hpp:263`, `:292` | uuid |
| Scale filtering, blending method, blending mode | `OBSBasic_SceneItems.cpp.hpp:364`, `:420`, `:472` | uuid |
| Delete scene item(s) | `OBSBasic_SceneItems.cpp.hpp:876-906` | whole scene backup |
| Move up/down/top/bottom | `OBSBasic_Scenes.cpp.hpp:864` (via `MoveSceneItem` `:882-890`) | scene backup of moved sources |
| Rotate / flip / fit / stretch / center (Edit > Transform) | `OBSBasic_SceneItems.cpp.hpp:1107`, `:1121`, `:1135`, `:1178`, `:1193`, `:1240`, `:1254`, `:1355`, `:1369`, `:1383` | transform states |
| Reset transform | `OBSBasic_Scenes.cpp.hpp:999` | transform states |
| Transform dialog | `frontend/dialogs/OBSBasicTransform.cpp.hpp:126` | transform states |
| Volume / mute / monitoring (mixer) | `frontend/components/VolumeControl.cpp.hpp:832` (repeatable), `:704`, `:726` | uuid |
| Rename via mixer | `VolumeControl.cpp.hpp:559` | uuid |
| Adv. audio: volume, balance, sync, force mono, monitoring, tracks | `frontend/components/OBSAdvAudioCtrl.cpp.hpp:387`, `:406`, `:473`, `:501` (repeatable), `:448`, `:522`, `:547` | uuid |
| Drag-and-drop files from the OS | `frontend/widgets/OBSBasic_Dropfiles.cpp.hpp:179` | name only |
| Show/hide scene item | `frontend/components/SourceTreeItem.cpp.hpp:136` | uuid |
| Rename source in tree | `SourceTreeItem.cpp.hpp:429` | uuid |
| Group items | `SourceTreeItem.cpp.hpp:338-347`, fallback `frontend/components/SourceTree.cpp.hpp:476-481` | whole scene backup |
| Ungroup | `frontend/components/SourceTreeModel.cpp.hpp:361-371` | whole scene backup |
| Reorder sources by drag in the tree | `SourceTree.cpp.hpp:231`, `:392-399` | scene backup |
| Duplicate / delete / add / rename scene | `OBSBasic_Scenes.cpp.hpp:295`, `:469`, `:711`, `:923` | scene JSON / name / uuid |
| Show/hide transition override (single / multi) | `frontend/widgets/OBSBasic_Transitions.cpp.hpp:1034`, `:1094` | transition JSON |
| Filter settings | `frontend/dialogs/OBSBasicFilters.cpp.hpp:188` | full filter settings JSON |
| Add / rename / delete filter | `OBSBasicFilters.cpp.hpp:572`, `:985`, `:1095` | filter JSON |
| Source properties (OK in dialog) | `frontend/dialogs/OBSBasicProperties.cpp.hpp:293` | full settings JSON, old and new |
| Context-bar toolbar property edits | `frontend/components/SourceToolbar.cpp.hpp:53` | full settings JSON |

### 1b. Not undoable

No area below is excluded on purpose anywhere in writing: `docs/`,
`frontend/` comments and the frontend API doc say nothing about undo scope.
The only trace of intent is an orphan string, `Undo.SceneCollection.Switch`
(`frontend/data/locale/en-US.ini:334`), which no code references.

| Operation | Undoable? | Evidence (mutation, no `add_action`) | Gap type |
|---|---|---|---|
| Video/canvas settings | No | `frontend/settings/OBSBasicSettings.cpp.hpp:3372` `SaveVideoSettings`; no `undo` in the file | Unimplemented, plausibly right to exclude (dialog has Cancel/Apply); undocumented |
| Output settings | No | `OBSBasicSettings.cpp.hpp:3602` | same |
| Audio settings | No | `OBSBasicSettings.cpp.hpp:3745` | same |
| Hotkeys | No | `OBSBasicSettings.cpp.hpp:3857` `SaveHotkeySettings` | same |
| Additional canvases | No | `frontend/widgets/OBSBasic_Canvases.cpp.hpp:20`, `:58`; no `undo` in the file | Unimplemented, undocumented |
| Scene-collection switch / new / dup / rename / remove / import | No; **stack is cleared** | `ClearSceneData` `frontend/widgets/OBSBasic_SceneCollections.cpp.hpp:1536`, `undo_s.clear()` `:1594`, called from `LoadData` `:1158-1160` and `CreateDefaultScene` `:1019-1023`; import `:623`, remove `:582` | Deliberate in effect (the clear), undocumented; the orphan locale key suggests it was once planned |
| Scene-collection export | n/a (no state change) | `OBSBasic_SceneCollections.cpp.hpp:631` | — |
| Profile switch / remove | No, and stack is **not** cleared | `frontend/widgets/OBSBasic_Profiles.cpp.hpp:695`, `:561`; no `undo` in the file | Unimplemented, undocumented |
| Scripts (add/remove/settings in Tools > Scripts) | No | `plugins/frontend-tools/scripts.cpp.hpp` has no `undo` | Unimplemented |
| Plugins | Only if they call the API | `frontend/api/obs-frontend-api.cpp.hpp:642-648` | Opt-in by design |
| obs-websocket requests | No | e.g. `plugins/obs-websocket/src/requesthandler/RequestHandler_SceneItems.cpp:527`, `RequestHandler_Inputs.cpp:370`, `RequestHandler_Filters.cpp:339`; no `undo` in `plugins/obs-websocket/src` | Unimplemented; must not be fixed inside the plugin (AGENTS.md "OBS WebSocket compatibility directive") |
| Studio mode toggle, preview/program scene | No | `frontend/widgets/OBSBasic_StudioMode.cpp.hpp:175`; `SetCurrentScene` `OBSBasic_Transitions.cpp.hpp:698-704` | Correctly excluded (navigation, not edit); undocumented |
| Projector windows | No | `frontend/widgets/OBSBasic_Projectors.cpp.hpp:140` | Correctly excluded (view state) |
| Scene list reorder (drag) | No | `frontend/components/SceneTree.cpp.hpp:141` emits `scenesReordered`, handler `frontend/widgets/OBSBasic.cpp.hpp:416-417` only refreshes multiview | Unimplemented |
| Lock/unlock scene item | No | `SourceTreeItem.cpp.hpp:150` | Unimplemented |
| Filter enable toggle | No | `frontend/components/VisibilityItemWidget.cpp.hpp:25` | Unimplemented |
| Filter reorder | No | `OBSBasicFilters.cpp.hpp:706`, `:714`, `:752`, `:760`, `:1123` | Unimplemented |
| Transition select / duration | No | `OBSBasic_Transitions.cpp.hpp:435`, `:1600` | Unimplemented |
| Group operations | Yes (group, ungroup) | see 1a | — |
| OS drag-and-drop | Yes (file drop) | see 1a | — |

## 2. Architecture recommendation

| Option | Verdict |
|---|---|
| Keep the C++ class | **Recommended for now.** It is 169 lines, works, and the bugs (section 4) are in the call sites and `scene_load`, not the container. The four-tier policy applies if it is ported later. |
| Port to Rust 1:1 | Low value alone: the class is a `QObject` bound to `Ui::OBSBasic` (`undo_stack.hpp:3`, `:17`, `:27`). A port should first split the pure deque/coalescing logic (testable without Qt) from the menu wiring. |
| Command layer in core (libobs) | Changes architecture: needs an RFC in obs-rust/rfcs. Not decided here. |

Smallest hooks:

- **Plugins:** already exist (`obs_frontend_add_undo_redo_action`,
  `frontend/api/obs-frontend-api.h:254`). Nothing new is needed.
- **obs-websocket:** the plugin must not be modified (AGENTS.md). The only
  route is on the Rust/frontend side: observe libobs signals (item
  transform, source update) and record entries. That records *every*
  libobs change, which would also catch the frontend's own changes twice and
  put remote edits on a local user's stack. Recommendation: draw the boundary
  at the frontend; remote edits are not undoable. If anyone wants otherwise,
  it is an RFC.

## 3. Semantics

| Topic | Current behaviour | Recommendation |
|---|---|---|
| Coalescing | Same-name repeatable entries merge, keeping the first undo and the latest redo (`undo_stack.cpp.hpp:46-50`) while the 3000 ms single-shot timer runs (`:8-9`, `:42-44`). Only mixer/adv-audio sliders pass `repeatable=true` (`VolumeControl.cpp.hpp:834`, `OBSAdvAudioCtrl.cpp.hpp:389`, `:408`, `:475`, `:503`). Nudges coalesce with their own 1 s timer (`OBSBasic_Preview.cpp.hpp:352-372`); drags make one entry on release (`OBSBasicPreview.cpp.hpp:860-876`). The 500 ms timer fixed in #765 is a different one, in `shared/properties-view/properties-view.cpp:2145`. | Keep. Name-based merge can wrongly join two different sources only if their names match; key on name+uuid when ported. |
| Redo invalidation | Any new entry clears redo (`undo_stack.cpp.hpp:56`). A coalesced merge returns before that line (`:49`), so redo survives a merged slider move. | Move `clear_redo()` before the merge return. Minor. |
| Memory at 5000 | Only `undo_items` is capped; eviction copies the entry before dropping it (`:38`). Two blobs per entry. Estimates from what sites store: uuid/name entries < 200 B; transform states store every item in the scene (`OBSBasic_Preview.cpp.hpp:354`, `true` = all items; ~350 B/item from `libobs/obs-scene.c.h:2312-2324`), so a 30-item scene is ~10 KB/entry, ~50 MB at 5000; a whole-scene backup (`OBSBasic_Scenes.cpp.hpp:789-811`) carries every source's full `obs_save_source` JSON including filters, typically 20-200 KB per blob, i.e. **0.2-2 GB** worst case at 5000. | Cap by bytes (e.g. 64 MB) as well as count, and store diffs for transforms (only changed items). |
| Collection switch | Stack cleared (`OBSBasic_SceneCollections.cpp.hpp:1594`), so no entry can outlive its objects. Profile switch does not clear (`OBSBasic_Profiles.cpp.hpp:695`), which is fine: profile objects are not in entries. | Keep. Document it; show a one-time hint or toast that history was reset. |
| Missing objects | Scene backups re-resolve by **name** (`OBSBasic_Scenes.cpp.hpp:836`) and recreate a missing source with `obs_load_source` (`:838`); transform states resolve by item id and silently skip missing items (`libobs/obs-scene.c.h:2387-2388`). | Resolve by uuid everywhere. |
| Enable state bug | `enable_internal()` enables Undo even when `undo_items` is empty (`undo_stack.cpp.hpp:117`), e.g. after any `pop_disabled()`/`enable()`. A visible button would be enabled and do nothing. | Prerequisite fix: `setEnabled(!undo_items.empty())`. |

## 4. Upstream bugs in this fork

| Bug | Status | Reasoning from code |
|---|---|---|
| obsproject/obs-studio#10120 (undo/redo breaks layout, ignores history) | **Unknown** (upstream could not repro on a minimal collection either) | Plausible mechanisms are all still here: scene-backup entries look sources up by name (`OBSBasic_Scenes.cpp.hpp:836`), so after a rename an older entry creates a duplicate (`:838`); scene reloads replace all items (`libobs/obs-scene.c.h:1121`); transform entries skip items whose id no longer matches (`obs-scene.c.h:2387-2388`); group lookup in transform restore is by name (`obs-scene.c.h:2422`). Needs a repro with the reporter's `bugged.json`. |
| #5516 (window/display capture blank after undo/redo) | **Unknown**; Windows-only (WGC), not testable on Linux | Upstream commenters reproduce it without undo, so it is likely a capture/studio-mode bug. Undo makes it more likely: transform undo calls `SetCurrentScene(..., true)` (`OBSBasic_SceneItems.cpp.hpp:1021`), and scene-backup undo re-runs `obs_source_load2` on every backed-up source (`OBSBasic_Scenes.cpp.hpp:855`, `libobs/obs-source.c.h:4325-4335`). |
| #5152 (sources flicker on undo of delete) | **Present** (by code; no runtime repro run) | `CreateSceneUndoRedoAction` calls `obs_source_update` on the scene (`OBSBasic_Scenes.cpp.hpp:849`), whose `scene_load` removes every item and re-adds it (`libobs/obs-scene.c.h:1115-1142`, `remove_all_items` at `:1121`). This is the cause upstream's maintainer gave. |

## 5. The affordance

| Item | Finding |
|---|---|
| Actions | `actionMainUndo`/`actionMainRedo` are the live pair: in the Edit menu (`frontend/forms/OBSBasic.ui:856-857`), defined `enabled=false` (`:2142-2157`), triggered at `frontend/widgets/OBSBasic_MainControls.cpp.hpp:590-598`, shortcuts Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y app-wide (`frontend/widgets/OBSBasic.cpp.hpp:374-381`). `actionUndo`/`actionRedo` (`OBSBasic.ui:1698-1713`) are dead: not in any menu, no code references. Remove them. |
| Enable state | Driven by the stack itself (`undo_stack.cpp.hpp:23-27`, `:58-62`, `:78-86`, `:102-110`, `:113-129`). A toolbar `QToolButton` bound to the same `QAction` inherits text, tooltip and enable state for free; fix the bug at `:117` first. |
| Toolbars | Only `scenesToolbar` (`OBSBasic.ui:1055`) and `sourcesToolbar` (`:1184`). No main toolbar; adding the actions to `sourcesToolbar` is the least disruptive spot. |
| Icons | No undo/redo icon in `frontend/forms/images/`. `revert.svg` exists (`frontend/forms/obs.qrc:38`) with theme variants (`frontend/data/themes/Yami.obt:539`, `Yami_Light.ovt:221`, `System.obt:85`). New `undo.svg`/`redo.svg` (redo = mirrored undo) must be added to `obs.qrc` and each theme's `Dark/`/`Light/` folders, following the `revert.svg` pattern. |
| Localization | `Undo.Undo`, `Undo.Redo`, `Undo.Item.Undo="Undo %1"`, `Undo.Item.Redo="Redo %1"` exist (`frontend/data/locale/en-US.ini:329-336`) with 47 per-action names (`:331-382`), already shown as menu text. A tooltip can reuse them; no new keys needed except for a toast or history panel. |
| Options | **Toolbar button:** a day's work, reuses existing wiring. **Toast** ("Undid: Rename 'X'"): small, uses the existing name; needs one key. **History panel:** needs read access to the deques and a jump-to API; a project of its own. |

## Split

| Pure discoverability (no behaviour change) | Real coverage work |
|---|---|
| Undo/Redo toolbar buttons bound to `actionMainUndo/Redo` | Fix #5152 (diff-based scene restore instead of `scene_load`) |
| undo/redo icons in all themes | Resolve by uuid, not name (#10120 mechanisms) |
| Fix empty-stack enable bug (`undo_stack.cpp.hpp:117`) | Byte cap, transform diffs |
| Remove dead `actionUndo`/`actionRedo` | Lock, filter toggle/reorder, scene reorder, transitions |
| Optional toast | Settings / hotkeys / scripts (decide in or out, and write it down) |

**Verdict:** the claim holds. The button is a day of work, because enable
state, names, shortcuts and localization already exist. Coverage and
correctness are a project: #5152 is present by code, #10120 and #5516 need
repros, and at least six user-visible edit types (section 1b) have no entry.

## Proposed first implementation PR

1. RED: a test that the Undo action is disabled after `push_disabled()`/`pop_disabled()` on an empty stack; GREEN: fix `undo_stack.cpp.hpp:117`.
2. Add `actionMainUndo`/`actionMainRedo` to `sourcesToolbar` with new `undo.svg`/`redo.svg` icons in every theme; tooltips from the existing names.
3. Remove the dead `actionUndo`/`actionRedo`.
4. A `docs/features.md` ledger line (user-visible change).

\#5152 is a prerequisite in principle, since a button advertises it, but its
fix touches libobs `scene_load` semantics and should be its own PR landing
before or with the button. #10120 and #5516 stay follow-ups until reproduced.

## Follow-up issues to file

| Title | One line |
|---|---|
| bug: undo/redo of scene edits recreates every item (upstream #5152) | Replace `obs_source_update` on the scene (`OBSBasic_Scenes.cpp.hpp:849`) with a diff-based restore. |
| bug: undo entries resolve sources by name, not uuid | `OBSBasic_Scenes.cpp.hpp:836` creates duplicates after a rename. |
| repro: upstream #10120 against this fork | Use the reporter's `bugged.json`; record steps and result. |
| repro: upstream #5516 on Windows (WGC + studio mode) | Determine whether undo is involved at all. |
| bug: Undo stays enabled on an empty stack after `pop_disabled()` | `undo_stack.cpp.hpp:117`. |
| bug: coalesced merge keeps a stale redo stack | `undo_stack.cpp.hpp:46-56`, merge returns before `clear_redo()`. |
| perf: cap undo history by bytes, store transform diffs | 5000 whole-scene backups can reach gigabytes. |
| feat: undo for lock, filter enable/reorder, scene reorder, transition changes | Sites listed in section 1b. |
| docs: write down undo scope | Settings, collections, profiles, websocket, studio mode, projectors are out; say why. Remove or use `Undo.SceneCollection.Switch`. |
| cleanup: remove dead `actionUndo`/`actionRedo` | `OBSBasic.ui:1698-1713`, no references. |
| feat: undo toast and/or history panel | After the toolbar button lands. |
