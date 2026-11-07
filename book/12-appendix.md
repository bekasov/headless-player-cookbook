# Приложения

## Скрипты

Все скрипты живут на плате в домашней папке. Создавать их удобнее в nvim или через
`printf '%s\n' 'строка' 'строка' ... > файл` (heredoc во fish не работает, а на плате
так проще копировать с сервера). После создания: `chmod +x файл` и проверка `cat файл`.

### `make-gst-mix.sh`: набор плагинов с исправленным `v4l2codecs`

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

### `play11.sh`: видео и звук через композитор

Запускать **только с консоли платы**: `./play11.sh ~/test/фильм.mkv`.

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
      ! alsasink device=plughw:1,0 ; } > "$HOME/test/play11-$(basename "$1").log" 2>&1
```

Чтение результата по SSH:

```
grep -E 'last-message|ERROR|Got EOS|real|user|sys' ~/test/play11-фильм.mkv.log | tail -n 6
```

### `play7.sh`: замер минимума свободной CMA

Запускать с консоли платы, файлы списком: `./play7.sh a.mkv b.mkv c.mkv`.
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

### `bisect.sh`: метод исключения без экрана

Запускать по SSH: `./bisect.sh ~/test/фильм.mkv`. Каждый вариант идёт 12 секунд.

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
run F_audio_only d.audio_0 ! aacparse ! faad ! fakesink
run M_raw_both d.video_0 ! fakesink d.audio_0 ! fakesink
run J_av_huge_aq $V d.audio_0 ! queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 ! fakesink
```

### Второй стенд (Banana Pi M1, A20, armhf)

Скрипты ниже работали на втором стенде. `make-gst-mix.sh` тот же, что выше, но системные
плагины на armhf лежат в другой папке: замените путь на
`/usr/lib/arm-linux-gnueabihf/gstreamer-1.0`. На этой плате heredoc в bash работает, но
создавать файлы, как и раньше, удобно в nvim.

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

---

## Патч для v4l2codecs

Правка `sys/v4l2codecs/gstv4l2format.c` из gst-plugins-bad 1.26.11 для прямого вывода
плиточных кадров Cedrus на плоскость (рецепты 7.8, 8.6, 8.7). Получена `diff -u` на втором
стенде; применение к чистому дереву командой
`cd gst-plugins-bad-1.26.11/sys/v4l2codecs && patch gstv4l2format.c < путь/к/патчу`
мы не проверяли.

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

---

## Словарь

| Термин | Значение |
|---|---|
| **AFBC** | Сжатый формат кадров ARM; его умеет выдавать декодер, но наш дисплей не принимает |
| **ALSA** | Звуковая подсистема ядра; устройства `hw:N,M` и `plughw:N,M` |
| **caps (подписи)** | Описание формата на соединении элементов GStreamer |
| **Cedrus** | Драйвер видеодекодера Allwinner в ядре Linux, «без состояния» |
| **CMA** | Область физически непрерывной памяти для устройств |
| **CRTC** | Часть дисплейного контроллера, формирующая видеосигнал |
| **DMABuf** | Способ передать буфер между устройствами и программами без копирования |
| **Devicetree (DTB)** | Описание железа платы для ядра |
| **DRM/KMS** | Подсистема ядра для графики и вывода изображения |
| **Демультиплексор** | Элемент, раскладывающий контейнер на потоки |
| **EOS** | Конец потока (end of stream) |
| **Main / Main 10** | Профили H.265: 8 и 10 бит |
| **NV12** | Формат кадра YUV 4:2:0, 8 бит, яркость отдельно от цвета |
| **NV12_32L32 (ST12)** | Плиточный вариант NV12 у Allwinner |
| **Плоскость (plane)** | Слой изображения, который дисплей накладывает на другие |
| **Preroll (предзагрузка)** | Этап, когда каждый приёмник ждёт первый буфер перед воспроизведением |
| **SPS/PPS** | Наборы параметров потока H.264/H.265, без которых кадры не декодируются |
| **Stateless/stateful** | Декодер без состояния требует от программы разбор потока; с состоянием разбирает сам |
| **U-Boot** | Загрузчик, запускающий ядро на ARM-платах |
| **V4L2** | Видеоподсистема ядра (Video4Linux2), даёт `/dev/videoN` |
| **Wayland-композитор** | Программа, собирающая изображение окон и выводящая его на экран |
| **Zero-copy** | Путь, при котором кадр не копируется процессором |
| **DMA_DRM** | Вид подписи caps, где формат кадра задан парой DRM «fourcc:модификатор», например `NV12:0x0900000000000001` |
| **Модификатор DRM** | Число, описывающее раскладку кадра в памяти (линейная, плитки); `ALLWINNER_TILED` это `0x0900000000000001` |
| **Шаг (stride, pitch)** | Расстояние в байтах между началами соседних строк кадра; для плиточных кадров GStreamer хранит его в упакованном виде |
| **vblank, vsync** | Промежуток между двумя обновлениями экрана; при 60 Гц раз в 16,7 мс |
| **Атомарный драйвер KMS** | Драйвер дисплея, который применяет изменения разом на ближайшем обновлении экрана |
| **QoS** | Сообщения приёмника вверх по конвейеру о том, что он опаздывает; декодер в ответ пропускает кадры |
| **Опоздание кадра (jitter)** | Разница между моментом, когда кадр дошёл до приёмника, и сроком его показа |
| **Каденс (джаддер)** | Неравномерный показ 24 кадров/с на экране 60 Гц: кадр держится то 2, то 3 обновления |
| **debugfs** | Отладочная файловая система ядра в `/sys/kernel/debug`, доступна только root |

