# AMU – Mod Download- & Entpack-Ablauf

Diese Doku beschreibt **wohin Mods heruntergeladen, zwischengespeichert, entpackt und installiert** werden.
Alle Zeilenangaben beziehen sich auf den Stand der Analyse (2026-06-30).

Steam-App-ID für ARK: Survival Evolved = **`346110`** (fest verdrahtet in `amu.au3`).

---

## 1. Überblick (eine Mod-Reise in 4 Stufen)

```
GameUserSettings.ini (ActiveMods=...)        ← welche Mods ein Server will
            │
            ▼
[1] DOWNLOAD   steamcmd  →  lib\steamcmd\steamapps\workshop\content\346110\<modid>\   (komprimiert, *.z)
            │
            ▼
[2] PRÜFUNG    _GetUnpackedModSize + ACF-Manifest  →  „muss aktualisiert werden?"
            │
            ▼
[3] ENTPACKEN  __ModDecomp + _UnPack (zlib.dll)
            │      *.z  →  entpackte Datei (ohne .z-Endung)
            ▼
[4] INSTALL    <server_path>\ShooterGame\Content\Mods\<modid>\   +   <modid>.mod
```

---

## 2. Stufe 1 – Download (steamcmd)

**Funktion:** `_DownloadAndInstallMods()` (`amu.au3:1350`)

### Beteiligte Binärdateien / Ordner
| Zweck | Pfad |
|---|---|
| steamcmd-Binary | `lib\steamcmd\steamcmd.exe` (per `FileInstall` beim Start, `amu.au3:111`; Ordner per `DirCreate`, `amu.au3:108`) |
| Temporäres Runscript | `%TEMP%\<random>.tmp` (`_WinAPI_GetTempFileName(@TempDir)`, `amu.au3:1384`) |

### Ablauf
1. AMU sammelt alle benötigten Mod-IDs aus jedem Server:
   `<server_path>\ShooterGame\Saved\Config\WindowsServer\GameUserSettings.ini` → `[ServerSettings] ActiveMods` (`amu.au3:1358`), dedupliziert sie.
2. Es schreibt ein **temporäres steamcmd-Runscript** (`amu.au3:1391-1407`):
   ```
   @ShutdownOnFailedCommand 1
   @NoPromptForPassword 1
   login anonymous            ; oder: login <user> <pass> <guard>  (aus DB-Settings, server_id=-1)
   workshop_download_item 346110 <modid> validate    ; eine Zeile pro Mod
   quit
   ```
3. Es startet steamcmd versteckt mit STDOUT-Pipe (`amu.au3:1433`):
   ```
   "lib\steamcmd\steamcmd.exe" +runscript "<tempscript>"
   ```
4. **Download-Ziel (von steamcmd selbst angelegt):**
   ```
   lib\steamcmd\steamapps\workshop\content\346110\<modid>\
   ```
   Da steamcmd ohne `force_install_dir` aus seinem eigenen Ordner läuft, landet der Workshop-Inhalt unter dessen `steamapps\workshop`. Bestätigt im Code u. a. bei `amu.au3:1627, 1675, 1802`.
5. **Manifest-Datei:** `lib\steamcmd\steamapps\workshop\appworkshop_346110.acf` (`amu.au3:1420`) – liefert pro Mod `manifest`, `size`, `timeupdated` (gelesen via `_GetModInfosFromACF`).
6. **Sicherheit:** Das Runscript enthält das Steam-Login im Klartext und wird nach dem Lauf mit Strichen überschrieben und gelöscht (`amu.au3:1525-1533`).

