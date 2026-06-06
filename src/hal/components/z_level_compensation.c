/*
 * z_level_compensation — RT HAL component for Z-axis surface compensation.
 *
 * Reads a probed height map file and applies real-time Z offset correction
 * based on current X/Y position using bilinear interpolation on a 1mm grid.
 *
 * The probe map is in work coordinates.  Connect x-pos and y-pos to
 * work-coordinate position signals (e.g. halui.axis.x.pos-relative).
 *
 * Map loading is controlled via the GMI REST API:
 *   POST /api/v1/<instance>/load   {"filename": "...", "scale": 0.001}
 *   POST /api/v1/<instance>/unload
 *
 * For scattered probe points (non-regular grid), Inverse Distance Weighting
 * is used to fill the interpolation grid at load time.
 *
 * Usage:
 *   loadrt z_level_compensation
 *   addf z_level_compensation.compensate servo-thread
 *
 * Copyright (C) 2024 LinuxCNC contributors
 * License: GPL v2+
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gomc_env.h"
#include "gomc_hal.h"
#include "gomc_rtapi.h"
#include "gomc_log.h"
#include "z_level_compensation_api.h"

#define COMP_NAME "z_level_compensation"

/* Maximum probe points and grid dimensions */
#define MAX_PROBE_POINTS 10000
#define MAX_GRID_DIM     2000

/* -------------------------------------------------------------------------- */
/* Instance data                                                              */
/* -------------------------------------------------------------------------- */

typedef struct {
    /* HAL pins */
    gomc_hal_bit_t   *enable_in;
    gomc_hal_bit_t   *clear;
    gomc_hal_float_t *x_pos;
    gomc_hal_float_t *y_pos;
    gomc_hal_float_t *z_pos;
    gomc_hal_float_t *fade_height;
    gomc_hal_s32_t   *counts;
    gomc_hal_float_t *scale;
} hal_pins_t;

typedef struct {
    cmod_t            base;
    const cmod_env_t *env;
    int               comp_id;
    hal_pins_t       *pins;

    /* Interpolation grid (1mm resolution) */
    double           *grid;       /* [x_steps * y_steps] */
    int               x_min, x_max;
    int               y_min, y_max;
    int               x_steps, y_steps;
    int               grid_valid;

    /* Scale for counts output */
    double            out_scale;

    /* GMI API callbacks */
    z_level_compensation_callbacks_t api_cb;
} inst_t;

/* -------------------------------------------------------------------------- */
/* Forward declarations                                                       */
/* -------------------------------------------------------------------------- */

static int  load_probe_map(inst_t *inst, const char *filename);
static void compensate_funct(void *arg, long period);

/* -------------------------------------------------------------------------- */
/* Grid interpolation helpers                                                 */
/* -------------------------------------------------------------------------- */

