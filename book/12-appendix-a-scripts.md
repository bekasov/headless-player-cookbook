---
aliases:
  - Приложение А
  - Скрипты
tags:
  - книга/приложение
---

# Приложение А. Скрипты

Все скрипты живут на плате в домашней папке. Те же файлы лежат в папке `scripts/`
репозитория (скрипты второго стенда в `scripts/a20/`), их можно скопировать на плату
целиком:

```fish
# [сервер]
scp scripts/*.sh пользователь@адрес-платы:
```

Если репозитория под рукой нет, создавайте их в nvim или через
`printf '%s\n' 'строка' 'строка' ... > файл` (heredoc во fish не работает).
После создания: `chmod +x файл` и проверка `cat файл`.

## `make-gst-mix.sh`: набор плагинов с исправленным `v4l2codecs`

Пересоздавать после установки любого пакета GStreamer.

```bash
#!/bin/bash
rm -rf "$HOME/gst-mix"; mkdir -p "$HOME/gst-mix"
for f in /usr/lib/aarch64-linux-gnu/gstreamer-1.0/*.so; do
  [ "$(basename "$f")" = libgstv4l2codecs.so ] || ln -s "$f" "$HOME/gst-mix/"
done
ln -s "$HOME/build-gst/gst-plugins-bad-1.26.11/build/sys/v4l2codecs/libgstv4l2codecs.so" "$HOME/gst-mix/"
ls "$HOME/gst-mix" | wc -l
```

После пересоздания: `rm -f ~/test/gst-registry-mix.bin`.

## `play-av.sh`: видео и звук через композитор

Запускать **только с консоли платы**: `./play-av.sh ~/test/фильм.mkv`.

```bash
#!/bin/bash
export GST_REGISTRY="$HOME/test/gst-registry-mix.bin"
export GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
Q="queue max-size-buffers=0 max-size-bytes=0 max-size-time=10000000000"
{ time cage -- timeout 80 stdbuf -oL gst-launch-1.0 -v filesrc location="$1" \
    ! matroskademux name=d \
    d.video_0 ! $Q ! h265parse ! v4l2slh265dec \
      ! fpsdisplaysink text-overlay=false video-sink=waylandsink \
    d.audio_0 ! $Q ! aacparse ! faad ! audioconvert ! audioresample \
      ! alsasink device=plughw:1,0 ; } > "$HOME/test/play-av-$(basename "$1").log" 2>&1
```

Чтение результата по SSH:

```bash
grep -E 'last-message|ERROR|Got EOS|real|user|sys' ~/test/play-av-фильм.mkv.log | tail -n 6
```

## `cma-min.sh`: замер минимума свободной CMA

Запускать с консоли платы, файлы списком: `./cma-min.sh a.mkv b.mkv c.mkv`.
Результат в `~/test/cma2.txt`.

```bash
#!/bin/bash
export GST_REGISTRY="$HOME/test/gst-registry-mix.bin"
export GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
: > "$HOME/test/cma2.txt"
for f in "$@"; do
  ( while :; do grep CmaFree /proc/meminfo; sleep 0.3; done ) > "$HOME/test/cma-samples.tmp" &
  S=$!
  L="$HOME/test/seq-$(basename "$f")-$(date +%s).log"
  cage -- timeout 8 stdbuf -oL gst-launch-1.0 -v filesrc location="$f" ! matroskademux name=d \
    d.video_0 ! h265parse ! v4l2slh265dec ! fpsdisplaysink text-overlay=false video-sink=waylandsink \
    > "$L" 2>&1
  kill $S 2>/dev/null
  echo "$(basename "$f"): min CmaFree $(tr -s " " < "$HOME/test/cma-samples.tmp" | cut -d" " -f2 | sort -n | head -n 1) kB, errors $(grep -c "Not enough memory" "$L")" >> "$HOME/test/cma2.txt"
done
```

## `bisect.sh`: метод исключения без экрана

Запускать по SSH: `./bisect.sh ~/test/фильм.mkv`. Каждый вариант идёт 12 секунд.
Буквы вариантов совпадают с таблицей рецепта 10.4. Вариант D выводит звук на HDMI, поэтому
ему нужен `alsasink` в наборе плагинов.

> [!note] Что восстановлено
> На стенде варианты D, E, G, H, I и N запускались вручную. Здесь они восстановлены по
> описанию из таблицы и в таком виде на стенде не прогонялись.

