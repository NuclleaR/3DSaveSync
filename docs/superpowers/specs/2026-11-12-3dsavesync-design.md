# 3DSaveSync — Design

Date: 2026-11-12
Status: draft, awaiting owner approval

## Intent

The owner has twice lost every 3DS save to cheap SD cards that failed. 3DSaveSync is a homebrew app
for the owner's single 3DS that backs up game saves to Koofr cloud storage over WebDAV, keeps every
version, and lets the owner fetch any version back so it can be restored.

It is a backup-and-restore tool, not a multi-device sync. There is one console.

## Goals

- **Launch = backup.** Opening the app immediately backs up the save data and extdata of every
  installed game to Koofr, with no further input.
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

## Constraints found during research (2026-11-12)

- **Koofr requires TLS 1.2 or newer.** Checked with `openssl s_client` against `app.koofr.net`:
  TLS 1.1 is rejected (protocol_version alert); TLS 1.2 works, both with ECDHE-CHACHA20-POLY1305 and
  with plain AES128-SHA256.
- **The console's built-in SSL service tops out at TLS 1.1** (3dbrew, and Pretendo forum reports that
  the 3DS cannot reach TLS-1.2-only servers). Therefore `httpc`/`sslc` cannot be used; all network
  traffic goes through libcurl + mbedTLS.
- devkitPro was not yet installed on the build machine at the time of writing.

## How it works

### Credentials and settings

- Koofr WebDAV endpoint: `https://app.koofr.net/dav/Koofr`, HTTP Basic auth with the Koofr account
  email and a Koofr **app password** (Koofr issues app-specific passwords for WebDAV; they can be
  revoked without changing the main password).
- On first launch (no stored credentials) the app shows a setup screen: email and app password are
  entered with the system software keyboard, the connection is tested, and on success they are saved.
- Storage: the app's own save-data archive. As an installed CIA the app has its own title ID; save
  data of SD-installed titles is stored under `Nintendo 3DS/` encrypted by the console with a
  console-unique key, so the SD card alone does not reveal it.
- Settings screen: change credentials, change the cloud root folder.

### Backup (runs automatically on launch)

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
/3DSaveSync/saves/<Checkpoint title folder>/<version folder>/...
/3DSaveSync/extdata/<Checkpoint title folder>/<version folder>/...
```

The title-folder and version-folder names must match exactly what Checkpoint itself produces and
expects under `/3ds/Checkpoint/saves/` and `/3ds/Checkpoint/extdata/`. The builder confirms the exact
format against the source of the latest Checkpoint release before writing any upload code; the owner
checks on the console later that Checkpoint sees downloaded versions.

A version of a game is its save folder and its extdata folder with the same version name. Both
belong to the same snapshot.

## Upload protocol

- Each new version is uploaded under a temporary name and gets its final version name only once
  every file of the snapshot (save data and extdata) has been uploaded.
- A version counts as complete only when it carries its final name. Temporary uploads never appear
  in Restore or Delete and never count as "the latest version" for change detection.
- On the next launch the app deletes leftover temporary uploads from interrupted runs (Cancel,
  network loss, power off). This is the only automatic deletion the app ever performs; it never
  touches a complete version.

This layout also keeps the door open for a later version that writes saves back itself, without
changing what is already stored in the cloud.

## Screens

Approved mockup: `.superpowers/brainstorm/1790841-1794438123/content/flow.html`. Top screen 400×240,
bottom touch screen 320×240. UI text is in English. Touch and the D-pad/A/B both work.

## The designer's calls (not discussed with the owner)

- Restoring a version downloads both its save data and its extdata into Checkpoint's folders.
- The TLS server certificate is verified (CA certificates bundled with the app), since the
  connection carries the Koofr credentials.

## Left to the builder

- How "changed since the latest version" is detected (e.g. a hash manifest stored with each version).
- The mechanics of the temporary name and the final rename (e.g. a temp folder plus WebDAV MOVE),
  including the two folders (saves/ and extdata/) a single version spans.
- Which CA certificates to bundle and how.
- Title-name source (SMDH) and folder-name sanitising, matching Checkpoint.
- Filesystem access permissions in the CIA's RSF needed to open other titles' save and extdata
  archives (Checkpoint's RSF is a reference).
- Network timeouts, retries and error messages.
- What to do when the target folder already exists in Checkpoint's folder during Restore.
- CIA title ID / product code in the homebrew range, and placeholder icon and banner art.
- Project structure and build scripts.
