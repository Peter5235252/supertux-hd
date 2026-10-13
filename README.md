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
> too** via bundled MoltenVK (default) and KosmicKrisp (opt-in on Apple
> Silicon + macOS 26+) ICDs — no separate Metal or DX renderer. **macOS status:
> the game boots and renders via MoltenVK** (verified with a 600-frame
> screenshot run on CI's macOS 15 runner and on a Hackintosh ThinkPad T480 with
> Intel HD 620). Those earlier packages used stock textures. **New macOS
> builds bundle the HD texture set**, with HD-specific CI tests described
> below. **KosmicKrisp loads but has failed GPU enumeration in CI**;
> it remains opt-in, not a verified rendering path. The T480 now runs
> CachyOS; macOS testing continues on GitHub's runners. HDR output
> groundwork is included. This rewrite is **very fresh: expect some
> instability and graphical glitches**. Changes are logged in
> [CHANGELOG-FORK.md](CHANGELOG-FORK.md). Original license: GPL-3.0
> ([LICENSE.txt](LICENSE.txt), unmodified); game data: CC-BY-SA.
> The original upstream README is preserved below the fork-specific sections.

---

## Known issues of this fork

* **White boxes instead of background clouds (HD texture set, cause
  unverified).** With the AI-upscaled `data-hd/` textures, some large
  transparent backgrounds have rendered as opaque white rectangles
  instead of soft clouds. This was previously blamed on the texture
  files, but the root cause was never conclusively diagnosed — and the
  earlier macOS CI / ThinkPad tests used stock textures. New macOS CI
  tests load the bundled HD clouds and capture comparison screenshots. If you
  reproduce it, report a screenshot, the level name, and the commit hash
  from the window title; per-file upscale records live in
  `data-hd/images/UPSCALE_LOG.txt`.
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
- **MoltenVK is the default on all Macs.** To explicitly test KosmicKrisp:
  `SUPERTUX_MAC_VULKAN_DRIVER=kosmickrisp /path/to/SuperTux.app/Contents/MacOS/SuperTux`.
  This requires an arm64 process on macOS 26+; it does not silently fall back
  to MoltenVK. `auto` or `moltenvk` selects the compatibility path.
- [macOS HD smoke tests](https://github.com/Peter5235252/supertux2-hd/actions/workflows/hd-mac-test.yml)
  test the exact build artifact after moving it to a path containing spaces:
  HD menu, HD clouds in *Welcome to Antarctica*, FSR, and HD-off fallback.
  Logs must show actual HD decoding, the expected ICD, clean exit and screenshots.
  MoltenVK runs on macOS 15 and 26; KosmicKrisp has a separate strict test.
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