```bash
#!/bin/bash
export GST_REGISTRY="$HOME/test/gst-registry-mix.bin"
export GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
F="$1"
run() { n="$1"; shift
  timeout 12 stdbuf -oL gst-launch-1.0 -v filesrc location="$F" ! matroskademux name=d "$@" \
    > "$HOME/test/bis-$n.log" 2>&1
  echo "$n: PREROLLED=$(grep -c PREROLLED "$HOME/test/bis-$n.log") $(grep last-message "$HOME/test/bis-$n.log" | tail -n 1 | grep -oE "rendered: [0-9]+") ERRORS=$(grep -c ERROR "$HOME/test/bis-$n.log")"
}
V="d.video_0 ! h265parse ! v4l2slh265dec ! fpsdisplaysink text-overlay=false video-sink=fakesink"
VQ="d.video_0 ! queue ! h265parse ! v4l2slh265dec ! fpsdisplaysink text-overlay=false video-sink=fakesink"
run A_video $V
run B_video_queue $VQ
run C_av_fakesink $V d.audio_0 ! aacparse ! faad ! fakesink
run D_av_hdmi $V d.audio_0 ! aacparse ! faad ! audioconvert ! audioresample ! alsasink device=plughw:1,0
run E_av_queues $VQ d.audio_0 ! queue ! aacparse ! faad ! fakesink
run F_audio_only d.audio_0 ! aacparse ! faad ! fakesink
run G_av_raw_audio $V d.audio_0 ! fakesink
run H_av_raw_audio_queues $VQ d.audio_0 ! queue ! fakesink
run I_av_aacparse $V d.audio_0 ! aacparse ! fakesink
run M_raw_both d.video_0 ! fakesink d.audio_0 ! fakesink
run N_raw_h265parse d.video_0 ! h265parse ! fakesink d.audio_0 ! fakesink
run J_av_huge_aq $V d.audio_0 ! queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 ! fakesink
```

## Второй стенд (Banana Pi M1, A20, armhf)

Скрипты ниже работали на втором стенде. `make-gst-mix.sh` тот же, что выше, но системные
плагины на armhf лежат в другой папке: замените путь на
`/usr/lib/arm-linux-gnueabihf/gstreamer-1.0`. На этой плате heredoc в bash работает, но
создавать файлы, как и раньше, удобно в nvim.

В репозитории эти скрипты лежат в `scripts/a20/`. `play-retry.sh` и `catch-fail.sh` вызывают
`play.sh` из папки `~/bpi-m1-notes/`, где он жил на стенде:

```fish
# [сервер]
scp scripts/a20/* пользователь@адрес-платы:bpi-m1-notes/
```

### `play.sh`: видео H.264 на плоскость и звук AC-3 по HDMI

Запускать можно по SSH и без `sudo` (пользователь в группе `video`): `./play.sh фильм.mp4`.

```bash
#!/bin/bash
# usage: play.sh FILE   (H.264 mp4/mkv with AC-3 audio; HDMI video+audio)
export GST_REGISTRY="$HOME/.cache/gst-registry-mix.bin" GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
exec gst-launch-1.0 -q filesrc location="$1" ! qtdemux name=d \
  d.video_0 ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! h264parse ! v4l2slh264dec ! queue max-size-buffers=4 max-size-time=0 max-size-bytes=0 ! kmssink driver-name=sun4i-drm plane-id=37 skip-vsync=true qos=false \
  d.audio_0 ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! avdec_ac3 ! audioconvert ! audio/x-raw,channels=2 ! audioresample ! alsasink device=plughw:CARD=sun4ihdmi,DEV=0
```

### `play-retry.sh`: повтор при сбое памяти на старте

До трёх попыток, если запуск упал с `Not enough memory` в первые 10 секунд. Каждый случай
пишется в `~/bpi-m1-notes/retry.log` (рецепт 9.5).

```bash
#!/bin/bash
# usage: play-retry.sh FILE   (play.sh with up to 3 attempts on "Not enough memory" at start)
f="$1"; log="$(mktemp)"
for try in 1 2 3; do
  start=$SECONDS
  "$HOME/bpi-m1-notes/play.sh" "$f" 2> "$log"
  rc=$?
  if [ "$rc" -ne 0 ] && grep -q 'Not enough memory' "$log" && [ $((SECONDS - start)) -lt 10 ]; then
    echo "$(date '+%F %T') attempt $try failed: not enough memory ($f)" >> "$HOME/bpi-m1-notes/retry.log"
    sleep 1
    continue
  fi
  break
done
cat "$log" >&2
rm -f "$log"
exit "$rc"
```

### `run-fps2.sh`: таблица запусков (рецепты 7.9 и 11.7)

`./run-fps2.sh имя секунды 'приёмник' true|false`; результаты в `~/test/имя.log`,
`имя.time`, `имя.vm`. Файл фильма прописан внутри.

