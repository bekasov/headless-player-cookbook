---
aliases:
  - Глава 12
  - Практикум DRM
tags:
  - книга/глава
---

# Глава 12. Практикум: экран своими руками на C

В главе 7 кадры на экран клал `kmssink`, а мы смотрели на него снаружи: подписи, журналы,
ошибки ядра. Здесь мы сделаем то же самое сами, тремя короткими программами на C. Первая
спрашивает дисплей, что у него есть. Вторая рисует линии в RGB-буфере и кладёт его на
плоскость поверх консоли. Третья делает то же с кадром в YUV, как его выдаёт видеодвижок.

После этой главы слова «плоскость», «fourcc», «шаг строки» и «владелец экрана» станут
строчками кода, которые можно поменять и посмотреть, что выйдет.

> [!note] Что проверено
> Программы собраны gcc и clang без единого предупреждения (включая `-Wconversion` и
> `-fanalyzer`), `drm-info` запущен на ноутбуке с видеочипом Intel. **На стендах программы ещё
> не запускались**, поэтому «ответов стенда» в этой главе нет: ожидания выведены из замеров
> главы 7 и помечены как ожидания.

Нужно знать C на уровне указателей и структур. Всё остальное объясняется по ходу; про
плоскости и форматы кадра коротко рассказано в рецептах 0.5 и 0.6.

---

## Рецепт 12.1. Подготовить сборку и понять, где искать вызовы и константы

**Уровень:** ★★☆ средний

**Задача.** Поставить всё для сборки и научиться самому находить, какую функцию вызвать и
откуда берётся константа вроде `DRM_FORMAT_XRGB8888`.

**Решение.** Поставить компилятор и заголовки libdrm, скопировать примеры на плату и собрать:

```bash
# [плата]
sudo apt install build-essential libdrm-dev
```

```fish
# [сервер]
scp -r examples/drm пользователь@адрес-платы:
```

```bash
# [плата]
make -C ~/drm
```

Где что искать:

| Что нужно узнать | Где смотреть |
|---|---|
| Какие функции есть и их аргументы | `/usr/include/xf86drmMode.h` и `/usr/include/xf86drm.h` (libdrm) |
| Константы форматов (`XRGB8888`, `NV12`) | `/usr/include/libdrm/drm_fourcc.h` |
| Структуры и номера `ioctl` ядра | `/usr/include/libdrm/drm.h` и `drm_mode.h` |
| Общая картина и порядок вызовов | `man 7 drm`, `man 7 drm-kms`, `man 7 drm-memory`, `man 3 drmModeGetResources` |
| Правила для свойств и плоскостей | документация ядра: KMS и uAPI (ссылки в конце главы) |
| Как это делают рабочие программы | исходники `modetest` (libdrm, `tests/modetest`), `kmscube`, `kmssink` |
| Что программа на деле просит у ядра | `strace -e trace=ioctl ./программа` |
| Почему ядро отказало | `drm.debug` (рецепт 7.8) |

Найти константу и её смысл:

```bash
# [плата]
grep -n 'DRM_FORMAT_XRGB8888\|DRM_FORMAT_NV12' /usr/include/libdrm/drm_fourcc.h
grep -n '16.16' /usr/include/libdrm/drm_mode.h
pkg-config --cflags --libs libdrm
```

Ответ на ноутбуке автора (заголовки libdrm одинаковы на всех машинах):

```text
175:#define DRM_FORMAT_XRGB8888	fourcc_code('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
315:#define DRM_FORMAT_NV12		fourcc_code('N', 'V', '1', '2') /* 2x2 subsampled Cr:Cb plane */
308:	/* Source values are 16.16 fixed point */
-I/usr/include/libdrm -ldrm
```

**Как это работает.**

- **libdrm это тонкая обёртка над `ioctl`.** Почти каждая функция `drmMode...` заполняет
  структуру из `drm_mode.h` и делает один `ioctl` к `/dev/dri/cardN`. Поэтому описание
  структуры в заголовке ядра это и есть описание функции. Заголовки в `/usr/include/libdrm`
  это копия заголовков ядра `include/uapi/drm`.
- **fourcc** (four character code) это формат, записанный четырьмя буквами в одно 32-битное
  число. `XRGB8888` это `'X','R','2','4'`, то есть те самые `XR24` из вывода `modetest`
  (рецепт 7.1). Комментарий справа говорит раскладку: 32 бита, в старшем байте пусто, дальше
  красный, зелёный, синий, порядок байт little endian.
