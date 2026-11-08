# Aid

**Data-driven spatial design and communication for Unreal Engine**  
**Version:** `0.1.0-alpha.2`  
**Initial target:** Unreal Engine 5.8.0–5.8.3, Win64 (source only; not yet compiled in the target environment)

Aid lets designers describe intent directly in world space. An **Aid Document** owns the notes, relationships and visual metadata. **No level actor is required to create or preview annotations in the editor.** An optional **Aid Display Actor** reads the *same* document in Play In Editor, standalone or packaged builds, allowing playtesters and QA to see spatial explanations during testing.

> **Alpha status:** This is an early development package, not a validated or feature-complete 1.0 release. The initial alpha.1 build was attempted in UE 5.8.3 and failed on two incorrect include paths; both paths are corrected in alpha.2 using the UE 5.8 documented headers. This updated package has not yet been compiled or exercised in an Unreal Engine installation. Use the test plan and report any additional compiler or editor errors before wider use.

## What is actually implemented

- Native runtime `UAidDocument` Primary Data Asset with persistent note and relationship records, stable GUIDs, categories, styling fields and per-annotation visibility.
- Two anchor types: world-space coordinates, and actor-tag binding with world-position fallback. Actor-tag binding does **not** serialize a hard reference to a map Actor.
- Native dockable **Aid Browser**, available from **Tools → Aid Browser**, containing an Aid Document picker, Add Note at Camera, Connect Last Two, Refresh List, Validate, and an editable Details view.
- Editor-only, actor-free **Aid Overlay mode** showing annotation markers/titles and straight connections in the level editor viewport.
- Optional placeable `Aid Display Actor` that reads an Aid Document and draws its note texts and connection lines **during gameplay**; it does not own the note data.
- Built-in native automation tests for core graph operations, invalid identity and unresolved anchor fallback.
- Per-edit Undo transactions for Add Note and Connect Last Two. Standard Data Asset edits use the regular editor property transaction system.
- Entire plugin has no dependencies on any other Mippi plugin.

## Quick start: actor-free editor authoring

1. Copy the **entire `Aid` folder** into `YourProject/Plugins/Aid`.
2. Regenerate project files and build the project Editor target with Unreal Engine 5.8.
3. Enable **Aid** under Edit → Plugins if needed, then restart the editor.
4. In the Content Browser, create a **Data Asset** and choose **AidDocument** as its class. Name it something like `AD_PuzzleFlow`.
5. Open **Tools → Aid Browser** and choose that document from the picker. The document can also be edited directly using its normal Data Asset Details editor.
6. Face the desired location and press **Add Note at Camera**. The note is placed 600 Unreal units in front of the current level viewport camera.
7. Add another note and press **Connect Last Two**. This creates a directed, straight connection.
8. Edit Titles, Bodies, positions, categories, styles, and other properties in the lower half of the Aid Browser. Use **Refresh List** if a property edit has not updated the browser tree yet.
9. Save the **Aid Document** asset (not the level). With the Aid Overlay visible, the notes and connections can be reviewed without placing any Actors.

> Some appearance fields are persisted for future renderers but are **not** visually implemented in this alpha. See [Feature Status](Docs/ROADMAP.md).

## Quick start: runtime QA communication

1. Reuse the **same** Aid Document created above.
2. Place one **Aid Display Actor** in the level, then set its **Document** property to that Aid Document.
3. Play the level (PIE or standalone). World-space note text and straight connection lines appear at their document positions.
4. For a moving Actor, add a unique Actor Tag (e.g. `QA_Generator_01`) to that Actor. Set the Aid note anchor type to **Actor Tag**, set **Actor Tag** to the same name, and adjust **Local Offset**. The note follows that tagged Actor at runtime.
5. **Allow In Shipping** disables *drawing* by default in shipping builds; it does **not** prevent referenced annotation assets from being cooked into the package. Do not ship confidential QA content until packaging exclusion is implemented and verified. `Show In Play` is the local playtest toggle.

**Important:** Display Actors do modify the map when placed, like normal Actors. The *authoring data* stays in the separate Aid Document asset. Editor preview requires no display actor and does not dirty the map by itself.

## Limitations in this first iteration

- Editor rendering is currently points + titles + straight relationship lines, not finished cards with headers/bodies, screen-size modes, depth fade, hover, click picking or spline editing.
- Runtime rendering uses `UTextRenderComponent` and a `ULineBatchComponent` per display actor; visual style parity with editor cards is not implemented. Runtime lines are solid even when connection metadata specifies dashed/dotted or arrowheads.
- `Connect Last Two` is a temporary authoring shortcut, not the finished direct-manipulation UX.
- Actor Tag bindings assume a **unique** matching tag. Duplicate tags are ambiguous; missing tags use the saved fallback position.
- The current browser's active document/overlay state is session-only. Refresh List may be needed after changing data manually in Details.
- This alpha should be treated as suitable for **small test graphs**, not the 1,000+ annotation performance target.
- The optional display actor's line rendering is intended for design/QA builds; performance and shipping behavior require build verification.

## Documentation

- [Documentation Index](Docs/README.md)
- [Original PRD (unchanged reference)](Docs/Design/ORIGINAL_PRD.md)
- [Product Vision and Scope](Docs/Design/PRODUCT_VISION.md)
- [UX Design](Docs/Design/UX_DESIGN.md)
- [Technical Architecture](Docs/Technical/ARCHITECTURE.md)
- [Runtime / QA Integration](Docs/Technical/RUNTIME_QA.md)
- [Rendering and Performance Plan](Docs/Technical/RENDERING.md)
- [Roadmap and Current Feature Status](Docs/ROADMAP.md)
- [Validation / Test Plan](Docs/Testing/TEST_PLAN.md)
- [Changelog](CHANGELOG.md)

## License / support

No engine binaries, compiled files or third-party libraries are included. This source-only handoff is an early development package; license and distribution terms have not yet been assigned. Unreal Engine is a trademark of Epic Games, Inc.
