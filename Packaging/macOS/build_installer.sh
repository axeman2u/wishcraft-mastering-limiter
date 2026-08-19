#!/bin/bash
# Builds the macOS installer for Wishcraft Mastering Limiter: a .pkg that places the
# VST3 and AU (both universal Apple Silicon + Intel binaries) in their standard system
# locations, plus a copy of the PDF manual alongside each (not a drag-and-drop DMG the
# user has to place files from manually), wrapped in a DMG for distribution.
#
# UNSIGNED for now (no Apple Developer ID yet) -- Gatekeeper will show an "unidentified
# developer" warning on first launch; users need to right-click the installer and choose
# Open, or approve it in System Settings > Privacy & Security. Once a Developer ID is
# available, add `codesign` for the plugin bundles and `productsign` for the .pkg before
# this script's productbuild step, then `xcrun notarytool submit` +
# `xcrun stapler staple` on the finished .pkg.
#
# Usage: ./build_installer.sh [version]
#   version defaults to CMakeLists.txt's project() VERSION.
#
# TRIAL=1 ./build_installer.sh builds a time-limited trial version instead (see
# Source/TrialLicense.h and Wishcraft_Limiter_Spec.md's "Trial Build" section) --
# uses a separate build-trial/ CMake directory so it can never leave
# WISHCRAFT_TRIAL_BUILD accidentally cached ON for a normal build reusing build/.
# TRIAL_DAYS overrides the trial length (default 30).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TRIAL="${TRIAL:-0}"
TRIAL_DAYS="${TRIAL_DAYS:-30}"
if [[ "$TRIAL" == "1" ]]; then
    BUILD_DIR="$PROJECT_ROOT/build-trial"
else
    BUILD_DIR="$PROJECT_ROOT/build"
fi
ARTEFACTS="$BUILD_DIR/WishcraftMasteringLimiter_artefacts/Release"
WORK_DIR="$SCRIPT_DIR/work"
OUT_DIR="$SCRIPT_DIR/dist"

APP_NAME="Wishcraft Mastering Limiter"
IDENTIFIER_PREFIX="com.Wishcraft.WishcraftMasteringLimiter"

VERSION="${1:-$(grep -m1 'project(WishcraftMasteringLimiter VERSION' "$PROJECT_ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')}"
if [[ -z "$VERSION" ]]; then
    echo "Could not determine version; pass one explicitly: ./build_installer.sh 1.0.0" >&2
    exit 1
fi

if [[ "$TRIAL" == "1" ]]; then
    OUT_LABEL="Wishcraft Mastering Limiter TRIAL $VERSION"
else
    OUT_LABEL="Wishcraft Mastering Limiter $VERSION"
fi

echo "==> Building $OUT_LABEL installer"

# ---------------------------------------------------------------------------
# 1. Build Release (skip with SKIP_BUILD=1 if you already have a fresh Release build)
# ---------------------------------------------------------------------------
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    if [[ "$TRIAL" == "1" ]]; then
        echo "==> Configuring + building Release (TRIAL, $TRIAL_DAYS-day)"
        cmake -B "$BUILD_DIR" -G Xcode "$PROJECT_ROOT" \
            -DWISHCRAFT_TRIAL_BUILD=ON -DWISHCRAFT_TRIAL_DAYS="$TRIAL_DAYS" > /dev/null
    else
        echo "==> Configuring + building Release"
        cmake -B "$BUILD_DIR" -G Xcode "$PROJECT_ROOT" > /dev/null
    fi
    cmake --build "$BUILD_DIR" --config Release
fi

for required in \
    "$ARTEFACTS/VST3/$APP_NAME.vst3" \
    "$ARTEFACTS/AU/$APP_NAME.component"
do
    if [[ ! -e "$required" ]]; then
        echo "Missing build artefact: $required" >&2
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# 2. Stage the file layout each component package will install
# ---------------------------------------------------------------------------
echo "==> Staging install layout"
# Only wipe the scratch WORK_DIR, never OUT_DIR -- OUT_DIR is shared between normal and
# TRIAL runs (both build into Packaging/macOS/dist/), so rm -rf'ing it here used to
# delete whatever DMG a previous run (e.g. the normal build) had already placed there.
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/root-vst3" "$WORK_DIR/root-au" "$OUT_DIR"

cp -R "$ARTEFACTS/VST3/$APP_NAME.vst3" "$WORK_DIR/root-vst3/"
cp -R "$ARTEFACTS/AU/$APP_NAME.component" "$WORK_DIR/root-au/"

# A loose PDF alongside the .vst3/.component bundles doesn't confuse any host (they scan
# by bundle extension, not folder contents) -- and the plugin's own Help overlay has a
# "Manual (PDF)" button (Source/GUI/HelpOverlay.h's findManualFile()) that looks for it
# at exactly this path, so users never need to remember where it lives.
MANUAL_PDF="$PROJECT_ROOT/Manual/Wishcraft_Mastering_Limiter_Manual.pdf"
if [[ -f "$MANUAL_PDF" ]]; then
    cp "$MANUAL_PDF" "$WORK_DIR/root-vst3/"
    cp "$MANUAL_PDF" "$WORK_DIR/root-au/"
else
    echo "Warning: manual PDF not found at $MANUAL_PDF -- installer will ship without it" >&2
fi

# cp -R leaves AppleDouble resource-fork sidecar files (._*) in the payload on some
# filesystems -- harmless but sloppy for a real installer, strip them before packaging.
find "$WORK_DIR/root-vst3" "$WORK_DIR/root-au" \
    \( -name '._*' -o -name '.DS_Store' \) -delete