```bash
#!/bin/bash
# usage: run-fps2.sh NAME SECONDS 'video-sink string' SYNC(true|false)
export GST_REGISTRY="$HOME/test/gst-registry-mix-root.bin" GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
vmstat 1 $(( $2 - 2 )) > "$HOME/test/$1.vm" &
( time sudo env GST_REGISTRY="$GST_REGISTRY" GST_PLUGIN_SYSTEM_PATH_1_0="$GST_PLUGIN_SYSTEM_PATH_1_0" timeout "$2" stdbuf -oL gst-launch-1.0 -v filesrc location="$HOME/film_test.mp4" ! qtdemux name=d d.video_0 ! h264parse ! v4l2slh264dec ! fpsdisplaysink text-overlay=false sync="$4" video-sink="$3" > "$HOME/test/$1.log" 2>&1 ) 2> "$HOME/test/$1.time"
wait
```

Чтение: `grep -o 'rendered: [0-9]*, dropped: [0-9]*, current: [0-9.]*, average: [0-9.]*' ~/test/имя.log | tail -n 4`.

### `run-lat.sh`: опоздание каждого кадра (рецепты 10.8 и 11.8)

`./run-lat.sh имя файл секунды 'элементы перед приёмником, с ! в конце' 'свойства kmssink'`;
последние два параметра могут быть пустыми (`''`).

```bash
#!/bin/bash
# usage: run-lat.sh NAME FILE SECONDS 'middle elements ending with ! (may be empty)' 'extra kmssink props (may be empty)'
mid="${4:-}"
extra="${5:-}"
( for i in $(seq 1 $(( $3 - 2 ))); do awk '/CmaFree/ {print $2}' /proc/meminfo; sleep 1; done > "$HOME/test/$1.cma" ) &
sudo env GST_REGISTRY="$HOME/test/gst-registry-mix-root.bin" GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix" GST_DEBUG=basesink:5 timeout "$3" gst-launch-1.0 filesrc location="$2" ! qtdemux name=d d.video_0 ! h264parse ! v4l2slh264dec ! $mid kmssink driver-name=sun4i-drm plane-id=37 skip-vsync=true qos=false $extra > "$HOME/test/$1.log" 2>&1
wait
grep 'clock returned' "$HOME/test/$1.log" | awk '{j=$NF; s=1; c=substr(j,1,1); if (c=="-") {s=-1; j=substr(j,2)}; split(j,a,":"); print s*(a[1]*3600+a[2]*60+a[3])*1000}' | sort -n | awk '{v[NR]=$1; if ($1>0) l0++; if ($1>20) l20++; if ($1>40) l40++} END {printf "n=%d min=%.1f p50=%.1f p90=%.1f p99=%.1f max=%.1f ms; late>0: %d, >20ms: %d, >40ms: %d\n", NR, v[1], v[int(NR*0.5)], v[int(NR*0.9)], v[int(NR*0.99)], v[NR], l0, l20, l40}'
echo "CmaFree min kB: $(sort -n "$HOME/test/$1.cma" | head -n 1)"
```

### `catch-fail.sh`: поймать плавающий сбой (рецепт 9.5)

`./catch-fail.sh файл число_попыток`. Останавливается на первом сбое и выводит журнал
плагина перед ошибкой, журнал ядра после метки и свободную CMA по кускам. Перед запуском
`sudo true`, чтобы цикл не остановился на запросе пароля.

```bash
#!/bin/bash
# usage: catch-fail.sh FILE N  (repeat 4-second starts, stop at the first "Not enough memory")
f="$1"; n="$2"
for i in $(seq 1 "$n"); do
  before=$(awk '/CmaFree/ {print $2}' /proc/meminfo)
  sudo sh -c "echo MARK-run-$i > /dev/kmsg"
  GST_DEBUG='v4l2codecs*:5' timeout 4 "$HOME/bpi-m1-notes/play.sh" "$f" > "$HOME/test/catch.log" 2>&1
  if grep -q 'Not enough memory' "$HOME/test/catch.log"; then
    echo "run $i of $n: FAIL, CmaFree before=$before kB"
    grep -B6 -m1 'Not enough memory' "$HOME/test/catch.log"
    echo "--- kernel log since the mark:"
    sudo dmesg | sed -n "/MARK-run-$i\$/,\$p"
    echo "--- CMA free blocks:"
    sudo grep ' CMA ' /proc/pagetypeinfo
    cp "$HOME/test/catch.log" "$HOME/test/catch-fail.log"
    exit 1
  fi
done
echo "no failures in $n runs"
```

Метка в журнале ядра (`echo ... > /dev/kmsg`) позволяет потом показать только то, что
случилось после неё: `sed -n "/МЕТКА$/,\$p"`.

### `v4l2codecs-sunxi-tiled-drm.patch`: патч для v4l2codecs

