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

#include "interp_parameter_io.hh"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include "interp_internal.hh"

struct file_io_ctx {
    char *filename;
};

static int file_restore(void *ctx, double parameters[INTERP_PARAM_MAX])
{
    auto *fctx = static_cast<file_io_ctx *>(ctx);
    FILE *infile;
    char line[256];
    int variable;
    double value;
    int k;

    // Zero all parameters first.
    for (k = 0; k < INTERP_PARAM_MAX; k++)
        parameters[k] = 0;

    // It's OK if the file doesn't exist yet.
    if (access(fctx->filename, F_OK) == -1)
        return 0;

    infile = fopen(fctx->filename, "r");
    if (infile == NULL)
        return -1;

    k = 0;
    while (feof(infile) == 0) {
        if (fgets(line, sizeof(line), infile) == NULL)
            break;
        if (sscanf(line, "%d %lf", &variable, &value) == 2) {
            if (variable <= 0 || variable >= INTERP_PARAM_MAX) {
                fclose(infile);
                return -1;
            }
            if (variable < k) {
                fclose(infile);
                return -1; // out of order
            }
            parameters[variable] = value;
            k = variable + 1;
        }
    }
    fclose(infile);
    return 0;
}

static int file_save(void *ctx, const double parameters[INTERP_PARAM_MAX],
                     const int required_params[])
{
    auto *fctx = static_cast<file_io_ctx *>(ctx);
    FILE *infile;
    FILE *outfile;
    char line[PATH_MAX];
    int variable;
    double value;
    int required;
    int index;
    int k;

    std::string tempfile = std::string(fctx->filename) + ".new";
    outfile = fopen(tempfile.c_str(), "w");
    if (outfile == NULL)
        return -1;

    infile = fopen(fctx->filename, "r");
    if (!infile)
        infile = fopen("/dev/null", "r");

    k = 0;
    index = 0;
    required = required_params[index++];
    while (feof(infile) == 0) {
        if (fgets(line, sizeof(line), infile) == NULL)
            break;
        if (sscanf(line, "%d %lf", &variable, &value) == 2) {
            if (variable <= 0 || variable >= INTERP_PARAM_MAX) {
                fclose(infile);
                fclose(outfile);
                unlink(tempfile.c_str());
                return -1;
            }
            for (; k < INTERP_PARAM_MAX; k++) {
                if (k > variable) {
                    fclose(infile);
                    fclose(outfile);
                    unlink(tempfile.c_str());
                    return -1; // out of order
                } else if (k == variable) {
                    snprintf(line, sizeof(line), "%d\t%f\n", k, parameters[k]);
                    fputs(line, outfile);
                    if (k == required)
                        required = required_params[index++];
                    k++;
                    break;
                } else if (k == required) {
                    snprintf(line, sizeof(line), "%d\t%f\n", k, parameters[k]);
                    fputs(line, outfile);
                    required = required_params[index++];
                }
            }
        }
    }
    fclose(infile);
    for (; k < INTERP_PARAM_MAX; k++) {
        if (k == required) {
            snprintf(line, sizeof(line), "%d\t%f\n", k, parameters[k]);
            fputs(line, outfile);
            required = required_params[index++];
        }
    }

    fflush(outfile);
    fdatasync(fileno(outfile));
    fclose(outfile);

    std::string bakfile = std::string(fctx->filename) + RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX;
    unlink(bakfile.c_str());
    if (link(fctx->filename, bakfile.c_str()) < 0)
        perror("link (updating variable file)");
    if (rename(tempfile.c_str(), fctx->filename) < 0)
        perror("rename (updating variable file)");

    return 0;
}

interp_param_io_t interp_param_io_file_create(const char *filename)
{
    auto *fctx = new file_io_ctx;
    fctx->filename = strdup(filename);

    interp_param_io_t io = {};
    io.restore = file_restore;
    io.save = file_save;
    io.ctx = fctx;
    return io;
}

void interp_param_io_file_destroy(interp_param_io_t *io)
{
    if (io && io->ctx) {
        auto *fctx = static_cast<file_io_ctx *>(io->ctx);
        free(fctx->filename);
        delete fctx;
        io->ctx = NULL;
    }
}
