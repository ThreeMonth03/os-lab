#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/../.." && pwd)
log_path=${1:-"${repo_root}/build/logs/display-profile.log"}

if [[ ! -f "${log_path}" ]]; then
    printf 'display profile log not found: %s\n' "${log_path}" >&2
    exit 1
fi

awk '
function flush_command() {
    if (command == "") {
        return
    }
    printf "%-46s %8s %8s %8s %8s %14s %14s %14s %14s %14s %14s %14s %14s %14s %8s\n", \
        command, elapsed, operations, presenter_calls, scrolls, pixels, terminal_copy_pixels, scene_scroll_pixels, scene_move_pixels, scene_backing_pixels, front_scroll_pixels, window_repaint_pixels, move_repaint_pixels, visual_repaint_pixels, preview_repaint_pixels, fallback
}

{
    gsub(/\r/, "")
}

BEGIN {
    printf "%-46s %8s %8s %8s %8s %14s %14s %14s %14s %14s %14s %14s %14s %14s %8s\n", \
        "command", "ticks", "ops", "calls", "scrolls", "pixels", "term_copy", "scene_scroll", "scene_move", "scene_backing", "front_scroll", "window_repaint", "move_repaint", "visual_repaint", "preview_repaint", "fallback"
}

/^os-lab display profile: command=/ {
    flush_command()
    command = $0
    sub(/^os-lab display profile: command=/, "", command)
    elapsed = operations = presenter_calls = scrolls = pixels = terminal_copy_pixels = scene_scroll_pixels = scene_move_pixels = scene_backing_pixels = front_scroll_pixels = window_repaint_pixels = move_repaint_pixels = visual_repaint_pixels = preview_repaint_pixels = fallback = 0
    next
}

/^  elapsed ticks:/ {
    elapsed = $3
    next
}

/^  present operations:/ {
    operations = $3
    next
}

/^  presenter present calls:/ {
    presenter_calls = $4
    next
}

/^  present scrolls:/ {
    scrolls = $3
    next
}

/^  presented pixels:/ {
    pixels = $3
    next
}

/^  scene scroll copy pixels:/ {
    scene_scroll_pixels = $5
    next
}

/^  scene move copy pixels:/ {
    scene_move_pixels = $5
    next
}

/^  terminal backing copy pixels:/ {
    terminal_copy_pixels = $5
    next
}

/^  scene compose from backing pixels:/ {
    scene_backing_pixels = $6
    next
}

/^  presenter front-scroll pixels:/ {
    front_scroll_pixels = $4
    next
}

/^  window repaint pixels:/ {
    window_repaint_pixels = $4
    next
}

/^  window move repaint pixels:/ {
    move_repaint_pixels = $5
    next
}

/^  window visual repaint pixels:/ {
    visual_repaint_pixels = $5
    next
}

/^  window preview repaint pixels:/ {
    preview_repaint_pixels = $5
    next
}

/^  repaint plan fallback count:/ {
    fallback = $5
    next
}

END {
    flush_command()
}
' "${log_path}"