/* Check if probe points form a regular grid */
static int detect_regular_grid(const double *xs, const double *ys, int n,
                               int *out_nx, int *out_ny,
                               double *out_x0, double *out_y0,
                               double *out_dx, double *out_dy)
{
    if (n < 4) return 0;

    /* Find unique sorted X and Y values */
    double ux[MAX_GRID_DIM], uy[MAX_GRID_DIM];
    int nux = 0, nuy = 0;

    for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; j < nux; j++) {
            if (fabs(ux[j] - xs[i]) < 0.01) { found = 1; break; }
        }
        if (!found && nux < MAX_GRID_DIM) ux[nux++] = xs[i];

        found = 0;
        for (int j = 0; j < nuy; j++) {
            if (fabs(uy[j] - ys[i]) < 0.01) { found = 1; break; }
        }
        if (!found && nuy < MAX_GRID_DIM) uy[nuy++] = ys[i];
    }

    /* Check if nux * nuy == n (complete grid) */
    if (nux * nuy != n) return 0;
    if (nux < 2 || nuy < 2) return 0;

    /* Sort unique values */
    for (int i = 0; i < nux - 1; i++)
        for (int j = i + 1; j < nux; j++)
            if (ux[j] < ux[i]) { double t = ux[i]; ux[i] = ux[j]; ux[j] = t; }
    for (int i = 0; i < nuy - 1; i++)
        for (int j = i + 1; j < nuy; j++)
            if (uy[j] < uy[i]) { double t = uy[i]; uy[i] = uy[j]; uy[j] = t; }

    /* Check uniform spacing */
    double dx = ux[1] - ux[0];
    double dy = uy[1] - uy[0];
    if (dx < 0.01 || dy < 0.01) return 0;

    for (int i = 2; i < nux; i++)
        if (fabs((ux[i] - ux[i-1]) - dx) > 0.01) return 0;
    for (int i = 2; i < nuy; i++)
        if (fabs((uy[i] - uy[i-1]) - dy) > 0.01) return 0;

    *out_nx = nux;
    *out_ny = nuy;
    *out_x0 = ux[0];
    *out_y0 = uy[0];
    *out_dx = dx;
    *out_dy = dy;
    return 1;
}

/* Inverse Distance Weighting (Shepard's method, p=2) */
static double idw_interpolate(const double *xs, const double *ys,
                              const double *zs, int n,
                              double xq, double yq)
{
    double wsum = 0.0, vsum = 0.0;
    for (int i = 0; i < n; i++) {
        double dx = xs[i] - xq;
        double dy = ys[i] - yq;
        double d2 = dx * dx + dy * dy;
        if (d2 < 1e-10) return zs[i];
        double w = 1.0 / d2;
        wsum += w;
        vsum += w * zs[i];
    }
    return (wsum > 0) ? vsum / wsum : 0.0;
}

