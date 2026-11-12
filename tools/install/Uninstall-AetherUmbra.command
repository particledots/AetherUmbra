#!/bin/bash
# Removes the AetherUmbra test shells (Audio Units and VST3s) from this Mac. Double-click to run; asks for the Mac
# password once. It deletes only these eight items, if they exist:
NAMES=("AetherUmbra Test Instrument" "AetherUmbra Test Music Effect")
ITEMS=()
for N in "${NAMES[@]}"; do
  ITEMS+=("/Library/Audio/Plug-Ins/Components/$N.component" "/Library/Audio/Plug-Ins/VST3/$N.vst3"
          "$HOME/Library/Audio/Plug-Ins/Components/$N.component" "$HOME/Library/Audio/Plug-Ins/VST3/$N.vst3")
done

echo "This will remove, if present:"
for P in "${ITEMS[@]}"; do [ -e "$P" ] && echo "  $P"; done
echo
read -n 1 -s -r -p "Press y to remove, any other key to cancel: " ANS
echo
if [ "$ANS" != "y" ]; then echo "Cancelled. Nothing was removed."; read -n 1 -s -r -p "Press any key to close."; exit 0; fi

for P in "${ITEMS[@]}"; do
  case "$P" in
    "$HOME"/*) rm -rf "$P" ;;
    *) sudo rm -rf "$P" ;;
  esac
done
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Removed. In Logic: Settings > Plug-in Manager > Reset & Rescan Selection."
read -n 1 -s -r -p "Press any key to close."
