#!/bin/bash
# build_release.sh
# Builds a self-contained Konvolute.app and zips it for sharing.
# Usage: ./build_release.sh
set -e

OF="/Users/sdoc0001/Documents/dev/of_v0.12.1"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP="$PROJECT_DIR/bin/Konvolute.app"
ZIP="$PROJECT_DIR/bin/Konvolute.zip"

echo "▶ Building Release (Universal Binary)..."
xcodebuild \
  -project "$PROJECT_DIR/Konvolute.xcodeproj" \
  -scheme "Konvolute Release" \
  -configuration Release \
  -arch arm64 -arch x86_64 \
  ONLY_ACTIVE_ARCH=NO \
  build

if [ ! -d "$APP" ]; then
  echo "❌ Build failed — $APP not found."
  exit 1
fi

echo "▶ Bundling dylibs..."
if [ -f "$OF/scripts/osx/fixup_bundle.sh" ]; then
  bash "$OF/scripts/osx/fixup_bundle.sh" "$APP"
else
  echo "⚠️  fixup_bundle.sh not found at $OF/scripts/osx/ — skipping dylib bundling."
  echo "   The app may not run on machines without OF installed."
fi

# Ensure data folder is inside the bundle for distribution
if [ -d "$PROJECT_DIR/bin/data" ]; then
  echo "▶ Bundling data folder..."
  # Clean old data if exists
  rm -rf "$APP/Contents/Resources/data"
  mkdir -p "$APP/Contents/Resources/data"
  cp -R "$PROJECT_DIR/bin/data/" "$APP/Contents/Resources/data/"
fi

# Update application icon
if [ -f "$PROJECT_DIR/of.icns" ]; then
  echo "▶ Updating application icon..."
  cp "$PROJECT_DIR/of.icns" "$APP/Contents/Resources/of.icns"
  touch "$APP"
fi

echo "▶ Zipping..."
rm -f "$ZIP"
cd "$PROJECT_DIR/bin"
zip -r Konvolute.zip Konvolute.app

echo "✅ Done: bin/Konvolute.zip ($(du -sh "$ZIP" | cut -f1))"