/* Build the 1mm interpolation grid from probe points */
static int build_grid(inst_t *inst, const double *xs, const double *ys,
                      const double *zs, int n)
{
    const gomc_rtapi_t *rtapi = inst->env->rtapi;

    /* Find bounding box */
    double xmin = xs[0], xmax = xs[0];
    double ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < n; i++) {
        if (xs[i] < xmin) xmin = xs[i];
        if (xs[i] > xmax) xmax = xs[i];
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }

    inst->x_min = (int)floor(xmin);
    inst->x_max = (int)ceil(xmax);
    inst->y_min = (int)floor(ymin);
    inst->y_max = (int)ceil(ymax);
    inst->x_steps = inst->x_max - inst->x_min + 1;
    inst->y_steps = inst->y_max - inst->y_min + 1;

    if (inst->x_steps > MAX_GRID_DIM || inst->y_steps > MAX_GRID_DIM) {
        gomc_log_errorf(inst->env->log, COMP_NAME,
                        "grid too large: %d x %d (max %d)",
                        inst->x_steps, inst->y_steps, MAX_GRID_DIM);
        return -1;
    }

    size_t grid_size = (size_t)inst->x_steps * inst->y_steps * sizeof(double);

    /* Free old grid if any */
    if (inst->grid) {
        rtapi->free(rtapi->ctx, inst->grid);
        inst->grid = NULL;
    }

    inst->grid = rtapi->calloc(rtapi->ctx, grid_size);
    if (!inst->grid) return -1;

    /* Try to detect regular grid in input data */
    int nx, ny;
    double x0, y0, dx, dy;
    if (detect_regular_grid(xs, ys, n, &nx, &ny, &x0, &y0, &dx, &dy)) {
        gomc_log_infof(inst->env->log, COMP_NAME,
                       "regular input grid %dx%d, spacing %.1fx%.1f",
                       nx, ny, dx, dy);

        /* For each 1mm grid point, bilinear from the input grid */
        for (int gy = 0; gy < inst->y_steps; gy++) {
            double qy = inst->y_min + gy;
            double fy = (qy - y0) / dy;
            int iy0 = (int)floor(fy);
            int iy1 = iy0 + 1;
            double ty = fy - iy0;
            if (iy0 < 0) { iy0 = 0; ty = 0; }
            if (iy1 >= ny) { iy1 = ny - 1; ty = 1.0; }
            if (iy0 >= ny) iy0 = ny - 1;

            for (int gx = 0; gx < inst->x_steps; gx++) {
                double qx = inst->x_min + gx;
                double fx = (qx - x0) / dx;
                int ix0 = (int)floor(fx);
                int ix1 = ix0 + 1;
                double tx = fx - ix0;
                if (ix0 < 0) { ix0 = 0; tx = 0; }
                if (ix1 >= nx) { ix1 = nx - 1; tx = 1.0; }
                if (ix0 >= nx) ix0 = nx - 1;

                /* Find Z values at 4 corners of input cell */
                double z00 = 0, z10 = 0, z01 = 0, z11 = 0;
                for (int k = 0; k < n; k++) {
                    int kx = (int)round((xs[k] - x0) / dx);
                    int ky = (int)round((ys[k] - y0) / dy);
                    if (kx == ix0 && ky == iy0) z00 = zs[k];
                    else if (kx == ix1 && ky == iy0) z10 = zs[k];
                    else if (kx == ix0 && ky == iy1) z01 = zs[k];
                    else if (kx == ix1 && ky == iy1) z11 = zs[k];
                }

                double z = z00 * (1 - tx) * (1 - ty)
                         + z10 * tx * (1 - ty)
                         + z01 * (1 - tx) * ty
                         + z11 * tx * ty;
                inst->grid[gy * inst->x_steps + gx] = z;
            }
        }
    } else {
        /* Scattered points — use IDW to fill each grid cell */
        gomc_log_infof(inst->env->log, COMP_NAME,
                       "scattered input (%d points), using IDW", n);
        for (int gy = 0; gy < inst->y_steps; gy++) {
            double qy = inst->y_min + gy;
            for (int gx = 0; gx < inst->x_steps; gx++) {
                double qx = inst->x_min + gx;
                inst->grid[gy * inst->x_steps + gx] =
                    idw_interpolate(xs, ys, zs, n, qx, qy);
            }
        }
    }

    inst->grid_valid = 1;
    gomc_log_infof(inst->env->log, COMP_NAME,
                   "grid loaded: %d x %d mm, bounds [%d..%d, %d..%d]",
                   inst->x_steps, inst->y_steps,
                   inst->x_min, inst->x_max, inst->y_min, inst->y_max);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* File loading                                                               */
/* -------------------------------------------------------------------------- */

static int load_probe_map(inst_t *inst, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        gomc_log_errorf(inst->env->log, COMP_NAME,
                        "cannot open probe file: %s", filename);
        inst->grid_valid = 0;
        return -1;
    }

    /* Read probe points: "X Y Z" per line */
    double *xs = NULL, *ys = NULL, *zs = NULL;
    int n = 0, cap = 256;

    xs = malloc(cap * sizeof(double));
    ys = malloc(cap * sizeof(double));
    zs = malloc(cap * sizeof(double));
    if (!xs || !ys || !zs) goto oom;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        double x, y, z;
        if (sscanf(line, "%lf %lf %lf", &x, &y, &z) != 3) continue;

        if (n >= cap) {
            cap *= 2;
            if (cap > MAX_PROBE_POINTS) {
                gomc_log_errorf(inst->env->log, COMP_NAME,
                                "too many probe points (max %d)",
                                MAX_PROBE_POINTS);
                goto fail;
            }
            double *tmp;
            tmp = realloc(xs, cap * sizeof(double)); if (!tmp) goto oom; xs = tmp;
            tmp = realloc(ys, cap * sizeof(double)); if (!tmp) goto oom; ys = tmp;
            tmp = realloc(zs, cap * sizeof(double)); if (!tmp) goto oom; zs = tmp;
        }
        xs[n] = x;
        ys[n] = y;
        zs[n] = z;
        n++;
    }
    fclose(f);
    f = NULL;

    if (n < 3) {
        gomc_log_errorf(inst->env->log, COMP_NAME,
                        "too few probe points (%d, need at least 3)", n);
        goto fail;
    }

    gomc_log_infof(inst->env->log, COMP_NAME,
                   "read %d probe points from %s", n, filename);

    int ret = build_grid(inst, xs, ys, zs, n);
    free(xs); free(ys); free(zs);
    return ret;

