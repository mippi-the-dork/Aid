# Changelog

## 0.2.0-alpha.2 (2026-11-10) — Aid viewport body-text compilation fix

- Fixed the UE 5.8 C++ `C2371` collision in `AidViewportMode.cpp`: the `FCanvasTileItem Body` and `FString Body` previously occupied the same local scope. The description string is now `BodyText` throughout its text rendering path.
- Resolves the follow-on `C2039` (`IsEmpty`, `Len`, `Left`) and `C2661` (`FCanvasTextItem`) errors caused by treating the canvas tile as a string.
- Added a static regression guard for the distinct identifiers. No annotation schema, visual behavior, runtime display logic or non-Aid plugin was changed.
- Verification: static package checks and ZIP integrity only. UE 5.8.3 compilation and native automation tests remain to be run.

## 0.2.0-alpha.1 (2026-11-10) — UX and visibility pass

- Consolidated note preview and selected-note editing into one editable row per note; document description moved to top of Aid Browser.
- Added title/body/category quick color swatches, native color pickers, View/Connect/Delete buttons and note gear menus.
- Added two-column inline connection editing with relationship presets, line color picker, gear, and per-connection delete.
- Added config-backed **Aid Project Settings** with category and relationship style presets; edits can override presets per item.
- Replaced square note anchors with radial ring/dot markers. Cards expand inside half of per-note view distance, are dot-only from 50% to 100%, and hidden beyond 100%. Backward-facing card rendering is suppressed.
- Connection visibility now uses logical OR of endpoint note visibilities. Long links remain drawn when one endpoint is in range; manual hide and optional line distance are supported.
- Added screen-space Arrow/Circle/Square/None caps, visible gap before marker and Solid/Dashed/Dotted rendering.
- Added per-document soft level associations, current-level filtering and session multi-document visibility.
- Added **+ Note on Actor** authoring that assigns a GUID-based Actor Tag and follows actor transforms (explicitly dirties the chosen actor).
- Fixed viewport card render use of FontSize and BorderThickness; removed CornerRadius property and derived border color automatically from title fill.
- Added native data visibility/preset tests and detailed manual UX/runtime validation instructions.
- **Unverified:** This release passed source/static checks only; compilation and runtime behavior must be validated in UE 5.8.3.

# Aid Changelog

## 0.1.0-alpha.6 — InputCore linker dependency

### Fixed

- Added `InputCore` to the `AidEditor` module dependencies. The viewport edit mode uses `EKeys::LeftMouseButton`, `EKeys::Escape`, `EKeys::LeftControl`, `EKeys::RightControl`, `EKeys::LeftAlt`, and `EKeys::RightAlt`; their exported definitions are supplied by `InputCore`. This addresses the six `LNK2019` errors and the resulting `LNK1120` in the alpha.5 build log.
- Extended the static package validation to require `InputCore` whenever the viewport mode uses `EKeys` input constants.

### Verification

- User build log for alpha.5 confirms Aid and AidEditor translation units compile, Aid runtime links, and only AidEditor fails on the reported missing `EKeys` symbols.
- This is an Aid-only build-rule change. No annotation data schema, rendering, UX behavior, or unrelated plugin is modified.
- Static package and ZIP integrity checks run locally; **UE 5.8.3 rebuild and automated tests remain to be confirmed**.

## 0.1.0-alpha.5 — UE 5.8 Asset Factory override fix

### Fixed

- Corrected `UAidDocumentFactory::GetDefaultNewAssetName()` to match UE 5.8's `UFactory` signature: `FString GetDefaultNewAssetName() const override;`.
- Applied the same `const` qualifier to its `.cpp` definition. Addresses Aid's `C3668`, `C4263`, and `C4264` errors from the alpha.4 build log.
- Added static regression coverage requiring both signatures to remain in sync.

### Verification

- The alpha.4 user build log confirms that Unreal Header Tool completed, the Aid runtime DLL linked, and the reported Aid failure was the factory override mismatch.
- Static package checks and ZIP integrity passed. **Alpha.5 has not been compiled or tested in UE 5.8.3** in this environment.
- No changes to annotation data schema, existing Aid documents, visual authoring behavior, or runtime display behavior.

## 0.1.0-alpha.4 — Dedicated assets and visual authoring

### Added

