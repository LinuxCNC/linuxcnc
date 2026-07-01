/*
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef INTERP_PARAMETER_IO_HH
#define INTERP_PARAMETER_IO_HH

// Parameter array size — must match interp_parameter_def.hh RS274NGC_MAX_PARAMETERS.
#define INTERP_PARAM_MAX 5602

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback interface for interpreter parameter persistence.
 *
 * The interpreter uses these callbacks to load and save persistent
 * parameters (G28/G30 home, G92 offsets, coordinate systems, etc.)
 * without knowing the storage backend.
 *
 * Two implementations are expected:
 *  - File-based (default, exported by librs274ngc for SAI etc.)
 *  - GMI persist-backed (provided by gomc/milltask)
 */

struct interp_param_io_t {
    /**
     * Restore parameters from storage.
     *
     * Must fill parameters[0..INTERP_PARAM_MAX-1].
     * Unset parameters should be zeroed.
     *
     * @param ctx       opaque context pointer
     * @param parameters  array to fill (size INTERP_PARAM_MAX)
     * @return 0 on success, non-zero on error
     */
    int (*restore)(void *ctx, double parameters[INTERP_PARAM_MAX]);

    /**
     * Save parameters to storage.
     *
     * Only parameters listed in required_params need to be persisted.
     * The required_params array is terminated by INTERP_PARAM_MAX.
     *
     * @param ctx             opaque context pointer
     * @param parameters      array to read (size INTERP_PARAM_MAX)
     * @param required_params sorted list of parameter indices that must be saved,
     *                        terminated by INTERP_PARAM_MAX
     * @return 0 on success, non-zero on error
     */
    int (*save)(void *ctx, const double parameters[INTERP_PARAM_MAX],
                const int required_params[]);

    /** Opaque context passed to restore/save. */
    void *ctx;
};
typedef struct interp_param_io_t interp_param_io_t;

/**
 * Create a file-based parameter I/O implementation.
 * This is the default used by SAI and other standalone consumers.
 *
 * @param filename  path to the parameter file
 * @return filled interp_param_io_t (caller owns; ctx must be freed via
 *         interp_param_io_file_destroy)
 */
struct interp_param_io_t interp_param_io_file_create(const char *filename);

/**
 * Destroy a file-based parameter I/O context.
 */
void interp_param_io_file_destroy(struct interp_param_io_t *io);

#ifdef __cplusplus
}
#endif

#endif // INTERP_PARAMETER_IO_HH
