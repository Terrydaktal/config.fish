# Fish Shell Configuration Summary

* A consistent **Java 21** environment (sets `JAVA_HOME` and adds `$JAVA_HOME/bin` to `PATH` so Java tools resolve cleanly).

* A secret **`PASSGEN_PEPPER`** exported to all programs launched from your shell (so your passgen tooling can use it without prompting).

* A larger Fish history capacity (**50,000** commands) for deeper recall and reuse.

* Customized **terminal color rules** via `LS_COLORS` (distinct styling for symlinks/orphans/missing targets and common extensions like `.py/.js/.sh/.cpp/.txt`).

* A PATH that prioritizes your user tooling (`~/.local/bin`) and Rust binaries (`~/.cargo/bin`) without needing absolute paths.

* Stronger safety defaults for file operations:

  * `cp -i` / `mv -i` prompts before overwriting.
  * `rm` routes to your `trash` script (non-destructive deletes by default).
  * `sudo rm ...` is intercepted and redirected to `sudo trash ...` to avoid accidental root-level permanent deletion.

* Faster, bounded directory inspection defaults:

  * `dust -r -d 1` for a shallow recursive size summary.
  * `tree -F -L 2 --filelimit 20` to prevent huge, noisy tree output.

* Automatic, per-command **timestamps** printed after every command completes (useful for session auditing and timing).

* A Ctrl+Backspace behavior optimized for both shell editing and full-screen apps:

  * Deletes the previous word in Fish when editing a prompt,
  * but passes through Ctrl+W when the prompt is empty so applications can still use it.

* A GPU power policy tweak applied on shell startup (sets NVIDIA `GPUPowerMizerMode=1`, output suppressed).

* A DPI-scaled launcher for **PCManFM** (wraps `pcmanfm` with `GDK_DPI_SCALE=1.5` to increase UI scale).

---

## Scope and Execution Model

### Interactive-only block

Everything inside:

```fish
if status is-interactive
    ...
end
```

runs **only when Fish is attached to an interactive terminal** (your prompt). It does **not** apply to non-interactive shells (e.g., scripts that run `fish -c ...`, cron jobs, etc.), which prevents your “quality of life” settings from breaking automation.

### Lines outside the interactive guard

These execute whenever this file is sourced, including contexts where Fish might not be fully interactive, depending on how/when the config is loaded:

```fish
nvidia-settings ...
alias pcmanfm=...
```

In practice, `config.fish` is typically sourced for interactive shells, but these being outside the guard means they’re not explicitly restricted.

---

## Environment Variables

### `JAVA_HOME`

```fish
set -gx JAVA_HOME /usr/lib/jvm/java-21-openjdk-amd64
```

* Sets a globally-exported Java home pointing at OpenJDK 21.
* Used by Java tooling (Gradle, Maven, IDEs, `javac`, etc.) to locate the JVM and related binaries/libraries.

### `PASSGEN_PEPPER`

```fish
set -gx PASSGEN_PEPPER "REDACTED"
```

* Exports a secret “pepper” value for your `passgen` tooling.
* A *pepper* is typically an extra secret combined with passwords/inputs (e.g., in hashing or key derivation).
* Any program launched from Fish inherits it, so treat it like a secret credential.

### Fish history size

```fish
set -gx fish_history_limit 50000
```

* Increases Fish’s history limit to 50,000 entries.
* Useful for recall of long command workflows.

### `LS_COLORS` extensions

```fish
set -gx LS_COLORS "$LS_COLORS:ln=01;36:or=01;31:mi=01;31:*.txt=01;36:*.py=01;32:*.js=01;33:*.cpp=01;31:*.sh=01;35:"
```

* Extends existing `LS_COLORS` to apply custom ANSI color styles for:

  * `ln` (symlinks) → cyan (`01;36`)
  * `or` (orphan symlinks) → red (`01;31`)
  * `mi` (missing file) → red (`01;31`)
  * File extensions:

    * `*.txt` cyan
    * `*.py` green
    * `*.js` yellow
    * `*.cpp` red
    * `*.sh` magenta
* Affects `ls` coloring (and any tool honoring `LS_COLORS`, e.g., `dircolors`-compatible tools).

---

## PATH Management

```fish
fish_add_path ~/.local/bin
fish_add_path ~/.cargo/bin
test -d "$JAVA_HOME/bin"; and fish_add_path "$JAVA_HOME/bin"
```

* Adds user-level executables first-class into PATH:

  * `~/.local/bin` for personal scripts (e.g., your `trash` script).
  * `~/.cargo/bin` for Rust-installed binaries.
* Conditionally adds Java’s `bin` directory if it exists, so `java`, `javac`, etc. are discoverable without typing absolute paths.
* `fish_add_path` is Fish-native and generally avoids duplicate PATH entries.

---

## Safety / Convenience Aliases

### Interactive prompts for overwrites

```fish
alias cp 'cp -i'
alias mv 'mv -i'
```