Правка `sys/v4l2codecs/gstv4l2format.c` из gst-plugins-bad 1.26.11 для прямого вывода
плиточных кадров Cedrus на плоскость (рецепты 7.8, 8.6, 8.7). Получена `diff -u` на втором
стенде. К чистым исходникам 1.26.11 он накладывается без смещений, тремя блоками
(проверено `patch --dry-run`, а сборка с ним шла только на стенде). Команды наложения
приведены после патча.

```diff
--- gstv4l2format.c.orig
+++ gstv4l2format.c
@@ -53,7 +53,7 @@
   {V4L2_PIX_FMT_P010,             GST_VIDEO_FORMAT_P010_10LE,       DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID, 0},
   {V4L2_PIX_FMT_YUYV,             GST_VIDEO_FORMAT_YUY2,            DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID, 0},
   {V4L2_PIX_FMT_MM21,             GST_VIDEO_FORMAT_NV12_16L32S,     DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID, 0},
-  {V4L2_PIX_FMT_SUNXI_TILED_NV12, GST_VIDEO_FORMAT_NV12_32L32,      DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID, 0},
+  {V4L2_PIX_FMT_SUNXI_TILED_NV12, GST_VIDEO_FORMAT_NV12_32L32,      DRM_FORMAT_NV12, DRM_FORMAT_MOD_ALLWINNER_TILED, 0},
   {V4L2_PIX_FMT_NV12_4L4,         GST_VIDEO_FORMAT_NV12_4L4,        DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID, 0},
   {V4L2_PIX_FMT_NC12,             GST_VIDEO_FORMAT_UNKNOWN,         DRM_FORMAT_NV12,    DRM_FORMAT_MOD_BROADCOM_SAND128, 2},
   {V4L2_PIX_FMT_NV12,             GST_VIDEO_FORMAT_NV12,            DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID, 0},
@@ -80,6 +80,11 @@
             gst_video_dma_drm_format_from_gst_format (gst_v4l2_descriptions
             [i].gst_fmt, &drm_modifier);
         v4l2_descs[i].drm_modifier = drm_modifier;
+        if (v4l2_descs[i].drm_fourcc == DRM_FORMAT_INVALID &&
+            gst_v4l2_descriptions[i].drm_fourcc != DRM_FORMAT_INVALID) {
+          v4l2_descs[i].drm_fourcc = gst_v4l2_descriptions[i].drm_fourcc;
+          v4l2_descs[i].drm_modifier = gst_v4l2_descriptions[i].drm_modifier;
+        }
 
         info = gst_video_format_get_info (gst_v4l2_descriptions[i].gst_fmt);
         v4l2_descs[i].num_planes = GST_VIDEO_FORMAT_INFO_N_PLANES (info);
@@ -206,6 +211,19 @@
   }
 
   if (drm_fourcc == DRM_FORMAT_NV12
+      && drm_mod == DRM_FORMAT_MOD_ALLWINNER_TILED) {
+    /* Cedrus: bytesperline is the 32-aligned width, chroma follows luma */
+    guint32 bpl = V4L2_TYPE_IS_MULTIPLANAR (fmt->type) ?
+        pix_mp->plane_fmt[0].bytesperline : pix->bytesperline;
+
+    out_drm_info->vinfo.offset[0] = 0;
+    out_drm_info->vinfo.offset[1] = bpl * GST_ROUND_UP_32 (pix_mp->height);
+    out_drm_info->vinfo.stride[0] = bpl;
+    out_drm_info->vinfo.stride[1] = bpl;
+    return TRUE;
+  }
+
+  if (drm_fourcc == DRM_FORMAT_NV12
       && drm_mod == DRM_FORMAT_MOD_BROADCOM_SAND128) {
     out_drm_info->vinfo.offset[1] = pix_mp->height * 128;
     out_drm_info->vinfo.stride[0] = pix_mp->plane_fmt[0].bytesperline;
```

Три части правки: пара DRM в строке таблицы; запасной вариант, который берёт пару из
таблицы, если библиотека GStreamer её не знает; ветка, которая кладёт в шаг и смещение
значения драйвера, а не упакованный шаг плиток.

Наложить патч на исходники из рецепта 8.3:

```bash
# [плата]
cd ~/build-gst/gst-plugins-bad-1.26.11/sys/v4l2codecs
patch --dry-run gstv4l2format.c < ~/bpi-m1-notes/v4l2codecs-sunxi-tiled-drm.patch
patch gstv4l2format.c < ~/bpi-m1-notes/v4l2codecs-sunxi-tiled-drm.patch
```

---

Назад: [Глава 11. Методика](11-method.md) | Далее: [Приложение Б. Словарь](13-appendix-b-glossary.md)
