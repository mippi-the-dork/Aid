# Aid

**Data-driven spatial design and communication for Unreal Engine**  
**Version:** `0.2.0-alpha.2`  
**Primary target:** Unreal Engine 5.8.0–5.8.3 / Win64 source; **not yet compiled in Unreal in this environment**.

Aid stores designer notes and their visual relationships in an **Aid Document** asset. The Editor can render any number of those documents over a level without requiring Aid Actors. An optional **Aid Display Actor** reads exactly the same document at runtime for QA and playtest communication.

## What is in this release

- Dedicated Content Browser **Aid > Aid Document** asset factory and asset definition.
- One dockable **Tools > Aid Browser** with a document picker, editable document description, per-level document list, multi-document visibility toggles, and a single inline authoring view.
- Node authoring by clicking level geometry, placing at the editor camera, or clicking an actor to attach a tagged note. Runtime follows a tagged Actor and uses its stored world fallback if it goes missing.
- Editable note title, body, category, five quick color swatches, per-note **View / Connect / Gear / Delete**, and inline connection labels/target names, relationship types, line-color swatch, gear/delete.
- Project-wide categories and relationships configurable in **Edit > Project Settings > Plugins > Aid**. Selecting a preset applies its default style; per-item overrides are preserved until that preset is explicitly reapplied.
- Note styling: header/body fill and text colors, automatically brighter border based on title fill, border thickness, font size. There is no corner-radius property.
- Viewport dots with a filled radial center and an outlined ring. A note is fully hidden beyond its view distance, dot-only between 50% and 100%, and expanded within 50%.
- A connection stays visible while **either** endpoint is visible, even if its other endpoint is beyond its own view distance. Both hidden = hidden line. Manual line hide and optional line view distance still apply; the default line distance is unlimited (0).
- Connection stroke styles Solid/Dashed/Dotted, start and end caps Arrow/Circle/Square/None and inset spacing from node markers.
- Actor-free multi-document viewport previews. Documents can be associated with any number of maps using soft level references. The list defaults to current-level docs, but Show All and independent visibility toggles allow deliberately viewing a document in an unrelated level.
- Separate optional runtime QA viewer. Its card presentation is currently lightweight TextRender; editor cards are Canvas.
- Undoable note/connection authoring and native automation tests.

## Quick start

1. Install the entire `Aid` folder under `YourProject/Plugins/Aid` and compile `YourProjectEditor` for UE 5.8.
2. Right-click in Content Browser **Aid > Aid Document** and name it (e.g. `AD_PuzzleFlow`). Double-click it to open Aid Browser, or use **Tools > Aid Browser** and the asset picker.
3. Add a document description at the top of Aid Browser. For the current level, check **Associate selected document with this level**. This modifies the document asset, **not the level**.
4. Click **+ Place Note** and click level geometry, or **+ Note at Camera**. No Actor is created. Click **+ Note on Actor** and then an Actor to pin to it. **Actor pinning does add a unique Aid Tag to the selected Actor and therefore dirties that Actor's level/external package.**
5. Edit title, note text and category directly in its note row. Two title swatches, two body swatches and one category swatch provide fast color changes. Gear opens common settings.
6. Click **Connect** on a source note, then click the target note/dot in the viewport or Browser. Edit the connection label and relationship type inline. Use gear for caps, line style, thickness, visibility and line range; trash deletes either note or connection.
7. To show more documents, expand **Documents / Multi-document visibility** and check each document. **Current level only** filters the list; uncheck it to see all. You can open unrelated documents intentionally. Use **View** to focus a note in the viewport.
8. Save Aid Documents through Unreal's normal asset save workflow.

## Visibility example

With a note View Distance of **2,000 cm**:

| Camera distance | Node representation |
|---|---|
| > 2,000 cm | Hidden |
| > 1,000 to 2,000 cm | Dot and ring only |
| <= 1,000 cm | Full card + dot anchor |

A 10,000 cm connection remains visible if its source is inside its own view distance, even when its target is not. The full line can be followed across the level. If both endpoints are beyond their individual ranges, the line hides. An explicit connection View Distance can additionally limit the line; **0** means unlimited.

## Test commands

Run in Unreal Editor's console after a successful build:

```text
Automation RunTests Aid.Data
Automation RunTests Aid.Editor
```

See [full automated and manual validation](Docs/Testing/ALPHA7_TESTS.md).

## Runtime QA

Place an **Aid Display Actor**, assign the Aid Document, and play. This display actor does not author or duplicate the note data. It uses the same underlying distance and edge visibility rules but not yet the finished editor card visuals. Shipping builds disable drawing unless explicitly enabled; **this does not prevent referenced annotation data from being cooked**. Excluding confidential QA content from shipping packages remains a required release safeguard.

## Known gaps

- UE 5.8.3 compilation and in-engine visual validation of this iteration are pending. The original early Aid tests passed in the user's engine before this pass; new code is not yet verified in that environment.
- Runtime TextRender markers are an approximation, not full Slate/Canvas styling parity.
- Multi-document visible states currently live only in the editor session; per-user persistence is planned.
- Actor anchoring uses unique Actor Tags (and dirties an Actor); non-invasive soft actor identity references require more validation, especially across World Partition and PIE.
- Cross-document edges, drag-to-connect handles, custom spline routing, card wrapping, and high-volume performance testing remain future work.
- Visual line caps/labels need additional near-plane and screen clipping polish.

## Documentation

See [Documentation Index](Docs/README.md), [UX and three-state visibility rules](Docs/Design/ALPHA7_BROWSER_AND_VISIBILITY.md), [architecture changes](Docs/Technical/ALPHA7_DATA_VISIBILITY.md), [tests](Docs/Testing/ALPHA7_TESTS.md), [roadmap](Docs/ROADMAP.md) and [changelog](CHANGELOG.md). Original concept document is retained unchanged under `Docs/Design/ORIGINAL_PRD.md`.

No other Unreal plugin is required.
