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