- **Комментарии в заголовках важнее man-страниц.** Строка «Source values are 16.16 fixed
  point» в `drm_mode.h` объясняет, почему в `drmModeSetPlane` размер исходного
  прямоугольника сдвигают на 16 бит (рецепт 12.3). В man этого нет.
- **Рабочий код это лучший справочник.** Когда непонятно, как сделать шаг, ищите его в
  `modetest`: например, NV12 в одном буфере (рецепт 12.4) сделан ровно как в его файле
  `buffers.c`.

> [!warning] Подводные камни
> - Man-страницы libdrm описывают только общую картину и несколько функций. Для остального
>   читайте комментарии в заголовках, документацию ядра и исходники `modetest`.
> - Порядок полей в структуре ядра и в функции libdrm может различаться: у
>   `struct drm_mode_set_plane` идёт `src_h`, потом `src_w`, а `drmModeSetPlane` принимает
>   `src_w, src_h`. Пока вы зовёте функцию, это не важно; при прямом `ioctl` легко ошибиться.
> - Без пакета `libdrm-dev` нет заголовков, и компилятор скажет `xf86drm.h: No such file`.

---

## Рецепт 12.2. Спросить дисплей своей программой

**Уровень:** ★★☆ средний

**Задача.** Получить список разъёмов, CRTC и плоскостей с их форматами, как у `modetest`,
но своей программой на 100 строк.

**Решение.** Программа `drm-info.c`:

```c
// examples/drm/drm-info.c
// Свой маленький modetest: какие разъёмы, CRTC и плоскости видит драйвер дисплея.
// Сборка: make drm-info    Запуск: ./drm-info [/dev/dri/card0]
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Значение свойства объекта по имени ("type", "zpos"); -1, если такого свойства нет. */
static long long prop_value(int fd, uint32_t obj, uint32_t obj_type, const char *name)
{
	drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, obj, obj_type);
	long long val = -1;

	for (uint32_t i = 0; props && i < props->count_props; i++) {
		drmModePropertyPtr p = drmModeGetProperty(fd, props->props[i]);
		if (p && strcmp(p->name, name) == 0)
			val = (long long)props->prop_values[i];
		drmModeFreeProperty(p);
	}
	drmModeFreeObjectProperties(props);
	return val;
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/dri/card0";
	static const char *plane_types[] = { "overlay", "primary", "cursor" };
	int fd = open(path, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return 1;
	}

	drmVersionPtr ver = drmGetVersion(fd);
	printf("driver: %s\n", ver ? ver->name : "?");
	drmFreeVersion(ver);

	/* Без этого ядро покажет только оверлеи, без основной плоскости и курсора. */
	drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

	drmModeResPtr res = drmModeGetResources(fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources: %s (это устройство дисплея?)\n", strerror(errno));
		return 1;
	}

	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnectorPtr c = drmModeGetConnector(fd, res->connectors[i]);
		if (!c)
			continue;
		printf("connector %u %s-%u: %s", c->connector_id,
		       drmModeGetConnectorTypeName(c->connector_type), c->connector_type_id,
		       c->connection == DRM_MODE_CONNECTED ? "connected" : "disconnected");
		if (c->count_modes)
			printf(", first mode %s@%u", c->modes[0].name, c->modes[0].vrefresh);
		putchar('\n');
		drmModeFreeConnector(c);
	}

	for (int i = 0; i < res->count_crtcs; i++) {
		drmModeCrtcPtr crtc = drmModeGetCrtc(fd, res->crtcs[i]);
		if (!crtc)
			continue;
		printf("crtc %u (index %d): %s\n", crtc->crtc_id, i,
		       crtc->mode_valid ? crtc->mode.name : "off");
		drmModeFreeCrtc(crtc);
	}

	drmModePlaneResPtr pres = drmModeGetPlaneResources(fd);
	for (uint32_t i = 0; pres && i < pres->count_planes; i++) {
		drmModePlanePtr p = drmModeGetPlane(fd, pres->planes[i]);
		if (!p)
			continue;
		long long type = prop_value(fd, p->plane_id, DRM_MODE_OBJECT_PLANE, "type");
		long long zpos = prop_value(fd, p->plane_id, DRM_MODE_OBJECT_PLANE, "zpos");
		printf("plane %u: %s, crtc mask 0x%x, zpos %lld, fb %u, formats:", p->plane_id,
		       type >= 0 && type <= 2 ? plane_types[type] : "?", p->possible_crtcs, zpos,
		       p->fb_id);
		/* Формат это четыре буквы, упакованные в 32-битное число (drm_fourcc.h). */
		for (uint32_t f = 0; f < p->count_formats; f++) {
			uint32_t v = p->formats[f];
			printf(" %c%c%c%c", (int)(v & 0xff), (int)((v >> 8) & 0xff),
			       (int)((v >> 16) & 0xff), (int)((v >> 24) & 0xff));
		}
		putchar('\n');
		drmModeFreePlane(p);
	}

	drmModeFreePlaneResources(pres);
	drmModeFreeResources(res);
	close(fd);
	return 0;
}
```