### Form des heruntergeladenen Inhalts
Im Ordner `content\346110\<modid>\` liegt der Inhalt in **Steam-komprimierter Form**:
- Asset-Dateien als **`*.z`** (zlib-gechunktes „arkit"-Format), je optional begleitet von einer **`*.uncompressed_size`**-Textdatei.
- Metadaten: **`mod.info`** (Kartenliste) und **`modmeta.info`** (ModType-Blob).
- Optional eine **`WindowsNoEditor`**-Unterebene (AMU prüft darauf, `amu.au3:2065`).

### Cache-Verhalten
Beim **ersten Download des Tages** wird der gesamte Steam-Workshop-Cache geleert
(`DirRemove(lib\steamcmd\steamapps\workshop\, 1)`, `amu.au3:1420-1426`), um veraltete Stände zu vermeiden.

---

## 3. Stufe 2 – Update-Entscheidung (kein Entpacken)

**Funktion:** `_Go4Update()` (`amu.au3:1558`), Hilfsfunktion `_GetUnpackedModSize()` (`amu.au3:781`)

- Existiert die Mod auf dem Server (`<server_path>\ShooterGame\Content\Mods\<modid>`) bereits, wird verglichen:
  - bisherige Größe (`DirGetSize`) vs. **geschätzte entpackte Größe** und
  - gespeicherter `timeupdated` vs. ACF-Manifest (`amu.au3:1668, 1679`).
- `_GetUnpackedModSize()` **entpackt nicht**, sondern schätzt die Zielgröße: Summe der `*.uncompressed_size`-Werte + Größe der Nicht-`.z`-Dateien (`amu.au3:790-802`).
- Bei „Force Update" entfällt der Vergleich.

Vor dem eigentlichen Tausch fährt AMU den Server geordnet herunter (RCON-Broadcasts → `saveworld` → auf frische `.ark`-Datei warten → `ProcessClose`), optional mit Map-Backup. Das gehört zur Restart-Choreografie, nicht zum Entpacken.

---

## 4. Stufe 3 + 4 – Entpacken & Installation

**Funktionen:** `_Go4Update` (Tausch-Logik, `amu.au3:1786-1806`), `__ModDecomp` (`amu.au3:2063`), `_UnPack` (`inc/UnpackZx64.au3:86`)

### Zwei Installationswege (`amu.au3:1796-1803`)
1. **Schnellpfad (DirMove)** – *aktuell praktisch ungenutzt:*
   Existiert eine vorab entpackte Kopie unter `lib\Mods\<modid>` (+ `lib\Mods\<modid>.mod`), wird sie per `DirMove`/`FileMove` in den Server verschoben.
   Hinweis: Der Code, der `lib\Mods` befüllen würde, ist auskommentiert (`amu.au3:1673`), daher läuft in der Praxis fast immer der Else-Zweig (direktes Entpacken).
2. **Direkt-Entpacken (Normalfall):**
   ```
   __ModDecomp(
       $server_id,
       lib\steamcmd\steamapps\workshop\content\346110,         ← Quelle ($source)
       <server_path>\ShooterGame\Content\Mods,                 ← Ziel   ($dest)
       <modid>)
   ```

### Was `__ModDecomp` macht (`amu.au3:2063-2123`)
1. **Quellordner bestimmen:**
   `source_dir = <source>\<modid>\WindowsNoEditor` falls vorhanden, sonst `<source>\<modid>` (`amu.au3:2065-2069`).
2. **Zielordner:** `dest_dir = <server_path>\ShooterGame\Content\Mods\<modid>` (`amu.au3:2070`).
3. Vorhandenen Zielordner + `.mod`-Datei löschen, Zielordner neu anlegen (`amu.au3:2072-2081`).
4. **`.mod`-Deskriptor schreiben** via `_CreateModFile(modmeta.info, mod.info, <modid>, <dest>\<modid>.mod)` (`amu.au3:2082`, Implementierung in `inc/MakeMod.au3`).
5. **Alle Dateien rekursiv** auflisten (relativ, außer `*.uncompressed_size`) und durchgehen (`amu.au3:2086-2121`):
   - **`.z`-Datei →** `_UnPack(source_dir\<relpfad>, dest_dir\<relpfad ohne .z>)`, bis zu **3 Versuche** (`amu.au3:2112-2117`).
     Aus `foo.uasset.z` wird also `foo.uasset` im Server-Mod-Ordner.
   - **andere Datei →** `FileCopy` unverändert ins Ziel (`amu.au3:2119`).

### Was `_UnPack` macht (`inc/UnpackZx64.au3:86`)
1. Liest die ganze `.z`-Datei in einen Speicherpuffer (`UnpackZx64.au3:89-91`).
2. Liest den **32-Byte-Header**: Signatur (muss `2653586369` sein), `UnpackedChunkSize`, `packedFullSize`, `UnpackedSize` (`UnpackZx64.au3:65, 97-102`).
3. Liest den **Chunk-Index** (Paare aus *komprimiert*/*unkomprimiert* INT64), bis die Summe der unkomprimierten Größen == `UnpackedSize` (`UnpackZx64.au3:114-131`).
4. Dekomprimiert jeden Chunk per **`uncompress` aus `lib\zlib.dll`** (lazy via `DllOpen`, `UnpackZx64.au3:42-52, 215-235`).
5. Setzt die Chunks zusammen und schreibt die fertige Binärdatei (`FileOpen` BINARY+OVERWRITE, `FileWrite`) an den Zielpfad (`UnpackZx64.au3:192-195`).

> **DLL-Abhängigkeit:** `lib\zlib.dll` wird beim Start per `FileInstall` nach `lib\zlib.dll` gelegt (`amu.au3:110`). Frühere Versionen nutzten einen In-Memory-Loader – der wurde durch das On-Disk-`DllOpen` ersetzt (AV-Härtung).

### Nach dem Entpacken
- DB-Eintrag der Mod wird aktualisiert (Größe, entpackte Größe, Name, Preview, `timeupdated`) (`amu.au3:1812`).
- Wenn mindestens eine Mod aktualisiert wurde, startet AMU den Server über das hinterlegte Startscript neu (`amu.au3:1820-1822`).

---

## 5. Vollständige Ordnerstruktur (Beispiel Mod-ID `123456789`)

```
<AMU-Installdir>\
├─ amu.exe
├─ lib\
│  ├─ zlib.dll                                  ← Entpacker-DLL
│  ├─ sqlite3_x64.dll
│  ├─ amu.db                                    ← Config/DB
│  ├─ Mods\                                     ← (vestigialer Schnellpfad-Cache, i.d.R. leer)
│  └─ steamcmd\
│     ├─ steamcmd.exe
│     └─ steamapps\
│        └─ workshop\
│           ├─ appworkshop_346110.acf           ← Manifest (Größe/Datum/Update)
│           └─ content\
│              └─ 346110\
│                 └─ 123456789\                  ← [1] DOWNLOAD-ZIEL (komprimiert)
│                    ├─ mod.info
│                    ├─ modmeta.info
│                    ├─ <assets>.z
│                    ├─ <assets>.uncompressed_size
│                    └─ (ggf. WindowsNoEditor\...)
│
└─ <server_path>\                               ← pro Server, Pfad aus der DB
   └─ ShooterGame\
      ├─ Saved\Config\WindowsServer\GameUserSettings.ini   ← ActiveMods, RCON-Settings
      └─ Content\
         └─ Mods\
            ├─ 123456789\                        ← [4] INSTALL-ZIEL (entpackt)
            │  └─ <assets>                       ← .z entfernt, Inhalt entpackt
            └─ 123456789.mod                     ← Deskriptor (aus _CreateModFile)
