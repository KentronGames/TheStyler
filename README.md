# The Styler

An Unreal Editor plugin for people who spend their day inside Blueprint graphs and the Content Browser.
It arranges nodes, re-routes wires as right angles instead of splines, and keeps folder colours the same
in everyone's checkout.

It changes how the editor LOOKS and how fast a graph can be tidied. It ships no content, adds no runtime
code, and nothing it does reaches a packaged game.

## What it does

**Arrange a graph.** `Shift+Q` lays out the current graph — the selected nodes, or all of them when the
selection is empty. `Shift+F` does the same for just the wire-connected component around the selection, so
one branch can be tidied without disturbing the rest. Column spacing, row spacing and how many
crossing-reduction passes the layout makes are settings.

Alongside them: align left / right / top / bottom, align on a shared centre line, and even out horizontal
or vertical gaps. They sit in the graph context menu and take a key chord if you bind one.

**Re-route wires.** Execution wires are drawn as rounded right angles (Manhattan) instead of the engine's
spline; `Metro 45` routes them diagonally and `Straight` leaves them alone. Corner radius, wire thickness,
the distance below which a wire stays straight, and how far apart parallel corridors must be before they
are nudged off each other are all adjustable. Data wires keep the engine spline unless you ask otherwise —
a graph reads better when the two kinds of wire do not look alike.

Only graph schemas that do not bring their own connection-drawing policy can be restyled. Blueprint and K2
graphs always are; a project adds its own custom schemas by class name. The engine's own editors (Niagara,
Behavior Tree, PCG, Material, MetaSound, Control Rig) each provide a policy of their own, so listing them
has no effect.

**Focus mode.** A toolbar toggle dims everything but the selected nodes. It can flip the matching flag on
other graph editors' settings classes at the same time, so one button means one thing everywhere — but only
for the classes a project lists; it never touches a settings object outside that list.

**Folder colours, shared.** Content Browser folder colours normally live in a per-user file, so a team
never sees the same tree twice. The Styler stores a table of colours by folder name — `Materials`,
`Textures`, whatever your structure calls them, with `*_Data`-style suffix matching — and paints every
matching folder across the project on demand, at editor start, or the moment a folder is created.

**Hidden folders.** Service folders a project never browses by hand can be kept out of the Content Browser
entirely. The list ships empty; an entry that names no mounted root is reported in the log rather than
silently dropped.

## Installing

1. Copy `TheStyler` into your project's `Plugins/` folder.
2. Restart the editor and enable **The Styler** in Edit → Plugins → Editor Tools.
3. A C++ project rebuilds on the next start. A Blueprint-only project needs the plugin compiled for its
   engine version first — the plugin is editor-only, so this never affects your packaged build.

Requires Unreal Engine 5.8 on Windows or Mac.

## Settings

Two objects, split by who owns the answer:

- **The Styler** (Project Settings → Plugins) — what a team wants identical in every checkout: the colour
  table, the hidden-folder list, extra graph schemas, arranger spacing. Saved to the project's
  `DefaultEditor.ini`, which is the file you commit.
- **The Styler (View)** (same place) — how graphs look to one person at one machine: wire style, thickness,
  corner radius, focus dimming. Saved per user, so a toolbar toggle never dirties a source-controlled file
  or flips a setting under a teammate.

## What it does not depend on

Engine modules only — no third-party libraries, no other plugins, no content packs. It ships empty lists
rather than opinions: no folder is coloured, hidden or restyled until a project says so, and the plugin
does not assume a particular content layout, naming convention or asset type. Uninstalling it leaves your
project exactly as it was, minus the colours you asked it to paint.

The build treats warnings as errors only inside its own home project, so a newer engine or a stricter
toolchain cannot turn a warning into a hard failure of a build you did not write.

## Licence

© 2026 Kentron Cowboys. All rights reserved.