Запуск:

```bash
# [плата]
cd ~/drm && ./drm-info /dev/dri/card0
```

Ответ на ноутбуке автора (Intel, драйвер `i915`), сокращённо:

```text
driver: i915
connector 111 eDP-1: connected, first mode 1920x1080@60
connector 119 DP-1: disconnected
crtc 59 (index 0): 1920x1080
crtc 84 (index 1): off
plane 35: primary, crtc mask 0x1, zpos 0, fb 144, formats: C8   RG16 XR24 XB24 AR24 ... NV12 XYUV
plane 44: overlay, crtc mask 0x1, zpos 1, fb 0, formats: C8   RG16 XR24 XB24 AR24 ... NV12 XYUV
plane 53: cursor, crtc mask 0x1, zpos 2, fb 148, formats: AR24
```

Чего ждать на стендах (по выводу `modetest` из главы 7): на H616 три плоскости (33, 39, 43),
у всех только RGB-форматы; на A20 восемь плоскостей, среди форматов есть `NV12`.

**Как это работает.**

- Дисплей в DRM описан цепочкой объектов: **разъём** (connector, физический выход) получает
  картинку от **CRTC** (генератор сигнала), а CRTC собирает её из **плоскостей**. У каждого
  объекта свой номер (id); эти номера и передают в `plane-id=` у `kmssink`.
- `drmSetClientCap(..., DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)` просит показать **все**
  плоскости. Без этого ядро по старой привычке прячет основную и курсор.
- `possible_crtcs` это битовая маска: бит N значит «плоскость может работать с CRTC номер
  N в списке». Номер в списке (index) и id объекта это разные числа.
- Тип плоскости и `zpos` это **свойства** объекта. Их читают по имени через
  `drmModeObjectGetProperties`; значения типа (0 оверлей, 1 основная, 2 курсор) определены в
  `xf86drmMode.h` как `DRM_PLANE_TYPE_...`.
- На ноутбуке видно то, чего нет у H616: оверлей принимает `NV12`. Такому дисплею композитор
  для видео не обязателен (рецепт 0.5).

> [!warning] Подводные камни
> - `/dev/dri/renderD128` не подходит: это узел только для вычислений на GPU, там нет
>   дисплея. На ноутбуке программа ответила `drmModeGetResources: Permission denied`.
> - Номер `cardN` может отличаться: проверьте по рецепту 5.1. На ноутбуке дисплей был
>   `card1`, а `card0` не существовал.
> - `zpos -1` значит, что драйвер не сообщает порядок слоёв. Тогда, какая плоскость выше,
>   выясняется опытом (рецепт 7.2).

---

## Рецепт 12.3. Нарисовать линию в RGB-слое

**Уровень:** ★★☆ средний

**Задача.** Выделить буфер, нарисовать в нём рамку и две диагонали и показать его на
плоскости поверх консоли.

**Решение.** Общие шаги вынесены в `drm-common.h`, им пользуются и этот рецепт, и
следующий:

