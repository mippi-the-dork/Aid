# Aid

**Spatial design and communication for Unreal Engine**  
**Version:** `0.3.0-alpha.4` · **Target:** Unreal Engine 5.8.0–5.8.3 / Win64

Aid stores spatial notes and visual relationships in a dedicated **Aid Document** asset. The editor displays one or more documents over a level **without spawning Aid Actors**. The optional `Aid Display Actor` consumes that same document for PIE and QA playtests.

## Current release

- **Content Browser → Aid → Aid Document**: Create a dedicated asset, not a generic Data Asset wizard.
- **Tools → Aid**: Opens the native Slate tool. Double-clicking an Aid Document also selects it here; the internal tab ID remains `AidBrowser` for workspace compatibility.
- **Documents outliner**: Resizable upper section with category groups, search, per-document visibility eyes, current-level filtering and per-document level-association checkboxes. Documents with no category appear at the root. Group presets are configured in Project Settings.
- **Notes editor**: Selected document below, editable description, persistent count header outside the scroll area, compact expandable notes, connection rows inside their source note.
- **Unplaced notes**: `+ Note` creates a data-only note. It appears in the browser as **Not Placed** until anchored; no viewport marker or spurious line to world origin is drawn.
- **Placement from the note**: Use its placement menu for Place in World, Attach to Actor, or Place at Camera. Viewport placement is a preview; click again to adjust, then choose **Accept** / **Cancel** in the viewport, or Enter / Escape. Actor tags are only added on Confirm.
- **Presets**: Note Category and Relationship Type are read-only labels + preset dropdowns. Each offers None and only project-defined entries, plus an Edit Presets shortcut. The permanent Default preset and named presets use one base color; the note body/footer and connection footer colors are derived. Individual colors and connection line properties remain overrideable.
- **Fast editing**: Square base-color swatches, title/body inputs, dedicated tags menu, direct cap/line-style cycling, editable connection targets, and inline View Distance and thickness controls. Font size and note border thickness are project-wide settings.
- **Viewport selection**: Clicking an Aid note, its radial marker, or a connection label selects it and expands the source note in Aid, even in Review mode. Edit mode remains required for authoring tools.
- **Distance states**: More than 100% of View Distance = hidden; between 50–100% = dot marker; at/below 50% = expanded card. Connection remains visible if **either** placed endpoint is visible; both hidden = line hidden.
- **Presentation**: Text wrapping and layout-dependent card height replace fixed card geometry. Note and connection footers show preset names or custom labels. End caps and connections use a single screen-space renderer; close notes draw on top of far notes.
- **Data ownership**: Editor annotations live entirely in the Aid Document package. Associating a document to a map modifies that *document*. Actor attachment **does modify the Actor** (a unique tag is added on confirmation).

## Getting started

1. Close Unreal Editor and extract the entire `Aid` folder into `YourProject/Plugins/Aid`.
2. Build your game's Editor target on UE 5.8.3 Win64. With automatic level discovery enabled (the default), Aid displays associated documents even if you do not open the Aid panel.
3. Create an asset via **Content Browser → Aid → Aid Document**.
4. In the Documents outliner, disable **Current Level** to show all documents; check the level-association checkbox on your document to associate it with the currently open map. You can associate a document with more than one map.
5. Select your document in the outliner. Its description and notes appear in the lower pane. Use `+ Note` to create your first note; it starts unplaced and can be edited immediately.
6. From the note header's placement menu, select World, Actor, or Camera. Confirm the preview at the bottom of the viewport. To move it later, repeat the same operation on the existing note.
7. To connect notes, click the source note's Connect action, then the target note or its dot. Edit the new connection's text and relationship type in the source note's expanded row.
8. Save the Aid Document normally. No Aid Actor is needed to author or show it in the editor. Use **Viewport Show → Developer → Aid** to hide or show annotations in an individual viewport, independently of the document eyes.

**Visibility** of the current document is distinct from **selection** (which document you're editing) and **level association** (which maps it's organized under). The eye is a local editor visibility toggle. Changing the association only changes the Aid Document.

## QA runtime display

Place an `Aid Display Actor` into a test map and assign one of the same Aid Documents. It reads the document in PIE and QA builds; its runtime presentation is currently a simplified TextRender representation and not yet identical to the editor cards. Rendering can be disabled for shipping builds, but **merely hiding it does not exclude the referenced document data from cooking**. See [Runtime QA requirements](Docs/Technical/RUNTIME_QA.md).

## Automated tests

After a successful Unreal Editor build, run these commands in the Unreal console:

```text
Automation RunTests Aid.Data
Automation RunTests Aid.Editor
```

The current source includes tests for graph validation, unplaced notes, visibility boundaries, long connections, document category metadata, asset creation and staged placement. The latest manual test matrix is at [Alpha 10 Tests](Docs/Testing/ALPHA10_TESTS.md).

**Verification:** This ZIP is source and documentation. Package/static checks can be performed here, but the alpha.4 changes **have not been compiled, linked or UI-tested in UE 5.8.3 in this environment**. Please test the build and the new Slate workflows before relying on them in production.

## Documentation

- [Documentation Index](Docs/README.md)
- [Latest preset and auto-display UX](Docs/Design/ALPHA10_PRESETS_AND_DISCOVERY.md)
- [Workspace UX](Docs/Design/ALPHA9_NATIVE_WORKSPACE.md)
- [Current Technical Implementation](Docs/Technical/ALPHA9_VIEWPORT_GEOMETRY.md)
- [Native Tests and Manual QA](Docs/Testing/ALPHA10_TESTS.md)
- [Architecture](Docs/Technical/ARCHITECTURE.md)
- [Original PRD](Docs/Design/ORIGINAL_PRD.md) (preserved)
- [Changelog](CHANGELOG.md)

No other Unreal plugin is required.

### Alpha 0.3.0-alpha.2 notes

Connection drawing now uses one screen-space pipeline for line, caps and labels. Aid enforces one connection per unordered note pair, shows category/type in the card footer, and uses the Unreal accent-blue selection border. The Aid tab indicates unsaved document packages with `*` and provides a Save action. See `Docs/Testing/ALPHA9_TESTS.md` for visual regression checks. UE 5.8.3 engine compilation and UI behavior have not been verified by the package authoring environment.

## Aid alpha.3 improvements

Associated documents can appear automatically whenever their level opens, even if the Aid tab remains closed. The behavior is configured at **Project Settings → Plugins → Aid** with *Auto-show Aid documents for the open level* (on by default). Individual document eyes stay independent from **Viewport Show → Developer → Aid**, which is a per-viewport master visibility control. A second setting, *Auto-collapse unselected notes*, defaults on and controls the authoring panel only.

Note and connection styling use one preset base color (or a Custom override), derived body/footer surfaces, direct connection cap/line controls and editable connection targets. See `Docs/Design/ALPHA10_PRESETS_AND_DISCOVERY.md` and `Docs/Testing/ALPHA10_TESTS.md`.
