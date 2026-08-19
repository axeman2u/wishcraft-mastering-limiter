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

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppName "Wishcraft Mastering Limiter"
#define MyAppPublisher "Glenn Burgos"
#define ArtefactsDir "..\..\build\WishcraftMasteringLimiter_artefacts\Release"

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
OutputBaseFilename=Wishcraft Mastering Limiter Setup {#MyAppVersion}
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