```c
// examples/drm/drm-common.h
// Общие шаги для drm-line.c и drm-nv12.c: найти включённый экран, выбрать плоскость,
// выделить буфер в памяти, показать его на плоскости и убрать.
#ifndef DRM_COMMON_H
#define DRM_COMMON_H

#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); exit(1); } while (0)

struct screen {
	int fd;
	uint32_t crtc_id;
	int crtc_index;          /* номер CRTC в списке: по нему читается possible_crtcs */
	uint32_t width, height;  /* текущий режим экрана */
};

struct dumb {
	uint32_t handle, pitch;  /* pitch: байт от начала одной строки до начала следующей */
	uint64_t size;
	uint8_t *map;            /* буфер, отображённый в память программы */
};

static long long prop_value(int fd, uint32_t obj, uint32_t obj_type, const char *name)
{
	drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, obj, obj_type);
	long long val = -1;

	for (uint32_t i = 0; props && i < props->count_props; i++) {
		drmModePropertyPtr p = drmModeGetProperty(fd, props->props[i]);
		if (p && strcmp(p->name, name) == 0)
			val = (long long)props->prop_values[i];
		drmModeFreeProperty(p);
	}
	drmModeFreeObjectProperties(props);
	return val;
}

/* Открыть устройство и найти CRTC, который сейчас что-то показывает (обычно консоль). */
static void screen_open(struct screen *s, const char *path)
{
	s->fd = open(path, O_RDWR | O_CLOEXEC);
	if (s->fd < 0)
		DIE("%s: %s", path, strerror(errno));
	if (drmSetClientCap(s->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1))
		DIE("DRM_CLIENT_CAP_UNIVERSAL_PLANES: %s", strerror(errno));

	drmModeResPtr res = drmModeGetResources(s->fd);
	if (!res)
		DIE("drmModeGetResources: %s", strerror(errno));
	s->crtc_id = 0;
	for (int i = 0; i < res->count_crtcs && !s->crtc_id; i++) {
		drmModeCrtcPtr c = drmModeGetCrtc(s->fd, res->crtcs[i]);
		if (c && c->mode_valid) {
			s->crtc_id = c->crtc_id;
			s->crtc_index = i;
			s->width = c->mode.hdisplay;
			s->height = c->mode.vdisplay;
		}
		drmModeFreeCrtc(c);
	}
	drmModeFreeResources(res);
	if (!s->crtc_id)
		DIE("нет включённого CRTC: подключён ли монитор?");
}

/* Плоскость для нашего CRTC, которая принимает формат. Если номер задан, только проверяем
 * его. Иначе берём оверлей с наибольшим zpos: он лежит выше консоли. */
static uint32_t pick_plane(struct screen *s, uint32_t wanted, uint32_t format)
{
	drmModePlaneResPtr pres = drmModeGetPlaneResources(s->fd);
	uint32_t best = 0;
	long long best_z = -2;

	if (!pres)
		DIE("drmModeGetPlaneResources: %s", strerror(errno));
	for (uint32_t i = 0; i < pres->count_planes; i++) {
		drmModePlanePtr p = drmModeGetPlane(s->fd, pres->planes[i]);
		if (!p)
			continue;
		int ok_crtc = (p->possible_crtcs >> s->crtc_index) & 1;
		int ok_fmt = 0;
		for (uint32_t f = 0; f < p->count_formats; f++)
			ok_fmt |= p->formats[f] == format;
		long long type = prop_value(s->fd, p->plane_id, DRM_MODE_OBJECT_PLANE, "type");
		long long z = prop_value(s->fd, p->plane_id, DRM_MODE_OBJECT_PLANE, "zpos");

		if (wanted && p->plane_id == wanted) {
			if (!ok_crtc)
				DIE("плоскость %u не подключается к CRTC %u", wanted, s->crtc_id);
			if (!ok_fmt)
				DIE("плоскость %u не принимает формат %.4s", wanted, (const char *)&format);
			best = wanted;
		} else if (!wanted && ok_crtc && ok_fmt && type == DRM_PLANE_TYPE_OVERLAY && z > best_z) {
			best = p->plane_id;
			best_z = z;
		}
		drmModeFreePlane(p);
	}
	drmModeFreePlaneResources(pres);
	if (!best && wanted)
		DIE("плоскости %u нет", wanted);
	if (!best)
		DIE("нет оверлея, который принимает %.4s (проверьте ./drm-info)", (const char *)&format);
	return best;
}

/* Буфер в памяти, к которому есть доступ и у программы, и у дисплея (man drm-memory). */
static void dumb_create(int fd, uint32_t w, uint32_t h, uint32_t bpp, struct dumb *d)
{
	struct drm_mode_create_dumb create = { .width = w, .height = h, .bpp = bpp };
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create))
		DIE("DRM_IOCTL_MODE_CREATE_DUMB: %s", strerror(errno));

	struct drm_mode_map_dumb map = { .handle = create.handle };
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map))
		DIE("DRM_IOCTL_MODE_MAP_DUMB: %s", strerror(errno));

	/* Ядро дало не адрес, а «смещение» для mmap по этому же файлу устройства. */
	d->map = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map.offset);
	if (d->map == MAP_FAILED)
		DIE("mmap: %s", strerror(errno));
	d->handle = create.handle;
	d->pitch = create.pitch;
	d->size = create.size;
}

static void dumb_destroy(int fd, struct dumb *d)
{
	struct drm_mode_destroy_dumb destroy = { .handle = d->handle };

	munmap(d->map, d->size);
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

/* Показать кадр fb размером w x h по центру экрана на несколько секунд и убрать. */
static void show(struct screen *s, uint32_t plane, uint32_t fb, uint32_t w, uint32_t h, int seconds)
{
	int x = ((int)s->width - (int)w) / 2, y = ((int)s->height - (int)h) / 2;

	/* Координаты на экране в пикселях, а прямоугольник в буфере в формате 16.16
	 * (так написано в drm_mode.h у struct drm_mode_set_plane), отсюда << 16. */
	if (drmModeSetPlane(s->fd, plane, s->crtc_id, fb, 0, x, y, w, h, 0, 0, w << 16, h << 16))
		DIE("drmModeSetPlane: %s%s", strerror(errno),
		    errno == EACCES || errno == EPERM ?
		    " (экраном владеет другая программа, например композитор; запустите с консоли)" : "");
	printf("плоскость %u, CRTC %u, кадр %ux%u в точке (%d,%d), ждём %d с\n",
	       plane, s->crtc_id, w, h, x, y, seconds);
	sleep((unsigned)seconds);
	drmModeSetPlane(s->fd, plane, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);  /* fb 0 убирает слой */
}

#endif
```

