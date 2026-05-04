# Konvolute Manual

This manual summarizes keyboard shortcuts and gestural controls for performance and onboarding.

## Quick Start

1. Press `o` to load a points JSON or composition JSON.
2. Press `b` to enter Browse mode.
3. Press `m` to enable video mode.
4. Hold mouse down in Browse mode to audition points.

## Keyboard Shortcuts

### Modes

- `n`: Navigate mode
- `f`: Freehand Draw mode
- `w`: Wander mode (auto-path from nearest point)
- `b`: Toggle Browse mode

### Global Playback and Paths

- `Space`: Play/pause selected path
- `Enter`: Toggle global playback (all paths)
- `q`: Create sequential path from points
- `e`: Toggle step mode for selected sequential path
- `j`: Toggle jitter mode on selected path
- `l`: Toggle default path mode (LOOP/ONCE)
- `y`: Toggle ping-pong on selected/current path
- `k`: Refresh selected path (`path-0` if none selected)
- `+` or `=`: Increase sample layer count on selected path
- `-` or `_`: Decrease sample layer count on selected path

### Path Selection and Editing

- `c`: Deselect current path
- `Delete` or `Backspace`: Delete selected path
- `Shift + Left Arrow`: Select previous path
- `Shift + Right Arrow`: Select next path
- `Shift + Down Arrow`: Deselect all paths
- `Up Arrow`: Increase speed on selected path (or `path-0`)
- `Down Arrow`: Decrease speed on selected path (or `path-0`)

### Data and Session

- `o`: Load points/composition file
- `s`: Save composition
- `x`: Clear all paths and points
- `z`: Zoom to extents
- `0`: Pull all annotation labels inside point bounds

### Video

- `m`: Toggle video mode on/off
- `M` (`Shift + m`): Toggle video routing on selected path (or `path-0` if none selected)
- `;`: Toggle video trigger lock/unlock

### Display and UI

- `t`: Toggle text labels
- `i`: Toggle title (when neighbour mode is OFF)
- `h`: Toggle help overlay
- `,`: Toggle settings panel
- `d`: Toggle debug display
- `p`: Toggle secondary projector fullscreen window

### Point Cloud Projections and Clusters

- `1`: Cloud mode LOCAL
- `2`: Cloud mode MID
- `3`: Cloud mode GLOBAL
- `4`: Cycle third dimension display
- `[`: Previous cluster filter
- `]`: Next cluster filter
- `\\`: Clear cluster filter

### Neighbour Mode

- `N` (`Shift + n`): Toggle neighbour mode
- `i` (lowercase): Play selected point's neighbours sequentially (only when neighbour mode is ON)
- `u` or `U`: Trigger all selected point neighbours once (only when neighbour mode is ON)

Notes:

- Neighbour playback volume is distance-weighted: near neighbours are louder than far neighbours.
- In movie mode, Browse and neighbour-sequence triggers attempt to trigger matching video clips.

### Annotation Mode

- `Tab`: Toggle annotation edit mode
- `Shift + Tab`: Toggle annotation visibility

When annotation mode is ON:

- Click empty space: create a new annotation and start typing
- Click a label box: drag label box
- Click a curve: edit label text
- `Delete` or `Backspace` while hovering a label/curve: delete non-cluster annotation

Typing controls:

- `Enter`: confirm/finish editing
- `Esc`: confirm/finish editing
- `Shift + Enter`: insert line break
- `Backspace`: delete previous character

## Mouse and Gestural Controls

### Navigate Mode

- Left drag: pan view
- `Shift + drag`: marquee zoom box

### Draw Mode

- Click/drag: draw freehand path
- Release: finalize path (very short paths are discarded)

### Browse Mode

- Hold mouse button: audition nearest point continuously
- Click near a path and drag: move that path

### Gesture Recording and Scrub

- `Alt + click` on selected path: begin gesture recording
- `Alt + drag`: record gesture (position and volume over time)
- `Alt + move`: scrub selected path position by X and volume by Y

### Drag Modifiers

- Hold `r` then drag vertically: adjust selected path radius
- Hold `v` then drag vertically: adjust speed (selected path) or volume (`path-0` fallback)

## Additional Behavior Notes

- Grid is drawn above video and below points.
- Grid is hidden when no point cloud/composition is loaded.
- Zoom and pan transitions are animated.
- Zoom-to-extents includes annotation bounds only when annotations are visible.
