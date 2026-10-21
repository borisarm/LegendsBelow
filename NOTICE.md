# Licensing notice — LegendsBelow

## Source code

Copyright © 2026 Boris Armenta. **All rights reserved.**

This repository is published so the work can be read and reviewed. No license — express or
implied — is granted to use, copy, modify, merge, publish, redistribute, sublicense or create
derivative works from the source code, the game data under `Assets/Data/`, or the documentation,
except as stated below for third-party material.

If you want to use any part of it, ask first: boris@barquillo.net.

## Third-party assets

These are **not** mine and are **not** covered by the paragraph above. They are used under their
own licenses, and the license file for each set sits next to the assets.

| Asset set | Author | License |
|---|---|---|
| 3D models and animations — KayKit Dungeon Pack, Skeletons, Adventurers, Character Animations, Mystery Monthly characters | Kay Lousberg (kaylousberg.com) | CC0 1.0 |
| Sound effects — Impact Sounds, RPG Audio | Kenney (kenney.nl) | CC0 1.0 |
| Menu frames — Fantasy UI Borders | Kenney (kenney.nl) | CC0 1.0 |
| Font — MedievalSharp | wmk69 | SIL Open Font License 1.1 |
| Music — "Mystical Theme" | Alexandr Zhelanov (soundcloud.com/alexandr-zhelanov) | **CC-BY 3.0** |
| Music — "Battle Theme A" | cynicmusic (pixelsphere.org) | CC0 1.0 |
| Music — "Menu Music" | mrpoly | CC0 1.0 |
| Dungeon ambience and creature voices | made for LegendsBelow | CC0 1.0 |

CC0 material carries no obligations, but it is credited anyway (see `Assets/Data/credits.txt`
and `Assets/Music/CREDITS.txt`).

**The CC-BY 3.0 track does carry an obligation.** Any build of the game, source or binary, must
keep the credit to Alexandr Zhelanov visible in the game's credits screen. Do not remove it.

## Third-party libraries

The build resolves its dependencies through vcpkg (`vcpkg.json`): directx-dxc, directx-headers,
directx12-agility, directxmath, directxtk12, drlibs, dstorage, fastgltf, gameinput, ms-gdk,
nlohmann-json, proxy, stb and winpixevent. Each one is distributed under its own upstream license
(mostly MIT or public domain, plus the Microsoft GDK and Agility SDK terms). Those licenses govern
their respective code; nothing here overrides them.

## Microsoft GDK

Building and distributing Windows and Xbox binaries with the Microsoft GDK is governed by the
Microsoft GDK license terms, independently of this notice.
