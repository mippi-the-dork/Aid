# Current status and next work (2026-11-10)

**Aid 0.2.0-alpha.2 source iteration** implements inline note/relationship editing, quick swatches and gears, project presets, radial distance-driven markers, one-endpoint connection visibility, level association, multi-document previews and click-to-bind actors. No in-engine compilation has been run in this environment. Refer to [Alpha 0.2 tests](Testing/ALPHA7_TESTS.md) before considering these features validated.

**Next priorities after validation:** native Slate viewport card rendering with proper multiline layout, draggable spatial connectors with preview, true Actor selection/World Outliner binding without requiring a line trace, automated editor viewport integration tests, cross-document graph relationships, per-user persistent visibility/filter state, stable World Partition actor IDs and unloaded references, runtime card parity, and packaging exclusion for internal QA data.

---

> **Progress:** Alpha.3 passed its three data tests in UE 5.8.3. Alpha.4 introduces dedicated assets and the first visual note/connection authoring pass, pending compilation and manual validation. See [Alpha.4 Visual Authoring](Design/ALPHA4_VISUAL_AUTHORING.md).

# Aid — Development Roadmap & Feature Status

**Current iteration:** `0.1.0-alpha.1` — architecture and end-to-end path proof.  
**Release status:** Source generated and statically reviewed; **not compiled or tested in UE 5.8.3 here**.

## Alpha 1 feature status

| Feature | Status | Details |
| --- | --- | --- |
| Runtime and editor modules separated | Implemented in source | `Aid` + `AidEditor` |
| Aid Document Content asset | Implemented in source | `UPrimaryDataAsset`, no world actors needed |
| Note title/body/category/tags/style | Implemented in data | Final visual card rendering pending |
| World anchor | Implemented in source | World-space location |
| Actor Tag anchor & fallback | Implemented in source | Assumes unique tags; no diagnostics for ambiguous tags yet |
| Independent connection records | Implemented in data | GUID endpoints for two Aid notes |
| Directed line representation | First-pass renderer | Straight lines only; no caps/dashes/curves |
| Editor viewport visualization | First-pass renderer | Points, projected titles and straight lines |
| Dockable Aid Browser | First-pass UI | Asset picker, list, Details, Add Note, Connect Last Two, Go, Validate |
| Actor-free edit / source-control | Implemented by asset separation | Save/level dirty behavior not yet verified in editor |
| Runtime display Actor | First-pass runtime viewer | One Actor reads document; text + lines in play |
| Runtime shipping opt-in | Implemented by build-time check | Shipping packaging not yet verified |
| Native automation tests | Source included | Not executed in UE 5.8 here |
| Undo for Browser Add / Connect | Implemented in source | Requires editor verification |
| Full Slate cards / screen-size modes | Planned | Next renderer batch |
| Viewport drag-and-drop / manipulation | Planned | Edit mode, hit testing, handles |
| Multi-target actor/component/world endpoints | Planned | Generalized endpoint model |
| Smart/grouped layouts and regions | Planned | Requires renderer and UX infrastructure |
| Per-user filters and categories | Planned | Separate from document authoring state |
| Shared reusable style assets | Planned | Appearance values included, preset assets not implemented |
| World Partition robust binding | Planned | Validation required |
| Markdown/JSON export | Planned | Format design pending |
| Source-control conflict reduction | Planned | One asset per document currently |

## Work batches

### Batch 0 — Prove architecture (Alpha 1, current)

- [x] Separate runtime and editor modules.
- [x] Asset-based source of truth.
- [x] Actor-free editor renderer scaffold.
- [x] Optional runtime Actor consuming identical document.
- [x] Native Slate browser scaffold.
- [x] Basic graph validation tests and documentation.
- [ ] Compile project in UE 5.8.3.
- [ ] Execute automated suite and manual workflow.
- [ ] Validate save/reload and map dirty behavior.
- [ ] Test packaged Development and Shipping modes.

### Batch 1 — Complete note authoring

Design goals: Create note at cursor/selected Actor/selected Component; rich cards with separate header and body; inline editing; world and actor/component anchors; authoring category presets; visible card states (marker/compact/expanded); explicit selection and Review/Edit modes; source-control-friendly package isolation; automatic GUID/repair behavior.

**Acceptance:** A designer can complete a full room's note layout without opening raw arrays or needing a runtime Actor. Normal viewport navigation remains unmodified outside Edit state.

### Batch 2 — Relationships and spatial meaning

Design goals: Draw/fan-out from a note to Actors, Components, world anchors and other notes; direct manipulation; start/end caps; Solid/Dashed/Dotted strokes; straight/smooth/custom curves; arc-length labels; semantic relationship presets; source/target navigation; robust undo.

**Acceptance:** Linking 20 objects is quick, connections stay attached under relevant transforms, and a reviewer can follow dependencies without manual ID entry.

### Batch 3 — Group, region, collaboration and rendering scale

Design goals: Spatial regions, grouped flows, category/layer filters (per user), keyboard UX, multi-select/batch restyle, World Partition handling, stable actor references and repair, document association/discovery, export, source-control conflict behavior and meaningful 1k/5k stress benchmarks.

**Acceptance:** Multiple designers can work in the same level with independent documents, annotations survive normal level rework and views remain responsive.

### Batch 4 — Runtime QA and release validation

Design goals: Same authored card/connection presentation in PIE and packaged QA builds; test-only category and permission profiles; configurable runtime visibility and focus actions; **verified Shipping package exclusion of sensitive QA data** (not merely hiding it); packaged Win64 verification; full automated/native editor tests and playtester UX QA.

**Acceptance:** A QA tester receives the appropriate notes inside the build without game-code modification or leaking internal annotations in default shipping configuration.

## Deliberately deferred ideas

- Automatic obstacle-avoiding 3D connection pathfinding: may visually imply a gameplay route when the edge represents abstract logic.
- Runtime connection behaviors, quest triggers, and debug authority: out of product scope unless user explicitly expands it.
- Integration with other plugins: optional, never required for core Aid.

## Release gate

Do not call v1.0 complete until compile, native automation, manual authoring, source-control, performance, World Partition and packaged QA/shipping tests are documented and passing. An alpha handoff is not proof of those conditions.
