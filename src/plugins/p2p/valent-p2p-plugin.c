// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-p2p-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-p2p-plugin.h"

struct _ValentP2PPlugin
{
  ValentDevicePlugin parent_instance;

  char              *p2p_ip;
  uint16_t           p2p_port;
  gboolean           link_active;
};

G_DEFINE_FINAL_TYPE (ValentP2PPlugin, valent_p2p_plugin, VALENT_TYPE_DEVICE_PLUGIN)

static void
handle_p2p_response (ValentP2PPlugin *self,
                     JsonNode        *packet)
{
  const char *ip = NULL;
  int64_t port = 0;

  g_assert (VALENT_IS_P2P_PLUGIN (self));

  if (valent_packet_get_string (packet, "ip", &ip))
    {
      g_free (self->p2p_ip);
      self->p2p_ip = g_strdup (ip);
      self->link_active = TRUE;
      g_debug ("%s(): Wi-Fi Direct P2P link established at %s", G_STRFUNC, self->p2p_ip);
    }

  if (valent_packet_get_int (packet, "port", &port))
    self->p2p_port = (uint16_t)port;
}

static void
handle_p2p_request (ValentP2PPlugin *self,
                    JsonNode        *packet)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;

  g_assert (VALENT_IS_P2P_PLUGIN (self));

  /* Acknowledge P2P negotiation request */
  valent_packet_init (&builder, "kdeconnect.p2p.response");
  json_builder_set_member_name (builder, "status");
  json_builder_add_string_value (builder, "ready");
  response = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

static void
valent_p2p_plugin_handle_packet (ValentDevicePlugin *plugin,
                                 const char         *type,
                                 JsonNode           *packet)
{
  ValentP2PPlugin *self = VALENT_P2P_PLUGIN (plugin);

  g_assert (VALENT_IS_P2P_PLUGIN (self));

  if (g_strcmp0 (type, "kdeconnect.p2p.request") == 0)
    handle_p2p_request (self, packet);
  else if (g_strcmp0 (type, "kdeconnect.p2p.response") == 0)
    handle_p2p_response (self, packet);
}

void
valent_p2p_plugin_request_link (ValentP2PPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_P2P_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.p2p.request");
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "start");
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

void
valent_p2p_plugin_release_link (ValentP2PPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_P2P_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.p2p.request");
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "stop");
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
  self->link_active = FALSE;
  g_clear_pointer (&self->p2p_ip, g_free);
}

static void
valent_p2p_plugin_finalize (GObject *object)
{
  ValentP2PPlugin *self = VALENT_P2P_PLUGIN (object);

  g_clear_pointer (&self->p2p_ip, g_free);

  G_OBJECT_CLASS (valent_p2p_plugin_parent_class)->finalize (object);
}

static void
valent_p2p_plugin_class_init (ValentP2PPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->finalize = valent_p2p_plugin_finalize;
  plugin_class->handle_packet = valent_p2p_plugin_handle_packet;
}

static void
valent_p2p_plugin_init (ValentP2PPlugin *self)
{
}
