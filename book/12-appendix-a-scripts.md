---
aliases:
  - Приложение А
  - Скрипты
tags:
  - книга/приложение
---

# Приложение А. Скрипты

Все скрипты живут на плате в домашней папке. Те же файлы лежат в папке `scripts/`
репозитория, их можно скопировать на плату целиком:

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

---

Назад: [Глава 11. Методика](11-method.md) | Далее: [Приложение Б. Словарь](13-appendix-b-glossary.md)