Сама программа `drm-line.c`:

```c
// examples/drm/drm-line.c
// Нарисовать линии в RGB-буфере и показать его на плоскости дисплея поверх консоли.
// Запуск с консоли платы: ./drm-line [/dev/dri/card0] [номер плоскости] [секунды]
#include "drm-common.h"

#define W 640
#define H 360

static void put_pixel(struct dumb *d, int x, int y, uint32_t color)
{
	if (x < 0 || y < 0 || x >= W || y >= H)
		return;
	/* Строка начинается через pitch байт, а не через W * 4: драйвер может её выровнять. */
	uint32_t *row = (uint32_t *)(d->map + (size_t)y * d->pitch);
	row[x] = color;
}

/* Алгоритм Брезенхэма: только целые числа, одна точка на шаг. */
static void line(struct dumb *d, int x0, int y0, int x1, int y1, uint32_t color)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	for (;;) {
		put_pixel(d, x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

int main(int argc, char **argv)
{
	struct screen s;
	struct dumb d;
	uint32_t fb;

	screen_open(&s, argc > 1 ? argv[1] : "/dev/dri/card0");
	if (s.width < W || s.height < H)
		DIE("экран %ux%u меньше кадра %dx%d", s.width, s.height, W, H);
	uint32_t plane = pick_plane(&s, argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0,
				    DRM_FORMAT_XRGB8888);
	int seconds = argc > 3 ? atoi(argv[3]) : 5;

	/* XRGB8888: 32 бита на точку, число 0x00RRGGBB (в памяти байты B, G, R, X). */
	dumb_create(s.fd, W, H, 32, &d);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			put_pixel(&d, x, y, 0x00202830);            /* тёмный фон */
	line(&d, 0, 0, W - 1, 0, 0x00FFFFFF);               /* рамка по краям */
	line(&d, W - 1, 0, W - 1, H - 1, 0x00FFFFFF);
	line(&d, W - 1, H - 1, 0, H - 1, 0x00FFFFFF);
	line(&d, 0, H - 1, 0, 0, 0x00FFFFFF);
	line(&d, 0, 0, W - 1, H - 1, 0x0040C0FF);           /* голубая диагональ */
	line(&d, 0, H - 1, W - 1, 0, 0x00FFB040);           /* оранжевая диагональ */

	/* Буфер превращается в framebuffer: дисплею нужно знать формат и шаг строки. */
	uint32_t handles[4] = { d.handle }, pitches[4] = { d.pitch }, offsets[4] = { 0 };
	if (drmModeAddFB2(s.fd, W, H, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &fb, 0))
		DIE("drmModeAddFB2: %s", strerror(errno));

	show(&s, plane, fb, W, H, seconds);

	drmModeRmFB(s.fd, fb);
	dumb_destroy(s.fd, &d);
	close(s.fd);
	return 0;
}
```

