#!/bin/bash
# package_app.sh — build DOOM.app, icon, zip and dmg into dist/
set -e
cd "$(dirname "$0")/.."

APP_NAME="DOOM"
BINARY="bin/doom"
DIST="dist"
APP="$DIST/$APP_NAME.app"

echo "==> staging $APP"
rm -rf "$APP" "$DIST/$APP_NAME-macOS.zip" "$DIST/$APP_NAME.dmg"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp "$BINARY" "$APP/Contents/MacOS/doom"

# --- icon: render with the engine, convert bmp -> icns ---
if command -v iconutil >/dev/null && command -v sips >/dev/null; then
  echo "==> generating icon"
  TMPICON="$(mktemp -d)"
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$BINARY" --icon "$TMPICON/icon_512.bmp"
  for size in 16 32 64 128 256 512; do
    sips -s format png -z $size $size "$TMPICON/icon_512.bmp" \
        --out "$TMPICON/icon_${size}x${size}.png" >/dev/null
  done
  # retina @2x variants
  cp "$TMPICON/icon_32x32.png"   "$TMPICON/icon_16x16@2x.png"
  cp "$TMPICON/icon_64x64.png"   "$TMPICON/icon_32x32@2x.png"
  cp "$TMPICON/icon_256x256.png" "$TMPICON/icon_128x128@2x.png"
  cp "$TMPICON/icon_512x512.png" "$TMPICON/icon_256x256@2x.png" 2>/dev/null || true
  mkdir -p "$TMPICON/icon.iconset"
  mv "$TMPICON"/icon_*.png "$TMPICON/icon.iconset/"
  iconutil -c icns "$TMPICON/icon.iconset" -o "$APP/Contents/Resources/doom.icns"
  rm -rf "$TMPICON"
else
  echo "!! sips/iconutil not found, skipping icon"
fi

# --- Info.plist ---
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>                 <string>DOOM</string>
    <key>CFBundleDisplayName</key>          <string>DOOM</string>
    <key>CFBundleExecutable</key>           <string>doom</string>
    <key>CFBundleIdentifier</key>           <string>org.doommac.engine</string>
    <key>CFBundleVersion</key>              <string>1.0.0</string>
    <key>CFBundleShortVersionString</key>   <string>1.0.0</string>
    <key>CFBundlePackageType</key>          <string>APPL</string>
    <key>CFBundleIconFile</key>             <string>doom</string>
    <key>LSMinimumSystemVersion</key>       <string>12.0</string>
    <key>LSApplicationCategoryType</key>    <string>public.app-category.games</string>
    <key>NSHighResolutionCapable</key>      <true/>
    <key>NSHumanReadableCopyright</key>
    <string>Original engine, written in C. Not affiliated with id Software.</string>
</dict>
</plist>
PLIST

# ad-hoc codesign so Gatekeeper doesn't complain about broken signatures
codesign --force --deep -s - "$APP" 2>/dev/null || echo "!! codesign failed (continuing)"

# sanity: the bundled binary must run headless
echo "==> smoke-testing bundled binary"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$APP/Contents/MacOS/doom" --version

echo "==> zip"
cd "$DIST"
zip -qry "$APP_NAME-macOS.zip" "$APP_NAME.app"

echo "==> dmg"
hdiutil create -volname "$APP_NAME" -srcfolder "$APP_NAME.app" -ov -format UDZO "$APP_NAME.dmg" >/dev/null

cd ..
echo "==> done:"
ls -lh "$DIST" | awk '{print "   " $9 " (" $5 ")"}'
