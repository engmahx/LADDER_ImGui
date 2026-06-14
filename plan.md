# LADDER Editor — Complete Project Plan

## Overview
A desktop LADDER logic editor built with C++17, Dear ImGui, GLFW, and OpenGL 3.0.  
Target: Windows (portable to macOS/Linux via GLFW/OpenGL).

---

## Architecture

```
┌──────────────────────────────────────────────────┐
│                    main.cpp                       │
│  (GLFW window, ImGui init, render loop, DPI)     │
├──────────────────────────────────────────────────┤
│                  LadderEditor                     │
│  (orchestrator: menu, canvas, tools, status)      │
├─────────┬──────────┬──────────┬───────────────────┤
│ Canvas  │ Tools    │ Project  │ Serialization     │
│ (grid,  │ (palette,│ (rungs,  │ (JSON save/load)  │
│  rungs, │  props,  │  elems,  │                   │
│  wiring)│  undo)   │  undo)   │                   │
└─────────┴──────────┴──────────┴───────────────────┘
```

## File Layout

```
p9/
├── CMakeLists.txt              # Build config (FetchContent: GLFW, ImGui)
├── plan.md                     # This file
└── src/
    ├── main.cpp                # Entry point, window, render loop
    ├── LadderEditor.h/.cpp     # Top-level orchestrator
    ├── Canvas.h/.cpp           # Grid, rungs, elements, wiring, interaction
    ├── ToolsPanel.h/.cpp       # Left/right tool palette, element library
    ├── PropertiesPanel.h/.cpp  # Selected-element property editor
    ├── Project.h/.cpp          # Rung/element data model, undo stack
    ├── Elements.h              # Element types (NO, NC, Coil, Timer, etc.)
    ├── Serializer.h/.cpp       # JSON save/load (using imjson or nlohmann)
    ├── UndoStack.h/.cpp        # Command-pattern undo/redo
    └── Renderer.h/.cpp         # Optional: custom OpenGL renderer for canvas
```

## Phase 1 — Foundation (DONE)

| Task | Status |
|------|--------|
| GLFW + ImGui setup via CMake FetchContent | Done |
| OpenGL 3.0 loader (ImGui built-in) | Done |
| Window creation (1400×900, title "LADDER Editor") | Done |
| Dark theme with rounded corners (12px) | Done |
| Menu bar (File, Edit, View, Help) | Done |
| Three-panel layout (Tools, Canvas, Properties) | Done |
| Status bar (tool, grid, zoom info) | Done |

## Phase 2 — Canvas & Grid

| Task | Details | Effort |
|------|---------|--------|
| Dotted background grid | Configurable spacing (12–48px), dot radius | Done |
| Scrollable canvas | ImGui child with scroll bars | Small |
| Zoom (25%–300%) | Apply scale to grid spacing + element rendering | Small |
| Left/right power rails | Vertical bars at edges of each rung | Done |
| Rung lines | Horizontal boundaries per rung | Done |
| Grid snapping | Elements snap to nearest grid cell | Small |

## Phase 3 — Element System

| Task | Details | Effort |
|------|---------|--------|
| Element data model | Base `LadderElement` struct with type, pos, label, params | Small |
| NO contact (Normally Open) | `──[ ]──` symbol, `XIC` equivalent | Small |
| NC contact (Normally Closed) | `──[\/]──` symbol, `XIO` equivalent | Small |
| Coil | `──( )──` symbol, `OTE` equivalent | Small |
| Output latch/unlatch | `──(L)──` / `──(U)──` | Small |
| Timer (TON, TOF, TP) | `──TON──` with preset value, accumulated time | Medium |
| Counter (CTU, CTD) | `──CTU──` with preset, accumulated count | Medium |
| Branch (parallel) | Vertical branch connector for parallel logic | Medium |
| Comparison blocks (GEQ, EQU, LES) | `──GEQ──` with two inputs | Medium |
| Text/comment annotation | Free-form text labels on canvas | Small |

### Element Visual Rendering

Each element types needs a `Draw(ImDrawList*, pos, size, color)` method:
- **NO**: Two vertical lines with gap (open switch)
- **NC**: Two vertical lines with diagonal slash (closed switch)
- **Coil**: Circle with label inside
- **Timer**: Rectangle with preset/time display
- **Counter**: Rectangle with count/preset display
- **Branch**: T-junction lines

## Phase 4 — Interaction