Запуск **с клавиатуры платы** (как `cage` в рецепте 7.4), номер плоскости из главы 7:

```bash
# [консоль платы]
cd ~/drm
./drm-line /dev/dri/card0 43     # основной стенд, H616 (рецепт 7.2)
./drm-line /dev/dri/card0 37     # второй стенд, A20 (рецепт 7.6)
```

Ожидаемо: по центру экрана на 5 секунд появится тёмный прямоугольник 640x360 с белой рамкой
и двумя диагоналями, вокруг останется видна консоль.

**Как это работает.** Шесть шагов, по одному на вызов:

| Шаг | Вызов | Что происходит |
|---|---|---|
| 1 | `open("/dev/dri/card0")` | доступ к дисплею (нужна группа `video`, рецепт 5.1) |
| 2 | `drmModeGetResources`, `drmModeGetCrtc` | какой CRTC включён и какого размера экран |
| 3 | `drmModeGetPlane` | какая плоскость подходит к CRTC и принимает `XRGB8888` |
| 4 | `DRM_IOCTL_MODE_CREATE_DUMB`, `MAP_DUMB`, `mmap` | буфер в памяти, видный и дисплею, и программе |
| 5 | `drmModeAddFB2` | буфер становится framebuffer: к нему привязаны формат и шаг строки |
| 6 | `drmModeSetPlane` | плоскость начинает показывать framebuffer |

- **Dumb buffer** («простой буфер», `man 7 drm-memory`) это буфер, который рисует процессор,
  без ускорения. Ядро выделяет его так, чтобы дисплей мог читать его сам; на наших платах
  это непрерывная память (CMA, глава 9).
- **Шаг строки** (`pitch`) выбирает драйвер, и он может быть больше `ширина × 4`. Поэтому
  адрес точки считается как `y × pitch + x × 4`, а не `(y × ширина + x) × 4` (рецепт 0.6).
- **Цвет** в `XRGB8888` записывается числом `0x00RRGGBB`; в памяти байты идут в обратном
  порядке (B, G, R, X), потому что процессор little endian.
- **Два прямоугольника** в `drmModeSetPlane`: где показать на экране (в пикселях) и какую
  часть буфера взять (в формате 16.16, отсюда `<< 16`). Если размеры разные, дисплей
  масштабирует, если умеет.
- **Владелец экрана.** Менять картинку на экране может только одна программа, «мастер» DRM.
  Композитор (`cage`, рабочий стол) держит это право, пока работает. Консоль права не держит,
  поэтому с консоли программа и `kmssink` (рецепт 7.7) работают, а при запущенном
  композиторе `drmModeSetPlane` вернёт `Permission denied`.
- **Линия Брезенхэма.** Чтобы провести линию по точкам, на каждом шаге решаем, сдвинуться ли
  ещё и по второй оси. Решение принимается по накопленной ошибке `err`, только целыми
  числами, без деления и дробей.

> [!warning] Подводные камни
> - Без номера плоскости программа выберет оверлей с наибольшим `zpos`. Если драйвер `zpos`
>   не сообщает, она может взять слой **под** консолью, и на экране ничего не будет видно.
>   Передайте номер явно, как в рецепте 7.2.
> - Не запускайте пример, пока работает `cage` или графический сеанс: экраном владеет он.
> - Рисовать по точке удобно для учёбы, но медленно для видео: для этого и нужны
>   видеодвижок и прямой путь из главы 7.

---

## Рецепт 12.4. Положить на слой кадр в YUV

**Уровень:** ★★★ продвинутый

**Задача.** Показать на плоскости кадр в формате `NV12` (рецепт 0.6) и своими глазами
увидеть разницу между H616 и A20 из рецепта 0.5.

**Решение.** Программа `drm-nv12.c`:

