//
// filestream.c — combined file-backed HAL sample/replay component (cmod)
//
// A single instance can REPLAY values from a file onto HAL output pins and/or
// CAPTURE HAL input pins into a file, sharing one sample counter and one
// non-realtime I/O thread.  It is the deterministic, file-based sibling of the
// WebSocket sampler/streamer comps, intended for the runtests suite (and any
// local capture/replay) where a self-contained golden file is wanted rather
// than a live network stream.
//
// Design:
//   - Two exported RT functions so the drive/capture ordering is preserved:
//       <name>.write  (replay: file -> stream pins)  -- addf EARLY
//       <name>.read   (capture: sample pins -> file) -- addf LATE
//     i.e.  addf <name>.write ; addf <comp-under-test> ; addf <name>.read
//   - Two SPSC ring buffers (replay, capture) between the RT funcs and one
//     non-RT I/O thread; the RT side never touches files.
//   - One shared sample counter; replay drives until the input file is
//     exhausted, then raises the `done` pin (so tests can wait on it instead of
//     sleeping).
//
// The capture text format is byte-identical to the halsampler client, and the
// replay input format matches the halstreamer client (whitespace-separated
// fields, '#' comment lines and blank lines skipped), so existing golden files
// are reusable unchanged.
//
// Usage:
//   load filestream stream_cfg=f infile=in.txt sample_cfg=ff outfile=out.txt depth=350 [tag]
//   Either direction may be omitted (capture-only or replay-only).
//
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "gomc_env.h"
#include "hal_stream_common.h"

#define DEFAULT_DEPTH   1000
#define LINEBUF         512
#define IO_POLL_US      200   // I/O thread poll interval

// ---------------------------------------------------------------------------
// Per-instance state
// ---------------------------------------------------------------------------

typedef struct {
    // shared
    volatile unsigned *enable;
    volatile int32_t  *sample_num;   // captured-sample counter (halsampler tag)
    volatile unsigned *done;         // run complete (replay drained / N captured)
    int depth;
    int max_samples;                 // stop capturing after N (0 = unbounded)

    // --- replay side (file -> stream pins) ---
    int  n_stream;
    char stream_types[HAL_STREAM_MAX_PINS];
    hal_stream_pin_t stream_pins[HAL_STREAM_MAX_PINS];
    hal_stream_val_t *replay;        // replay[depth * n_stream]
    volatile uint32_t rep_wpos;      // written by I/O thread
    uint32_t rep_rpos;               // read by write_funct (RT) only
    volatile int infile_eof;         // set by I/O thread after last line
    volatile int32_t *stream_num;    // replayed-sample counter
    volatile int32_t *underruns;     // replay ring empty but not EOF
    FILE *infp;

    // --- capture side (sample pins -> file) ---
    int  n_sample;
    char sample_types[HAL_STREAM_MAX_PINS];
    hal_stream_pin_t sample_pins[HAL_STREAM_MAX_PINS];
    hal_stream_val_t *capture;       // capture[depth * n_sample]
    int32_t *cap_seq;                // sample number tagged per slot
    volatile uint32_t cap_wpos;      // written by read_funct (RT) only
    uint32_t cap_rpos;               // read by I/O thread
    volatile int32_t *overruns;      // capture ring full (dropped)
    FILE *outfp;
    int tag;                         // prefix each captured line with seq

    // I/O thread
    pthread_t io_tid;
    volatile int io_stop;
    int io_running;
} filestream_inst_t;

typedef struct {
    cmod_env_t env;
    cmod_t mod;
    int comp_id;
    char name[64];
    filestream_inst_t inst;
} filestream_priv_t;

// ---------------------------------------------------------------------------
// RT function: replay — pop one sample from the replay ring onto stream pins
// ---------------------------------------------------------------------------

static void write_funct(void *arg, long period) {
    filestream_inst_t *m = (filestream_inst_t *)arg;
    (void)period;

    if (!*(m->enable)) return;

    if (m->rep_rpos == m->rep_wpos) {
        // ring empty: either replay is complete, or the I/O thread is behind.
        // When there is no capture side to signal completion, the replay-drained
        // condition is the run's `done`.
        if (m->infile_eof) {
            if (m->n_sample == 0) *(m->done) = 1;
        } else {
            (*(m->underruns))++;
        }
        return;
    }

    uint32_t idx = m->rep_rpos % m->depth;
    hal_stream_val_t *src = &m->replay[idx * m->n_stream];
    for (int n = 0; n < m->n_stream; n++)
        hal_stream_apply(m->stream_types[n], m->stream_pins[n], &src[n]);

    m->rep_rpos++;
    (*(m->stream_num))++;
}

// ---------------------------------------------------------------------------
// RT function: capture — push current sample pins into the capture ring
// ---------------------------------------------------------------------------

