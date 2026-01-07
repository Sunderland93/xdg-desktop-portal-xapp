/*
 * Copyright © 2017-2018 Red Hat, Inc
 * Copyright © 2026 Linux Mint Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "cinnamonscreencast.h"
#include "common-dbus.h"

#include <stdint.h>

enum
{
    STREAM_SIGNAL_READY,

    N_STREAM_SIGNALS
};

guint stream_signals[N_STREAM_SIGNALS];

enum
{
    SESSION_SIGNAL_READY,
    SESSION_SIGNAL_CLOSED,

    N_SESSION_SIGNALS
};

guint session_signals[N_SESSION_SIGNALS];

enum
{
    ENABLED,
    DISABLED,

    N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct _CinnamonScreenCastStream
{
    GObject parent;

    CinnamonScreenCastSession *session;

    ScreenCastSourceType source_type;

    char *path;
    OrgCinnamonMuffinScreenCastStream *proxy;

    uint32_t pipewire_node_id;

    gboolean has_position;
    int x;
    int y;

    gboolean has_size;
    int width;
    int height;
} CinnamonScreenCastStream;

typedef struct _CinnamonScreenCastStreamClass
{
    GObjectClass parent_class;
} CinnamonScreenCastStreamClass;

typedef struct _CinnamonScreenCastSession
{
    GObject parent;

    char *path;
    OrgCinnamonMuffinScreenCastSession *proxy;
    gulong closed_handler_id;

    GList *streams;
    int n_needed_stream_node_ids;
} CinnamonScreenCastSession;

typedef struct _CinnamonScreenCastSessionClass
{
    GObjectClass parent_class;
} CinnamonScreenCastSessionClass;

typedef struct _CinnamonScreenCast
{
    GObject parent;

    int api_version;

    guint screen_cast_name_watch;
    OrgCinnamonMuffinScreenCast *proxy;
} CinnamonScreenCast;

typedef struct _CinnamonScreenCastClass
{
    GObjectClass parent_class;
} CinnamonScreenCastClass;

static GType cinnamon_screen_cast_stream_get_type (void);
G_DEFINE_TYPE (CinnamonScreenCastStream, cinnamon_screen_cast_stream, G_TYPE_OBJECT)

static GType cinnamon_screen_cast_session_get_type (void);
G_DEFINE_TYPE (CinnamonScreenCastSession, cinnamon_screen_cast_session, G_TYPE_OBJECT)

static GType cinnamon_screen_cast_get_type (void);
G_DEFINE_TYPE (CinnamonScreenCast, cinnamon_screen_cast, G_TYPE_OBJECT)

static uint32_t
cinnamon_screen_cast_stream_get_pipewire_node_id (CinnamonScreenCastStream *stream)
{
    return stream->pipewire_node_id;
}

static gboolean
cinnamon_screen_cast_stream_get_position (CinnamonScreenCastStream *stream,
                                          int *x,
                                          int *y)
{
    if (!stream->has_position)
        return FALSE;

    *x = stream->x;
    *y = stream->y;

    return TRUE;
}

static gboolean
cinnamon_screen_cast_stream_get_size (CinnamonScreenCastStream *stream,
                                      int *width,
                                      int *height)
{
    if (!stream->has_size)
        return FALSE;

    *width = stream->width;
    *height = stream->height;

    return TRUE;
}

static void
cinnamon_screen_cast_stream_finalize (GObject *object)
{
    CinnamonScreenCastStream *stream = (CinnamonScreenCastStream *)object;

    g_clear_object (&stream->proxy);
    g_free (stream->path);

    G_OBJECT_CLASS (cinnamon_screen_cast_stream_parent_class)->finalize (object);
}

static void
on_pipewire_stream_added (OrgCinnamonMuffinScreenCastStream *stream_proxy,
                          unsigned int arg_node_id,
                          CinnamonScreenCastStream *stream)
{
    stream->pipewire_node_id = arg_node_id;
    g_signal_emit (stream, stream_signals[STREAM_SIGNAL_READY], 0);

    stream->session->n_needed_stream_node_ids--;
    if (stream->session->n_needed_stream_node_ids == 0)
        g_signal_emit (stream->session, session_signals[SESSION_SIGNAL_READY], 0);
}

static void
cinnamon_screen_cast_stream_init (CinnamonScreenCastStream *stream)
{
}

static void
cinnamon_screen_cast_stream_class_init (CinnamonScreenCastStreamClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cinnamon_screen_cast_stream_finalize;

    stream_signals[STREAM_SIGNAL_READY] = g_signal_new ("ready",
                                                        G_TYPE_FROM_CLASS (klass),
                                                        G_SIGNAL_RUN_LAST,
                                                        0,
                                                        NULL, NULL,
                                                        NULL,
                                                        G_TYPE_NONE, 0);
}

const char *
cinnamon_screen_cast_session_get_stream_path_from_id (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                                      uint32_t stream_id)
{
    GList *l;

    for (l = cinnamon_screen_cast_session->streams; l; l = l->next)
      {
        CinnamonScreenCastStream *stream = l->data;

        if (stream->pipewire_node_id == stream_id)
            return stream->path;
      }

    return NULL;
}

void
cinnamon_screen_cast_session_add_stream_properties (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                                    GVariantBuilder *streams_builder)
{
    GList *streams;
    GList *l;

    streams = cinnamon_screen_cast_session->streams;
    for (l = streams; l; l = l->next)
      {
        CinnamonScreenCastStream *stream = l->data;
        GVariantBuilder stream_properties_builder;
        int x, y;
        int width, height;
        uint32_t pipewire_node_id;


        g_variant_builder_init (&stream_properties_builder, G_VARIANT_TYPE_VARDICT);

        g_variant_builder_add (&stream_properties_builder, "{sv}",
                               "source_type",
                               g_variant_new ("u", stream->source_type));

        if (cinnamon_screen_cast_stream_get_position (stream, &x, &y))
            g_variant_builder_add (&stream_properties_builder, "{sv}",
                                   "position",
                                   g_variant_new ("(ii)", x, y));
        if (cinnamon_screen_cast_stream_get_size (stream, &width, &height))
                g_variant_builder_add (&stream_properties_builder, "{sv}",
                                       "size",
                                       g_variant_new ("(ii)", width, height));

        pipewire_node_id = cinnamon_screen_cast_stream_get_pipewire_node_id (stream);
        g_variant_builder_add (streams_builder, "(ua{sv})",
                               pipewire_node_id,
                               &stream_properties_builder);
      }
}

static uint32_t
cursor_mode_to_cinnamon_cursor_mode (ScreenCastCursorMode cursor_mode)
{
    switch (cursor_mode)
      {
        case SCREEN_CAST_CURSOR_MODE_NONE:
            g_assert_not_reached ();
            return -1;
        case SCREEN_CAST_CURSOR_MODE_HIDDEN:
            return 0;
        case SCREEN_CAST_CURSOR_MODE_EMBEDDED:
            return 1;
        case SCREEN_CAST_CURSOR_MODE_METADATA:
            return 2;
      }

    g_assert_not_reached ();
}

static gboolean
cinnamon_screen_cast_session_record_monitor (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                             Monitor                 *monitor,
                                             ScreenCastSelection     *select,
                                             GError                 **error)
{
    OrgCinnamonMuffinScreenCastSession *session_proxy =
    cinnamon_screen_cast_session->proxy;
    GVariantBuilder properties_builder;
    GVariant *properties;
    g_autofree char *stream_path = NULL;
    GDBusConnection *connection;
    OrgCinnamonMuffinScreenCastStream *stream_proxy;
    CinnamonScreenCastStream *stream;
    GVariant *parameters;
    const char *connector;

    g_variant_builder_init (&properties_builder, G_VARIANT_TYPE_VARDICT);
    if (select->cursor_mode)
      {
        uint32_t cinnamon_cursor_mode;

        cinnamon_cursor_mode = cursor_mode_to_cinnamon_cursor_mode (select->cursor_mode);
        g_variant_builder_add (&properties_builder, "{sv}",
                               "cursor-mode",
                               g_variant_new_uint32 (cinnamon_cursor_mode));
      }
    connector = monitor_get_connector (monitor);
    properties = g_variant_builder_end (&properties_builder);

    if (!org_cinnamon_muffin_screen_cast_session_call_record_monitor_sync (session_proxy,
                                                                           connector,
                                                                           properties,
                                                                           &stream_path,
                                                                           NULL,
                                                                           error))
        return FALSE;

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (session_proxy));
    stream_proxy =
    org_cinnamon_muffin_screen_cast_stream_proxy_new_sync (connection,
                                                           G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                           "org.cinnamon.Muffin.ScreenCast",
                                                           stream_path,
                                                           NULL,
                                                           error);
    if (!stream_proxy)
        return FALSE;

    stream = g_object_new (cinnamon_screen_cast_stream_get_type (), NULL);
    stream->source_type = SCREEN_CAST_SOURCE_TYPE_MONITOR;
    stream->session = cinnamon_screen_cast_session;
    stream->path = g_strdup (stream_path);
    stream->proxy = stream_proxy;

    parameters = org_cinnamon_muffin_screen_cast_stream_get_parameters (stream->proxy);
    if (parameters)
      {
        if (g_variant_lookup (parameters, "position", "(ii)",
            &stream->x, &stream->y))
            stream->has_position = TRUE;
        if (g_variant_lookup (parameters, "size", "(ii)",
            &stream->width, &stream->height))
            stream->has_size = TRUE;
      }
    else
      {
        g_warning ("Screen cast stream %s missing parameters",
                   stream->path);
      }

    g_signal_connect (stream_proxy, "pipewire-stream-added",
                      G_CALLBACK (on_pipewire_stream_added),
                      stream);

    cinnamon_screen_cast_session->streams =
    g_list_prepend (cinnamon_screen_cast_session->streams, stream);
    cinnamon_screen_cast_session->n_needed_stream_node_ids++;

    return TRUE;
}

gboolean
cinnamon_screen_cast_session_record_selections (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                                GPtrArray                 *streams,
                                                ScreenCastSelection       *select,
                                                GError                   **error)
{
    guint i;

    for (i = 0; i < streams->len; i++)
      {
        ScreenCastStreamInfo *info = g_ptr_array_index (streams, i);

        switch (info->type)
          {
            case SCREEN_CAST_SOURCE_TYPE_MONITOR:
                if (!cinnamon_screen_cast_session_record_monitor (cinnamon_screen_cast_session,
                                                                  info->data.monitor,
                                                                  select,
                                                                  error))
                    return FALSE;
                break;
          }
      }

    return TRUE;
}

gboolean
cinnamon_screen_cast_session_stop (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                   GError **error)
{
    OrgCinnamonMuffinScreenCastSession *session_proxy =
    cinnamon_screen_cast_session->proxy;

    g_signal_handler_disconnect (cinnamon_screen_cast_session->proxy,
                                 cinnamon_screen_cast_session->closed_handler_id);

    if (!org_cinnamon_muffin_screen_cast_session_call_stop_sync (session_proxy,
        NULL,
        error))
        return FALSE;

    return TRUE;
}

gboolean
cinnamon_screen_cast_session_start (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                    GError **error)
{
    OrgCinnamonMuffinScreenCastSession *session_proxy =
    cinnamon_screen_cast_session->proxy;

    if (!org_cinnamon_muffin_screen_cast_session_call_start_sync (session_proxy,
        NULL,
        error))
        return FALSE;

    return TRUE;
}

static void
cinnamon_screen_cast_session_finalize (GObject *object)
{
    CinnamonScreenCastSession *session = (CinnamonScreenCastSession *)object;

    g_list_free_full (session->streams, g_object_unref);
    g_clear_object (&session->proxy);
    g_free (session->path);

    G_OBJECT_CLASS (cinnamon_screen_cast_session_parent_class)->finalize (object);
}

static void
cinnamon_screen_cast_session_init (CinnamonScreenCastSession *cinnamon_screen_cast_session)
{
}

static void
cinnamon_screen_cast_session_class_init (CinnamonScreenCastSessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cinnamon_screen_cast_session_finalize;

    session_signals[SESSION_SIGNAL_READY] = g_signal_new ("ready",
                                                          G_TYPE_FROM_CLASS (klass),
                                                          G_SIGNAL_RUN_LAST,
                                                          0,
                                                          NULL, NULL,
                                                          NULL,
                                                          G_TYPE_NONE, 0);
    session_signals[SESSION_SIGNAL_CLOSED] = g_signal_new ("closed",
                                                           G_TYPE_FROM_CLASS (klass),
                                                           G_SIGNAL_RUN_LAST,
                                                           0,
                                                           NULL, NULL,
                                                           NULL,
                                                           G_TYPE_NONE, 0);
}

static void
on_muffin_session_closed (OrgCinnamonMuffinScreenCastSession *session_proxy,
                          CinnamonScreenCastSession *cinnamon_screen_cast_session)
{
    g_signal_emit (cinnamon_screen_cast_session,
                   session_signals[SESSION_SIGNAL_CLOSED], 0);
}

CinnamonScreenCastSession *
cinnamon_screen_cast_create_session (CinnamonScreenCast *cinnamon_screen_cast,
                                     const char *remote_desktop_session_id,
                                     GError **error)
{
    GVariantBuilder properties_builder;
    GVariant *properties;
    g_autofree char *session_path = NULL;
    GDBusConnection *connection;
    OrgCinnamonMuffinScreenCastSession *session_proxy;
    CinnamonScreenCastSession *cinnamon_screen_cast_session;

    g_variant_builder_init (&properties_builder, G_VARIANT_TYPE_VARDICT);
    if (remote_desktop_session_id)
      {
        g_variant_builder_add (&properties_builder, "{sv}",
                               "remote-desktop-session-id",
                               g_variant_new_string (remote_desktop_session_id));
      }
    properties = g_variant_builder_end (&properties_builder);
    if (!org_cinnamon_muffin_screen_cast_call_create_session_sync (cinnamon_screen_cast->proxy,
                                                                   properties,
                                                                   &session_path,
                                                                   NULL,
                                                                   error))
        return NULL;

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (cinnamon_screen_cast->proxy));
    session_proxy =
    org_cinnamon_muffin_screen_cast_session_proxy_new_sync (connection,
                                                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                            "org.cinnamon.Muffin.ScreenCast",
                                                            session_path,
                                                            NULL,
                                                            error);
    if (!session_proxy)
        return NULL;

    cinnamon_screen_cast_session =
    g_object_new (cinnamon_screen_cast_session_get_type (), NULL);
    cinnamon_screen_cast_session->path = g_steal_pointer (&session_path);
    cinnamon_screen_cast_session->proxy = g_steal_pointer (&session_proxy);
    cinnamon_screen_cast_session->closed_handler_id =
    g_signal_connect (cinnamon_screen_cast_session->proxy,
                      "closed", G_CALLBACK (on_muffin_session_closed),
                      cinnamon_screen_cast_session);

    return cinnamon_screen_cast_session;
}

int
cinnamon_screen_cast_get_api_version (CinnamonScreenCast *cinnamon_screen_cast)
{
    return cinnamon_screen_cast->api_version;
}

static void
cinnamon_screen_cast_name_appeared (GDBusConnection *connection,
                                    const char *name,
                                    const char *name_owner,
                                    gpointer user_data)
{
    CinnamonScreenCast *cinnamon_screen_cast = user_data;
    g_autoptr(GError) error = NULL;

    cinnamon_screen_cast->proxy =
    org_cinnamon_muffin_screen_cast_proxy_new_sync (connection,
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                    "org.cinnamon.Muffin.ScreenCast",
                                                    "/org/cinnamon/Muffin/ScreenCast",
                                                    NULL,
                                                    &error);
    if (!cinnamon_screen_cast->proxy)
      {
        g_warning ("Failed to acquire org.cinnamon.Muffin.ScreenCast proxy: %s",
                   error->message);
        return;
      }

    cinnamon_screen_cast->api_version =
    org_cinnamon_muffin_screen_cast_get_version (cinnamon_screen_cast->proxy);

    g_signal_emit (cinnamon_screen_cast, signals[ENABLED], 0);
}

static void
cinnamon_screen_cast_name_vanished (GDBusConnection *connection,
                                    const char *name,
                                    gpointer user_data)
{
    CinnamonScreenCast *cinnamon_screen_cast = user_data;

    g_clear_object (&cinnamon_screen_cast->proxy);

    g_signal_emit (cinnamon_screen_cast, signals[DISABLED], 0);
}

static void
cinnamon_screen_cast_init (CinnamonScreenCast *cinnamon_screen_cast)
{
}

static void
cinnamon_screen_cast_class_init (CinnamonScreenCastClass *klass)
{
    signals[ENABLED] = g_signal_new ("enabled",
                                     G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     NULL,
                                     G_TYPE_NONE, 0);
    signals[DISABLED] = g_signal_new ("disabled",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 0);
}

CinnamonScreenCast *
cinnamon_screen_cast_new (GDBusConnection *connection)
{
    CinnamonScreenCast *cinnamon_screen_cast;

    cinnamon_screen_cast = g_object_new (cinnamon_screen_cast_get_type (), NULL);
    cinnamon_screen_cast->screen_cast_name_watch =
    g_bus_watch_name (G_BUS_TYPE_SESSION,
                      "org.cinnamon.Muffin.ScreenCast",
                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                      cinnamon_screen_cast_name_appeared,
                      cinnamon_screen_cast_name_vanished,
                      cinnamon_screen_cast,
                      NULL);

    return cinnamon_screen_cast;
}
