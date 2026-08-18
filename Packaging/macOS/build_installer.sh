#!/bin/bash
# Builds the macOS installer for Wishcraft Mastering Limiter: a .pkg that places the
# VST3, AU, and Standalone app in their standard system locations (not a drag-and-drop
# DMG the user has to place files from manually), wrapped in a DMG for distribution.
#
# UNSIGNED for now (no Apple Developer ID yet) -- Gatekeeper will show an "unidentified
# developer" warning on first launch; users need to right-click the app/installer and
# choose Open, or approve it in System Settings > Privacy & Security. Once a Developer
# ID is available, add `codesign` for the plugin bundles/app and `productsign` for the
# .pkg before this script's productbuild step, then `xcrun notarytool submit` +
# `xcrun stapler staple` on the finished .pkg.
#
# Usage: ./build_installer.sh [version]
#   version defaults to CMakeLists.txt's project() VERSION.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
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

echo "==> Building Wishcraft Mastering Limiter installer v$VERSION"

# ---------------------------------------------------------------------------
# 1. Build Release (skip with SKIP_BUILD=1 if you already have a fresh Release build)
# ---------------------------------------------------------------------------
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    echo "==> Configuring + building Release"
    cmake -B "$BUILD_DIR" -G Xcode "$PROJECT_ROOT" > /dev/null
    cmake --build "$BUILD_DIR" --config Release
fi

for required in \
    "$ARTEFACTS/VST3/$APP_NAME.vst3" \
    "$ARTEFACTS/AU/$APP_NAME.component" \
    "$ARTEFACTS/Standalone/$APP_NAME.app"
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
rm -rf "$WORK_DIR" "$OUT_DIR"
mkdir -p "$WORK_DIR/root-vst3" "$WORK_DIR/root-au" "$WORK_DIR/root-app/$APP_NAME" "$OUT_DIR"

cp -R "$ARTEFACTS/VST3/$APP_NAME.vst3" "$WORK_DIR/root-vst3/"
cp -R "$ARTEFACTS/AU/$APP_NAME.component" "$WORK_DIR/root-au/"
cp -R "$ARTEFACTS/Standalone/$APP_NAME.app" "$WORK_DIR/root-app/$APP_NAME/"

MANUAL_PDF="$PROJECT_ROOT/Manual/Wishcraft_Mastering_Limiter_Manual.pdf"
if [[ -f "$MANUAL_PDF" ]]; then
    cp "$MANUAL_PDF" "$WORK_DIR/root-app/$APP_NAME/"
else
    echo "Warning: manual PDF not found at $MANUAL_PDF -- installer will ship without it" >&2
fi

# cp -R leaves AppleDouble resource-fork sidecar files (._*) in the payload on some
# filesystems -- harmless but sloppy for a real installer, strip them before packaging.
find "$WORK_DIR/root-vst3" "$WORK_DIR/root-au" "$WORK_DIR/root-app" \
    \( -name '._*' -o -name '.DS_Store' \) -delete

# ---------------------------------------------------------------------------
# 3. Build one component package per install location
# ---------------------------------------------------------------------------
echo "==> Building component packages"
mkdir -p "$WORK_DIR/pkgs"

pkgbuild --root "$WORK_DIR/root-vst3" \
         --identifier "$IDENTIFIER_PREFIX.vst3" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$WORK_DIR/pkgs/vst3.pkg" > /dev/null

pkgbuild --root "$WORK_DIR/root-au" \
         --identifier "$IDENTIFIER_PREFIX.au" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         "$WORK_DIR/pkgs/au.pkg" > /dev/null

pkgbuild --root "$WORK_DIR/root-app" \
         --identifier "$IDENTIFIER_PREFIX.app" \
         --version "$VERSION" \
         --install-location "/Applications" \
         "$WORK_DIR/pkgs/app.pkg" > /dev/null

# ---------------------------------------------------------------------------
# 4. Combine into one distribution package with a welcome/readme screen
# ---------------------------------------------------------------------------
echo "==> Building distribution package"
cat > "$WORK_DIR/welcome.txt" <<EOF
This installs Wishcraft Mastering Limiter $VERSION:

  - VST3  -> /Library/Audio/Plug-Ins/VST3/
  - Audio Unit (AU)  -> /Library/Audio/Plug-Ins/Components/
  - Standalone app + PDF manual -> /Applications/$APP_NAME/

This installer is not yet code-signed. If macOS blocks it as being from an
unidentified developer, right-click the installer (or the app) and choose
Open, or allow it in System Settings > Privacy & Security.
EOF

cat > "$WORK_DIR/license.txt" <<EOF
Wishcraft Mastering Limiter
Concept, design, and specification by Glenn Burgos.
(C) $(date +%Y) Glenn Burgos.
EOF

productbuild --synthesize \
    --package "$WORK_DIR/pkgs/vst3.pkg" \
    --package "$WORK_DIR/pkgs/au.pkg" \
    --package "$WORK_DIR/pkgs/app.pkg" \
    "$WORK_DIR/distribution.xml" > /dev/null

# --synthesize only lists <pkg-ref>s; add the readable title/options productbuild needs
# for a proper install UI (welcome/license screens, sane window title).
python3 - "$WORK_DIR/distribution.xml" "$VERSION" <<'PYEOF'
import sys
path, version = sys.argv[1], sys.argv[2]
with open(path) as f:
    xml = f.read()
xml = xml.replace(
    "<installer-gui-script minSpecVersion=\"1\">",
    "<installer-gui-script minSpecVersion=\"1\">\n"
    "    <title>Wishcraft Mastering Limiter</title>\n"
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
    "$WORK_DIR/Wishcraft Mastering Limiter $VERSION.pkg" > /dev/null

# ---------------------------------------------------------------------------
# 5. Wrap the finished .pkg in a DMG for distribution
# ---------------------------------------------------------------------------
echo "==> Building DMG"
DMG_STAGE="$WORK_DIR/dmg-stage"
mkdir -p "$DMG_STAGE"
cp "$WORK_DIR/Wishcraft Mastering Limiter $VERSION.pkg" "$DMG_STAGE/"

DMG_PATH="$OUT_DIR/Wishcraft Mastering Limiter $VERSION.dmg"
hdiutil create -volname "Wishcraft Mastering Limiter $VERSION" \
    -srcfolder "$DMG_STAGE" \
    -ov -format UDZO \
    "$DMG_PATH" > /dev/null

echo "==> Done: $DMG_PATH"