static void read_funct(void *arg, long period) {
    filestream_inst_t *m = (filestream_inst_t *)arg;
    (void)period;

    if (!*(m->enable)) return;

    // Stop after the requested number of samples so the captured file has a
    // deterministic length (and signal completion via `done`).
    if (m->max_samples > 0 && *(m->sample_num) >= m->max_samples) {
        *(m->done) = 1;
        return;
    }

    uint32_t used = m->cap_wpos - m->cap_rpos;
    if (used >= (uint32_t)m->depth) {
        (*(m->overruns))++;   // I/O thread fell behind; drop this sample
        return;
    }

    uint32_t idx = m->cap_wpos % m->depth;
    hal_stream_val_t *dst = &m->capture[idx * m->n_sample];
    for (int n = 0; n < m->n_sample; n++)
        hal_stream_capture(&dst[n], m->sample_types[n], m->sample_pins[n]);

    (*(m->sample_num))++;
    m->cap_seq[idx] = *(m->sample_num);

    __sync_synchronize();     // publish sample before advancing the write index
    m->cap_wpos = m->cap_wpos + 1;

    if (m->max_samples > 0 && *(m->sample_num) >= m->max_samples)
        *(m->done) = 1;
}

// ---------------------------------------------------------------------------
// I/O thread helpers (non-RT)
// ---------------------------------------------------------------------------

// Read the next data line from infp into `out` (n_stream values).
// Returns 1 on success, 0 at EOF.  Blank lines and '#' comments are skipped;
// malformed lines are skipped with a warning.
static int read_replay_line(filestream_inst_t *m, hal_stream_val_t *out) {
    char line[LINEBUF];
    while (fgets(line, sizeof(line), m->infp)) {
        // trim leading whitespace
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0' || *p == '#') continue;   // blank or comment

        int n = 0;
        char *tok, *save;
        int bad = 0;
        for (tok = strtok_r(p, " \t\r\n", &save);
             tok && n < m->n_stream;
             tok = strtok_r(NULL, " \t\r\n", &save), n++) {
            char *end;
            switch (m->stream_types[n]) {
            case 'f': out[n].f = strtod(tok, &end); break;
            case 'b': out[n].b = (strtoul(tok, &end, 10) != 0) ? 1 : 0; break;
            case 'u': out[n].u = (uint32_t)strtoul(tok, &end, 10); break;
            case 's': out[n].s = (int32_t)strtol(tok, &end, 10); break;
            default:  end = tok; break;
            }
            if (end == tok) bad = 1;
        }
        if (bad || n != m->n_stream) {
            fprintf(stderr, "filestream: skipping malformed replay line\n");
            continue;
        }
        return 1;
    }
    return 0;
}

// Format one captured sample onto outfp (byte-identical to halsampler).
static void write_capture_sample(filestream_inst_t *m,
                                 const hal_stream_val_t *s, int32_t seq) {
    if (m->tag) fprintf(m->outfp, "%d ", seq);
    for (int n = 0; n < m->n_sample; n++) {
        switch (m->sample_types[n]) {
        case 'f': fprintf(m->outfp, "%f ", s[n].f); break;
        case 'b': fputs(s[n].b ? "1 " : "0 ", m->outfp); break;
        case 'u': fprintf(m->outfp, "%u ", s[n].u); break;
        case 's': fprintf(m->outfp, "%d ", s[n].s); break;
        }
    }
    fputc('\n', m->outfp);
}

// Drain everything currently in the capture ring to the output file.
static void drain_capture(filestream_inst_t *m) {
    if (!m->outfp) return;
    int wrote = 0;
    while (m->cap_rpos != m->cap_wpos) {
        uint32_t idx = m->cap_rpos % m->depth;
        write_capture_sample(m, &m->capture[idx * m->n_sample], m->cap_seq[idx]);
        m->cap_rpos++;
        wrote = 1;
    }
    if (wrote) fflush(m->outfp);
}