- Native `UAidDocumentFactory`: Content Browser **Aid > Aid Document** creation, with `AD_NewDocument` suggested naming.
- `UAssetDefinition_AidDocument`: dedicated asset name/category/color and double-click routing to the active Aid Browser document.
- Visual Browser note cards and context editing for Title, Note and Category; inline edge label editing. Advanced native Data Asset details remain available in a collapsed section.
- Color-block viewport note cards (header, body, border, selected state) inspired by Unmark Notes, without spawning Actors or per-card UMG components.
- Explicit Review/Edit interaction distinction. Click a card in Edit mode to select it, click geometry to place a note, or select source then target to create a relationship.
- Transactional content edits, connection label edits, and node deletion that removes dependent edges.
- Native tests for deletion cleanup and dedicated asset factory creation.
- Updated visual authoring and testing documentation.

### Status

- Alpha.3 passed 3/3 `Aid.Data` tests in UE 5.8.3 (user-confirmed).
- Alpha.4 source and ZIP integrity can be checked here, but **the new code has not been compiled or tested in Unreal yet**.
- Alpha.4 does not yet include drag-to-connect handles, floating in-viewport text editors, curved connections, full Unmark Notes rendering parity, or per-asset editor toolkit tabs.

## 0.1.0-alpha.3 — UE 5.8 editor mode linker fix

### Fixed

- Added `EditorFramework` to `AidEditor` private module dependencies. `FEditorModeRegistry::RegisterMode` instantiates an `FEditorModeInfo`, whose constructor is exported by `EditorFramework` (not `UnrealEd`). This fixes the alpha.2 `LNK2019` for `FEditorModeInfo::FEditorModeInfo(...)` and subsequent `LNK1120`, subject to a fresh UE 5.8.3 build.
- Extended static package validation to require the dependency and guard against this regression.

### Verification

- The alpha.2 UE 5.8.3 log confirms all Aid translation units compiled. `Aid` runtime DLL linked, while `AidEditor` DLL failed on the missing symbol.
- Epic UE 5.8 API places `FEditorModeInfo` in the `EditorFramework` module and the same linker signature is independently documented as resolved by the `EditorFramework` dependency.
- Alpha.3 static checks / ZIP integrity pass; **a new UE 5.8.3 link/build has not yet been run.**
- No changes to Aid asset formats, rendering or editor UI.

## 0.1.0-alpha.2 — Unreal Engine 5.8 compile header fixes

### Fixed

- Replaced invalid `Styling/SlateIcon.h` with documented `Textures/SlateIcon.h` in the Aid editor module.
- Replaced invalid `GameFramework/PlayerCameraManager.h` with documented `Camera/PlayerCameraManager.h` in the runtime display actor.
- Added a static package check to prevent these two invalid header paths from reappearing.

### Verification

- Alpha.1 UE 5.8.3 build log showed only these two Aid errors; other Aid translation units completed.
- Alpha.2 source/package checks passed; **the revised plugin has not yet been compiled in UE 5.8.3.**
- No changes to Aid Document structure, serialization, runtime drawing logic, or browser UX.

## 0.1.0-alpha.1 — Initial implementation

### Added

- Two-module C++ plugin (`Aid` runtime, `AidEditor` editor-only).
- Canonical `UAidDocument` Primary Data Asset for nodes, edge records, positions, tags and styles.
- Actor-free editor point/title/line rendering via an Aid Editor Mode.
- Native dockable Aid Browser with document selection, add note, connect last two, goto, validation and document Details inspector.
- Optional `AAidDisplayActor` for PIE, standalone and packaged QA visualization of the **same Aid Document**.
- Actor-tag anchoring with world-position fallback and per-pass resolution cache.
- Shipping-build *visual* gating defaults off; runtime display can be explicitly opted in. Referenced documents may still be cooked.
- Unreal native graph and anchor automation tests.
- Product, UX, technical architecture, runtime QA, rendering, roadmap and testing documentation.

### Known gaps

- Not compiled or executed in Unreal Engine 5.8.3 in this environment.
- Alpha renderer does not yet support full styled cards, curved paths, caps, dashed lines, interactive viewport links, robust map binding or runtime viewer scaling.
- Editor Browser uses temporary authoring shortcuts, not final direct-manipulation interface.
- Source-control independence is at the document-asset level, not per annotation.
