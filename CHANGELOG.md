# Aid Changelog

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
