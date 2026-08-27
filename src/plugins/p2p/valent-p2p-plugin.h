// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_P2P_PLUGIN (valent_p2p_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentP2PPlugin, valent_p2p_plugin, VALENT, P2P_PLUGIN, ValentDevicePlugin)

void valent_p2p_plugin_request_link (ValentP2PPlugin *plugin);
void valent_p2p_plugin_release_link (ValentP2PPlugin *plugin);

G_END_DECLS
