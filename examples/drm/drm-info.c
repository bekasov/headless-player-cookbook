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
