#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------
# Usage:
#   ./compile-with-docker.sh [Preset] [CMake options...]
# Examples:
#   ./compile-with-docker.sh Custom
#   ./compile-with-docker.sh Bandscope -DENABLE_SPECTRUM=ON
#   ./compile-with-docker.sh Broadcast -DENABLE_FEAT_F4HWN_GAME=ON -DENABLE_NOAA=ON
#   ./compile-with-docker.sh All
# Default preset: "Custom"
# ---------------------------------------------

IMAGE=uvk1-uvk5v3
PRESET=${1:-USB}

shift || true  # remove preset from arguments if present

# Any remaining args will be treated as CMake cache variables
CLEAN_BUILD=false
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--clean)
      CLEAN_BUILD=true
      shift
      ;;
    *)
      EXTRA_ARGS+=("$1")
      shift
      ;;
  esac
done

# ---------------------------------------------
# Nettoyage si l'option est activée
# ---------------------------------------------
if [ "$CLEAN_BUILD" = true ]; then
  echo "🧹 Cleaning build directory..."
  rm -rf build/
fi

# ---------------------------------------------
# Validate preset name
# ---------------------------------------------
if [[ ! "$PRESET" =~ ^(Dev|USB|RS232|NO_COM|All)$ ]]; then
  echo "❌ Unknown preset: '$PRESET'"
  echo "Valid presets are: Dev USB RS232 NO_COM All"
  exit 1
fi

VERSION_FILE="App/version.h"
if [ -f "$VERSION_FILE" ]; then
    # On cherche la ligne, on extrait le 3ème mot (le numéro)
    CURRENT_VERSION=$(grep "#define APP_VERSION" "$VERSION_FILE" | awk '{print $3}')
    
    if [ -n "$CURRENT_VERSION" ]; then
        NEW_VERSION=$((CURRENT_VERSION + 1))
        # Utilisation de sed pour remplacer la ligne entière
        sed -i "s/#define APP_VERSION $CURRENT_VERSION/#define APP_VERSION $NEW_VERSION/" "$VERSION_FILE"
        echo "🔢 Version mise à jour : $CURRENT_VERSION -> $NEW_VERSION"
    else
        echo "⚠️  Ligne '#define APP_VERSION' introuvable dans $VERSION_FILE"
    fi
else
    echo "❌ Fichier $VERSION_FILE introuvable au chemin spécifié."
fi

# ---------------------------------------------
# Build the Docker image (only needed once)
# ---------------------------------------------
if [[ "$(docker images -q $IMAGE)" == "" ]]; then
  echo "Building Docker image..."
  docker build -t "$IMAGE" .
fi

# ---------------------------------------------
# Clean existing CMake cache to ensure toolchain reload
# ---------------------------------------------
# rm -rf build
export MSYS_NO_PATHCONV=1
# ---------------------------------------------
# Function to build one preset
# ---------------------------------------------
build_preset() {
  local preset="$1"
  echo -e "\n=== 🚀 Building: ${preset} ===\n---------------------------------------------"

  docker run --rm -u $(id -u):$(id -g) -v "$PWD":/src -w /src "$IMAGE" \
  bash -c "cmake --preset ${preset} ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} && \
           cmake --build --preset ${preset} -j" \
  2>&1 | sed "s|/src/|C:/Perso/Robzyl_K1/|g"

  docker run --rm -v "$PWD":/src -w /src "$IMAGE" \
    arm-none-eabi-size ./build/${preset}/ROBZYL.K1.${preset}.elf

  echo "✅ Done: ${preset}"
}

# text : Votre code (Flash).
# data + bss : Votre RAM statique (dont vos buffers USB et BParams).
# La somme data + bss + _Min_Stack_Size + _Min_Heap_Size doit rester inférieure à la RAM totale du PY32 (8 Ko ou 16 Ko selon la variante).
# Pour un projet stable, la recommandation standard est de ne pas dépasser 80% de la RAM totale en combinant data + bss + stack + heap.

# ---------------------------------------------
# Handle 'All' preset
# ---------------------------------------------
if [[ "$PRESET" == "All" ]]; then
  PRESETS=(Dev USB RS232 NO_COM)
  for p in "${PRESETS[@]}"; do
    build_preset "$p"
  done
  echo ""
  echo "🎉 All presets built successfully!"
else
  build_preset "$PRESET"
fi

# ---------------------------------------------
# Automatic flash
# ---------------------------------------------

# Définition du nom du binaire selon le preset
case "$PRESET" in
  "USB")
    BIN_NAME="ROBZYL.K1.USB.bin"
    ;;
  "Dev")
    BIN_NAME="ROBZYL.K1.Dev.bin"
    ;;
  "All")
    # Pour 'All', on peut flasher un binaire par défaut ou ignorer
    echo "⏭️  Preset 'All' détecté, flash automatique ignoré."
    exit 0
    ;;
  *)
    ;;
esac

echo "⚡ Flashing firmware: ${BIN_NAME} on COM14..."

# Vérification de l'existence du fichier avant de flasher
IFILE="./build/${PRESET}/${BIN_NAME}"

if [[ -f "$IFILE" ]]; then
    python flash.py "$IFILE" -p COM14
    echo "✅ Flash terminé avec succès !"
else
    echo "❌ Erreur : Le fichier binaire est introuvable : $IFILE"
    exit 1
fi
