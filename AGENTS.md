# Gravelbyte

C++17 shared game and software 3D renderer for PicoSystem and browsers.
Keep the original picorally checkout intact. Build with CMake; format owned C++
with pinned clang-format. All three cars must finish all three courses through
public controls, with five splits and no automatic recovery. Keep Standard on
Bracken Ridge physics compatible with course version 3 saves.

Use PascalCase for project-owned identifiers, including namespaces, types,
functions, methods, fields, variables, parameters, constants and enum members
in C++ and browser JavaScript. Use descriptive words instead of short names,
abbreviations or single-letter temporaries: DeltaTimeSeconds instead of dt,
RenderingContext instead of ctx, FramebufferWidth instead of W, and
CarSpecification instead of CarSpec. Name positions and operands by their role
(for example, PositionX, SegmentIndex, LeftOperand and RightOperand). Include
units where they clarify a value. Preserve names required by external APIs,
platform entry points, build flags and persisted formats; use descriptive project-owned
wrappers where helpful. Apply this convention to all new code and deliberate
renames. Run the
clang-tidy naming checks in addition to clang-format and Prettier; the
formatters handle layout, not identifier naming.

120x120 framebuffer, 264KB RP2040 SRAM. Generate only the selected track.
Preserve 3D roads and handling. Target 50fps, require 30fps minimum in normal
racing; only full-course device telemetry can establish hardware performance.
Before flashing identify the device and make and verify a full flash backup.
Keep benchmark firmware separate from player firmware and restore the latter.

Keep reference art and its prompt. Update .agent/HANDOFF.md before ending work. Keep that file local and ignored;
never add it to Git.