```c
// examples/drm/drm-nv12.c
// Тот же вывод на плоскость, но кадр в YUV (NV12), как его отдаёт видеодвижок.
// На A20 плоскость должна его принять, на H616 с ядром стенда такой плоскости нет.
// Запуск с консоли платы: ./drm-nv12 [/dev/dri/card0] [номер плоскости] [секунды]
#include "drm-common.h"

#define W 640
#define H 360  /* чётная: строк цветности вдвое меньше */

int main(int argc, char **argv)
{
	struct screen s;
	struct dumb d;
	uint32_t fb;

	screen_open(&s, argc > 1 ? argv[1] : "/dev/dri/card0");
	if (s.width < W || s.height < H)
		DIE("экран %ux%u меньше кадра %dx%d", s.width, s.height, W, H);
	uint32_t plane = pick_plane(&s, argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0,
				    DRM_FORMAT_NV12);
	int seconds = argc > 3 ? atoi(argv[3]) : 5;

	/* Один буфер «8 бит на точку, высота x 1,5»: сверху яркость, под ней цветность.
	 * Так же делает modetest (libdrm, tests/modetest/buffers.c). */
	dumb_create(s.fd, W, H * 3 / 2, 8, &d);
	uint8_t *luma = d.map;
	uint8_t *chroma = d.map + (size_t)d.pitch * H;

	/* Яркость: плавно слева направо, в ограниченном диапазоне 16..235 (рецепт 1.5). */
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			luma[(size_t)y * d.pitch + x] = (uint8_t)(16 + 219 * x / (W - 1));
	/* Цветность: пары U, V на каждый квадрат 2x2 точки. U растёт вправо, V вниз. */
	for (int y = 0; y < H / 2; y++)
		for (int x = 0; x < W / 2; x++) {
			chroma[(size_t)y * d.pitch + 2 * x] = (uint8_t)(255 * x / (W / 2 - 1));
			chroma[(size_t)y * d.pitch + 2 * x + 1] = (uint8_t)(255 * y / (H / 2 - 1));
		}

	/* У NV12 две части в одном буфере: тот же handle, разные смещения. */
	uint32_t handles[4] = { d.handle, d.handle };
	uint32_t pitches[4] = { d.pitch, d.pitch };
	uint32_t offsets[4] = { 0, d.pitch * H };
	if (drmModeAddFB2(s.fd, W, H, DRM_FORMAT_NV12, handles, pitches, offsets, &fb, 0))
		DIE("drmModeAddFB2: %s", strerror(errno));

	show(&s, plane, fb, W, H, seconds);

	drmModeRmFB(s.fd, fb);
	dumb_destroy(s.fd, &d);
	close(s.fd);
	return 0;
}
```

```bash
# [консоль платы]
cd ~/drm
./drm-nv12 /dev/dri/card0 37     # второй стенд, A20
./drm-nv12 /dev/dri/card0        # основной стенд, H616: плоскость подберёт сама
```

Ожидаемо: на A20 появится цветной градиент, тёмный слева и светлый справа, с цветом,
меняющимся вправо и вниз. На H616 программа остановится до вывода:
`нет оверлея, который принимает NV12`, потому что плоскости стенда принимали только RGB
(рецепт 7.1).

**Как это работает.**

- Кадр NV12 лежит в одном буфере: сначала строки яркости, затем вдвое меньше строк, где
  чередуются U и V. Поэтому буфер заказан с 8 битами на точку и высотой `H × 1,5`, а в
  `drmModeAddFB2` переданы **две** части с одним `handle` и разными смещениями.
- Дисплей A20 сам переводит YUV в RGB при выводе (блок frontend, рецепт 0.5). Ни GPU, ни
  композитор, ни процессор кадр не трогают.
- От этой программы до прямого пути из главы 7 один шаг. Вместо своего буфера берут кадр
  видеодвижка: его DMABuf превращают в `handle` вызовом `drmPrimeFDToHandle`, а вместо
  `drmModeAddFB2` вызывают `drmModeAddFB2WithModifiers` с модификатором
  `DRM_FORMAT_MOD_ALLWINNER_TILED`, потому что кадр плиточный. Это и делает `kmssink`, и
  именно здесь в рецепте 7.8 ядро отвергло неверный шаг строки.

