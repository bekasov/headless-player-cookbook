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