# ---------------------------------------------------------------------------
# 3. Build one component package per install location
# ---------------------------------------------------------------------------
echo "==> Building component packages"
mkdir -p "$WORK_DIR/pkgs"

VST3_PKGBUILD_ARGS=()
if [[ "$TRIAL" == "1" ]]; then
    # Writes the trial marker file this build's TrialLicense.h checks against, ONLY if
    # one doesn't already exist -- reinstalling (without first deleting it) does NOT
    # reset the trial clock, and this project's macOS packaging has no uninstaller at
    # all, so there's nothing that would clean this file up automatically either. The
    # secret string here MUST exactly match Source/TrialLicense.h's.
    mkdir -p "$WORK_DIR/scripts-vst3"
    cat > "$WORK_DIR/scripts-vst3/postinstall" <<'POSTINSTALL'
#!/bin/bash
set -euo pipefail
MARKER_DIR="/Library/Application Support/Wishcraft Mastering Limiter"
MARKER_FILE="$MARKER_DIR/.trial"
SECRET="Wishcraft-Trial-K7q2Zx9p"

if [[ -f "$MARKER_FILE" ]]; then
    exit 0
fi

mkdir -p "$MARKER_DIR"
DATE="$(date +%Y-%m-%d)"
HASH="$(printf '%s|%s' "$SECRET" "$DATE" | shasum -a 256 | awk '{print $1}')"
printf '%s\n%s\n' "$DATE" "$HASH" > "$MARKER_FILE"
chmod 444 "$MARKER_FILE"
exit 0
POSTINSTALL
    chmod +x "$WORK_DIR/scripts-vst3/postinstall"
    VST3_PKGBUILD_ARGS=(--scripts "$WORK_DIR/scripts-vst3")
fi

pkgbuild --root "$WORK_DIR/root-vst3" \
         --identifier "$IDENTIFIER_PREFIX.vst3" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "${VST3_PKGBUILD_ARGS[@]+"${VST3_PKGBUILD_ARGS[@]}"}" \
         "$WORK_DIR/pkgs/vst3.pkg" > /dev/null

pkgbuild --root "$WORK_DIR/root-au" \
         --identifier "$IDENTIFIER_PREFIX.au" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         "$WORK_DIR/pkgs/au.pkg" > /dev/null

# ---------------------------------------------------------------------------
# 4. Combine into one distribution package with a welcome/readme screen
# ---------------------------------------------------------------------------
echo "==> Building distribution package"
if [[ "$TRIAL" == "1" ]]; then
    TRIAL_WELCOME_NOTE="

This is a $TRIAL_DAYS-day TRIAL build. Enjoyed it after that? Email
wishcraftmusicstudio@gmail.com and I'll send you a free full version."
else
    TRIAL_WELCOME_NOTE=""
fi
cat > "$WORK_DIR/welcome.txt" <<EOF
This installs $OUT_LABEL:

  - VST3 + PDF manual  -> /Library/Audio/Plug-Ins/VST3/
  - Audio Unit (AU) + PDF manual  -> /Library/Audio/Plug-Ins/Components/

Both plugin formats are universal binaries (Apple Silicon + Intel). Open the
manual any time from within the plugin itself via the "?" button, then
"Manual (PDF)".

This installer is not yet code-signed. If macOS blocks it as being from an
unidentified developer, right-click the installer and choose Open, or allow
it in System Settings > Privacy & Security.${TRIAL_WELCOME_NOTE}
EOF

cat > "$WORK_DIR/license.txt" <<EOF
Wishcraft Mastering Limiter
Concept, design, and specification by Glenn Burgos.
(C) $(date +%Y) Glenn Burgos.
EOF

productbuild --synthesize \
    --package "$WORK_DIR/pkgs/vst3.pkg" \
    --package "$WORK_DIR/pkgs/au.pkg" \
    "$WORK_DIR/distribution.xml" > /dev/null

# --synthesize only lists <pkg-ref>s; add the readable title/options productbuild needs
# for a proper install UI (welcome/license screens, sane window title).
python3 - "$WORK_DIR/distribution.xml" "$OUT_LABEL" <<'PYEOF'
import sys
path, title = sys.argv[1], sys.argv[2]
with open(path) as f:
    xml = f.read()
xml = xml.replace(
    "<installer-gui-script minSpecVersion=\"1\">",
    "<installer-gui-script minSpecVersion=\"1\">\n"
    f"    <title>{title}</title>\n"
    "    <welcome file=\"welcome.txt\"/>\n"
    "    <license file=\"license.txt\"/>\n"
    "    <options customize=\"never\" require-scripts=\"false\"/>\n"
)
with open(path, "w") as f:
    f.write(xml)
PYEOF

productbuild --distribution "$WORK_DIR/distribution.xml" \
    --package-path "$WORK_DIR/pkgs" \
    --resources "$WORK_DIR" \
    "$WORK_DIR/$OUT_LABEL.pkg" > /dev/null

# ---------------------------------------------------------------------------
# 5. Wrap the finished .pkg in a DMG for distribution
# ---------------------------------------------------------------------------
echo "==> Building DMG"
DMG_STAGE="$WORK_DIR/dmg-stage"
mkdir -p "$DMG_STAGE"
cp "$WORK_DIR/$OUT_LABEL.pkg" "$DMG_STAGE/"

DMG_PATH="$OUT_DIR/$OUT_LABEL.dmg"
hdiutil create -volname "$OUT_LABEL" \
    -srcfolder "$DMG_STAGE" \
    -ov -format UDZO \
    "$DMG_PATH" > /dev/null

echo "==> Done: $DMG_PATH"