oom:
    gomc_log_errorf(inst->env->log, COMP_NAME, "out of memory loading probe map");
fail:
    free(xs); free(ys); free(zs);
    if (f) fclose(f);
    inst->grid_valid = 0;
    return -1;
}

/* -------------------------------------------------------------------------- */
/* GMI API callbacks                                                          */
/* -------------------------------------------------------------------------- */

static bool gmi_z_level_compensation_load(void *ctx, const char *filename,
                                          double scale)
{
    inst_t *inst = (inst_t *)ctx;

    if (!filename || filename[0] == '\0') {
        gomc_log_errorf(inst->env->log, COMP_NAME, "load: empty filename");
        return false;
    }
    if (scale <= 0.0) {
        gomc_log_errorf(inst->env->log, COMP_NAME,
                        "load: scale must be positive (got %g)", scale);
        return false;
    }

    inst->out_scale = scale;

    if (load_probe_map(inst, filename) < 0)
        return false;

    return true;
}

static bool gmi_z_level_compensation_unload(void *ctx)
{
    inst_t *inst = (inst_t *)ctx;
    const gomc_rtapi_t *rtapi = inst->env->rtapi;

    inst->grid_valid = 0;
    if (inst->grid) {
        rtapi->free(rtapi->ctx, inst->grid);
        inst->grid = NULL;
    }
    *inst->pins->counts = 0;

    gomc_log_infof(inst->env->log, COMP_NAME, "probe map unloaded");
    return true;
}

/* -------------------------------------------------------------------------- */
/* RT function — called at servo rate                                         */
/* -------------------------------------------------------------------------- */

static void compensate_funct(void *arg, long period)
{
    inst_t *inst = (inst_t *)arg;
    hal_pins_t *p = inst->pins;
    (void)period;

    /* Not enabled or no valid grid: output zero */
    if (!*p->enable_in || !inst->grid_valid) {
        *p->counts = 0;
        *p->scale = inst->out_scale;
        return;
    }

    /* Clear pin: zero the output */
    if (*p->clear) {
        *p->counts = 0;
        *p->scale = inst->out_scale;
        return;
    }

    /* Get work-coordinate position */
    double x = *p->x_pos;
    double y = *p->y_pos;
    double z = *p->z_pos;

    /* Clamp to grid bounds */
    if (x < inst->x_min) x = inst->x_min;
    if (x > inst->x_max) x = inst->x_max;
    if (y < inst->y_min) y = inst->y_min;
    if (y > inst->y_max) y = inst->y_max;

    /* Grid-relative coordinates */
    double gx = x - inst->x_min;
    double gy = y - inst->y_min;

    /* Integer grid indices */
    int ix = (int)floor(gx);
    int iy = (int)floor(gy);

    /* Clamp indices */
    if (ix < 0) ix = 0;
    if (ix >= inst->x_steps - 1) ix = inst->x_steps - 2;
    if (iy < 0) iy = 0;
    if (iy >= inst->y_steps - 1) iy = inst->y_steps - 2;

    /* Fractional part */
    double tx = gx - ix;
    double ty = gy - iy;

    /* Bilinear interpolation */
    double z00 = inst->grid[iy * inst->x_steps + ix];
    double z10 = inst->grid[iy * inst->x_steps + ix + 1];
    double z01 = inst->grid[(iy + 1) * inst->x_steps + ix];
    double z11 = inst->grid[(iy + 1) * inst->x_steps + ix + 1];

    double z_offset = z00 * (1 - tx) * (1 - ty)
                    + z10 * tx * (1 - ty)
                    + z01 * (1 - tx) * ty
                    + z11 * tx * ty;

    /* Apply fade height */
    double fade = *p->fade_height;
    double comp_scale = 1.0;
    if (fade > 0.0) {
        if (z >= fade) {
            comp_scale = 0.0;
        } else {
            comp_scale = (fade - z) / fade;
            if (comp_scale > 1.0) comp_scale = 1.0;
        }
    }

    /* Output: counts * scale = desired offset */
    int32_t c = (int32_t)(z_offset * comp_scale / inst->out_scale);
    *p->counts = c;
    *p->scale = inst->out_scale;
}