---

## Ссылки

**Плата и ОС**
- [Armbian: Orange Pi Zero2](https://www.armbian.com/boards/orangepizero2)
- [Armbian: документация](https://docs.armbian.com/)
- [LibreELEC: загрузки Allwinner](https://libreelec.tv/downloads/allwinner/)
- [LibreELEC: ночные сборки 13.0](https://test.libreelec.tv/13.0/Allwinner/)
- [linux-sunxi: Cedrus](https://linux-sunxi.org/Cedrus)

**Ядро**
- [Интерфейс декодеров без состояния](https://docs.kernel.org/userspace-api/media/v4l/dev-stateless-decoder.html)
- [Сжатые форматы V4L2](https://docs.kernel.org/userspace-api/media/v4l/pixfmt-compressed.html)
- [Отладка медиаподсистемы](https://docs.kernel.org/process/debugging/media_specific_debugging_guide.html)
- [KMS и плоскости](https://docs.kernel.org/gpu/drm-kms.html)
- [Параметры ядра](https://docs.kernel.org/admin-guide/kernel-parameters.html)

**GStreamer**
- [v4l2slh265dec](https://gstreamer.freedesktop.org/documentation/v4l2codecs/v4l2slh265dec.html)
- [Плагин v4l2codecs](https://gstreamer.freedesktop.org/documentation/v4l2codecs/index.html)
- [gst-launch-1.0](https://gstreamer.freedesktop.org/documentation/tools/gst-launch.html)
- [Инструменты отладки](https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html)
- [Заметки о выпусках 1.26](https://gstreamer.freedesktop.org/releases/1.26/)
- [Дефект обрезки в 1.26.2 (Raspberry Pi, issue #406)](https://github.com/RPi-Distro/repo/issues/406)

**Прочее**
- [ffprobe](https://ffmpeg.org/ffprobe.html)
- [cage](https://github.com/cage-kiosk/cage)
- [Форум Armbian: v4l2request для Rockchip и Allwinner](https://forum.armbian.com/topic/32449-repository-for-v4l2request-hardware-video-decoding-rockchip-allwinner/)
- [Jellyfin: аппаратное ускорение](https://jellyfin.org/docs/general/post-install/transcoding/hardware-acceleration/)

**Второй стенд**
- [linux-sunxi: Banana Pi](https://linux-sunxi.org/Banana_Pi)
- [gst-plugins-bad: запрос 1437, плитки NV12_32L32 в v4l2codecs](https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/1437)
- [GStreamer: запрос 7355, перевод форматов V4L2 в пары DRM](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/7355)
- [Ядро: sun4i frontend и плиточный формат Allwinner (патч Bootlin)](https://patchwork.kernel.org/project/dri-devel/patch/20181204142218.16284-13-paul.kocialkowski@bootlin.com/)
- [ffmpeg-devel: серия патчей V4L2 Request API, 2024](https://ffmpeg.org/pipermail/ffmpeg-devel/2024-August/332034.html)
- [GStreamer: kmssink](https://gstreamer.freedesktop.org/documentation/kms/index.html)
- [GStreamer: GstBaseSink](https://gstreamer.freedesktop.org/documentation/base/gstbasesink.html)

---

## Ответы на вопросы

### Глава 1
1. Ничего: MKV это контейнер, внутри может быть любой видеокодек. Смотреть `ffprobe`.
2. Транскодирование это декодирование плюс кодирование; тяжелее кодирование.
3. `VLD` значит аппаратное декодирование, `EncSlice` значит аппаратное кодирование.
4. Проверить профиль и формат пикселей инструментом (рецепт 2.2).
5. Неверная матрица лишь слегка меняет пересчёт цветов, а HDR использует другие передаточную
   функцию и цветовой охват, и без преобразования яркость и цвет читаются совсем иначе.

### Глава 2
1. `ffprobe -v error -select_streams v:0 -show_entries stream=profile,pix_fmt -of default=nw=1 файл`.
2. Без `-count_frames` читаются только заявленные параметры, а не сами кадры.
3. Ограниченный диапазон, матрица, передача и первичные BT.709: обычное HD-видео.
4. Вырезать кусок и посчитать в нём кадры (рецепт 2.3).
5. 100 звуковых пакетов по ~21 мс, около 2,1 секунды.
6. На втором стенде `ffprobe` показал у клипа 1080p `coded_height=1080`, а драйвер выделил
   под кадр 1088 строк. Выравнивание спрашивают у драйвера (рецепт 5.7).
7. На экране 60 Гц 30 кадров/с показываются ровно, а 24 нет. Если 30 идёт гладко, а 24 с
   подёргиванием, причина в каденсе экрана, а не в конвейере.

### Глава 3
1. Разные загрузчик и его место на носителе, разное описание платы, разные драйверы.
2. Что система загружается и видит базовое железо; декодер, Wi-Fi и вывод видео не гарантированы.
3. Там нет V4L2 и драйвера Cedrus, значит, нет аппаратного декодирования.
4. Это самое жёсткое требование: без драйвера в ядре никакая ОС не поможет.

### Глава 4
1. Имя в подписи может быть любым, а отпечаток однозначно определяет ключ.
2. Сравнить `lsblk` до и после вставки: новое устройство нужного размера, `RM=1`, `TRAN=usb`.
3. `dd` не завершится, пока данные не будут физически записаны.
4. Чтобы иметь точку отсчёта и не путать эффекты обновлений с результатами проверок.
5. Файл образа есть, устройство съёмное (`removable=1`), размер в заданном коридоре,
   ничего не примонтировано.
6. Версия в баннере или в `/etc/armbian-release` не совпадает с именем образа; журнал
   `/var/log/apt/history.log` покажет, когда и что ставилось.

### Глава 5
1. Драйвер может быть встроен в ядро, это видно по `dmesg`.
2. `v4l2-ctl -d /dev/video0 --list-formats-out-ext` (строка `S265`).
3. С состоянием разбирает поток сам; без состояния ждёт от программы разобранные параметры.
4. Он не сможет открыть устройство: нужна группа `video` или root.
5. `/proc/device-tree` это символическая ссылка, а `find` по умолчанию в неё не заходит;
   нужна `/` в конце пути.
6. Одним вызовом `v4l2-ctl` задать вход и выход и прочитать `--get-fmt-video`: на втором
   стенде `1920/1088`.

### Глава 6
1. Чтобы видеть предел декодера, а не скорость по часам.
2. Вывод в файл копился в буфере, а `timeout` убил процесс до его сброса.
3. Процесс висит в ожидании, а не считает.
4. В caps есть `(memory:DMABuf)`, а в журнале плагина нет `copying frames`.
5. 1920 × 800 × 30 ≈ 46 Мпикс/с; для 1080p это около 22 кадров/с, меньше 24. Замер
   дал 22,05.

### Глава 7
1. Плоскости нашего дисплея принимают только линейные RGB-форматы.
2. Плоскость это слой, который дисплей накладывает на другие; плоскость 33 лежит под
   основной, где консоль.
3. Чтение процессором кадров из буферов декодера стоило около 200 мс на кадр.
4. GPU по команде композитора `cage`.
5. Если размер соответствует видимым 1080 строкам, а не внутренним 1088, это копия.
6. В блоке `IN_FORMATS` у `NV12` есть модификатор `ALLWINNER_TILED(0x900000000000001)`.
7. (1 309 131 460 − 1920) / 799 = 1 638 460 = (25 << 16) | 60: упакованный шаг плиток
   GStreamer (рядов и плиток в ряду), а не шаг в байтах.
8. На кадр уходило около трёх периодов обновления (49,5 мс): `kmssink` ждал vsync сам,
   хотя атомарный драйвер уже ждёт его при смене кадра.

### Глава 8
1. Поток с высотой, кратной 16 (без обрезки), работал, а 1080 (с обрезкой) нет.
2. Описание может быть неточным, а в коде видно, что именно изменилось.
3. Чтобы meson не скачивал зависимости сам, а остановился с понятной ошибкой.
4. Системный плагин с тем же именем вытеснял наш; в наборе двойника нет.
5. Новый плагин не появится в наборе, потому что набор состоит из ссылок, сделанных раньше.
6. Для строк с известным форматом GStreamer колонки DRM не читаются: пару спрашивают у
   библиотеки, а та плиток Allwinner не знает.
7. Сравнить с копией: `diff -u файл.orig файл`; число блоков изменений должно совпасть с
   числом правок.

### Глава 9
1. Декодеру нужна физически непрерывная память из CMA, а её было 64 МБ на всё.
2. `grep Cma /proc/meminfo` и `cat /proc/cmdline | tr ' ' '\n' | grep cma`.
3. Пауза позволяла памяти освободиться; сбой был при запуске подряд.
4. Убедиться, что строка `extraargs` действительно попадёт в параметры ядра.
5. Добавляет перевод строки в конец файла, только если его там нет.
6. Что свободную CMA занимал файловый кеш.
7. Нехватка CMA не доказана (сбой был при 65 МБ свободных, а удачные запуски шли и при
   40), а сам сбой редкий и проявляется до первого кадра.

### Глава 10
1. Сработала взаимная блокировка предзагрузки: все ждут друг друга.
2. Вариант M: встаёт даже без парсера и декодера.
3. `ffprobe ... -show_entries packet=stream_index,pts_time ... | grep -n '^0,' | head -n 1`.
4. Безразмерная очередь вмещает больше секунды опережающего звука, стандартная нет.
5. `plughw` сам подгоняет формат и частоту под возможности устройства.
6. Новый поток начинается на выходе `queue`. Очередь перед декодером оставит декодер и
   приёмник в одном потоке, и декодер будет простаивать, пока приёмник ждёт часов.
7. Чтобы `audioconvert` свёл 5.1 в стерео для монитора с двумя динамиками.

### Глава 11
1. Ступень 2 не пройдена: кадры копируются, путь не «без копий», даже если темп держится.
2. `average`, `dropped`, `rendered` (или `Got EOS`), `(user+sys)/real`, минимум `CmaFree`.
3. Его нельзя проверить и повторить; лог может оказаться от другого запуска.
4. Их пропустил декодер в ответ на сообщения QoS от приёмника; `fpsdisplaysink` такие
   пропуски не считает.
5. 1% кадров пришёл к приёмнику позже срока больше чем на 104 мс, это около двух с
   половиной периодов кадра при 24 кадрах/с.

---

Назад: [Глава 11. Методика](11-method.md) | В начало: [Предисловие](00-preface.md)
