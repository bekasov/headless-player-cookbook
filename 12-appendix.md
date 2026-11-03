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

### Глава 5
1. Драйвер может быть встроен в ядро, это видно по `dmesg`.
2. `v4l2-ctl -d /dev/video0 --list-formats-out-ext` (строка `S265`).
3. С состоянием разбирает поток сам; без состояния ждёт от программы разобранные параметры.
4. Он не сможет открыть устройство: нужна группа `video` или root.

### Глава 6
1. Чтобы видеть предел декодера, а не скорость по часам.
2. Вывод в файл копился в буфере, а `timeout` убил процесс до его сброса.
3. Процесс висит в ожидании, а не считает.
4. В caps есть `(memory:DMABuf)`, а в журнале плагина нет `copying frames`.

### Глава 7
1. Плоскости нашего дисплея принимают только линейные RGB-форматы.
2. Плоскость это слой, который дисплей накладывает на другие; плоскость 33 лежит под
   основной, где консоль.
3. Чтение процессором кадров из буферов декодера стоило около 200 мс на кадр.
4. GPU по команде композитора `cage`.
5. Если размер соответствует видимым 1080 строкам, а не внутренним 1088, это копия.

### Глава 8
1. Поток с высотой, кратной 16 (без обрезки), работал, а 1080 (с обрезкой) нет.
2. Описание может быть неточным, а в коде видно, что именно изменилось.
3. Чтобы meson не скачивал зависимости сам, а остановился с понятной ошибкой.
4. Системный плагин с тем же именем вытеснял наш; в наборе двойника нет.
5. Новый плагин не появится в наборе, потому что набор состоит из ссылок, сделанных раньше.

### Глава 9
1. Декодеру нужна физически непрерывная память из CMA, а её было 64 МБ на всё.
2. `grep Cma /proc/meminfo` и `cat /proc/cmdline | tr ' ' '\n' | grep cma`.
3. Пауза позволяла памяти освободиться; сбой был при запуске подряд.
4. Убедиться, что строка `extraargs` действительно попадёт в параметры ядра.
5. Добавляет перевод строки в конец файла, только если его там нет.

### Глава 10
1. Сработала взаимная блокировка предзагрузки: все ждут друг друга.
2. Вариант M: встаёт даже без парсера и декодера.
3. `ffprobe ... -show_entries packet=stream_index,pts_time ... | grep -n '^0,' | head -n 1`.
4. Безразмерная очередь вмещает больше секунды опережающего звука, стандартная нет.
5. `plughw` сам подгоняет формат и частоту под возможности устройства.

### Глава 11
1. Ступень 2 не пройдена: кадры копируются, путь не «без копий», даже если темп держится.
2. `average`, `dropped`, `rendered` (или `Got EOS`), `(user+sys)/real`, минимум `CmaFree`.
3. Его нельзя проверить и повторить; лог может оказаться от другого запуска.

---

Назад: [Глава 11. Методика](11-method.md) | В начало: [Предисловие](00-preface.md)
