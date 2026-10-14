# SuperTux 2 HD (graphical-enhancement fork of SuperTux)

> **This is a fan-made graphical enhancement of
> [SuperTux](https://github.com/SuperTux/supertux) — the © 2003–2026
> SuperTux Team classic, written by Bill Kendrick, Ingo Ruhnke, and
> contributors (see [ATTRIBUTION.md](ATTRIBUTION.md)).**
> It targets the modern `supertux2` engine generation (0.5.0+, colloquially
> "SuperTux 2") and replaces blurry textures with **AI-upscaled 4K
> equivalents at the exact same world size** (see
> [AI_DISCLOSURE.md](AI_DISCLOSURE.md)).
>
> **The Vulkan migration is complete: the renderer is Vulkan via SDL_GPU
> on Windows and Linux, and OpenGL is fully gone** (SDL and OpenGL
> backends deleted; legacy names still map to Vulkan). **macOS gets Vulkan
> too** via bundled KosmicKrisp (preferred on Apple Silicon + macOS 26+)
> and MoltenVK (fallback, forced on Intel Macs and older macOS) ICDs —
> no separate Metal or DX renderer. **macOS status:
> the game boots and renders via MoltenVK** (verified with a 600-frame
> screenshot run on CI's macOS 15 runner and on a Hackintosh ThinkPad T480 with
> Intel HD 620). Those earlier packages used stock textures. **New macOS
> builds bundle the HD texture set**, with HD-specific CI tests described
> below. **KosmicKrisp is now the default on modern Apple Silicon but has
> failed GPU enumeration in CI**; it is not yet a verified rendering path.
> The T480 now runs
> CachyOS; macOS testing continues on GitHub's runners. HDR output
> groundwork is included. This rewrite is **very fresh: expect some
> instability and graphical glitches**. Changes are logged in
> [CHANGELOG-FORK.md](CHANGELOG-FORK.md). Original license: GPL-3.0
> ([LICENSE.txt](LICENSE.txt), unmodified); game data: CC-BY-SA.
> The original upstream README is preserved below the fork-specific sections.

## Download and play (test builds)

Rolling builds from `main`, published as
[`hd-test-build`](https://github.com/Peter5235252/supertux2-hd/releases/tag/hd-test-build)
release assets. Paste **one** block for your system; it downloads the right
package, extracts it, and starts the game. macOS ships the HD texture set
(3,867 textures); Linux/Windows currently use stock textures.

**Linux or macOS (Terminal):**

```sh
mkdir -p ~/SuperTux2-HD && cd ~/SuperTux2-HD \
&& if [ "$(uname -s)" = Darwin ]; then ASSET=supertux-hd-macos-universal.tar.gz; else ASSET=supertux-hd-linux-portable.tar.gz; fi \
&& curl -fL --retry 3 -o "$ASSET" "https://github.com/Peter5235252/supertux2-hd/releases/download/hd-test-build/$ASSET" \
&& tar -xzf "$ASSET" \
&& if [ "$(uname -s)" = Darwin ]; then xattr -dr com.apple.quarantine SuperTux.app && open SuperTux.app; \
else BIN=$(find "$PWD" -name supertux2 -type f | head -1) && "$BIN"; fi
```

**Windows (PowerShell):**

```powershell
mkdir "$env:USERPROFILE\SuperTux2-HD" -Force; cd "$env:USERPROFILE\SuperTux2-HD"; Invoke-WebRequest -Uri "https://github.com/Peter5235252/supertux2-hd/releases/download/hd-test-build/supertux-hd-windows-portable.zip" -OutFile "supertux-hd-windows-portable.zip"; Expand-Archive -Path "supertux-hd-windows-portable.zip" -DestinationPath "." -Force; $exe = (Get-ChildItem -Recurse -Filter supertux2.exe | Select-Object -First 1).FullName; Start-Process $exe
```

Notes: all three builds render with Vulkan, not OpenGL or native
DirectX/Metal — the macOS build speaks Vulkan through a bundled
Vulkan-on-Metal driver: KosmicKrisp is preferred on Apple Silicon +
macOS 26+, MoltenVK is the fallback everywhere else and is forced on
Intel Macs. `SUPERTUX_MAC_VULKAN_DRIVER` overrides with `moltenvk`,
`kosmickrisp`, or `auto`. On Linux the game also needs SDL3 +
OpenAL system libraries — if it reports a missing `.so`, install your
distro's `sdl3`/`openal` packages. On Windows, SmartScreen may flag the
unsigned build: More info → Run anyway. These are unsigned test builds,
not stable releases.

---

## Known issues of this fork

* **Background transparency in older HD sets.** The packaged texture set
  v1 restores original alpha in 18 images, including the fully transparent
  `transparent_up.png` that had become opaque. Local Vulkan gameplay
  captures with the repaired set show clouds without the white rectangle,
  with FSR both off and on. New macOS HD tests cover the same scene;
  results are tracked in CI. Other levels still need visual review.
* Fresh rewrite overall: expect some instability and graphical glitches
  elsewhere too; please report them with a screenshot, the level name,
  and the commit hash from the window title.

## macOS HD packages and validation

- The macOS packaging workflow downloads the versioned
  [HD texture release](https://github.com/Peter5235252/supertux2-hd/releases/tag/hd-textures-v1),
  verifies its pinned SHA-256, and bundles all **3,867 textures** plus
  scale metadata inside `SuperTux.app/Contents/Resources/share/data-hd/`.
  Original assets remain alongside it for the HD-off setting and fallback.
- File checksums, original dimensions, scale factors and alpha-channel
  presence are checked before packaging. The release restores source alpha
  in 18 previously opaque textures, recorded in `ALPHA_REPAIRS.json`.
  Creator credits, per-asset licensing and AI-use disclosure ship with it.
- **KosmicKrisp is preferred on Apple Silicon + macOS 26+; MoltenVK is
  the fallback and is forced on Intel Macs and older macOS.** Override
  with `SUPERTUX_MAC_VULKAN_DRIVER`: `moltenvk` forces the fallback,
  `kosmickrisp` forces KosmicKrisp (fails loudly when unsupported),
  `auto` (default) picks per machine. To force a driver:
  `SUPERTUX_MAC_VULKAN_DRIVER=moltenvk /path/to/SuperTux.app/Contents/MacOS/SuperTux`.
- [macOS HD smoke tests](https://github.com/Peter5235252/supertux2-hd/actions/workflows/hd-mac-test.yml)
  test the exact build artifact after moving it to a path containing spaces:
  HD menu, HD clouds in *Welcome to Antarctica*, FSR, and HD-off fallback.
  Logs must show actual HD decoding, the expected ICD, clean exit and screenshots.
  MoltenVK runs on macOS 15 and 26 (explicit fallback on 26); KosmicKrisp is
  the default on macOS 26 with a separate strict test.
  Screenshots still require visual review; CI is not proof that all GPUs or HDR displays work.
- Linux/Windows portable packages currently use stock textures unless an HD
  overlay is installed separately. This change only bundles HD assets on macOS.

---

[![Windows](https://github.com/SuperTux/supertux/actions/workflows/windows.yml/badge.svg?branch=master)](https://github.com/SuperTux/supertux/actions/workflows/windows.yml?branch=master)
[![MacOS](https://github.com/SuperTux/supertux/actions/workflows/macos.yml/badge.svg?branch=master)](https://github.com/SuperTux/supertux/actions/workflows/macos.yml?branch=master)
[![GNU/Linux](https://github.com/SuperTux/supertux/actions/workflows/gnulinux.yml/badge.svg?branch=master)](https://github.com/SuperTux/supertux/actions/workflows/gnulinux.yml?branch=master)
[![Android](https://github.com/SuperTux/supertux/actions/workflows/android.yml/badge.svg?branch=master)](https://github.com/SuperTux/supertux/actions/workflows/android.yml?branch=master)
[![FreeBSD](https://github.com/SuperTux/supertux/actions/workflows/freebsd.yml/badge.svg?branch=master)](https://github.com/SuperTux/supertux/actions/workflows/freebsd.yml?branch=master)
[![WebAssembly](https://github.com/SuperTux/supertux/actions/workflows/wasm.yml/badge.svg?branch=master)](https://github.com/SuperTux/supertux/actions/workflows/wasm.yml?branch=master)
[![Github All Releases](https://img.shields.io/github/downloads/supertux/supertux/total.svg?maxAge=2592000)](https://github.com/SuperTux/supertux)

SuperTux is a jump'n'run game with strong inspiration from the
Super Mario Bros. games for the various Nintendo platforms.

Run and jump through multiple worlds, fighting off enemies by jumping
on them, bumping them from below or tossing objects at them, grabbing
power-ups and other stuff on the way.

![Screenshot](https://www.supertux.org/images/0_7_0/github_preview.png)


## Story: Penny gets captured!

Tux and Penny were out having a nice picnic on the ice fields of
Antarctica. Suddenly, a creature jumped from behind an ice bush, there
was a flash, and Tux fell asleep!

When Tux wakes up, he finds that Penny is missing. Where she lay
before now lies a letter:
>Tux, my arch enemy! I have captured your beautiful Penny and have
>taken her to my fortress. The path to my fortress is littered with my
>minions. Give up on the thought of trying to reclaim her, you haven't
>got a chance!
>
>-Nolok

Tux looks and sees Nolok's fortress in the distance. Determined to
save his beloved Penny, he begins his journey.

## Installation

For major platforms, stable releases are built and available for download from
[supertux.org](https://www.supertux.org/download.html) or alternatively directly
from [GitHub](https://github.com/SuperTux/supertux/releases). You should be able
to install these using default tools provided by your platform. On macOS, when
Gatekeeper is enabled (default) it will refuse to open SuperTux. This is due to
the lack of a signature on the application. If you wish to open SuperTux anyway
without disabling the Gatekeeper feature entirely, you can open the application
from the context menu (control click on the icon). macOS will then remember your
choice the next time.

## Documentation

Important documentation for SuperTux is contained in multiple files.
Please see them:

* `INSTALL.md` - Requirements, compiling and installing.
* `README.md` - This file
* `NEWS.md` - Changes since the previous versions of SuperTux.
* `LICENSE.txt` - The GNU General Public License, under whose terms SuperTux is
licensed. (Most of the data subdirectory is also licensed under
CC-by-SA)
* `data/credits.stxt` - Credits for people that contributed to the creation of
SuperTux. (You can view these in the game menu as well.)


## Playing the game

Both keyboards and joysticks/gamepads are supported. You can view/change the
controls via **options > controls**. Basically, the only controls you will need
to use in-game are the following: jump, duck, move left & right, do an action,
and use the item pocket. An "action" allows you to pick up objects and use any
powerup you have, such as shooting fireballs with the fire flower, or to fire
ice pellets with the ice flower. The item pocket allows you to store a single
additional item, which can be useful in dire situations.

Other useful keys include the Esc key, which is used to go to the menu
or to go up a level in the menu. The menu can be navigated using the
arrow keys or the mouse.

## Community

In case you need help, feel free to reach out using the following means:

* **IRC:** [#supertux](ircs://irc.libera.chat/#supertux) on
  [Libera Chat](https://libera.chat) hosts most of the discussions between
  developers. Also, real-time support can be provided here. If you don't know
  how to use an IRC client, you access the channel using a web-based
  [client](https://kiwiirc.com/nextclient/irc.libera.chat:+6697/?nick=Guest?#supertux).
  Please stay around after asking questions, otherwise you will be disconnected
  and might miss potential answers.
* **[Forum](https://groups.f-hub.org/supertux):** The SuperTux
  community is also active on the forum, the discussions range from feature
  proposals to support questions. In particular, most community-contributed
  add-ons are published there, so this is worth checking.
* **Social Media:** Mostly on [Twitter](https://twitter.com/supertux_team) at
  the moment.
* **Discord:** Also, you can join our [Discord server](https://discord.com/invite/AcvtHWz) to get in touch with us.
