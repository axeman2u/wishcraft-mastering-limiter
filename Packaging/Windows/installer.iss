; Inno Setup script for Wishcraft Mastering Limiter. Builds a proper installer that
; places the VST3 in the standard system-wide VST3 folder, with a copy of the manual
; alongside it (also findable from within the plugin itself via its Help overlay's
; "Manual (PDF)" button -- see Source/GUI/HelpOverlay.h's findManualFile()) -- not a
; zip the user has to place files from manually.
;
; MyAppVersion defaults below but is normally overridden from CI via
; "iscc /DMyAppVersion=1.2.3 installer.iss" so the installer's version always matches
; CMakeLists.txt's project() VERSION (single source of truth lives there, not here).
;
; Paths are relative to this file and assume the standard CMake build layout:
;   build/WishcraftMasteringLimiter_artefacts/Release/VST3/Wishcraft Mastering Limiter.vst3
;
; Unsigned for now (no code-signing certificate yet) -- Windows SmartScreen will warn
; on first run; users click "More info" -> "Run anyway". Add signtool steps once a
; certificate is available.
;
; /DTRIAL=1 builds a time-limited trial version instead (see Source/TrialLicense.h and
; Wishcraft_Limiter_Spec.md's "Trial Build" section) -- the CMake build it packages must
; ALSO have been configured with -DWISHCRAFT_TRIAL_BUILD=ON (this script doesn't build
; the plugin itself, only packages an already-built Release). Writes the same
; tamper-evident marker file macOS's build_installer.sh's TRIAL=1 mode does, ONLY if one
; doesn't already exist -- and since it's written via [Code]/Exec rather than a [Files]
; entry, Inno's uninstaller has no record of it and won't remove it, so a user has to
; find and delete it by hand before reinstalling resets anything.

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppName "Wishcraft Mastering Limiter"
#define MyAppPublisher "Glenn Burgos"
#define ArtefactsDir "..\..\build\WishcraftMasteringLimiter_artefacts\Release"

#ifdef TRIAL
  #define OutputLabel "Wishcraft Mastering Limiter Setup " + MyAppVersion + " TRIAL"
#else
  #define OutputLabel "Wishcraft Mastering Limiter Setup " + MyAppVersion
#endif

[Setup]
; Fixed, arbitrary GUID identifying this app to Windows' installer/uninstaller registry
; -- must stay THE SAME across every future version so upgrades replace rather than
; duplicate the install. Do not regenerate this for future releases.
AppId={{B1E6C6C1-6B0A-4E9B-9C7B-6C6B6B6B6B6B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename={#OutputLabel}
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; VST3 -- system-wide, standard location every VST3 host scans by default.
Source: "{#ArtefactsDir}\VST3\{#MyAppName}.vst3\*"; \
    DestDir: "{commoncf64}\VST3\{#MyAppName}.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

; Manual, alongside the VST3 in the same shared folder (not Program Files) so it's
; right next to the plugin file itself -- matches findManualFile()'s Windows candidate
; path exactly. Filename is NOT renamed, for the same reason.
Source: "..\..\Manual\Wishcraft_Mastering_Limiter_Manual.pdf"; \
    DestDir: "{commoncf64}\VST3"; Flags: ignoreversion

[Icons]
Name: "{group}\User Manual"; Filename: "{commoncf64}\VST3\Wishcraft_Mastering_Limiter_Manual.pdf"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#MyAppName}.vst3"

#ifdef TRIAL
[Code]
// Writes the trial marker file Source/TrialLicense.h checks against, ONLY if one
// doesn't already exist. The secret string here MUST exactly match
// Source/TrialLicense.h's and Packaging/macOS/build_installer.sh's -- all three
// compute the same salted hash over the same install-date string.
procedure WriteTrialMarkerIfMissing;
var
  MarkerDir, MarkerFile, ScriptFile, ScriptContent: String;
  ResultCode: Integer;
begin
  MarkerDir := ExpandConstant('{commonappdata}\Wishcraft Mastering Limiter');
  MarkerFile := MarkerDir + '\.trial';
  if FileExists(MarkerFile) then
    Exit;
  if not DirExists(MarkerDir) then
    CreateDir(MarkerDir);

  // Delegates the actual date/hash computation to PowerShell (via a temp .ps1 file,
  // not an inline -Command string, to sidestep fragile nested-quoting) rather than
  // reimplementing SHA256 in Pascal.
  ScriptFile := ExpandConstant('{tmp}\wishcraft_trial_write.ps1');
  ScriptContent :=
    '$date = (Get-Date).ToString("yyyy-MM-dd")' + #13#10 +
    '$secret = "Wishcraft-Trial-K7q2Zx9p"' + #13#10 +
    '$sha = [System.Security.Cryptography.SHA256]::Create()' + #13#10 +
    '$bytes = [System.Text.Encoding]::UTF8.GetBytes("$secret|$date")' + #13#10 +
    '$hash = ([System.BitConverter]::ToString($sha.ComputeHash($bytes)) -replace "-","").ToLower()' + #13#10 +
    '"$date`n$hash" | Out-File -Encoding ascii -NoNewline "' + MarkerFile + '"' + #13#10;

  SaveStringToFile(ScriptFile, ScriptContent, False);
  Exec('powershell.exe', '-NoProfile -ExecutionPolicy Bypass -File "' + ScriptFile + '"',
       '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  DeleteFile(ScriptFile);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    WriteTrialMarkerIfMissing;
end;
#endif
