// examples/drm/drm-atomic.c
// Анимация через атомарный API: два буфера, смена кадра одним коммитом по обновлению
// экрана и замер интервала между сменами. Без разрывов картинки.
// Запуск с консоли платы: ./drm-atomic [/dev/dri/card0] [номер плоскости] [секунды]
#include <poll.h>
#include "drm-common.h"

#define W 640
#define H 360
#define BAR 24  /* ширина полосы: на разрыве кадра она ломается, это сразу видно */

enum { FB_ID, CRTC_ID, SRC_X, SRC_Y, SRC_W, SRC_H, CRTC_X, CRTC_Y, CRTC_W, CRTC_H, NPROPS };
static const char *prop_names[NPROPS] = {
	"FB_ID", "CRTC_ID", "SRC_X", "SRC_Y", "SRC_W", "SRC_H",
	"CRTC_X", "CRTC_Y", "CRTC_W", "CRTC_H",
};

struct flip_state {
	int pending;               /* коммит отправлен, событие ещё не пришло */
	long long last_us;         /* время предыдущей смены кадра */
	long long sum_us, min_us, max_us;
	int count;
};

/* В атомарном API всё задаётся свойствами, а свойство ищут по имени и получают его id. */
static uint32_t prop_id(int fd, uint32_t obj, uint32_t obj_type, const char *name)
{
	drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, obj, obj_type);
	uint32_t id = 0;

	for (uint32_t i = 0; props && i < props->count_props && !id; i++) {
		drmModePropertyPtr p = drmModeGetProperty(fd, props->props[i]);
		if (p && strcmp(p->name, name) == 0)
			id = p->prop_id;
		drmModeFreeProperty(p);
	}
	drmModeFreeObjectProperties(props);
	if (!id)
		DIE("у объекта %u нет свойства %s", obj, name);
	return id;
}

/* Ядро зовёт эту функцию из drmHandleEvent, когда коммит показан на экране. */
static void on_flip(int fd, unsigned int seq, unsigned int sec, unsigned int usec, void *data)
{
	struct flip_state *st = data;
	long long now = (long long)sec * 1000000 + usec;

	(void)fd;
	(void)seq;
	if (st->last_us) {
		long long d = now - st->last_us;
		st->sum_us += d;
		st->min_us = st->count && st->min_us < d ? st->min_us : d;
		st->max_us = st->max_us > d ? st->max_us : d;
		st->count++;
	}
	st->last_us = now;
	st->pending = 0;
}

static void draw(struct dumb *d, int bar_x)
{
	for (int y = 0; y < H; y++) {
		uint32_t *row = (uint32_t *)(d->map + (size_t)y * d->pitch);
		for (int x = 0; x < W; x++)
			row[x] = x >= bar_x && x < bar_x + BAR ? 0x00FFFFFF : 0x00202830;
	}
}

int main(int argc, char **argv)
{
	struct screen s;
	struct dumb buf[2];
	uint32_t fb[2], prop[NPROPS];
	struct flip_state st = { 0 };
	drmEventContext ev = { .version = 2, .page_flip_handler = on_flip };

	screen_open(&s, argc > 1 ? argv[1] : "/dev/dri/card0");
	if (drmSetClientCap(s.fd, DRM_CLIENT_CAP_ATOMIC, 1))
		DIE("драйвер не поддерживает атомарный API: %s", strerror(errno));
	if (s.width < W || s.height < H)
		DIE("экран %ux%u меньше кадра %dx%d", s.width, s.height, W, H);
	uint32_t plane = pick_plane(&s, argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0,
				    DRM_FORMAT_XRGB8888);
	int seconds = argc > 3 ? atoi(argv[3]) : 5;
	int frames = seconds * 60;  /* примерно секунды при 60 Гц; время меряем по событиям */

	for (int i = 0; i < NPROPS; i++)
		prop[i] = prop_id(s.fd, plane, DRM_MODE_OBJECT_PLANE, prop_names[i]);

	/* Два буфера: в один рисуем, пока другой на экране. */
	for (int i = 0; i < 2; i++) {
		uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
		dumb_create(s.fd, W, H, 32, &buf[i]);
		handles[0] = buf[i].handle;
		pitches[0] = buf[i].pitch;
		if (drmModeAddFB2(s.fd, W, H, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &fb[i], 0))
			DIE("drmModeAddFB2: %s", strerror(errno));
	}

	uint64_t val[NPROPS] = {
		[CRTC_ID] = s.crtc_id,
		[SRC_W] = (uint64_t)W << 16, [SRC_H] = (uint64_t)H << 16,  /* 16.16, как в 12.3 */
		[CRTC_X] = (s.width - W) / 2, [CRTC_Y] = (s.height - H) / 2,
		[CRTC_W] = W, [CRTC_H] = H,
	};

	for (int n = 0; n < frames; n++) {
		struct dumb *back = &buf[n & 1];
		drmModeAtomicReqPtr req = drmModeAtomicAlloc();

		draw(back, (n * 8) % (W - BAR));
		val[FB_ID] = fb[n & 1];
		/* Первый коммит задаёт всё: плоскость, CRTC, прямоугольники. Дальше меняем только
		 * буфер; остальное ядро помнит. */
		for (int i = 0; i < (n == 0 ? NPROPS : 1); i++)
			drmModeAtomicAddProperty(req, plane, prop[i], val[i]);

		/* Сначала спросить, примет ли железо такую настройку, ничего не показывая. */
		if (n == 0 && drmModeAtomicCommit(s.fd, req, DRM_MODE_ATOMIC_TEST_ONLY, NULL))
			DIE("проверка TEST_ONLY: %s (формат, размер или плоскость не подходят)", strerror(errno));
		if (drmModeAtomicCommit(s.fd, req, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, &st))
			DIE("drmModeAtomicCommit: %s", strerror(errno));
		drmModeAtomicFree(req);

		/* Ждём события «кадр показан». Только после него старый буфер свободен. */
		st.pending = 1;
		while (st.pending) {
			struct pollfd p = { .fd = s.fd, .events = POLLIN };
			if (poll(&p, 1, 1000) <= 0)
				DIE("за секунду не пришло событие смены кадра");
			drmHandleEvent(s.fd, &ev);
		}
	}

	if (st.count)
		printf("смен кадра: %d, интервал: средний %.2f мс, минимум %.2f, максимум %.2f\n",
		       st.count, (double)st.sum_us / 1000.0 / st.count, (double)st.min_us / 1000.0,
		       (double)st.max_us / 1000.0);

	/* Убрать плоскость с экрана: без буфера и без CRTC. Блокирующий коммит. */
	drmModeAtomicReqPtr off = drmModeAtomicAlloc();
	drmModeAtomicAddProperty(off, plane, prop[FB_ID], 0);
	drmModeAtomicAddProperty(off, plane, prop[CRTC_ID], 0);
	if (drmModeAtomicCommit(s.fd, off, 0, NULL))
		fprintf(stderr, "не удалось убрать плоскость: %s\n", strerror(errno));
	drmModeAtomicFree(off);

	for (int i = 0; i < 2; i++) {
		drmModeRmFB(s.fd, fb[i]);
		dumb_destroy(s.fd, &buf[i]);
	}
	close(s.fd);
	return 0;
}
