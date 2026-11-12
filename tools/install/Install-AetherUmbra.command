#!/bin/bash
# Installs AetherUmbra: the Audio Unit (for Logic) only, for all users. (The VST3 is not installed; see the README.)
# Leave this file in the unzipped plugin folder, beside the .component file(s), and double-click it.
# Asks for the Mac password once. It touches only the AetherUmbra .component files next to this file, the same-named
# copies in /Library/Audio/Plug-Ins/Components, and old same-named copies in ~/Library/Audio/Plug-Ins/Components.
cd "$(dirname "$0")" || exit 1
SRC="$(pwd)"
AU_DEST="/Library/Audio/Plug-Ins/Components"
AU_OLD="$HOME/Library/Audio/Plug-Ins/Components"

shopt -s nullglob
AUS=("$SRC"/AetherUmbra*.component)

if [ ${#AUS[@]} -eq 0 ]; then
  echo "Cannot find any AetherUmbra .component next to this file."
  echo "Leave this file in the unzipped plugin folder."
  read -n 1 -s -r -p "Press any key to close."
  exit 1
fi

echo "Found:"
for C in "${AUS[@]}"; do echo "  $(basename "$C")"; done
echo
xattr -dr com.apple.quarantine "$SRC" 2>/dev/null
echo "Your Mac password is needed to write to /Library/Audio/Plug-Ins (nothing shows while you type)."
sudo mkdir -p "$AU_DEST" || { read -n 1 -s -r -p "Password failed. Press any key to close."; exit 1; }

FAILED=0
install_one() {  # $1 = bundle, $2 = destination folder, $3 = old per-user folder
  local NAME
  NAME="$(basename "$1")"
  rm -rf "$3/$NAME"
  sudo rm -rf "$2/$NAME"
  sudo cp -R "$1" "$2/"
  if [ -d "$2/$NAME" ]; then
    echo "Installed: $2/$NAME"
  else
    echo "Install FAILED: $NAME is not in $2"
    FAILED=1
  fi
}
for C in "${AUS[@]}"; do install_one "$C" "$AU_DEST" "$AU_OLD"; done
killall -9 AudioComponentRegistrar 2>/dev/null

echo
if [ $FAILED -eq 0 ]; then
  echo "Done. Open Logic. If the plugin does not appear: Settings > Plug-in Manager > Reset & Rescan Selection."
fi
echo
read -n 1 -s -r -p "Press any key to close."
