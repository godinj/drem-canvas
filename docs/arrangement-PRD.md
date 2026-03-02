# Arrangement Window — Product Requirements Document

> Exhaustive specification of the arrangement window's features, workflows, and
> vim-modal interaction model for Drem Canvas.

## Table of Contents

1. [Timeline Coordinate System](#1-timeline-coordinate-system)
2. [Grid & Snap](#2-grid--snap)
3. [Track Model](#3-track-model)
4. [Clip Lifecycle](#4-clip-lifecycle)
5. [Clip Editing](#5-clip-editing)
6. [Crossfades](#6-crossfades)
7. [Edit Modes](#7-edit-modes)
8. [Selection Model](#8-selection-model)
9. [Vim Arrangement Motions](#9-vim-arrangement-motions)
10. [Vim Operators on Arrangement](#10-vim-operators-on-arrangement)
11. [Recording Workflow](#11-recording-workflow)
12. [Takes & Comping](#12-takes--comping)
13. [Automation Lanes](#13-automation-lanes)
14. [Automation Modes](#14-automation-modes)
15. [Tempo Track](#15-tempo-track)
16. [Markers & Ranges](#16-markers--ranges)
17. [Track Grouping](#17-track-grouping)
18. [Track Freeze / Bounce](#18-track-freeze--bounce)
19. [Import / Export](#19-import--export)
20. [Waveform Rendering](#20-waveform-rendering)
21. [MIDI Clip Preview](#21-midi-clip-preview)
22. [Viewport Management](#22-viewport-management)
23. [Lane Versions](#23-lane-versions)

---

## 1. Timeline Coordinate System

### 1.1 Dual Time Domains

The arrangement operates in two simultaneous time domains:

| Domain | Unit | Use |
|--------|------|-----|
| **Linear (absolute)** | Samples / seconds | Audio clip positions, playhead, recording boundaries |
| **Musical (relative)** | Bars:beats:ticks | Grid alignment, MIDI quantization, tempo-relative editing |

The **tempo map** (§15) defines the bidirectional mapping between these domains.
Components must be able to work in either domain depending on context:

- Audio clips default to **linear** positioning — they stay at the same absolute time
  even when tempo changes
- MIDI clips default to **musical** positioning — they follow tempo changes, stretching
  or compressing in linear time
- The user can override per-clip (lock an audio clip to musical time, or lock a MIDI
  clip to linear time)

### 1.2 Internal Representation

All positions are stored as **64-bit sample offsets** from the session origin (time = 0).
This is the canonical representation in the model. Conversion to seconds, beats, or
pixels happens at display time only.

```
Model:     int64_t samplePosition
              ↓
Engine:    samplePosition / sampleRate → seconds
              ↓
TempoMap:  seconds → bars:beats:ticks (for musical display/grid)
              ↓
GUI:       seconds × pixelsPerSecond → timeline pixels
           + headerWidth - scrollOffset → screen pixels
```

### 1.3 Resolution

- **Sample resolution**: All positions and lengths are sample-accurate (no sub-sample)
- **Tick resolution**: 960 PPQN (pulses per quarter note) for musical time. This gives
  sufficient resolution for swing, tuplets, and fine MIDI timing
- **Display resolution**: Pixel positions are floating-point; rendering snaps to
  device pixels via the GPU backend

### 1.4 Time Display Formats

The time ruler and status bar can display time in multiple formats, toggled by the user:

| Format | Example | Use case |
|--------|---------|----------|
| Bars:Beats:Ticks | `4|2|240` | Musical editing, MIDI work |
| Minutes:Seconds:Millis | `1:23.456` | Audio editing, podcast/film |
| Samples | `2,116,800` | Precision alignment, debugging |
| Timecode (SMPTE) | `00:01:23:12` | Film/video sync (24/25/30fps) |

**Vim**: `:set timeformat bars` / `min` / `samples` / `smpte`

### 1.5 Session Origin

Time zero is the session start. Negative positions are allowed — this enables a
pre-roll region before bar 1, count-ins, and placing clips before the musical start.
The ruler displays negative values to the left of bar 1.

---

## 2. Grid & Snap

### 2.1 Grid Modes

The grid determines the resolution for snap, nudge, and quantize operations:

| Mode | Grid lines at | When to use |
|------|--------------|-------------|
| **Bar** | Every bar boundary | Coarse structural editing |
| **Beat** | Every beat (quarter note in 4/4) | Standard arrangement work |
| **Division** | Subdivisions: 1/8, 1/16, 1/32, triplets | Detailed rhythm editing |
| **Adaptive** | Auto-selects based on zoom level | General use (recommended default) |
| **Absolute** | Fixed time intervals (1s, 100ms, 10ms, 1ms) | Audio editing without musical grid |
| **Off** | No grid (sample-accurate) | Free placement |

**Adaptive logic**: At low zoom (overview), show bars. As the user zooms in, progress
through beats → eighth notes → sixteenth notes → thirty-second notes. Thresholds are
defined by minimum pixel spacing between grid lines (e.g., 20px minimum).

### 2.2 Snap Behavior

Snap applies to:
- Clip placement (move, paste, import)
- Clip boundaries (trim start/end, split point)
- Playhead seeking (click on ruler, jump-to commands)
- Selection boundaries (range selection start/end)
- Automation breakpoints

**Snap targets** (in priority order):
1. Grid lines (based on current grid mode)
2. Clip boundaries (start/end of any clip on any track)
3. Markers
4. Playhead position
5. Selection boundaries

### 2.3 Snap Modifiers

| State | Behavior |
|-------|----------|
| Snap ON (default) | Operations snap to nearest grid line |
| Hold modifier key | Temporarily disable snap (free placement) |
| Snap OFF | Operations are sample-accurate; hold modifier to temporarily enable snap |

The snap toggle and modifier key are user-configurable. Snap state is visible in the
status bar.

### 2.4 Vim Grid Commands

| Key | Action |
|-----|--------|
| `:set grid bar` | Set grid to bar mode |
| `:set grid beat` | Set grid to beat mode |
| `:set grid 1/8` / `1/16` / `1/32` | Set grid to subdivision |
| `:set grid 1/8t` / `1/16t` | Triplet subdivisions |
| `:set grid adaptive` | Adaptive grid (auto) |
| `:set grid off` | Disable grid |
| `:set snap on` / `off` | Toggle snap |
| `g` (in grid-cursor mode) | Toggle snap for next operation only |

### 2.5 Grid Cursor

The **grid cursor** is a vertical line that moves independently of the playhead. It
represents the vim cursor position in the time domain and is the anchor point for
many operations (paste, split, insert).

The grid cursor snaps to grid lines when snap is enabled. It can be moved to arbitrary
positions when snap is off.

| Key | Grid cursor action |
|-----|-------------------|
| `h` / `l` | Move grid cursor left/right by one grid unit |
| `H` / `L` | Move grid cursor left/right by one bar |
| `w` / `b` | Jump to next/previous clip boundary |
| `0` / `$` | Jump to session start / end of last clip |
| `^` | Jump to first clip on current track |

---

## 3. Track Model

### 3.1 Track Types

| Type | Description | Clips | I/O |
|------|-------------|-------|-----|
| **Audio** | Records/plays audio files | Audio clips (waveforms) | Audio in/out |
| **MIDI** | Records/plays MIDI data | MIDI clips (note data) | MIDI in, audio out (via instrument plugin) |
| **Instrument** | Audio track with MIDI input | MIDI clips | MIDI in → instrument plugin → audio out |
| **Bus** | Submix destination | No clips (receives from sends/routes) | Audio in (from routing) → audio out |
| **Folder** | Organizational container | Contains child tracks (visual nesting) | Sums children (optional) |
| **Aux/Send** | Effect return | No clips | Audio in (from sends) → audio out |
| **Master** | Final output | No clips | Sums all routed tracks → hardware out |

### 3.2 Track Header

The track header occupies a fixed-width column on the left side of each track lane.
It displays:

```
┌─────────────────────────────────────┐
│ [▶] Track Name            [R] [M] [S] │
│     Audio 1                           │
└─────────────────────────────────────┘
```

| Element | Description |
|---------|-------------|
| Track color | Left edge color bar (user-assignable) |
| Disclosure triangle | Expand/collapse (folder tracks, take lanes, automation) |
| Track name | Editable inline (vim `i` on header to rename) |
| Track type icon | Audio/MIDI/Bus/Folder indicator |
| Record arm `[R]` | Red when armed |
| Mute `[M]` | Dim when muted |
| Solo `[S]` | Gold when soloed |
| Input monitor | Speaker icon when monitoring enabled |
| Volume mini-fader | Optional inline fader (if track height allows) |

### 3.3 Track Sizing

Tracks have variable height. Three sizing modes:

| Mode | Height | Shows |
|------|--------|-------|
| **Compact** | 24px | Name, arm/mute/solo only — no waveform detail |
| **Normal** | 80-120px | Full waveform/MIDI preview, all header controls |
| **Expanded** | 200px+ | Detailed waveform, automation lanes visible below |

Track height is per-track (each track can be different). Global resize commands
affect all tracks.

### 3.4 Track Ordering

Tracks are ordered top-to-bottom in the arrangement. Order is meaningful:
- Visual layout matches mixer left-to-right order
- Folder tracks contain all tracks below them until the next track at the same
  nesting depth

### 3.5 Vim Track Commands

| Key | Action |
|-----|--------|
| `j` / `k` | Select next/previous track |
| `J` (Normal) | Join track below into current (merge clips) |
| `gg` / `G` | Jump to first/last track |
| `{count}G` | Jump to track number `{count}` |
| `Ctrl+j` / `Ctrl+k` | Move selected track down/up in order |
| `r` | Toggle record arm |
| `M` | Toggle mute |
| `S` | Toggle solo |
| `:track "Name"` | Create new audio track |
| `:midi "Name"` | Create new MIDI track |
| `:bus "Name"` | Create new bus |
| `:folder "Name"` | Create new folder track |
| `dd` (on empty track) | Delete track |
| `i` (on track header) | Rename track (enter Insert mode for name field) |
| `cc` (on track header) | Clear track name and enter rename |
| `zt` / `zz` / `zb` | Scroll so current track is at top/center/bottom |
| `zo` / `zc` | Expand/collapse folder or automation lanes |
| `zA` | Toggle between compact/normal/expanded height |
| `z+` / `z-` | Increase/decrease current track height |
| `z=` | Set all tracks to same height |

---

## 4. Clip Lifecycle

### 4.1 Clip Types

| Type | Contents | Created by |
|------|----------|-----------|
| **Audio clip** | Reference to audio file + position/trim/fade metadata | Recording, import, paste |
| **MIDI clip** | Note/CC/automation data + position/length | Recording, import, paste, step input |
| **Audio reference** | Same source file as another clip (shared source, independent edits) | Duplicate, copy/paste |

Clips are **non-destructive references**. Multiple clips can reference the same source
audio file with different trim/fade/gain settings. The source file is never modified.

### 4.2 Clip Creation

| Method | Description |
|--------|-------------|
| **Record** | Arm track, press record — creates clip at playhead position |
| **Import file** | Drag file onto track lane, or `:import <path>` |
| **Paste** | `p` / `P` — paste from register at grid cursor position |
| **Duplicate** | `yd` or `yy` then `p` — yank clip, paste copy |
| **MIDI step input** | In Insert mode on MIDI track, notes create/extend clip |
| **Empty MIDI clip** | `:clip new` on MIDI track — creates empty clip for editing |

### 4.3 Clip Split

Split divides one clip into two at the split point, preserving all audio/MIDI data:

```
Before:  [==========clip A==========]
Split:   [====clip A====][====clip B====]
```

Both clips reference the same source. Clip A gets `trimEnd` updated, clip B gets
`trimStart` updated.

| Key | Action |
|-----|--------|
| `s` | Split clip at grid cursor position |
| `s` (Visual) | Split all clips in visual selection at grid cursor |

### 4.4 Clip Join

Join merges two adjacent clips on the same track into one, if they reference the same
source and their trim boundaries are contiguous:

```
Before:  [====A====][====B====]
Join:    [=========AB=========]
```

If clips reference different sources or have a gap, join creates a new audio file
(bounce) or concatenates MIDI data.

| Key | Action |
|-----|--------|
| `J` | Join clip under cursor with next clip |
| `J` (Visual) | Join all clips in visual selection |

### 4.5 Clip Delete

| Key | Action |
|-----|--------|
| `x` | Delete clip under cursor |
| `X` | Delete clip and ripple (close gap) |
| `dd` | Delete all clips on current track at grid cursor position |
| `d{motion}` | Delete clips covered by motion |
| `D` | Delete from grid cursor to end of track |

### 4.6 Clip Naming

Clips inherit their name from the source file or recording take. Names can be
overridden:

| Key | Action |
|-----|--------|
| `:clip rename "New Name"` | Rename clip under cursor |
| Clip display | Shows name in clip header (when track height allows) |

---

## 5. Clip Editing

### 5.1 Trim

Trimming adjusts the visible start or end of a clip without moving its position on the
timeline. The underlying source data beyond the trim boundary is preserved (non-destructive).

```
Source audio: [0000000XXXXXXXXXX0000000]
                     ↑ trimStart  ↑ trimEnd
Visible clip:        [XXXXXXXXXX]
```

| Key | Action |
|-----|--------|
| `[` | Trim clip start to grid cursor (reveal earlier content) |
| `]` | Trim clip end to grid cursor (reveal later content) |
| `Shift+[` | Trim clip start forward by one grid unit (hide content) |
| `Shift+]` | Trim clip end backward by one grid unit (hide content) |
| `{count}[` | Trim clip start by `{count}` grid units |

Trim is clamped to the source file boundaries — you cannot trim beyond available data.
If snap is on, trim boundaries snap to grid lines.

### 5.2 Slip

Slip moves the audio/MIDI content within the clip boundaries without changing the clip's
position or length on the timeline. Useful for aligning a specific beat within a loop.

```
Before: [XXXX|content|XXXX]    (clip position fixed)
Slip:   [XX|content|XXXXXX]    (content shifted left, more revealed on right)
```

| Key | Action |
|-----|--------|
| `Alt+h` / `Alt+l` | Slip content left/right by one grid unit |
| `Alt+H` / `Alt+L` | Slip content left/right by one bar |

### 5.3 Move

Move changes the clip's position on the timeline:

| Key | Action |
|-----|--------|
| `<` / `>` | Nudge clip left/right by one grid unit |
| `Shift+<` / `Shift+>` | Nudge clip left/right by one beat |
| `{count}<` | Nudge clip left by `{count}` grid units |
| `:clip move {time}` | Move clip to absolute position |
| `:clip move +{offset}` | Move clip by relative offset |

**Cross-track move**: Moving a clip to a different track:

| Key | Action |
|-----|--------|
| `Ctrl+j` / `Ctrl+k` | Move clip to track below/above (maintains time position) |

### 5.4 Duplicate

| Key | Action |
|-----|--------|
| `yy` then `p` | Yank clip, paste at grid cursor |
| `Ctrl+d` | Duplicate clip immediately after original (no gap) |
| `{count}Ctrl+d` | Create `{count}` duplicates end-to-end |

### 5.5 Fade In / Fade Out

Fades are per-clip. The fade region is drawn as a transparency gradient over the clip.

| Key | Action |
|-----|--------|
| `fi` / `fo` | Open fade-in / fade-out editing for clip under cursor |
| `fi{count}` | Set fade-in length to `{count}` grid units |
| `fo{count}` | Set fade-out length to `{count}` grid units |
| `:clip fadein {ms}` | Set fade-in length in milliseconds |
| `:clip fadeout {ms}` | Set fade-out length in milliseconds |

**Fade curves**: Linear (default), exponential, logarithmic, S-curve. Set via
`:clip fadecurve {type}`.

### 5.6 Clip Gain

Per-clip gain adjustment (pre-fader, applied before the track's plugin chain):

| Key | Action |
|-----|--------|
| `Ctrl++` / `Ctrl+-` | Adjust clip gain by 1dB |
| `Ctrl+Shift++` / `Ctrl+Shift+-` | Adjust clip gain by 0.1dB |
| `:clip gain {dB}` | Set absolute clip gain |
| `Ctrl+0` | Reset clip gain to 0dB |

### 5.7 Clip Color

Clips inherit track color by default. Override per-clip:

| Key | Action |
|-----|--------|
| `:clip color {color}` | Set clip color (name or hex) |
| `:clip color reset` | Reset to track color |

---

## 6. Crossfades

### 6.1 Overlap Behavior

When two clips overlap on the same track, a **crossfade** is created in the overlap
region. The first clip fades out while the second fades in.

```
Clip A:  [==========]
Clip B:        [==========]
Overlap:       [XXXX]          ← crossfade region
Result:  [=====╲╱╱╲==========]
              fade A→ ←fade B
```

### 6.2 Crossfade Types

| Type | Behavior |
|------|----------|
| **Equal power** | -3dB at midpoint — maintains perceived loudness (default) |
| **Equal gain (linear)** | -6dB at midpoint — simple linear ramp |
| **S-curve** | Slow start/end, fast middle — smooth transitions |
| **Custom** | User-defined bezier curve |

### 6.3 Auto-Crossfade

When `auto-crossfade` is enabled (default), moving or trimming a clip to overlap
another automatically creates a crossfade. The default crossfade length is
configurable:

`:set crossfade-length {ms}` — default crossfade duration when clips are butted
together (moved adjacent without explicit overlap).

When auto-crossfade is off, overlapping clips simply overlap — the later clip takes
priority (covers the earlier clip).

### 6.4 Vim Crossfade Commands

| Key | Action |
|-----|--------|
| `:crossfade {ms}` | Set crossfade length of crossfade under cursor |
| `:crossfade type {type}` | Set crossfade curve type |
| `:set auto-crossfade on/off` | Toggle auto-crossfade |
| `Enter` (on crossfade) | Open crossfade editor (curve adjustment) |

---

## 7. Edit Modes

Edit modes determine how surrounding clips respond when a clip is moved, trimmed,
or deleted. Only one edit mode is active at a time.

### 7.1 Slip Mode (Default)

Clips move freely without affecting neighbors. Overlapping clips crossfade or overlap
based on the auto-crossfade setting.

```
Delete B:    [A][B][C]  →  [A]   [C]     (gap remains)
Move C left: [A]   [C]  →  [A][C]        (free movement)
```

### 7.2 Ripple Mode

Deleting or shortening a clip shifts all subsequent clips on the same track to close
the gap. Lengthening or inserting pushes subsequent clips later.

```
Delete B:    [A][B][C]  →  [A][C]         (C slides left)
Insert X:    [A][C]     →  [A][X][C]      (C slides right)
```

Ripple has two sub-modes:

| Sub-mode | Behavior |
|----------|----------|
| **Ripple (track)** | Only clips on the same track shift |
| **Ripple (all)** | Clips on ALL tracks at the same or later time shift — maintains sync |

### 7.3 Shuffle Mode

Like ripple but clips cannot overlap or leave gaps. Clips lock edge-to-edge:

```
Move C before B:  [A][B][C]  →  [A][C][B]   (B shifts right to make room)
```

### 7.4 Slide Mode

Moving a clip slides the clip content within its current boundaries (equivalent to
slip-editing). The clip position and length stay fixed on the timeline, but the
internal content offset changes.

### 7.5 Vim Edit Mode Commands

| Key | Action |
|-----|--------|
| `:set editmode slip` | Slip mode (default) |
| `:set editmode ripple` | Ripple mode (track-only) |
| `:set editmode ripple-all` | Ripple mode (all tracks) |
| `:set editmode shuffle` | Shuffle mode |
| `:set editmode slide` | Slide mode |
| Status bar | Shows current edit mode: `[Slip]` / `[Ripple]` / etc. |

---

## 8. Selection Model

### 8.1 Selection Types

The arrangement supports multiple concurrent selection types:

| Selection | What is selected | Visual indicator |
|-----------|-----------------|-----------------|
| **Track selection** | Current track (row) | Highlighted track header, subtle row background |
| **Clip selection** | One or more clips | Bright outline / highlight on selected clips |
| **Range selection** | Time span × track span | Shaded rectangle overlay |
| **Grid cursor** | Point in time on current track | Thin vertical line (distinct from playhead) |

These selections are **independent and composable**:
- Track selection determines which track receives new clips, recording, etc.
- Clip selection determines which clips are affected by operators
- Range selection defines a time span for operations like bounce, export, cut
- Grid cursor is the anchor for paste, split, and insert operations

### 8.2 Clip Selection

| Key | Action |
|-----|--------|
| `h` / `l` | Move clip cursor to previous/next clip on current track |
| `Enter` | Toggle selection of clip under cursor (multi-select) |
| `v` | Enter Visual mode — extend clip selection from anchor to cursor |
| `V` | Enter Visual-Line mode — select all clips on selected tracks |
| `Ctrl+a` | Select all clips |
| `Escape` | Clear clip selection |

### 8.3 Range Selection

Range selection defines a rectangular region in time × tracks:

| Key | Action |
|-----|--------|
| `Shift+v` | Start range selection at grid cursor; extend with `h/l/j/k` |
| `{` / `}` | Extend range to previous/next clip boundary |
| `Shift+0` / `Shift+$` | Extend range to session start/end |

Range selection operates independently of clip selection. When an operator is applied
with a range selection active, it affects all clips (or portions of clips) that fall
within the range.

**Partial clip in range**: If a clip is partially within the range, the operator may:
- Split the clip at the range boundary and operate on the portion inside
- Or operate on the entire clip (depending on the operator)

### 8.4 Multi-Track Selection

| Key | Action |
|-----|--------|
| `V` | Visual-Line mode — select entire tracks |
| `Vj` / `Vk` | Extend track selection down/up |
| `V{count}j` | Select current + `{count}` tracks below |

### 8.5 Selection and Registers

All selection operations work with vim registers:

| Key | Action |
|-----|--------|
| `"ay` | Yank selection to register `a` |
| `"ap` | Paste from register `a` |
| `"+y` | Yank to system clipboard |
| `"+p` | Paste from system clipboard |
| `"0p` | Paste last yank (yank register) |
| `"1p` | Paste last delete (delete history) |

---

## 9. Vim Arrangement Motions

Motions define cursor movement and the range for operators. All motions can be
prefixed with a count.

### 9.1 Clip-Level Motions

| Motion | Movement |
|--------|----------|
| `h` | Previous clip on current track |
| `l` | Next clip on current track |
| `j` | Same-position clip on next track (or next track if no clip) |
| `k` | Same-position clip on previous track |
| `w` | Next clip boundary (start or end of any clip on current track) |
| `b` | Previous clip boundary |
| `e` | End of current clip |
| `0` | First clip on current track |
| `$` | Last clip on current track |
| `^` | First non-empty position on current track |
| `gg` | First track |
| `G` | Last track |
| `{count}G` | Track number `{count}` |

### 9.2 Grid-Level Motions

| Motion | Movement |
|--------|----------|
| `Shift+h` | One grid unit left |
| `Shift+l` | One grid unit right |
| `Shift+H` | One bar left |
| `Shift+L` | One bar right |
| `Ctrl+h` | Previous marker |
| `Ctrl+l` | Next marker |

### 9.3 Playhead Motions

| Motion | Movement |
|--------|----------|
| `Space` | Toggle play/stop |
| `Backspace` | Return playhead to position where play was last started |
| `.` (transport) | Move playhead to grid cursor position |
| `Ctrl+Home` | Playhead to session start |
| `Ctrl+End` | Playhead to end of last clip |

### 9.4 Jump Motions

| Motion | Movement |
|--------|----------|
| `'{char}` | Jump to marker named `{char}` |
| `''` | Jump back to position before last jump |
| `'.` | Jump to position of last edit |
| `f{char}` | Jump forward to marker starting with `{char}` |
| `F{char}` | Jump backward to marker starting with `{char}` |
| `/{pattern}` | Search forward for clip/track/marker matching pattern |
| `?{pattern}` | Search backward |
| `n` / `N` | Next/previous search match |
| `*` | Search for other clips using same source file |
| `#` | Search backward for other clips using same source file |

### 9.5 Text Objects (Clip-Aware)

Text objects define a range without moving the cursor. Used with operators:

| Text Object | Range |
|-------------|-------|
| `iw` | Inner clip (the clip under cursor, without gaps) |
| `aw` | A clip (the clip under cursor, including trailing gap) |
| `i[` | Inner track (all clips on current track) |
| `a[` | A track (all clips on current track, including track header) |
| `ip` | Inner phrase — contiguous group of clips with no gaps |
| `ap` | A phrase — contiguous group including surrounding gaps |
| `i{` | Inner range selection |
| `a{` | A range selection including boundaries |

---

## 10. Vim Operators on Arrangement

### 10.1 Core Operators

| Operator | Action | Example |
|----------|--------|---------|
| `d` | Delete | `dw` = delete to next clip boundary |
| `y` | Yank (copy) | `yy` = yank current clip |
| `c` | Change (delete + insert mode) | `ciw` = replace clip contents |
| `>` | Nudge right by grid unit | `>3l` = nudge 3 clips right |
| `<` | Nudge left by grid unit | `<w` = nudge to next boundary left |
| `g~` | Toggle mute | `g~iw` = toggle mute on clip under cursor |
| `gU` | Unmute | `gUi[` = unmute all clips on track |
| `gu` | Mute | `gui[` = mute all clips on track |

### 10.2 Operator + Motion Composition

The general form is:

```
[register] [count] operator [count] motion/text-object
```

Examples:

| Sequence | Meaning |
|----------|---------|
| `d3l` | Delete 3 clips to the right |
| `dG` | Delete all clips from cursor to last track |
| `y$` | Yank from cursor to end of track |
| `"adiw` | Delete clip under cursor into register `a` |
| `d/kick` | Delete from cursor to next clip matching "kick" |
| `yip` | Yank contiguous phrase of clips |
| `dV3j` | Delete current track + 3 below (linewise) |

### 10.3 Shorthand Operators

| Key | Equivalent | Action |
|-----|-----------|--------|
| `x` | `diw` | Delete clip under cursor |
| `X` | `dh` | Delete clip before cursor |
| `dd` | `d$` + `dk` | Delete entire track contents (or track if empty) |
| `yy` | `yiw` | Yank clip under cursor |
| `Y` | `yi[` | Yank entire track |
| `D` | `d$` | Delete from cursor to end of track |
| `C` | `c$` | Change from cursor to end of track |
| `p` | | Paste after grid cursor |
| `P` | | Paste before grid cursor |
| `u` | | Undo |
| `Ctrl+r` | | Redo |
| `.` | | Repeat last operator + motion |

### 10.4 Visual Mode Operators

When in Visual or Visual-Line mode, operators apply to the entire selection:

| Key | Action |
|-----|--------|
| `d` / `x` | Delete all selected clips |
| `y` | Yank all selected clips |
| `>` / `<` | Nudge all selected clips right/left by grid unit |
| `{count}>` | Nudge all selected clips by `{count}` grid units |
| `M` | Toggle mute on all selected clips |
| `S` | Toggle solo on all tracks containing selected clips |
| `J` | Join selected clips (if adjacent and compatible) |
| `:` | Enter command mode with range set to visual selection |

### 10.5 Dot Repeat (`.`)

The `.` command repeats the last operator + motion combination. Examples:

1. `dw` → `.` → deletes to next clip boundary again
2. `3>l` → `.` → nudges 3 clips right again
3. `yyp` → `.` → paste another copy

The repeat register stores: operator, count, motion, and any text entered in Insert
mode (for the `c` operator).

### 10.6 Macros

| Key | Action |
|-----|--------|
| `q{char}` | Start recording macro to register `{char}` |
| `q` | Stop recording |
| `@{char}` | Play macro from register `{char}` |
| `@@` | Repeat last played macro |
| `{count}@{char}` | Play macro `{count}` times |

---

## 11. Recording Workflow

### 11.1 Track Arming

A track must be armed before it can record. Multiple tracks can be armed simultaneously
for multi-track recording.

| Key | Action |
|-----|--------|
| `r` | Toggle record arm on selected track |
| `:arm all` | Arm all tracks |
| `:arm none` | Disarm all tracks |
| Visual indicator | Red record button in track header, track background tint |

### 11.2 Input Monitoring

When a track is armed, the user can choose to hear the input signal:

| Mode | Behavior |
|------|----------|
| **Auto** | Monitor when stopped, no monitor during playback (to avoid double) |
| **On** | Always monitor input (user manages latency) |
| **Off** | Never monitor — only hear playback |

`:set monitor auto` / `on` / `off` — per-track or global default.

### 11.3 Recording Modes

| Mode | Behavior |
|------|----------|
| **Normal** | Record creates new clip from transport start to stop |
| **Punch** | Record only within punch in/out range (pre-set time markers) |
| **Loop** | Record continuously through loop region; each pass creates a new take |

### 11.4 Punch In/Out

Punch recording limits recording to a pre-defined time range. The transport plays
normally before the punch-in point, records during the range, then continues playing
after the punch-out point.

| Key | Action |
|-----|--------|
| `:punch in {time}` | Set punch-in point |
| `:punch out {time}` | Set punch-out point |
| `:punch range` | Set punch range to current range selection |
| `:punch off` | Disable punch |
| `I` (on ruler) | Set punch-in at grid cursor |
| `O` (on ruler) | Set punch-out at grid cursor |

### 11.5 Pre-Roll and Count-In

| Setting | Purpose |
|---------|---------|
| **Pre-roll** | Playback starts N bars/seconds before the record point |
| **Count-in** | Metronome plays N bars before recording begins |

`:set preroll {bars}` and `:set countin {bars}`.

### 11.6 Recording Controls

| Key | Action |
|-----|--------|
| `Space` (with armed tracks) | Start recording |
| `Space` (during recording) | Stop recording |
| `Escape` (during recording) | Stop recording and discard take |
| After recording | New clip appears on the timeline |

---

## 12. Takes & Comping

### 12.1 Take Lanes

When recording over an existing region (or loop recording), the DAW creates **take
lanes** — stacked versions of the same time region on a single track.

```
Track: Vocals
├── Take 1: [=======original=======]
├── Take 2: [=====punch take 2=====]
├── Take 3: [=====punch take 3=====]   ← active take
└── Comp:   [==T1==][T3][===T2===]     ← assembled comp
```

### 12.2 Take Management

| Key | Action |
|-----|--------|
| `zo` | Expand take lanes (show all takes) |
| `zc` | Collapse take lanes (show only comp) |
| `{count}t` | Switch to take `{count}` (make it the active take) |
| `Alt+j` / `Alt+k` | Cycle through takes up/down |
| `dt` | Delete current take |
| `:take rename "Name"` | Rename current take |

### 12.3 Comping

Comping lets the user assemble the best portions of multiple takes into a single comp:

| Key | Action |
|-----|--------|
| `c` (in comp mode) | Comp selection — promote highlighted range from current take |
| Range select + `c` | Use selected range from current take in comp |
| `Ctrl+z` (comp) | Undo last comp edit |
| `:comp flatten` | Bounce comp to a single new clip, collapse take lanes |

**Comp workflow**:
1. Record multiple takes (loop or punch recording)
2. Expand take lanes (`zo`)
3. Navigate to desired take (`Alt+j/k`)
4. Select range of good material (range selection)
5. Promote to comp (`c`)
6. Repeat for each section
7. Flatten when satisfied (`:comp flatten`)

---

## 13. Automation Lanes

### 13.1 Concept

Automation lanes display and edit time-varying parameter values below their parent
track. Each automatable parameter (volume, pan, plugin parameters) can have its own
lane.

```
Track: Vocals (80px)
├── Automation: Volume     (40px, line graph)
├── Automation: Pan        (40px, line graph)
└── Automation: EQ Freq    (40px, line graph, from plugin)
```

### 13.2 Automation Data

An automation lane consists of **breakpoints** (time, value) connected by curves:

| Curve type | Shape |
|-----------|-------|
| **Linear** | Straight line between breakpoints |
| **Discrete/Step** | Hold value until next breakpoint, then jump |
| **Bezier** | Smooth curve with control points |
| **S-curve** | Smooth sigmoid between breakpoints |

Default is linear. Per-segment curve type is supported (different curves between
different breakpoint pairs).

### 13.3 Breakpoint Editing

| Key | Action |
|-----|--------|
| `a` | Add breakpoint at grid cursor position |
| `x` | Delete breakpoint under cursor |
| `j` / `k` | Adjust value of breakpoint under cursor (down/up) |
| `h` / `l` | Move to previous/next breakpoint |
| `Shift+j` / `Shift+k` | Fine adjust value (smaller increment) |
| `{count}j` | Adjust value by `{count}` increments |
| `v` | Visual select breakpoint range |
| `d` (Visual) | Delete selected breakpoints |
| `y` (Visual) | Copy selected breakpoints |

### 13.4 Drawing Automation

In addition to breakpoint editing, automation can be drawn freehand or in shapes:

| Key | Action |
|-----|--------|
| `:auto draw` | Enter draw mode — mouse/touchpad creates breakpoints along path |
| `:auto line` | Draw a straight line between two clicked points |
| `:auto sine {freq}` | Generate sine wave automation |
| `:auto ramp {start} {end}` | Generate linear ramp |
| `:auto clear` | Clear all automation in current lane |
| `:auto clear {range}` | Clear automation in time range |

### 13.5 Vim Automation Lane Commands

| Key | Action |
|-----|--------|
| `zo` | Show automation lanes for current track |
| `zc` | Hide automation lanes |
| `Tab` (in track context) | Cycle focus between clip lane and automation lanes |
| `J` / `K` (in auto lane) | Move to next/previous automation lane |
| `:auto add volume` | Add volume automation lane |
| `:auto add pan` | Add pan automation lane |
| `:auto add {plugin} {param}` | Add plugin parameter automation lane |
| `:auto remove` | Remove current automation lane |

---

## 14. Automation Modes

### 14.1 Mode Definitions

| Mode | During playback | During stop |
|------|----------------|-------------|
| **Off** | Automation data ignored — parameter stays at manual value | Manual control |
| **Read** | Parameter follows automation curve; manual changes are temporary | Manual control |
| **Write** | All parameter movements are recorded; overwrites existing automation | Manual control |
| **Touch** | Records when parameter is being adjusted; reverts to curve when released | Manual control |
| **Latch** | Like Touch, but holds last value after release instead of reverting | Manual control |
| **Trim** | Offsets existing curve by the adjustment amount (relative editing) | Manual control |

### 14.2 Mode Setting

| Key | Action |
|-----|--------|
| `:auto mode read` | Set current lane to Read mode |
| `:auto mode write` | Set current lane to Write mode |
| `:auto mode touch` | Set current lane to Touch mode |
| `:auto mode latch` | Set current lane to Latch mode |
| `:auto mode trim` | Set current lane to Trim mode |
| `:auto mode off` | Disable automation for current lane |
| Status bar | Shows automation mode when automation lane is focused |

### 14.3 Global Automation Controls

| Key | Action |
|-----|--------|
| `:auto suspend` | Temporarily bypass all automation (global) |
| `:auto resume` | Re-enable all automation |
| `:auto mode all read` | Set all lanes on all tracks to Read mode |

---

## 15. Tempo Track

### 15.1 Tempo Map

The tempo map defines the relationship between linear time (seconds) and musical time
(bars:beats:ticks). It consists of:

- **Tempo events**: BPM changes at specific positions
- **Time signature events**: Meter changes (e.g., 4/4 → 3/4) at bar boundaries
- **Tempo ramps**: Gradual tempo changes (accelerando/ritardando) between two points

### 15.2 Tempo Events

| Behavior | Description |
|----------|-------------|
| **Step** | Instant tempo change — tempo jumps to new value at the event position |
| **Ramp (linear)** | Gradual change — tempo transitions linearly from previous value to new value |
| **Ramp (curve)** | Gradual change with an ease-in/ease-out curve |

### 15.3 Tempo Track Display

The tempo track is a special lane at the top of the arrangement (above the first
regular track). It displays tempo as a line graph:

```
BPM: 140 ─────────╲─── 120 ───────────╱── 140
          bar 1    bar 5            bar 9
```

When the tempo track is visible, tempo events can be edited like automation breakpoints.

### 15.4 Vim Tempo Commands

| Key | Action |
|-----|--------|
| `:set tempo {bpm}` | Set tempo at session start |
| `:tempo add {bpm}` | Add tempo event at grid cursor |
| `:tempo ramp {bpm}` | Add tempo ramp from previous event to grid cursor |
| `:tempo delete` | Delete tempo event at grid cursor |
| `:set timesig {n}/{d}` | Set time signature (e.g., `3/4`, `6/8`, `7/8`) |
| `:timesig add {n}/{d}` | Add time signature change at grid cursor |
| `gt` | Toggle tempo track visibility |

### 15.5 Tempo and Audio Clips

When tempo changes, audio clips can respond in two ways:

| Mode | Behavior |
|------|----------|
| **Fixed (default for audio)** | Clip stays at same linear time — musical position shifts |
| **Musical** | Clip stays at same bar:beat — linear time shifts (time-stretch applied) |

Per-clip setting: `:clip timebase fixed` / `musical`.

MIDI clips default to **musical** — they follow tempo changes automatically since their
positions are stored in beats.

### 15.6 Time Stretch

When an audio clip is in musical timebase and tempo changes, time-stretching is applied:

| Algorithm | Quality | CPU | Use case |
|-----------|---------|-----|----------|
| **Elastique** | High | Medium | General purpose (if licensed) |
| **Rubber Band** | Good | Medium | Open-source default |
| **Repitch** | N/A | Low | Speed change + pitch change (like a turntable) |
| **Slice** | N/A | Low | Rhythmic material — slices at transients, repositions |

`:set stretch-algorithm {name}` — global default. Per-clip override via
`:clip stretch {name}`.

---

## 16. Markers & Ranges

### 16.1 Marker Types

| Type | Description | Visual |
|------|-------------|--------|
| **Position marker** | Named point in time | Flag icon on ruler with name label |
| **Range marker** | Named time span | Colored bar spanning the range on ruler |
| **Loop region** | Time span that repeats during playback | Highlighted bar on ruler (distinct color) |
| **Punch region** | Time span for punch recording | Highlighted bar on ruler (distinct color) |
| **Arrangement marker** | Structural section (Verse, Chorus, Bridge) | Wide colored band with label |

### 16.2 Marker Commands

| Key | Action |
|-----|--------|
| `m{a-z}` | Set position marker at grid cursor, named `{char}` |
| `m{A-Z}` | Set global (session-level) marker |
| `'{a-z}` | Jump to marker `{char}` |
| `''` | Jump to position before last jump |
| `'.` | Jump to position of last edit |
| `Ctrl+h` / `Ctrl+l` | Jump to previous/next marker |
| `:mark "Name"` | Set named marker at grid cursor |
| `:mark delete {char}` | Delete marker |
| `:marks` | List all markers |

### 16.3 Range Markers

Range markers define named sections of the arrangement:

| Key | Action |
|-----|--------|
| `:range "Verse 1"` | Create range marker from range selection |
| `:range delete "Verse 1"` | Delete range marker |
| `:range list` | List all range markers |
| `'{name}` | Jump to start of named range |

### 16.4 Loop Region

| Key | Action |
|-----|--------|
| `:loop` | Set loop to current range selection |
| `:loop {start} {end}` | Set loop to explicit range |
| `:loop off` | Disable looping |
| `Ctrl+L` | Toggle loop on/off |
| During playback with loop | Transport jumps from loop end to loop start |

### 16.5 Arrangement Markers (Song Structure)

Arrangement markers are high-level structural labels that span time ranges. They
display as colored bands above the time ruler:

```
[  Intro  |   Verse 1   |  Chorus  |   Verse 2   |  Chorus  |  Outro  ]
```

These serve as navigation landmarks and can be used for arrangement operations:

| Key | Action |
|-----|--------|
| `:section "Chorus"` | Create arrangement section from range selection |
| `Ctrl+h` / `Ctrl+l` | Jump to previous/next section boundary |
| `d` + section motion | Delete all clips within a section |
| `y` + section motion | Yank all clips within a section |

---

## 17. Track Grouping

### 17.1 Group Types

| Type | Purpose | Behavior |
|------|---------|----------|
| **Edit group** | Linked editing — operations on one member affect all | Move/trim/split/delete sync |
| **Mix group** | Linked mixing — fader/pan/mute/solo follow relative offsets | Relative fader tracking |
| **Folder track** | Visual nesting and routing | Collapses to hide children; optionally sums audio |

### 17.2 Edit Groups

When clips are edited on one track in an edit group, the same operation is mirrored on
all other group members at the same time position:

- Split → splits on all group tracks at the same position
- Move → moves clips on all group tracks by the same offset
- Trim → trims clips on all group tracks by the same amount
- Delete → deletes clips on all group tracks in the same range

### 17.3 Mix Groups

Mix groups link mixer parameters with **relative offsets** — moving one fader adjusts
all group members by the same dB amount, preserving level differences.

Parameters that can be grouped: volume, pan, mute, solo, record arm.

### 17.4 Folder Tracks

Folder tracks visually contain other tracks. Collapsing a folder hides its children:

```
▶ Drums (folder, collapsed)
   → Kick, Snare, HiHat, Overhead L, Overhead R   (hidden)
```

When a folder track has audio routing enabled, it sums all child track outputs
(acting as a submix bus).

### 17.5 Vim Group Commands

| Key | Action |
|-----|--------|
| `:group edit "Drums"` | Create edit group from selected tracks |
| `:group mix "Strings"` | Create mix group from selected tracks |
| `:group delete "Drums"` | Delete group (tracks remain) |
| `:group add {track}` | Add track to current group |
| `:group remove {track}` | Remove track from current group |
| `zo` / `zc` (on folder) | Expand/collapse folder |
| `zO` / `zC` | Expand/collapse all folders |

---

## 18. Track Freeze / Bounce

### 18.1 Track Freeze

Freeze renders a track's audio (including all plugins) to a temporary audio file and
bypasses the plugin chain. This reduces CPU usage at the cost of not being able to
edit plugin parameters.

| State | Plugins | CPU | Editing |
|-------|---------|-----|---------|
| **Unfrozen** | Active (processing in real-time) | Normal | Full editing |
| **Frozen** | Bypassed (playback from rendered file) | Minimal | Clips locked, plugins dimmed |

### 18.2 Freeze Commands

| Key | Action |
|-----|--------|
| `:freeze` | Freeze selected track |
| `:unfreeze` | Unfreeze selected track |
| `:freeze all` | Freeze all tracks |
| Visual indicator | Snowflake icon on frozen tracks; waveform shows rendered output |

Unfreezing restores the original clip + plugin chain. The rendered file is discarded.

### 18.3 Bounce In Place

Bounce in place renders the track's output (clips + plugins) to a new audio file and
replaces the original clips. Unlike freeze, this is **destructive** to the arrangement
(though undoable).

| Key | Action |
|-----|--------|
| `:bounce` | Bounce selected track in place |
| `:bounce range` | Bounce only the current range selection |
| `:bounce stem` | Bounce to new track (preserves original) |

After bounce, the track has a single audio clip with no plugins. The original state
is preserved in undo history.

---

## 19. Import / Export

### 19.1 Audio Import

| Method | Behavior |
|--------|----------|
| `:import {path}` | Import audio file to selected track at grid cursor |
| `:import {path} {track}` | Import to specific track |
| Drag-and-drop | Drop file onto track lane → creates clip at drop position |
| Drag to empty area | Creates new track + clip |

**Import behavior**:
- Files are **copied** to the session's `interchange/audiofiles/` directory (configurable)
- Or files are **referenced** in place (`:set import-mode copy` / `reference`)
- Sample rate conversion is applied if file SR differs from session SR
- Supported formats: WAV, AIFF, FLAC, OGG, MP3 (decode only)

### 19.2 MIDI Import

| Method | Behavior |
|--------|----------|
| `:import {path.mid}` | Import MIDI file — one track per MIDI channel |
| Import options | Channel mapping, tempo import, quantize on import |

### 19.3 Export / Bounce

| Command | Result |
|---------|--------|
| `:export` | Open export dialog |
| `:export {path}` | Export master bus to file |
| `:export stems {dir}` | Export each track as separate file |
| `:export range` | Export only the current range selection |

**Export settings**:
- Format: WAV, AIFF, FLAC, OGG
- Bit depth: 16, 24, 32 (float)
- Sample rate: 44100, 48000, 88200, 96000, or match session
- Dithering: None, triangular, shaped (for bit depth reduction)
- Normalize: Off, peak, loudness (LUFS target)
- Tail: Include reverb/delay tails after last clip

### 19.4 Stem Export

Stem export renders each track (or group of tracks) to individual files:

| Option | Behavior |
|--------|----------|
| **Per-track** | One file per track |
| **Per-bus** | One file per bus (includes all routed tracks) |
| **Per-folder** | One file per folder track |
| Naming | `{session}_{track}_{format}` |

---

## 20. Waveform Rendering

### 20.1 Zoom-Adaptive Detail

Waveform display adapts to the current zoom level:

| Zoom level | Display |
|------------|---------|
| **Overview** (< 10 px/sec) | RMS envelope only (filled shape) |
| **Normal** (10-500 px/sec) | Peak + RMS dual envelope |
| **Detailed** (500-5000 px/sec) | Individual sample waveform visible |
| **Sample** (> 5000 px/sec) | Sample points with interpolation curve |

### 20.2 Waveform Colors

| Element | Color |
|---------|-------|
| Peak envelope | Track color (or clip color if overridden), lighter shade |
| RMS envelope | Track color, full opacity |
| Clip background | Dark fill behind waveform |
| Selected clip | Brighter background, highlighted border |
| Muted clip | Desaturated / dimmed |
| Clip with gain ≠ 0dB | Waveform scaled vertically |

### 20.3 Waveform Cache

Waveforms are rendered from a multi-resolution cache:

- **Level 0**: Raw samples (used at highest zoom)
- **Level 1-N**: Progressively downsampled peak/RMS pairs
- Cache is generated asynchronously on clip creation/import
- Cache is stored in the session `peaks/` directory
- Cache is regenerated if source file changes

### 20.4 Transient Display

At sufficient zoom, detected transients are shown as vertical markers on the waveform.
These are used for:
- Quantize-to-transients
- Slice mode time stretching
- Beat detection

`:set show-transients on/off`

### 20.5 Spectral View (Future)

An alternative display mode showing frequency content:

`:set waveform-display waveform` / `spectral` / `both`

When `both`, the spectral view is shown as a colored overlay behind the waveform.

---

## 21. MIDI Clip Preview

### 21.1 Arrangement View

MIDI clips in the arrangement view show a miniature piano-roll preview:

```
┌──── MIDI Clip ──────────────────┐
│  ▄     ▄▄   ▄                  │  ← high notes
│  █  █  ██   █  █               │
│  █  █  ██   █  █  █            │  ← middle notes
│  ████  ██   █████  █           │  ← low notes (bass)
└─────────────────────────────────┘
```

- Note bars are colored by velocity (brighter = louder)
- Vertical axis spans the note range present in the clip (auto-scaled)
- Note bars are positioned proportionally in time

### 21.2 Display Modes

| Mode | Display |
|------|---------|
| **Piano roll** | Horizontal bars (default) |
| **Velocity** | Vertical bars showing velocity per note |
| **Drum** | Diamond/dot pattern (for drum tracks) |
| **Empty** | Just clip outline with name (compact mode) |

`:set midi-display piano` / `velocity` / `drum` / `minimal`

### 21.3 MIDI Clip Editing from Arrangement

Double-clicking a MIDI clip (or pressing `Enter` on it) opens the piano roll editor
as a split view below the arrangement. Closing the piano roll returns focus to the
arrangement.

| Key | Action |
|-----|--------|
| `Enter` | Open MIDI clip in piano roll |
| `Escape` (in piano roll) | Close piano roll, return to arrangement |
| `Ctrl+Enter` | Open MIDI clip in full-screen piano roll |

---

## 22. Viewport Management

### 22.1 Scrolling

| Key | Action |
|-----|--------|
| `Ctrl+e` | Scroll viewport down (tracks) |
| `Ctrl+y` | Scroll viewport up (tracks) |
| `Ctrl+f` | Scroll viewport right (forward in time) by one screenful |
| `Ctrl+b` | Scroll viewport left (backward in time) by one screenful |
| `Ctrl+d` | Scroll half-screen right |
| `Ctrl+u` | Scroll half-screen left |

### 22.2 Zoom

**Horizontal zoom** (time axis):

| Key | Action |
|-----|--------|
| `+` / `=` | Zoom in (increase pixelsPerSecond) |
| `-` | Zoom out (decrease pixelsPerSecond) |
| `z+` | Zoom in centered on grid cursor |
| `z-` | Zoom out centered on grid cursor |
| `zf` | Zoom to fit entire session in viewport |
| `zs` | Zoom to fit selection (time range of selected clips) |
| `zr` | Zoom to fit range selection |
| `Ctrl+=` / `Ctrl+-` | Fine zoom (smaller increments) |

**Vertical zoom** (track height):

| Key | Action |
|-----|--------|
| `Ctrl+Shift+=` | Increase all track heights |
| `Ctrl+Shift+-` | Decrease all track heights |
| `zA` | Toggle current track between compact/normal/expanded |
| `z=` | Equalize all track heights |

### 22.3 Zoom Presets

Zoom presets save and restore zoom level + scroll position:

| Key | Action |
|-----|--------|
| `:zoom save {n}` | Save current zoom state to preset `{n}` (1-9) |
| `:zoom {n}` | Recall zoom preset `{n}` |
| `z1` - `z9` | Quick recall zoom presets |

### 22.4 Follow Playhead

When playback is active, the viewport can optionally scroll to follow the playhead:

| Mode | Behavior |
|------|----------|
| **Off** | Viewport stays put; playhead may scroll off-screen |
| **Page** | When playhead reaches right edge, viewport jumps forward by one screenful |
| **Smooth** | Viewport scrolls continuously to keep playhead centered |

`:set follow off` / `page` / `smooth`

### 22.5 Overview / Minimap

An optional overview strip at the top of the arrangement shows the entire session at
reduced size. The current viewport is represented as a highlighted rectangle.

```
┌───────────────────────────────────────────────────────────────┐
│ ▓▓▓▓ overview of entire session ▓▓▓▓▓▓▓▓▓ [viewport] ▓▓▓▓▓ │
└───────────────────────────────────────────────────────────────┘
```

| Key | Action |
|-----|--------|
| `:set overview on/off` | Toggle minimap |
| Click on overview | Jump viewport to that position |

### 22.6 Center/Scroll Commands

| Key | Action |
|-----|--------|
| `zz` | Center current track vertically in viewport |
| `zt` | Scroll so current track is at top of viewport |
| `zb` | Scroll so current track is at bottom of viewport |
| `zh` | Center grid cursor horizontally in viewport |

---

## 23. Lane Versions

### 23.1 Concept

Lane versions allow any lane on a track to maintain multiple independent versions of
its content. A **lane** is either the clip lane (audio/MIDI clips) or an automation
lane (volume, pan, plugin parameter, etc.). Each lane has its own version stack,
and versions can be switched independently per lane.

This enables non-destructive experimentation: try a different vocal arrangement in
clip lane v2 while keeping the original in v1, or create an alternative volume
automation curve in v3, all without losing any prior work.

```
Track: Vocals
├── Clip lane        → v1 "Original"  |  v2 "Alt chorus"  |  v3 "Experimental"
├── Auto: Volume     → v1 "Default"   |  v2 "Dramatic"
├── Auto: Pan        → v1 (only one version)
└── Auto: EQ Freq    → v1 "Subtle"    |  v2 "Aggressive"
```

Each lane always has at least one version (v1). Additional versions are created
explicitly by the user.

### 23.2 Relationship to Takes

Lane versions and takes (§12) are orthogonal:

- **Takes** live *within* a clip lane version. They represent recording passes in a
  time region, created by loop or punch recording.
- **Lane versions** represent whole-lane alternatives — the entire clip layout,
  including any take lanes and comps.

Switching clip lane versions switches to a completely different set of clips, takes,
and comps:

```
Clip lane v1: [=====Verse 1=====][=====Chorus=====]
              ├── Take 1
              ├── Take 2
              └── Comp

Clip lane v2: [=====Verse 1=====][===Alt Chorus===][=Bridge=]
              ├── Take 1
              └── Take 2
```

### 23.3 Data Model

Each lane version stores:

| Field | Description |
|-------|-------------|
| `versionId` | Unique identifier |
| `name` | User-assigned name (optional, defaults to "v1", "v2", …) |
| `content` | Clip data (for clip lanes) or breakpoint data (for automation lanes) |
| `takes` | Take lanes and comp data (clip lanes only) |

The active version per lane is stored as a property on the lane itself. Inactive
versions are retained in the model but not rendered or played back.

### 23.4 Lane Version Commands

All commands operate on the currently focused lane (clip lane or automation lane).

| Command | Action |
|---------|--------|
| `:lv new` | Create a new empty lane version and switch to it |
| `:lv new "name"` | Create a new empty lane version with a name |
| `:lv dup` | Duplicate current lane version into a new version |
| `:lv dup "name"` | Duplicate current lane version with a name |
| `:lv {n}` | Switch to lane version `{n}` |
| `:lv rename "name"` | Rename current lane version |
| `:lv delete` | Delete current lane version (must have at least 2) |
| `:lv list` | List all versions for the current lane |

| Key | Action |
|-----|--------|
| `Alt+l` | Next lane version |
| `Alt+h` | Previous lane version |

### 23.5 Status Bar

When the focused lane has more than one version, the status bar shows the active
version:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ -- NORMAL -- │ [Slip] │ Grid: 1/8 │ Snap │ Track 3: "Vocals" │ LV:2 "Alt" │ …  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

The `LV:{n}` indicator is hidden when only one version exists (the common case).

### 23.6 Version Groups

Version groups coordinate lane version switches across multiple tracks and lanes.
A version group is a named collection of (track, lane, version) tuples. Activating
a group atomically switches all member lanes to their assigned versions.

Use cases:
- **Arrangement alternatives**: "Chorus A" vs "Chorus B" — different clip and
  automation versions across several tracks
- **Mix variations**: "Mix v1" vs "Mix v2" — different automation versions across
  the mixer
- **A/B comparison**: quickly toggle between two complete arrangements

```
Version group "Chorus B":
  Vocals   → Clip lane   → v2
  Drums    → Clip lane   → v3
  Bass     → Auto:Volume → v2
```

Lanes not included in a group are unaffected when the group is activated. Groups
target individual lanes selectively — they do not require all lanes on a track.

### 23.7 Version Group Commands

| Command | Action |
|---------|--------|
| `:lvgroup new "name"` | Create a new version group |
| `:lvgroup add {track} {lane} {version}` | Add a lane+version mapping to the group |
| `:lvgroup remove {track} {lane}` | Remove a lane from the group |
| `:lvgroup activate "name"` | Switch all member lanes to their assigned versions |
| `:lvgroup list` | List all version groups |
| `:lvgroup show "name"` | Show all mappings in a version group |
| `:lvgroup rename "old" "new"` | Rename a version group |
| `:lvgroup delete "name"` | Delete a version group |

**`{track}` identifier**: Track number or quoted track name (e.g., `3` or `"Vocals"`).

**`{lane}` identifier**: `clip` for the clip lane, or the automation parameter name
(e.g., `volume`, `pan`, `"EQ Freq"`).

**Examples**:
```
:lvgroup new "Chorus B"
:lvgroup add "Vocals" clip 2
:lvgroup add "Drums" clip 3
:lvgroup add "Bass" volume 2
:lvgroup activate "Chorus B"
```

### 23.8 Playback Behavior

- Only the **active version** of each lane is played back
- Switching lane versions during playback takes effect immediately (at the next
  audio buffer boundary)
- Activating a version group during playback switches all member lanes atomically
- Automation modes (§14) apply to the active version — recording automation writes
  to the active version only

---

## Appendix A: Default Key Reference

### Mode Entry/Exit

| Key | From → To |
|-----|-----------|
| `Escape` | Any → Normal |
| `v` | Normal → Visual |
| `V` | Normal → Visual-Line |
| `i` | Normal → Insert (context-dependent) |
| `:` | Normal → Command |
| `/` | Normal → Search |
| `q{char}` | Normal → Recording macro |

### Status Bar Indicators

```
┌──────────────────────────────────────────────────────────────────────────┐
│ -- NORMAL -- │ [Slip] │ Grid: 1/8 │ Snap │ Track 3: "Vocals" │ 4|2|0  │
└──────────────────────────────────────────────────────────────────────────┘
```

Segments: **Mode** | **Edit mode** | **Grid** | **Snap state** | **Cursor position** | **Time position**

---

## Appendix B: Command Mode Reference

All arrangement-related `:` commands in one place:

### Session
| Command | Action |
|---------|--------|
| `:w` | Save session |
| `:q` / `:wq` | Quit / save and quit |
| `:e {session}` | Open session |

### Tracks
| Command | Action |
|---------|--------|
| `:track "Name"` | New audio track |
| `:midi "Name"` | New MIDI track |
| `:bus "Name"` | New bus |
| `:folder "Name"` | New folder track |
| `:{n}` | Jump to track `{n}` |
| `:arm all` / `none` | Arm/disarm all tracks |

### Grid & Snap
| Command | Action |
|---------|--------|
| `:set grid {mode}` | Set grid mode |
| `:set snap on/off` | Toggle snap |
| `:set editmode {mode}` | Set edit mode |

### Tempo & Time
| Command | Action |
|---------|--------|
| `:set tempo {bpm}` | Set session tempo |
| `:tempo add {bpm}` | Add tempo event |
| `:tempo ramp {bpm}` | Add tempo ramp |
| `:set timesig {n}/{d}` | Set time signature |
| `:set timeformat {fmt}` | Set time display format |

### Clips
| Command | Action |
|---------|--------|
| `:clip rename "Name"` | Rename clip |
| `:clip move {time}` | Move clip to position |
| `:clip gain {dB}` | Set clip gain |
| `:clip color {color}` | Set clip color |
| `:clip fadein/fadeout {ms}` | Set fade length |
| `:clip timebase fixed/musical` | Set clip time mode |

### Automation
| Command | Action |
|---------|--------|
| `:auto add {param}` | Add automation lane |
| `:auto remove` | Remove automation lane |
| `:auto mode {mode}` | Set automation mode |
| `:auto draw/line/clear` | Drawing commands |
| `:auto suspend/resume` | Global bypass |

### Lane Versions
| Command | Action |
|---------|--------|
| `:lv new` / `:lv new "name"` | New lane version |
| `:lv dup` / `:lv dup "name"` | Duplicate lane version |
| `:lv {n}` | Switch to lane version |
| `:lv rename "name"` | Rename lane version |
| `:lv delete` | Delete lane version |
| `:lv list` | List lane versions |
| `:lvgroup new "name"` | Create version group |
| `:lvgroup add {track} {lane} {ver}` | Add to version group |
| `:lvgroup remove {track} {lane}` | Remove from version group |
| `:lvgroup activate "name"` | Activate version group |
| `:lvgroup list` | List version groups |
| `:lvgroup show "name"` | Show version group mappings |
| `:lvgroup rename "old" "new"` | Rename version group |
| `:lvgroup delete "name"` | Delete version group |

### Markers
| Command | Action |
|---------|--------|
| `:mark "Name"` | Set marker |
| `:mark delete {char}` | Delete marker |
| `:marks` | List markers |
| `:range "Name"` | Create range marker |
| `:section "Name"` | Create arrangement section |
| `:loop` / `:loop off` | Set/clear loop |

### Groups
| Command | Action |
|---------|--------|
| `:group edit "Name"` | Create edit group |
| `:group mix "Name"` | Create mix group |
| `:group delete "Name"` | Delete group |

### Processing
| Command | Action |
|---------|--------|
| `:freeze` / `:unfreeze` | Freeze/unfreeze track |
| `:bounce` | Bounce in place |
| `:bounce stem` | Bounce to new track |

### Import / Export
| Command | Action |
|---------|--------|
| `:import {path}` | Import audio/MIDI file |
| `:export` | Open export dialog |
| `:export {path}` | Export to file |
| `:export stems {dir}` | Export stems |

### Display
| Command | Action |
|---------|--------|
| `:set follow off/page/smooth` | Playhead follow mode |
| `:set overview on/off` | Toggle minimap |
| `:set waveform-display {mode}` | Waveform display mode |
| `:set midi-display {mode}` | MIDI clip display mode |
| `:set show-transients on/off` | Transient markers |
| `:set crossfade-length {ms}` | Default crossfade |
| `:set auto-crossfade on/off` | Auto crossfade |
| `:set stretch-algorithm {alg}` | Time stretch algorithm |
| `:set import-mode copy/reference` | File import behavior |
| `:set monitor auto/on/off` | Input monitoring |
| `:set preroll {bars}` | Pre-roll length |
| `:set countin {bars}` | Count-in length |
| `:zoom save {n}` / `:zoom {n}` | Zoom presets |
