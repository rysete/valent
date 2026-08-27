// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_p2p_plugin_basic (ValentTestFixture *fixture,
                       gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "p2p.request-link"));
  g_assert_true (g_action_group_has_action (actions, "p2p.release-link"));

  valent_test_fixture_connect (fixture);

  VALENT_TEST_CHECK ("Plugin actions are enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "p2p.request-link"));
  g_assert_true (g_action_group_get_action_enabled (actions, "p2p.release-link"));

  VALENT_TEST_CHECK ("Plugin requests a Wi-Fi Direct link when connected");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.p2p.request");
  v_assert_packet_cmpstr (packet, "action", ==, "start");
  json_node_unref (packet);
}

static void
test_p2p_plugin_handle_request (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  JsonNode *packet;

  valent_test_fixture_connect (fixture);
  packet = valent_test_fixture_expect_packet (fixture);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin acknowledges a Wi-Fi Direct request");
  packet = valent_test_fixture_lookup_packet (fixture, "p2p-request");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.p2p.response");
  v_assert_packet_cmpstr (packet, "status", ==, "ready");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles a Wi-Fi Direct stop request");
  packet = valent_test_fixture_lookup_packet (fixture, "p2p-stop");
  valent_test_fixture_handle_packet (fixture, packet);
}

static void
test_p2p_plugin_send_request (ValentTestFixture *fixture,
                              gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_fixture_connect (fixture);
  packet = valent_test_fixture_expect_packet (fixture);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `p2p.request-link` sends a start request");
  g_action_group_activate_action (actions, "p2p.request-link", NULL);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.p2p.request");
  v_assert_packet_cmpstr (packet, "action", ==, "start");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles a Wi-Fi Direct response");
  packet = valent_test_fixture_lookup_packet (fixture, "p2p-response");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin action `p2p.release-link` sends a stop request");
  g_action_group_activate_action (actions, "p2p.release-link", NULL);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.p2p.request");
  v_assert_packet_cmpstr (packet, "action", ==, "stop");
  json_node_unref (packet);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-p2p.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/p2p/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_p2p_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/p2p/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_p2p_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/p2p/send-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_p2p_plugin_send_request,
              valent_test_fixture_clear);

  return g_test_run ();
}
