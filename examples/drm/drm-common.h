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

static inline long long prop_value(int fd, uint32_t obj, uint32_t obj_type, const char *name)
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
static inline void screen_open(struct screen *s, const char *path)
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
static inline uint32_t pick_plane(struct screen *s, uint32_t wanted, uint32_t format)
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
static inline void dumb_create(int fd, uint32_t w, uint32_t h, uint32_t bpp, struct dumb *d)
{
	struct drm_mode_create_dumb create = { .width = w, .height = h, .bpp = bpp };
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create))
		DIE("DRM_IOCTL_MODE_CREATE_DUMB: %s", strerror(errno));

	struct drm_mode_map_dumb map = { .handle = create.handle };
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map))
		DIE("DRM_IOCTL_MODE_MAP_DUMB: %s", strerror(errno));

	/* Ядро дало не адрес, а «смещение» для mmap по этому же файлу устройства. */
	d->map = mmap(NULL, (size_t)create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map.offset);
	if (d->map == MAP_FAILED)
		DIE("mmap: %s", strerror(errno));
	d->handle = create.handle;
	d->pitch = create.pitch;
	d->size = create.size;
}

static inline void dumb_destroy(int fd, struct dumb *d)
{
	struct drm_mode_destroy_dumb destroy = { .handle = d->handle };

	munmap(d->map, (size_t)d->size);
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

/* Показать кадр fb размером w x h по центру экрана на несколько секунд и убрать. */
static inline void show(struct screen *s, uint32_t plane, uint32_t fb, uint32_t w, uint32_t h, int seconds)
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
