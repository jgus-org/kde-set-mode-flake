Nix flake providing **kde-set-mode**, a command-line tool that sets a KWin output's resolution and refresh rate through KDE Plasma's `kde-output-management-v2` Wayland protocol. The tool is first-party C source carried in this flake and built against nixpkgs; there is no upstream project.

## Outputs

- `packages.<system>.kde-set-mode` — the package, installing the `kde-set-mode` binary at `bin/kde-set-mode`.
- `packages.<system>.default` — the same package.

Packages are the flake's only outputs, and `x86_64-linux` is the only system it defines them for; the package's `meta.platforms` is `[ "x86_64-linux" ]`.

Consume the package from a flake input:

```nix
kde-set-mode.packages.${system}.kde-set-mode
```

Or build it directly:

```bash
nix build "github:jgus-org/kde-set-mode-flake#default"
```

## Running it

```
kde-set-mode <output> <width> <height> <fps>
```

```bash
kde-set-mode Virtual-0 2560 1440 60
```

`<output>` is the name KWin gives the output, matched exactly. Width, height, and fps are positive whole numbers; refresh is expressed in whole hertz. On success the tool prints `ok: <output> -> <width>x<height>@<fps>` and exits 0. Malformed arguments exit 2; a failed lookup or apply exits 1 with a message naming what failed.

## Runtime requirements

It works only against a running KDE Plasma 6 KWin session on Wayland that advertises the `kde-output-management-v2` global, and it connects to the Wayland display of the session it is run in. Run outside that session's environment it reports `cannot connect to wayland display`; against a compositor that does not implement the protocol it reports `kde_output_management_v2 not available`. Both exit 1.

Outputs are addressed by the name KWin reports for them, matched exactly, so the names this tool takes are the ones in KWin's own output listing. A name KWin does not report is rejected as `output '<name>' not found`. A name such as `Virtual-0` is a virtual output, which exists only in a headless or virtual session.

The change lands on the live session. If the output does not already advertise the requested mode, the tool adds it as a custom mode — with reduced blanking disabled — and then selects it; an output already advertising a mode of that size within 1 Hz of the requested refresh has that mode selected instead. A mode the compositor refuses is reported as an apply failure. The tool never reverts a change it applied.

Mode setting is all it does: it changes resolution and refresh rate, and nothing else about an output.
