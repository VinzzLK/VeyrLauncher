# VeyrLauncher (fork Zalith Launcher 2) - daftar perubahan

Target: Snapdragon 870 / Poco F3. FPS tetap uncapped, minim thermal throttling.

## Cara pakai (Termux)
    unzip VeyrLauncher-patch.zip -d ~/veyr-patch
    bash ~/veyr-patch/apply.sh ~/storage/downloads/Z2 ~/VeyrLauncher
Script menyalin Z2 ke folder baru, menimpa file yang diubah, lalu rename package.
Folder `gradle/` (libs.versions.toml + wrapper) harus ada di Z2 asli.

## Performa
1. Render thread di-pin ke big core (prime + gold), hanya thread itu, bukan seluruh JVM
   - jni/bigcoreaffinity.c + .h (ditulis ulang, bug read() di luar buffer dibenerin)
   - dipanggil dari calculateFPS() (jalur GL dan Vulkan)
2. ADPF (APerformanceHint, Android 13+) lewat dlsym, no-op di Android lama
   - jni/adpf.c + .h, target default 600 fps supaya boost CPU tetap aktif di 400+ FPS
     (env VEYR_ADPF_TARGET_FPS, 30..2000; turunin kalau kepanasan)
3. -XX:ActiveProcessorCount = jumlah big core (4 di SD870), bukan 8
   - utils/device/CpuTopology.kt, dipakai di game/launch/Launcher.kt
4. Toggle di Pengaturan Renderer: Pin render thread, ADPF, Limit JVM ke big core (default ON)
   - env: VEYR_PIN_RENDER_THREAD=0 / VEYR_ADPF=0 untuk mematikan

## Stabilitas
- gl_bridge.c / egl_bridge.c: NULL-check getenv (POJAV_RENDERER, LIBGL_ES)
- gl_bridge.c: usleep(750000) diganti polling 10 ms, berhenti lebih awal kalau window sudah mati
- jre_launcher.c: handler SIGABRT tidak lagi busy-loop (pause())
- utils/device/ThermalMonitor.kt: log status thermal (log saja, tidak membatasi FPS)

## Fork
- Package + applicationId: com.veyrzxo.veyrlauncher (tanpa suffix .v2), debug: .debug
- launcher_name=VeyrLauncher, launcher_app_name=Veyr Launcher, launcher_short_name=VL, versi 2.7-veyr1
- Label "Unofficial Modified Version" di splash dan top bar utama (syarat Additional Terms lisensi)
- LWJGL/ dan modul library lain TIDAK diubah (jar prebuilt masih merujuk com.movtery.zalithlauncher.CursorRegistry)
- .github/workflows/veyr-build.yml: build debug arm64 manual (belum dites)

## Belum dikerjakan
- Upgrade Mesa/Turnip (itu plugin renderer biner, bukan source ini)
- Preset GC/JVM (perlu cek GC apa yang ada di runtime bawaan dan benchmark dulu)
- Frame limiter (sengaja tidak, sesuai permintaan uncapped)
