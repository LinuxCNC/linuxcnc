/*
 * gmcui — Generic WebKit container for LinuxCNC web UIs.
 *
 * When invoked directly:
 *   gmcui --url URL [--title TITLE] [--width W] [--height H]
 *
 * When invoked via a symlink (e.g. "halscope"):
 *   The basename of argv[0] selects built-in defaults for URL path,
 *   window title, and size.  The server base URL comes from the
 *   GMC_REST_URL environment variable (default: http://127.0.0.1:5080).
 *
 * Copyright (C) 2026 LinuxCNC contributors
 * License: GPLv2
 */

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <libgen.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_REST_URL "http://127.0.0.1:5080"
#define ENV_REST_URL     "GMC_REST_URL"
#define ENV_INSTANCE     "GMC_INSTANCE"
#define DEFAULT_INSTANCE "milltask"

/* Built-in app profiles, selected by symlink name. */
typedef struct {
    const char *name;       /* argv[0] basename to match */
    const char *path;       /* URL path appended to base URL */
    const char *title;      /* window title */
    int         width;
    int         height;
} app_profile_t;

static const app_profile_t profiles[] = {
    { "halscope",  "/app/halscope/",  "HAL Oscilloscope", 1280, 800 },
    { "halshow",   "/app/halshow/",   "HAL Show",         1024, 700 },
    { "emccalib",  "/app/emccalib/",  "EMC Calibration",   900, 700 },
    { "tooledit",  "/app/tooledit/",  "Tool Editor",       900, 700 },
    { NULL, NULL, NULL, 0, 0 }
};

static const app_profile_t *find_profile(const char *name)
{
    for (const app_profile_t *p = profiles; p->name; p++) {
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

static const char *get_base_url(void)
{
    const char *env = getenv(ENV_REST_URL);
    return (env && *env) ? env : DEFAULT_REST_URL;
}

static void on_destroy(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    gtk_main_quit();
}

int main(int argc, char *argv[])
{
    const char *url = NULL;
    const char *title = NULL;
    int width = 1280;
    int height = 800;

    /* Determine invocation name (strip path). */
    char *progname = basename(argv[0]);
    const app_profile_t *profile = find_profile(progname);

    /* Parse command-line arguments. */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--url") == 0 || strcmp(argv[i], "-u") == 0) && i + 1 < argc) {
            url = argv[++i];
        } else if ((strcmp(argv[i], "--title") == 0 || strcmp(argv[i], "-t") == 0) && i + 1 < argc) {
            title = argv[++i];
        } else if ((strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-W") == 0) && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--height") == 0 || strcmp(argv[i], "-H") == 0) && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--url URL] [--title TITLE] [--width W] [--height H]\n\n", progname);
            printf("When invoked via symlink (e.g. halscope), built-in defaults are used.\n");
            printf("Server URL from %s env var (default: %s)\n", ENV_REST_URL, DEFAULT_REST_URL);
            printf("\nKnown app profiles:\n");
            for (const app_profile_t *p = profiles; p->name; p++)
                printf("  %-12s %s (%dx%d)\n", p->name, p->title, p->width, p->height);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "%s: unknown argument: %s\n", progname, argv[i]);
            return 1;
        }
        /* Ignore positional args (e.g. file path passed by axis). */
    }

    /* Build URL from profile if not given explicitly. */
    char url_buf[1024];
    if (!url) {
        if (!profile) {
            fprintf(stderr, "%s: no --url given and no built-in profile for '%s'\n",
                    progname, progname);
            fprintf(stderr, "Usage: %s --url URL [--title TITLE] [--width W] [--height H]\n", progname);
            return 1;
        }
        const char *base = get_base_url();
        const char *inst = getenv(ENV_INSTANCE);
        if (!inst || !*inst)
            inst = DEFAULT_INSTANCE;
        snprintf(url_buf, sizeof(url_buf), "%s%s?instance=%s", base, profile->path, inst);
        url = url_buf;
    }

    if (!title)
        title = profile ? profile->title : "LinuxCNC";
    if (profile) {
        /* Only use profile size if not overridden on command line. */
        if (width == 1280 && profile->width != 1280)
            width = profile->width;
        if (height == 800 && profile->height != 800)
            height = profile->height;
    }

    g_set_prgname(progname);
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);

    WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

    /* Enable developer tools for debugging. */
    WebKitSettings *settings = webkit_web_view_get_settings(webview);
    webkit_settings_set_enable_developer_extras(settings, TRUE);

    webkit_web_view_load_uri(webview, url);

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
