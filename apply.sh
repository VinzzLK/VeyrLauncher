#!/usr/bin/env bash
# VeyrLauncher patcher.
# Mengubah checkout ZalithLauncher2 yang bersih menjadi VeyrLauncher
# (package com.veyrzxo.veyrlauncher + optimasi Snapdragon 870).
#
# Pemakaian:  bash apply.sh [folder_Z2] [folder_tujuan]
#   default:  bash apply.sh ~/storage/downloads/Z2 ~/VeyrLauncher
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="${1:-$HOME/storage/downloads/Z2}"
DST="${2:-$HOME/VeyrLauncher}"
OLD_PKG="com.movtery.zalithlauncher"
NEW_PKG="com.veyrzxo.veyrlauncher"

[ -f "$SRC/ZalithLauncher/build.gradle.kts" ] || { echo "ERROR: $SRC bukan folder Z2 yang valid"; exit 1; }
[ -d "$SRC/gradle" ] || echo "PERINGATAN: $SRC/gradle tidak ada (libs.versions.toml + wrapper). Build gagal tanpa folder itu, pastikan ada di clone asli lu."
[ ! -e "$DST" ] || { echo "ERROR: $DST sudah ada. Hapus dulu atau pilih folder tujuan lain."; exit 1; }

echo "[1/4] Menyalin $SRC -> $DST"
cp -r "$SRC" "$DST"

echo "[2/4] Menimpa file yang dimodifikasi"
cp -r "$HERE/overlay/." "$DST/"

echo "[3/4] Rename package $OLD_PKG -> $NEW_PKG (hanya modul app, LWJGL/ tidak disentuh)"
cd "$DST/ZalithLauncher"
grep -rlIZ -e "com.movtery.zalithlauncher" -e "com/movtery/zalithlauncher" -e "Java_com_movtery_zalithlauncher_" \
    src build.gradle.kts proguard-rules.pro \
  | xargs -0 sed -i \
      -e 's/com\.movtery\.zalithlauncher/com.veyrzxo.veyrlauncher/g' \
      -e 's#com/movtery/zalithlauncher#com/veyrzxo/veyrlauncher#g' \
      -e 's/Java_com_movtery_zalithlauncher_/Java_com_veyrzxo_veyrlauncher_/g'

# applicationId jadi persis com.veyrzxo.veyrlauncher (tanpa suffix .v2)
sed -i '/applicationIdSuffix = "\.v2"/d' build.gradle.kts
# fixture unit test mengikuti applicationId baru
grep -rlIZ "veyrlauncher\.v2\.debug" src | xargs -0 -r sed -i 's/veyrlauncher\.v2\.debug/veyrlauncher.debug/g'

for set in main test androidTest; do
  OLD_DIR="src/$set/java/com/movtery/zalithlauncher"
  if [ -d "$OLD_DIR" ]; then
    mkdir -p "src/$set/java/com/veyrzxo"
    mv "$OLD_DIR" "src/$set/java/com/veyrzxo/veyrlauncher"
    rmdir "src/$set/java/com/movtery" 2>/dev/null || true
  fi
done

echo "[4/4] Cek sisa referensi lama"
LEFT="$(grep -rlI -e "com.movtery.zalithlauncher" -e "com/movtery/zalithlauncher" src build.gradle.kts proguard-rules.pro || true)"
if [ -n "$LEFT" ]; then
  echo "PERINGATAN: masih ada referensi lama di:"; echo "$LEFT"
else
  echo "OK: tidak ada sisa referensi $OLD_PKG di modul app"
fi
chmod +x "$DST/gradlew" 2>/dev/null || true

echo
echo "Selesai. Project siap di: $DST"
echo "Build: lewat GitHub Actions (.github/workflows/veyr-build.yml) atau ./gradlew :ZalithLauncher:assembleDebug -Darch=arm64"
