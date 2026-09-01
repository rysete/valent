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
  gboolean           link_requested;
};

G_DEFINE_FINAL_TYPE (ValentP2PPlugin, valent_p2p_plugin, VALENT_TYPE_DEVICE_PLUGIN)

typedef enum {
  PROP_ACTIVE = 1,
  PROP_HOST,
  PROP_PORT,
} ValentP2PPluginProperty;

static GParamSpec *properties[PROP_PORT + 1] = { NULL, };

static void
valent_p2p_plugin_clear_endpoint (ValentP2PPlugin *self)
{
  g_assert (VALENT_IS_P2P_PLUGIN (self));

  if (self->link_active)
    {
      self->link_active = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
    }

  if (self->p2p_ip != NULL)
    {
      g_clear_pointer (&self->p2p_ip, g_free);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST]);
    }

  if (self->p2p_port != 0)
    {
      self->p2p_port = 0;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PORT]);
    }
}

static void
valent_p2p_plugin_set_endpoint (ValentP2PPlugin *self,
                                const char      *host,
                                uint16_t         port)
{
  g_assert (VALENT_IS_P2P_PLUGIN (self));
  g_assert (host != NULL);

  if (g_set_str (&self->p2p_ip, host))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST]);

  if (self->p2p_port != port)
    {
      self->p2p_port = port;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PORT]);
    }

  if (!self->link_active)
    {
      self->link_active = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
    }
}

static void
handle_p2p_response (ValentP2PPlugin *self,
                     JsonNode        *packet)
{
  const char *ip = NULL;
  int64_t port = 0;

  g_assert (VALENT_IS_P2P_PLUGIN (self));

  if (!valent_packet_get_string (packet, "ip", &ip))
    {
      g_debug ("%s(): P2P response missing \"ip\" field", G_STRFUNC);
      return;
    }

  if (valent_packet_get_int (packet, "port", &port) &&
      (port < 0 || port > G_MAXUINT16))
    {
      g_debug ("%s(): expected \"port\" field holding a uint16", G_STRFUNC);
      return;
    }

  valent_p2p_plugin_set_endpoint (self, ip, (uint16_t)port);
  g_debug ("%s(): Wi-Fi Direct P2P link established at %s",
           G_STRFUNC,
           self->p2p_ip);
}

static void
handle_p2p_request (ValentP2PPlugin *self,
                    JsonNode        *packet)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  const char *action = NULL;

  g_assert (VALENT_IS_P2P_PLUGIN (self));

  if (valent_packet_get_string (packet, "action", &action) &&
      g_str_equal (action, "stop"))
    {
      valent_p2p_plugin_clear_endpoint (self);
      return;
    }

  /* Acknowledge P2P negotiation request */
  valent_packet_init (&builder, "kdeconnect.p2p.response");
  json_builder_set_member_name (builder, "status");
  json_builder_add_string_value (builder, "ready");
  response = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

/*
 * GActions
 */
static void
request_link_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  valent_p2p_plugin_request_link (VALENT_P2P_PLUGIN (user_data));
}

static void
release_link_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  valent_p2p_plugin_release_link (VALENT_P2P_PLUGIN (user_data));
}

static const GActionEntry actions[] = {
    {"request-link", request_link_action, NULL, NULL, NULL},
    {"release-link", release_link_action, NULL, NULL, NULL},
};

/*
 * ValentDevicePlugin
 */
static void
valent_p2p_plugin_update_state (ValentDevicePlugin *plugin,
                                ValentDeviceState   state)
{
  ValentP2PPlugin *self = VALENT_P2P_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_P2P_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);

  if (available && !self->link_requested)
    valent_p2p_plugin_request_link (self);
  else if (!available)
    {
      self->link_requested = FALSE;
      valent_p2p_plugin_clear_endpoint (self);
    }
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

static void
valent_p2p_trigger_nm_find (void)
{
  g_autoptr (GDBusConnection) system_bus = NULL;
  g_autoptr (GError) error = NULL;

  system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (system_bus == NULL)
    return;

  /* Call StartFind on NetworkManager Device 41 (p2p-dev-wlan0) */
  g_dbus_connection_call (system_bus,
                          "org.freedesktop.NetworkManager",
                          "/org/freedesktop/NetworkManager/Devices/41",
                          "org.freedesktop.NetworkManager.Device.WifiP2P",
                          "StartFind",
                          g_variant_new ("(@a{sv})", g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          1000,
                          NULL,
                          NULL,
                          NULL);
}

void
valent_p2p_plugin_request_link (ValentP2PPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_P2P_PLUGIN (self));

  /* Trigger local Wi-Fi Direct hardware probe beacon scan */
  valent_p2p_trigger_nm_find ();

  valent_packet_init (&builder, "kdeconnect.p2p.request");
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "start");
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
  self->link_requested = TRUE;
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
  self->link_requested = FALSE;
  valent_p2p_plugin_clear_endpoint (self);
}

/*
 * GObject
 */
static void
valent_p2p_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  G_OBJECT_CLASS (valent_p2p_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
}

static void
valent_p2p_plugin_finalize (GObject *object)
{
  ValentP2PPlugin *self = VALENT_P2P_PLUGIN (object);

  g_clear_pointer (&self->p2p_ip, g_free);

  G_OBJECT_CLASS (valent_p2p_plugin_parent_class)->finalize (object);
}

static void
valent_p2p_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentP2PPlugin *self = VALENT_P2P_PLUGIN (object);

  switch ((ValentP2PPluginProperty)prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, valent_p2p_plugin_get_active (self));
      break;

    case PROP_HOST:
      g_value_set_string (value, valent_p2p_plugin_get_host (self));
      break;

    case PROP_PORT:
      g_value_set_uint (value, valent_p2p_plugin_get_port (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_p2p_plugin_class_init (ValentP2PPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_p2p_plugin_constructed;
  object_class->finalize = valent_p2p_plugin_finalize;
  object_class->get_property = valent_p2p_plugin_get_property;

  plugin_class->handle_packet = valent_p2p_plugin_handle_packet;
  plugin_class->update_state = valent_p2p_plugin_update_state;

  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_HOST] =
    g_param_spec_string ("host", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       0, G_MAXUINT16, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_p2p_plugin_init (ValentP2PPlugin *self)
{
}

const char *
valent_p2p_plugin_get_host (ValentP2PPlugin *self)
{
  g_return_val_if_fail (VALENT_IS_P2P_PLUGIN (self), NULL);

  return self->p2p_ip;
}

uint16_t
valent_p2p_plugin_get_port (ValentP2PPlugin *self)
{
  g_return_val_if_fail (VALENT_IS_P2P_PLUGIN (self), 0);

  return self->p2p_port;
}

gboolean
valent_p2p_plugin_get_active (ValentP2PPlugin *self)
{
  g_return_val_if_fail (VALENT_IS_P2P_PLUGIN (self), FALSE);

  return self->link_active;
}