```

> **Temporär & flüchtig:** Das steamcmd-Runscript liegt in `%TEMP%` und wird nach dem Download gelöscht.
> **Git:** `lib/steamcmd/` und `srvX/ShooterGame/Content/` sind in `.gitignore` ausgeschlossen.

---

## 6. Bekannte Schwachstellen in genau diesem Pfad

(Aus der Codeanalyse – siehe Memory `amu-security-findings`.)

- **`__ModDecomp` ohne `IsArray`-Guard** (`amu.au3:2086-2088`): leerer/partieller Download → fataler Subscript-Fehler, ganzer Lauf bricht ab.
- **`_CreateModFile`-Fehler ignoriert** (`amu.au3:2082`): kaputte/leere `.mod` wird trotzdem als Erfolg gewertet → ARK lädt die Mod nicht, AMU meldet aber OK.
- **`.mod`-Datei kaputt bei Mod-ID > 2³¹** (`inc/MakeMod.au3:35-36`): 8-statt-4-Byte-ID + 4 Nullbytes verschiebt alle Folgefelder → betrifft moderne Workshop-IDs.
- **`_LoadDll()`-Fehler ignoriert** (`UnpackZx64.au3:224`): fehlende `zlib.dll` → irreführender dreifacher „chunk size mismatch"-Retry statt klarer Meldung.
- **OOB-Read bei manipuliertem/abgeschnittenem `.z`** (`UnpackZx64.au3:145`): Datenschleife ohne Längenprüfung (read-only, begrenzt).
