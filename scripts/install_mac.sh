#!/bin/bash
# BAKERY installer: copies BAKERY.vst3 and BAKERY.component into the system plug-in folders.
# Uses /Library/... (all users) because ~/Library/Audio/Plug-Ins/VST3 is root-owned on this Mac.
# Asks before replacing an existing copy and keeps a dated backup. Needs your Mac password (sudo).
set -euo pipefail
cd "$(dirname "$0")"
stamp="$(date +%Y%m%d-%H%M%S)"
install_one() {
  local src="$1" dest_dir="$2" name; name="$(basename "$src")"
  [[ -d "$src" ]] || { echo "Keep this script next to $name."; exit 1; }
  local dest="$dest_dir/$name"
  sudo mkdir -p "$dest_dir"
  if [[ -e "$dest" ]]; then
    echo "$name is already installed in $dest_dir."
    read -r -p "Replace it (a backup is kept)? Type yes: " answer
    if [[ "$answer" != yes ]]; then echo "Skipped $name."; return; fi
    sudo mv "$dest" "$HOME/Desktop/$name.backup-$stamp"
    echo "Old copy moved to ~/Desktop/$name.backup-$stamp"
  fi
  sudo cp -R "$src" "$dest"
  sudo xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
  codesign --verify --deep --strict "$dest"
  echo "Installed $dest"
}
echo "Close FL Studio before installing."
install_one "$PWD/BAKERY.vst3" "/Library/Audio/Plug-Ins/VST3"
install_one "$PWD/BAKERY.component" "/Library/Audio/Plug-Ins/Components"
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo
echo "Done. In FL Studio: Options > Manage plugins > Find installed plugins (or Start scan),"
echo "then add BAKERY from the Channel rack (+) or the plugin database (Generators)."