static void *io_thread(void *arg) {
    filestream_priv_t *priv = (filestream_priv_t *)arg;
    filestream_inst_t *m = &priv->inst;
    hal_stream_val_t pending[HAL_STREAM_MAX_PINS];
    int have_pending = 0;

    while (!m->io_stop) {
        // Fill the replay ring from the input file (if any).
        if (m->infp && !m->infile_eof) {
            for (;;) {
                uint32_t used = m->rep_wpos - m->rep_rpos;
                if (used >= (uint32_t)m->depth) break;        // ring full
                if (!have_pending) {
                    if (!read_replay_line(m, pending)) { m->infile_eof = 1; break; }
                    have_pending = 1;
                }
                uint32_t idx = m->rep_wpos % m->depth;
                memcpy(&m->replay[idx * m->n_stream], pending,
                       m->n_stream * sizeof(hal_stream_val_t));
                have_pending = 0;
                __sync_synchronize();   // publish sample before advancing index
                m->rep_wpos = m->rep_wpos + 1;
            }
        }
        // Drain the capture ring to the output file (if any).
        drain_capture(m);
        usleep(IO_POLL_US);
    }

    drain_capture(m);   // final flush of anything captured after the last poll
    return NULL;
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

static void filestream_destroy(cmod_t *self) {
    filestream_priv_t *priv = (filestream_priv_t *)self->priv;
    filestream_inst_t *m = &priv->inst;

    if (m->io_running) {
        m->io_stop = 1;
        pthread_join(m->io_tid, NULL);
    }
    if (priv->comp_id > 0)
        priv->env.hal->exit(priv->env.hal->ctx, priv->comp_id);
    if (m->infp)  fclose(m->infp);
    if (m->outfp) fclose(m->outfp);
    free(m->replay);
    free(m->capture);
    free(m->cap_seq);
    free(priv);
}

// ---------------------------------------------------------------------------
// New — constructor
// ---------------------------------------------------------------------------

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    int retval;
    const char *stream_cfg = NULL, *sample_cfg = NULL;
    const char *infile = NULL, *outfile = NULL;
    int depth = DEFAULT_DEPTH, tag = 0, samples = 0;

    for (int i = 0; i < argc; i++) {
        if      (strncmp(argv[i], "stream_cfg=", 11) == 0) stream_cfg = argv[i] + 11;
        else if (strncmp(argv[i], "sample_cfg=", 11) == 0) sample_cfg = argv[i] + 11;
        else if (strncmp(argv[i], "infile=",  7) == 0)     infile     = argv[i] + 7;
        else if (strncmp(argv[i], "outfile=", 8) == 0)     outfile    = argv[i] + 8;
        else if (strncmp(argv[i], "depth=",   6) == 0) {
            depth = atoi(argv[i] + 6);
            if (depth <= 0) depth = DEFAULT_DEPTH;
        }
        else if (strncmp(argv[i], "samples=", 8) == 0)     samples = atoi(argv[i] + 8);
        else if (strcmp(argv[i], "tag") == 0)              tag = 1;
    }

    if (!env->hal) { gomc_log_errorf(env->log, name, "HAL API not available"); return -EINVAL; }

    if (!stream_cfg && !sample_cfg) {
        gomc_log_errorf(env->log, name,
            "filestream needs stream_cfg= and/or sample_cfg=");
        return -EINVAL;
    }
    if (stream_cfg && !infile) {
        gomc_log_errorf(env->log, name, "stream_cfg= requires infile="); return -EINVAL;
    }
    if (sample_cfg && !outfile) {
        gomc_log_errorf(env->log, name, "sample_cfg= requires outfile="); return -EINVAL;
    }

    filestream_priv_t *priv = calloc(1, sizeof(filestream_priv_t));
    if (!priv) return -ENOMEM;
    priv->env = *env;
    snprintf(priv->name, sizeof(priv->name), "%s", name);
    filestream_inst_t *m = &priv->inst;
    m->depth = depth;
    m->tag = tag;
    m->max_samples = samples;

    if (stream_cfg) {
        m->n_stream = hal_stream_parse_cfg(stream_cfg, m->stream_types, HAL_STREAM_MAX_PINS);
        if (m->n_stream < 0) {
            gomc_log_errorf(env->log, name, "invalid stream_cfg '%s'", stream_cfg);
            free(priv); return -EINVAL;
        }
    }
    if (sample_cfg) {
        m->n_sample = hal_stream_parse_cfg(sample_cfg, m->sample_types, HAL_STREAM_MAX_PINS);
        if (m->n_sample < 0) {
            gomc_log_errorf(env->log, name, "invalid sample_cfg '%s'", sample_cfg);
            free(priv); return -EINVAL;
        }
    }

    // Allocate rings + open files
    if (m->n_stream) {
        m->replay = calloc((size_t)depth * m->n_stream, sizeof(hal_stream_val_t));
        if (!m->replay) { retval = -ENOMEM; goto fail_early; }
        m->infp = fopen(infile, "r");
        if (!m->infp) {
            gomc_log_errorf(env->log, name, "cannot open infile '%s'", infile);
            retval = -ENOENT; goto fail_early;
        }
    }
    if (m->n_sample) {
        m->capture = calloc((size_t)depth * m->n_sample, sizeof(hal_stream_val_t));
        m->cap_seq = calloc((size_t)depth, sizeof(int32_t));
        if (!m->capture || !m->cap_seq) { retval = -ENOMEM; goto fail_early; }
        m->outfp = fopen(outfile, "w");
        if (!m->outfp) {
            gomc_log_errorf(env->log, name, "cannot open outfile '%s'", outfile);
            retval = -ENOENT; goto fail_early;
        }
    }

    priv->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (priv->comp_id < 0) {
        gomc_log_errorf(env->log, name, "hal_init failed");
        retval = -1; goto fail_early;
    }

    char pin[128];
    #define PIN(field, pname, type, dir)                                       \
        do {                                                                   \
            snprintf(pin, sizeof(pin), "%s." pname, name);                     \
            retval = env->hal->pin_new(env->hal->ctx, pin, (type), (dir),      \
                         (void **)&(field), priv->comp_id);                    \
            if (retval != 0) goto fail;                                        \
        } while (0)

    PIN(m->enable,     "enable",     GOMC_HAL_BIT, GOMC_HAL_IN);
    PIN(m->sample_num, "sample-num", GOMC_HAL_S32, GOMC_HAL_IO);
    PIN(m->done,       "done",       GOMC_HAL_BIT, GOMC_HAL_OUT);
    *(m->enable) = 1;
    *(m->sample_num) = 0;
    *(m->done) = 0;

    int usefp_w = 0, usefp_r = 0;

    if (m->n_stream) {
        PIN(m->stream_num, "stream-num", GOMC_HAL_S32, GOMC_HAL_IO);
        PIN(m->underruns,  "underruns",  GOMC_HAL_S32, GOMC_HAL_IO);
        *(m->stream_num) = 0; *(m->underruns) = 0;
        for (int n = 0; n < m->n_stream; n++) {
            int t = GOMC_HAL_FLOAT;
            switch (m->stream_types[n]) {
            case 'f': t = GOMC_HAL_FLOAT; usefp_w = 1; break;
            case 'b': t = GOMC_HAL_BIT;  break;
            case 'u': t = GOMC_HAL_U32;  break;
            case 's': t = GOMC_HAL_S32;  break;
            }
            snprintf(pin, sizeof(pin), "%s.stream.%d", name, n);
            retval = env->hal->pin_new(env->hal->ctx, pin, t, GOMC_HAL_OUT,
                         (void **)&m->stream_pins[n], priv->comp_id);
            if (retval != 0) goto fail;
        }
    }
    if (m->n_sample) {
        PIN(m->overruns, "overruns", GOMC_HAL_S32, GOMC_HAL_IO);
        *(m->overruns) = 0;
        for (int n = 0; n < m->n_sample; n++) {
            int t = GOMC_HAL_FLOAT;
            switch (m->sample_types[n]) {
            case 'f': t = GOMC_HAL_FLOAT; usefp_r = 1; break;
            case 'b': t = GOMC_HAL_BIT;  break;
            case 'u': t = GOMC_HAL_U32;  break;
            case 's': t = GOMC_HAL_S32;  break;
            }
            snprintf(pin, sizeof(pin), "%s.sample.%d", name, n);
            retval = env->hal->pin_new(env->hal->ctx, pin, t, GOMC_HAL_IN,
                         (void **)&m->sample_pins[n], priv->comp_id);
            if (retval != 0) goto fail;
        }
    }
    #undef PIN

    // Export the two RT functions (only those with a configured direction).
    char fn[128];
    if (m->n_stream) {
        snprintf(fn, sizeof(fn), "%s.write", name);
        retval = env->hal->export_funct(env->hal->ctx, fn, write_funct, m,
                                        usefp_w, 0, priv->comp_id);
        if (retval < 0) { gomc_log_errorf(env->log, name, "export .write failed"); goto fail; }
    }
    if (m->n_sample) {
        snprintf(fn, sizeof(fn), "%s.read", name);
        retval = env->hal->export_funct(env->hal->ctx, fn, read_funct, m,
                                        usefp_r, 0, priv->comp_id);
        if (retval < 0) { gomc_log_errorf(env->log, name, "export .read failed"); goto fail; }
    }

    // Start the non-RT I/O thread.
    if (pthread_create(&m->io_tid, NULL, io_thread, priv) != 0) {
        gomc_log_errorf(env->log, name, "pthread_create failed");
        retval = -1; goto fail;
    }
    m->io_running = 1;

    env->hal->ready(env->hal->ctx, priv->comp_id);

    priv->mod.priv = priv;
    priv->mod.Init = NULL;
    priv->mod.Start = NULL;
    priv->mod.Stop = NULL;
    priv->mod.Destroy = filestream_destroy;
    *out = &priv->mod;
    return 0;

fail:
    env->hal->exit(env->hal->ctx, priv->comp_id);
fail_early:
    if (m->infp)  fclose(m->infp);
    if (m->outfp) fclose(m->outfp);
    free(m->replay);
    free(m->capture);
    free(m->cap_seq);
    free(priv);
    return retval;
}
