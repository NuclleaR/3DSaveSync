# 3DSaveSync — Design

Date: 2026-11-12
Status: approved by the owner 2026-11-12; implementation in progress
(`docs/superpowers/plans/2026-11-12-3dsavesync.md`). Updated with the decisions made during
implementation (cloud layout, upload protocol, restore rules, research results).

## Intent

The owner has twice lost every 3DS save to cheap SD cards that failed. 3DSaveSync is a homebrew app
for the owner's single 3DS that backs up game saves to Koofr cloud storage over WebDAV, keeps every
version, and lets the owner fetch any version back so it can be restored.

It is a backup-and-restore tool, not a multi-device sync. There is one console.

## Goals

- **Launch = backup.** Opening the app immediately backs up the save data and extdata of every
  installed game to Koofr, with no further input. Settings has a "Back up on start" checkbox, on by
  default; with it off the app opens on the menu and "Sync now" runs the backup (owner,
  2026-11-12).
- **Only changes create versions.** A game whose save has not changed since its latest version in the
  cloud is reported as "unchanged" and nothing is uploaded for it (owner accepted this).
- **Every run adds, never replaces.** Running the app several times on the same day adds a new version
  each time something changed; earlier versions from that day stay (owner's decision).
- **Restore any version.** The owner picks a game, then a version, and the app downloads it into
  Checkpoint's backup folder on the SD card. The owner then writes it into the game using Checkpoint.
  The game list comes from the cloud, so it includes games not currently installed (the case after a
  card dies and games are reinstalled).
- **Manual deletion.** The owner picks a game, ticks versions, confirms, and the app deletes them from
  Koofr. Complete versions are deleted only this way (owner's decision).
- **Credentials safe from someone holding only the SD card.**
- **Ships as a CIA** installed to the HOME Menu (owner's requirement).

## Non-goals (v1)

- Writing saves back into games. Checkpoint does this step in v1; moving it into this app is a
  possible later version.
- Cartridge games, DS games, DSiWare, Virtual Console or emulator saves. The owner plays only
  installed 3DS CIA titles.
- More than one console, conflict resolution, or two-way sync.
- Protecting credentials from someone who has the console itself (owner: protection against the
  SD card alone is enough).
- Any automatic pruning or retention policy.
- Encrypting the save files themselves in the cloud (not requested).

## Anti-goals

These would make the app a failure even if everything else works:

- **The app deletes or overwrites a complete version in the cloud on its own.** A dying card can
  produce corrupt saves that then get uploaded as the newest version; the only rescue is an older
  version, so older versions must survive every automatic action. The single, owner-approved
  exception is the app's own unfinished uploads (see "Upload protocol").
- **The app changes save data on the console in v1.** Backup access to game saves is read-only.
- **Plaintext credentials touch the SD card,** including a temporary import file. Deleted files on
  SD cards are often recoverable.

## Decided by the owner

| Area | Decision |
|---|---|
| Console | 3DS with Luma3DS; Checkpoint is installed |
| Output format | CIA |
| Language | C++ |
| Toolchain | devkitPro (libctru), built on Arch Linux |
| UI | citro2d |
| HTTPS / HTTP | libcurl + mbedTLS (devkitPro packages `3ds-curl`, `3ds-mbedtls`) |
| XML (WebDAV PROPFIND responses) | TinyXML2 (devkitPro package `3ds-tinyxml2`) |
| What is backed up | Save data **and** extdata for each title, as Checkpoint does |
| What a version is | A full snapshot of the game: save data plus extdata (if the game has any), even when only one of them changed |
| Unfinished uploads | Uploaded to a temporary location, given the final version name only when complete; leftovers from interrupted runs are deleted automatically on the next launch |
| Checkpoint compatibility | Folder naming follows the latest Checkpoint release; checked on the console later |
| Cloud | Koofr, via WebDAV |
| Cloud root folder | `/3DSaveSync` by default, changeable in Settings |
| Credentials entry | Typed once on the console's software keyboard on first launch |
| Credentials threat model | Someone who has only the SD card must not be able to read them |
| Restore path | App downloads into Checkpoint's folder; owner restores with Checkpoint |
| Deletion | Manual only, owner selects versions, with confirmation |
| Same-day runs | Add a new version alongside, never replace |
| Screens | The approved mockup (see "Screens") |
| Build system | Make (devkitPro's application template plus a `cia` target) |
| PC unit tests | doctest (single header, vendored) for the hardware-free code in `source/core/` |
| Version control | git, commits only on the owner's go-ahead, no AI attribution; remote `git@github.com:NuclleaR/3DSaveSync.git` |
| Releases | GitHub Actions (`.github/workflows/release.yml`): pushing a tag `v*` runs the PC tests and, only if they pass, builds the CIA and publishes it as a GitHub release named after the tag. makerom and bannertool are built from source at the versions used locally. Install by QR code in FBI (Remote Install → Scan QR Code): each release carries a QR code of its own CIA link, and the README shows a permanent one for the latest release (owner, 2026-11-12) |
| License | GPLv3 (owner, 2026-11-12), so code and tables adapted from Checkpoint (GPLv3 or later) can be used. Files adapted from Checkpoint keep its copyright notice and Additional Terms 7.b/7.c and are marked as modified. "Or later" matches Checkpoint (designer's call). |

## Constraints found during research (2026-11-12)

- **Koofr requires TLS 1.2 or newer.** Checked with `openssl s_client` against `app.koofr.net`:
  TLS 1.1 is rejected (protocol_version alert); TLS 1.2 works, both with ECDHE-CHACHA20-POLY1305 and
  with plain AES128-SHA256.
- **The console's built-in SSL service tops out at TLS 1.1** (3dbrew, and Pretendo forum reports that
  the 3DS cannot reach TLS-1.2-only servers). Therefore `httpc`/`sslc` cannot be used; all network
  traffic goes through libcurl + mbedTLS. The one use of `sslc` left is as a random-number source:
  devkitPro's mbedTLS port gets its entropy from `sslcGenerateRandomData` and does not start the
  service itself, so the app calls `sslcInit` before any TLS connection.
- **Koofr's WebDAV accepts only an app password.** The account password that works in the browser
  gets HTTP 401 on WebDAV.
- **Koofr's WebDAV answers as the upload protocol needs** (probe run 2026-11-12,
  `tools/koofr-probe.sh`):
  - MKCOL of a new folder → 201; MKCOL of an existing folder → 405.
  - PUT → 201; GET → 200; PROPFIND Depth 1 → 207.
  - PROPFIND of a missing folder → 404.
  - MOVE of a folder with `Overwrite: F` → 201; onto an existing name → 412.
  - DELETE of a folder (recursive) → 204; DELETE of a missing path → 404.
  
  PROPFIND hrefs are paths starting with `/dav/Koofr`, prefixed `D:`.
- **Koofr's certificate chain:** `*.koofr.net` ← RapidSSL TLS RSA CA G1 ← DigiCert Global Root G2.
- **Toolchain:** devkitARM GCC 16.1, so C++23 (`std::expected`) is available on the console as well as
  on the PC.

## How it works

### Credentials and settings

- Koofr WebDAV endpoint: `https://app.koofr.net/dav/Koofr`, HTTP Basic auth with the Koofr account
  email and a Koofr **app password** (Koofr issues app-specific passwords for WebDAV; they can be
  revoked without changing the main password).
- On first launch (no stored credentials) the app shows a setup screen: email and app password are
  entered with the system software keyboard, the connection is tested, and on success they are saved.
  The test lists the cloud folder; "not found" counts as success, because the first backup creates
  the folder. On failure the screen shows the error text and stays open. START exits the app.
- Storage: the app's own save-data archive. As an installed CIA the app has its own title ID; save
  data of SD-installed titles is stored under `Nintendo 3DS/` encrypted by the console with a
  console-unique key, so the SD card alone does not reveal it. The settings are one text file,
  `/settings.txt` (`user=`, `password=`, `root=` lines), committed with
  `ARCHIVE_ACTION_COMMIT_SAVE_DATA`. A fresh install has no archive yet; the app formats it on first
  use. When the file is missing or cannot be opened, the app treats it as a first launch and shows
  setup.
- Settings screen: change credentials, change the cloud root folder, sign out.
- Sign out (owner, 2026-11-12): a "Sign out" button in Settings, behind a confirmation. It deletes
  the saved email and app password and keeps the cloud folder, then shows the setup screen.

### Backup (runs automatically on launch unless "Back up on start" is off)

1. Connect to Koofr. If there is no network or authentication fails, show the error and go to the
   main menu.
2. Enumerate titles installed on the SD card; keep games that have save data and/or extdata.
   Skip system titles, updates and DLC.
3. For each game, read its save archive and its extdata (if any), read-only, and decide whether the
   game as a whole differs from the latest complete version in the cloud (a change in either part
   counts).
4. If it differs, upload a full snapshot (save data and extdata) as a new version. If not, mark it
   "unchanged".
5. Show per-game progress (uploaded / unchanged / reading… / error) on the top screen, with Cancel
   on the bottom screen. Then a summary (counts of uploaded, unchanged, errors) and the main menu.

### Restore

1. List every game that has versions in the cloud, with version counts.
2. Pick a game → its versions, newest first, latest marked.
3. "Download to Checkpoint" writes that version into Checkpoint's backup folders on the SD card
   (save data and, if present, extdata of that version), then tells the owner to restore it in
   Checkpoint.
4. An existing local folder is never written into. If `<version>` already exists under the game's
   Checkpoint saves or extdata folder, the download goes to `<version>-2`, `-3`, ….
5. Every downloaded file is checked against the SHA-256 in the version's manifest. On any failure
   (corrupted file, network, cancel) the folders written by this download are removed, so Checkpoint
   never sees a half-written backup.

### Delete versions

1. Same game list. Pick a game → versions with checkboxes.
2. "Delete N versions" → confirmation ("Delete N versions of <game> from Koofr? This cannot be
   undone." A = Delete, B = Cancel) → WebDAV DELETE.

### Main menu

Restore · Delete versions · Sync again · Settings. START exits.

## Cloud layout

Mirror Checkpoint's on-SD layout under the cloud root, so a downloaded version drops straight into
Checkpoint's folders and Checkpoint recognises it:

```
<root>/saves/<Checkpoint title folder>/<version>/...          save data files
<root>/extdata/<Checkpoint title folder>/<version>/...        extdata files (only if the game has any)
<root>/manifests/<Checkpoint title folder>/<version>.manifest list of the version's files
<root>/.incoming/<Checkpoint title folder>/<version>/...      uploads in progress
```

`<root>` is `/3DSaveSync` unless changed in Settings.

Names match Checkpoint exactly (checked against the source of the latest Checkpoint release):
- **Title folder.** `"0x%05X "` of `((u32)titleId) >> 8`, then the SMDH English short description.
  In the description, each of `.,!\/:?*"<>|` becomes a space and trailing spaces are dropped
  (e.g. `0x0175E Pokémon Moon`, `0x… Kid Icarus  Uprising`).
- **Version.** Local time as `YYYYMMDD-HHMMSS` (Checkpoint's default backup name). A clash adds
  `-2`, `-3`, ….
- **Extdata id.** `((u32)titleId) >> 8` except Checkpoint's known exceptions (Pokémon X/Y, ORAS,
  Sun/Moon, Ultra Sun/Moon, Fire Emblem Fates).

The owner checks on the console that Checkpoint sees downloaded versions.

A version of a game is its save folder and its extdata folder with the same version name, plus its
manifest. All three belong to the same snapshot.

**Manifest.** The manifest is a text file listing every file of the snapshot (part, size, SHA-256,
relative path) and every folder (part, relative path). It answers "has this game changed since its
latest version?" (same files, sizes, hashes and folders means unchanged) and lets Restore verify
every download. Restore also creates every listed folder, so empty folders come back too (owner,
2026-11-12). The folder lines are optional, so manifests without them still parse.

The manifests live in their own `manifests/` tree, not inside the version folders. Checkpoint
restores every file it finds in a backup folder into the game's save, so an extra file there would
end up in the save.

## Upload protocol

1. Every file of the snapshot is PUT under `<root>/.incoming/<title>/<version>/saves/…` and
   `…/extdata/…`, then the manifest under `<root>/.incoming/<title>/<version>/<version>.manifest`.
2. MOVE `…/saves` → `<root>/saves/<title>/<version>`; MOVE `…/extdata` →
   `<root>/extdata/<title>/<version>` (if the game has extdata).
3. MOVE the manifest → `<root>/manifests/<title>/<version>.manifest`. **This is the completion
   point:** a version is complete exactly when its manifest is in `manifests/`.
4. DELETE `<root>/.incoming/<title>/<version>`.

Only complete versions appear in Restore and Delete and count as "the latest version".

**Cleanup on launch** (the only automatic deletion, owner-approved). For each leftover
`<root>/.incoming/<title>/<version>`:
- if that version has no manifest, its `saves/` and `extdata/` folders of the same title and version
  are removed (partial moves of the interrupted upload), then the incoming folder;
- if the manifest exists (the crash came after the completion point), only the incoming folder is
  removed.

Cleanup never touches a saves/extdata folder without an `.incoming` counterpart, never touches a
complete version, and lists everything before deleting anything. If any listing fails, it deletes
nothing.

**Deleting a version** removes its saves folder, then its extdata folder, then the manifest last, so a
failed delete stays visible and can be retried.

This layout also keeps the door open for a later version that writes saves back itself, without
changing what is already stored in the cloud.

## Screens

Approved mockup: `docs/superpowers/mockups-2026-11-12-screens.html` (open it in a browser). Top screen 400×240,
bottom touch screen 320×240. UI text is in English. Touch and the D-pad/A/B both work.

The mockup has no setup screen. Its layout uses the mockup's colors and buttons: the top screen
shows "Koofr login", the email, whether the app password is set (never the password itself), the
cloud folder, the status or error text, and a hint that WebDAV needs a Koofr app password. The
bottom screen has the buttons "Email", "App password" and "Connect" (primary). The D-pad moves the
selection and A presses the selected button. The email and the cloud folder in use are shown under
the menu buttons and in Settings, so the owner can see which Koofr account the app is signed in to.

How the other screens work, beyond the mockup:
- **Sync.** It runs on a background thread, one priority step below the UI, so the screens keep
  drawing. B or Cancel stops it. A run stopped by an error is headed "Sync stopped · …". The summary
  lists failed games first, each with its error text, then the uploaded ones.
- **Menu.** Restore / Delete versions / Sync again / Settings. START leaves the app from any
  screen; a running job is cancelled first.
- **Restore and Delete.** They load every game in the cloud with its versions, showing progress.
  Versions show as "YYYY-MM-DD HH:MM". The D-pad picks a game on the top screen; A or Right moves to
  its versions on the bottom screen, and B or Left goes back. In Restore, A on a version downloads
  it. In Delete, A ticks a version, and the "Delete N versions" button asks for confirmation.
  Deleting cannot be cancelled once it has started.
- **Settings.** Changes stay in a draft until Save has checked them with Koofr. B drops unsaved
  changes, and the screen warns about them. The "Back up on start" checkbox is the exception: it is
  a local choice and is saved as soon as it is toggled. Sign out keeps it, together with the cloud
  folder.

## The designer's calls (not discussed with the owner)

- Restoring a version downloads both its save data and its extdata into Checkpoint's folders.
- The TLS server certificate is verified, since the connection carries the Koofr credentials. The
  app bundles Mozilla's CA set (`romfs/cacert.pem` from curl.se), not just Koofr's current root, so a
  change of certificate authority at Koofr does not break it.
- Change detection and restore verification use a per-version manifest with SHA-256 per file (see
  "Cloud layout").
- Restore never writes into an existing local folder (`-2` suffix), verifies every file, and rolls
  back on failure (see "Restore").
- Cleanup decides everything before deleting anything (see "Upload protocol").
- Extdata files that cannot be opened (in use) are skipped, as Checkpoint does; save data files that
  cannot be read fail that game.
- Game list: SD-installed titles, minus Checkpoint's exclusions (instruction manuals, updates
  `0x0004000E`, DSi data `0x0004800F`), that have save data or extdata and a readable SMDH (the name
  comes from it; DLC and data titles have none). Sorted by name. The app itself is never listed: its
  own save holds the login.
- Restore stops before writing anything if a local Checkpoint folder of that game cannot be listed,
  so it never reuses, overwrites or rolls back a folder it did not create.
- CIA: product code `CTR-H-3DSS`, unique id `0xF3D55`, 128 KB own save data (holds the credentials).
  The RSF filesystem access list follows Checkpoint's, which opens other titles' archives. The RSF
  also maps VRAM read-only (`1f000000-1f5fffff:r`): libctru reads the screens from VRAM when it
  starts a library applet such as the software keyboard.
  Placeholder icon and banner until the owner provides art.
- Error texts shown on the console:
  - No network: "No connection to Koofr. Check Wi-Fi."
  - TLS failure: "Secure connection failed. Check the console's date and time."
  - HTTP 401/403: "Koofr rejected the login. Check email and app password in Settings."
  - HTTP 507: "Koofr storage is full."
  - Reading the console's archives or writing the SD card failed: "Console storage error: <detail>"
  
  Network, TLS, login and storage-full errors stop a backup run. Other errors fail only the game
  they happened on.

## Network timeouts (owner, 2026-11-12)

10 s, "plenty on Wi-Fi". Opening a connection takes at most 10 s, and a request that receives or sends
no byte for 10 s is aborted. Either case reads "No connection to Koofr. Check Wi-Fi." No request is
retried. This also bounds the paths that have no Cancel button: deleting versions and the login check
on the setup screen.