| Task | Details | Effort |
|------|---------|--------|
| Select tool | Click to select, click empty to deselect | Small |
| Element placement | Click tool in palette → click cell to place | Small |
| Drag-move elements | Click-drag selected element to new cell | Medium |
| Multi-select | Ctrl+click or rubber-band selection | Medium |
| Right-click context menu | Delete, edit properties, copy | Small |
| Delete key | Remove selected element(s) | Small |
| Wiring (connecting elements) | Auto-connect horizontally between adjacent cells | Medium |
| Hotkeys | Ctrl+Z undo, Ctrl+Y redo, Ctrl+C/V copy-paste, Del delete | Small |

## Phase 5 — Undo / Redo

| Task | Details | Effort |
|------|---------|--------|
| Command pattern base | `UndoCommand` abstract class | Small |
| AddElement command | Stores element data, reverses on undo | Small |
| RemoveElement command | Stores element + position, restores on undo | Small |
| MoveElement command | Stores old/new positions | Small |
| EditProperties command | Stores old/new property values | Small |
| UndoStack (bounded) | Max N commands, handles empty/at-end states | Small |

## Phase 6 — Project Model

| Task | Details | Effort |
|------|---------|--------|
| `Project` class | Holds rungs[], elements[], metadata, undo stack | Medium |
| Rung model | Array of cells per rung, element references | Medium |
| Multiple rungs | Scrolling list of rungs with insert/delete | Small |
| Rung reordering | Drag rung handles or up/down buttons | Small |
| Element-to-rung mapping | Each element belongs to a rung + column | Small |

## Phase 7 — Serialization (Save/Load)

| Task | Details | Effort |
|------|---------|--------|
| JSON format design | Schema: version, rungs, elements with positions/properties | Small |
| Save to file | Serialize project to JSON, write to disk | Medium |
| Load from file | Parse JSON, rebuild project model | Medium |
| File dialogs | Use native file dialog (via `nfd` or Win32 API) | Small |
| Recent files list | Store in `imgui.ini` or separate config | Small |
| Auto-save / crash recovery | Periodic temp save | Medium |

## Phase 8 — Tools Panel

| Task | Details | Effort |
|------|---------|--------|
| Element palette | Categorized list of elements with icons | Small |
| Drag from palette to canvas | Drag-source on palette, drop-target on canvas | Medium |
| Search/filter palette | Text filter for element names | Small |
| Favorites / recently used | Top section with most-used elements | Small |
| Element properties panel | Dynamic form based on selected element type | Medium |
| Tag name editor | Text field for element tag/address | Small |
| Parameter editor (timer presets, etc.) | Number fields for timer/counter values | Small |

## Phase 9 — Advanced Canvas

| Task | Details | Effort |
|------|---------|--------|
| Rubber-band selection | Draw selection rectangle, select all inside | Medium |
| Minimap | Small overview of entire program in corner | Medium |
| Print / export to PDF | Save canvas as image or via 3rd-party lib | Medium |
| Cross-reference view | Spreadsheet of all tags used, where they appear | Medium |
| Error highlighting | Highlight duplicate coils, unconnected branches, etc. | Medium |

## Phase 10 — Polish & Quality

| Task | Details | Effort |
|------|---------|--------|
| DPI-aware scaling | Handle high-DPI displays | Small |
| Configurable editor settings | Grid size, colors, autosave interval | Small |
| Keyboard navigation | Tab between fields, arrow keys on canvas | Medium |
| Accessibility | High-contrast mode, readable fonts | Small |
| Installer / packaging | CPack or NSIS for distribution | Medium |
| Unit tests | Test serialization, undo, element logic | Medium |

---

## Technology Decisions

| Choice | Reason |
|--------|--------|
| **ImGui v1.90** (master, not docking) | Stable, widely used, built-in OpenGL loader |
| **GLFW 3.3.8** | Cross-platform window/input, simpler than raw Win32 |
| **OpenGL 3.0** | Minimum for ImGui, supported everywhere |
| **JSON** for file format | Human-readable, easy to debug, widely supported |
| **CMake FetchContent** | No manual dependency install, reproducible builds |
| **Command pattern** for undo/redo | Clean separation, easy to extend |

## Current State

Phase 1 is complete. Phase 2 (canvas/grid) is partially done — grid dots and power rail drawing exist but scrolling/panning need work. Phases 3–10 are unimplemented.

## Build & Run

```powershell
# Configure (first time or after CMakeLists changes)
cmake -B build

# Build
cmake --build build --config Release

# Run
build\Release\LadderEditor.exe
```