* Adds interactive confirmation before overwriting existing destination files.
* Reduces accidental clobbering during manual copy/move operations.

### `dust` defaults

```fish
alias dust 'dust -r -d 1'
```

* Runs `dust` recursively with max depth 1 (a quick “what’s big here” summary per directory).
* Intended for fast disk usage inspection.

### `rm` redirected to trash

```fish
alias rm '/home/lewis/.local/bin/trash'
```

* Replaces destructive deletes with your local `trash` script.
* This affects **only your interactive Fish usage** (not scripts that call `/bin/rm` directly, and not other shells unless they also alias it).
* The comment “do NOT use --save here” suggests your `trash` script has a persistent mode you intentionally avoid for normal interactive use.

### Bounded `tree`

```fish
alias tree 'tree -F -L 2 --filelimit 20'
```

* Makes `tree` safer and faster by default:

  * `-F` appends indicators (e.g., `/` for directories).
  * `-L 2` limits recursion to depth 2.
  * `--filelimit 20` prevents huge output.

---

## `sudo` Wrapper: Intercepting `sudo rm ...`

```fish
function sudo
    if test (count $argv) -ge 1; and test "$argv[1]" = "rm"
        command sudo /home/lewis/.local/bin/trash $argv[2..-1]
    else
        command sudo $argv
    end
end
```

Purpose:

* Prevents accidental permanent deletion when you type `sudo rm ...`.
* If the first argument after `sudo` is exactly `rm`, it runs:

  * `sudo trash <rest-of-args>` instead of `sudo rm ...`.

Mechanics:

* `command sudo` calls the real `sudo` binary (avoids recursion).
* `$argv[2..-1]` passes through everything after the `rm`.

Important caveat:

* This only triggers for the literal form `sudo rm ...`.
* It does **not** catch other patterns like:

  * `sudo -u root rm ...` (where `$argv[1]` is `-u`)
  * `sudo env X=1 rm ...`
  * `command sudo rm ...` (depends how Fish expands/execs)
    If you rely on this for safety, be aware those variants bypass the check.

---

## Post-command Timestamp Hook

```fish
function show_timestamp_after_command --on-event fish_postexec
    set_color grey
    date "+[%d/%m/%y %H:%M:%S]"
    set_color normal
end
```

* Fish event handler runs after each command completes (`fish_postexec`).
* Prints a grey timestamp like:

  * `[28/01/26 20:53:41]`
* This gives you a cheap execution timeline in your scrollback, helpful for:

  * correlating logs/actions
  * benchmarking “how long did that take”
  * auditing interactive sessions

---

## Keybindings

### Smart Ctrl+Backspace

```fish
function smart_ctrl_backspace
    set cmd (commandline)

    if string match -qr '.*\n.*' -- "$cmd"
        commandline -f backward-kill-word
    else if test -n "$cmd"
        commandline -f backward-kill-word
    else
        commandline -i -- "\x17"
    end
end

bind \b smart_ctrl_backspace
```

Goal:

* Make Ctrl+Backspace behave sensibly in Fish **without breaking applications** that expect Ctrl+W.

What it does:

* Reads the current commandline buffer.
* If there is content, it performs Fish’s `backward-kill-word` (delete previous word).
* If the commandline is empty, it inserts `\x17` (Ctrl+W) into the input stream, effectively “passing through” so full-screen terminal apps (editors, fzf, etc.) still receive the key they expect.

Note:

* The newline check (`.*\n.*`) and the “has text” check both currently do the same action (kill word). The real distinction is the “empty buffer” fallback which passes Ctrl+W through.

### Ctrl+Shift+C → cancel

```fish
bind \C-C 'commandline -f cancel'
```

* Maps Ctrl+C to cancel the current commandline input (like a typical SIGINT in interactive shells).
* In Fish, `commandline -f cancel` clears the current line / cancels the edit state.

(Pragmatically, this ensures Ctrl+C reliably resets the prompt input state.)

---

## System/UI Tweaks

### NVIDIA PowerMizer mode (GPU performance policy)

```fish
nvidia-settings -a "[gpu:0]/GPUPowerMizerMode=1" > /dev/null 2>&1
```

* Runs `nvidia-settings` to set GPU power/performance mode.
* `> /dev/null 2>&1` suppresses all output/errors.
* Because it’s outside the interactive guard, it will run whenever this config is sourced. Typically that means “every new Fish shell”, which can repeatedly apply the setting.

(Exact meaning of `GPUPowerMizerMode=1` depends on the NVIDIA driver, but it’s essentially selecting a specific power/performance profile.)

### PCManFM DPI scaling

```fish
alias pcmanfm='env GDK_DPI_SCALE=1.5 pcmanfm'
```

* Launches `pcmanfm` with `GDK_DPI_SCALE=1.5` set for that process.
* This scales UI elements (useful for HiDPI or preference).
* Only affects `pcmanfm` launched via this alias; doesn’t change global desktop settings.

---
