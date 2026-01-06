/*
 * Copyright © 2018 Red Hat, Inc
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

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <stdint.h>

#include "screencast.h"

typedef struct _CinnamonScreenCast CinnamonScreenCast;
typedef struct _CinnamonScreenCastSession CinnamonScreenCastSession;
typedef struct _CinnamonScreenCastStream CinnamonScreenCastStream;

const char * cinnamon_screen_cast_session_get_stream_path_from_id (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                                                   uint32_t stream_id);

void cinnamon_screen_cast_session_add_stream_properties (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                                         GVariantBuilder *streams_builder);

gboolean cinnamon_screen_cast_session_record_selections (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                                         GPtrArray               *streams,
                                                         ScreenCastSelection *select,
                                                         GError **error);

gboolean cinnamon_screen_cast_session_stop (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                            GError **error);

gboolean cinnamon_screen_cast_session_start (CinnamonScreenCastSession *cinnamon_screen_cast_session,
                                             GError **error);

CinnamonScreenCastSession *cinnamon_screen_cast_create_session (CinnamonScreenCast *cinnamon_screen_cast,
                                                                const char *remote_desktop_session_id,
                                                                GError **error);

int cinnamon_screen_cast_get_api_version (CinnamonScreenCast *cinnamon_screen_cast);

CinnamonScreenCast *cinnamon_screen_cast_new (GDBusConnection *connection);