/* -------------------------------------------------------------------------- */
/* cmod lifecycle                                                             */
/* -------------------------------------------------------------------------- */

static void inst_destroy(cmod_t *self)
{
    inst_t *inst = (inst_t *)self;
    const gomc_rtapi_t *rtapi = inst->env->rtapi;

    if (inst->grid)
        rtapi->free(rtapi->ctx, inst->grid);
    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    rtapi->free(rtapi->ctx, inst);
}

/* -------------------------------------------------------------------------- */
/* Entry point                                                                */
/* -------------------------------------------------------------------------- */

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    const gomc_hal_t *hal = env->hal;
    const gomc_rtapi_t *rtapi = env->rtapi;
    (void)argc;
    (void)argv;

    inst_t *inst = rtapi->calloc(rtapi->ctx, sizeof(inst_t));
    if (!inst) return -1;

    inst->env = env;
    inst->base.Destroy = inst_destroy;
    inst->out_scale = 0.001;  /* default until load() is called */

    /* Init HAL component */
    int cid = hal->init(hal->ctx, name, env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (cid < 0) goto err;
    inst->comp_id = cid;

    /* Allocate pins struct */
    inst->pins = hal->malloc(hal->ctx, sizeof(hal_pins_t));
    if (!inst->pins) goto err;

    /* Create pins */
    int r = 0;
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &inst->pins->enable_in, cid,
                                "%s.enable-in", name);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &inst->pins->clear, cid,
                                "%s.clear", name);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &inst->pins->x_pos, cid,
                                  "%s.x-pos", name);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &inst->pins->y_pos, cid,
                                  "%s.y-pos", name);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &inst->pins->z_pos, cid,
                                  "%s.z-pos", name);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &inst->pins->fade_height, cid,
                                  "%s.fade-height", name);
    r |= gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &inst->pins->counts, cid,
                                "%s.counts", name);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &inst->pins->scale, cid,
                                  "%s.scale", name);
    if (r) goto err;

    /* Register GMI API */
    if (env->api) {
        inst->api_cb.ctx = inst;
        inst->api_cb.load = gmi_z_level_compensation_load;
        inst->api_cb.unload = gmi_z_level_compensation_unload;
        r = z_level_compensation_api_register(env->api, name, &inst->api_cb);
        if (r) {
            gomc_log_errorf(env->log, COMP_NAME,
                            "failed to register GMI API: %d", r);
            goto err;
        }
    }

    /* Export RT function */
    char buf[128];
    snprintf(buf, sizeof(buf), "%s.compensate", name);
    r = hal->export_funct(hal->ctx, buf, compensate_funct, inst, 1, 0, cid);
    if (r) goto err;

    r = hal->ready(hal->ctx, cid);
    if (r) goto err;

    *out = &inst->base;
    return 0;

err:
    if (inst->grid)
        rtapi->free(rtapi->ctx, inst->grid);
    if (inst->comp_id > 0)
        hal->exit(hal->ctx, inst->comp_id);
    rtapi->free(rtapi->ctx, inst);
    return -1;
}
