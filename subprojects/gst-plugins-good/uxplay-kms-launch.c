/* SPDX-License-Identifier: LGPL-2.1-or-later */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Keep one DRM master descriptor across UxPlay renderer resets. The Meson
 * console primary plane is opaque and above its fixed-z-order NV12 plane. */
static uint64_t plane_type(int fd, uint32_t id)
{
    uint64_t type = UINT64_MAX;
    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(fd, id, DRM_MODE_OBJECT_PLANE);
    if (!props)
        return type;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (!prop)
            continue;
        if (strcmp(prop->name, "type") == 0)
            type = props->prop_values[i];
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);
    return type;
}

int main(int argc, char **argv)
{
    uint32_t connector = 0, crtc = 0, primary = 0, overlay = 0;
    int pipe_index = -1;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s /path/to/uxplay [options without -vs or -fs]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (access(argv[1], X_OK) < 0) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/dri/card0");
        return EXIT_FAILURE;
    }
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0) {
        perror("enable universal DRM planes");
        close(fd);
        return EXIT_FAILURE;
    }
    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) {
        perror("get DRM resources");
        close(fd);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < res->count_connectors && !crtc; ++i) {
        drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED &&
            conn->connector_type == DRM_MODE_CONNECTOR_HDMIA && conn->encoder_id) {
            drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                if (enc->crtc_id) {
                    connector = conn->connector_id;
                    crtc = enc->crtc_id;
                }
                drmModeFreeEncoder(enc);
            }
        }
        drmModeFreeConnector(conn);
    }
    for (int i = 0; i < res->count_crtcs; ++i)
        if (res->crtcs[i] == crtc)
            pipe_index = i;
    drmModeFreeResources(res);
    if (!crtc || pipe_index < 0 || pipe_index >= 32) {
        fprintf(stderr, "No active HDMI CRTC on /dev/dri/card0\n");
        close(fd);
        return EXIT_FAILURE;
    }
    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
    if (!planes) {
        perror("get DRM planes");
        close(fd);
        return EXIT_FAILURE;
    }
    for (uint32_t i = 0; i < planes->count_planes; ++i) {
        drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[i]);
        if (!plane)
            continue;
        if (plane->possible_crtcs & (UINT32_C(1) << pipe_index)) {
            uint64_t type = plane_type(fd, plane->plane_id);
            if (type == DRM_PLANE_TYPE_PRIMARY)
                primary = plane->plane_id;
            if (type == DRM_PLANE_TYPE_OVERLAY) {
                for (uint32_t j = 0; j < plane->count_formats; ++j)
                    if (plane->formats[j] == DRM_FORMAT_NV12)
                        overlay = plane->plane_id;
            }
        }
        drmModeFreePlane(plane);
    }
    drmModeFreePlaneResources(planes);
    if (!primary || !overlay) {
        fprintf(stderr, "HDMI has no matching primary and NV12 overlay planes\n");
        close(fd);
        return EXIT_FAILURE;
    }
    if (drmModeSetPlane(fd, primary, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) < 0) {
        perror("disable opaque console plane");
        close(fd);
        return EXIT_FAILURE;
    }
    char sink[128];
    snprintf(sink, sizeof(sink), "kmssink fd=%d connector-id=%" PRIu32
             " plane-id=%" PRIu32, fd, connector, overlay);
    char *child_argv[argc + 2];
    for (int i = 1; i < argc; ++i)
        child_argv[i - 1] = argv[i];
    child_argv[argc - 1] = "-vs";
    child_argv[argc] = sink;
    child_argv[argc + 1] = NULL;
    fprintf(stderr, "Direct NV12 HDMI: connector=%" PRIu32 " crtc=%" PRIu32
            " overlay=%" PRIu32 " console=%" PRIu32 " drm-fd=%d\n",
            connector, crtc, overlay, primary, fd);
    /* fd intentionally has no FD_CLOEXEC: kmssink must share this DRM master. */
    execv(argv[1], child_argv);
    perror("exec UxPlay");
    close(fd);
    return EXIT_FAILURE;
}