> [!warning] Подводные камни
> - Ширина и высота NV12 должны быть чётными: цветность хранится на квадрат 2x2.
> - Цвета приблизительные: как переводить YUV в RGB (BT.601 или BT.709), плоскость решает по
>   своим свойствам `COLOR_ENCODING` и `COLOR_RANGE`, если они у неё есть. Для градиента это
>   не важно, для фильма важно (рецепт 1.5).
> - Значения яркости взяты в ограниченном диапазоне 16..235. Если плоскость считает диапазон
>   полным, картинка будет чуть серее.

---

## Рецепт 12.5. Разобрать ошибку и подсмотреть вызовы

**Уровень:** ★★☆ средний

**Задача.** Понять, что делать, когда программа на C (своя или чужая) получает отказ от
ядра.

**Решение.** Сначала прочитать `errno`, потом посмотреть вызовы, потом спросить ядро:

```bash
# [плата]
sudo apt install strace
strace -e trace=ioctl ./drm-info 2>&1 | head -n 15
```

| Ошибка | Частая причина | Что делать |
|---|---|---|
| `No such file or directory` при `open` | не тот `cardN` | `ls -l /dev/dri`, рецепт 5.1 |
| `Permission denied` при `open` | нет группы `video` | `id`, рецепт 5.1 |
| `Permission denied` у `drmModeSetPlane` | экраном владеет композитор | закрыть его, запускать с консоли |
| `Permission denied` у `drmModeGetResources` | открыт `renderD128` вместо `cardN` | открыть `cardN` |
| `Invalid argument` у `AddFB2` или `SetPlane` | формат, размер или шаг не подошли | `drm.debug=0x04`, рецепт 7.8 |
| `Cannot allocate memory` у `CREATE_DUMB` | кончилась непрерывная память | `grep Cma /proc/meminfo`, глава 9 |

**Как это работает.**

- `strace` показывает каждый `ioctl` по имени: `DRM_IOCTL_MODE_GETRESOURCES`,
  `DRM_IOCTL_MODE_CREATE_DUMB` и так далее. Это быстрый способ сопоставить функцию libdrm с
  вызовом ядра или узнать, на каком шаге чужая программа получила отказ.
- Сообщение `errno` говорит только «что», а журнал ядра с `drm.debug` говорит «почему»: в
  рецепте 7.8 он назвал точное поле и числа.

> [!warning] Подводные камни
> `strace` замедляет программу. Для вывода видео он годится только как разовая проверка.

---

## Упражнения

1. Нарисуйте окружность (алгоритм средней точки) и залейте её.
2. Сдвиньте прямоугольник к краю экрана так, чтобы он частично вышел за край: координаты на
   экране знаковые. Что скажет драйвер?
3. Попросите масштабирование: источник 640x360, на экране 1280x720. На какой плате это
   сработает?
4. Сделайте анимацию: два буфера и переключение между ними. Почему без ожидания обновления
   экрана картинка рвётся, и как это связано с рецептом 7.9?
5. Большой шаг: замените свой буфер кадром видеодвижка через DMABuf и `drmPrimeFDToHandle`.
   Это уже ядро своего маленького плеера.

---

## Вопросы для самопроверки

1. Где найти, какому числу соответствует `DRM_FORMAT_XRGB8888`, и почему `modetest` пишет
   его как `XR24`?
2. Зачем в `drmModeSetPlane` размеры источника сдвигают на 16 бит?
3. Почему адрес точки считают через `pitch`, а не через ширину?
4. Почему программа работает с консоли, но не при запущенном `cage`?
5. Что нужно поменять в `drm-nv12.c`, чтобы показать кадр прямо из видеодвижка?

## Где почитать

- [Ядро: KMS, плоскости и их свойства](https://docs.kernel.org/gpu/drm-kms.html)
- [Ядро: пользовательский интерфейс DRM (uAPI)](https://docs.kernel.org/gpu/drm-uapi.html)
- [libdrm: исходники и man-страницы](https://gitlab.freedesktop.org/mesa/libdrm)
- [libdrm: modetest](https://gitlab.freedesktop.org/mesa/libdrm/-/tree/main/tests/modetest)
- [kmscube: вывод через KMS и GPU](https://gitlab.freedesktop.org/mesa/kmscube)
- [Алгоритм Брезенхэма — Wikipedia](https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm)

---

Назад: [Глава 11. Методика](11-method.md) | Далее: [Приложение А. Скрипты](13-appendix-a-scripts.md)
