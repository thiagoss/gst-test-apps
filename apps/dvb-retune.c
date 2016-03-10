/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Test application to do retuning to different channels of DVB streams.
 * It will move to the next stream every few seconds
 *
 * Build: libtool --mode=link gcc dvb-retune.c -o dvb-retune `pkg-config --cflags --libs gstreamer`
 *
 * How to run:
 * Update the hardcoded list of channel names according to your region in
 * update_uri() and run with: GST_DVB_CHANNELS_CONF=~/channels.conf ./dvb-retune
 * Where you can point the env var to your channels configuration file.
 */

#include <stdlib.h>
#include <gst/gst.h>

GMainLoop *loop;
GstElement *pipeline;
GstElement *dvbsrc;
GstElement *decodebin;
GstElement *v_conv, *v_queue, *v_sink;
GstElement *a_conv, *a_queue, *a_sink;

static gboolean
update_uri (GstElement * dvbsrc)
{
  static gint index = 0;
  const gchar *uris[] = {
    "dvb://TV Itararé HD",
    "dvb://TV PARAIBA HD",
  };
  const gchar *new_uri;
  GError *error = NULL;

  new_uri = uris[index++];
  if (index >= 2)
    index = 0;
  g_print ("Changing channel to: %s\n", new_uri);

  /* Retune option one: set the pipeline to null, change uri, set to playing */
#if 1
  gst_element_set_state (pipeline, GST_STATE_NULL);
  if (!gst_uri_handler_set_uri (GST_URI_HANDLER (dvbsrc), new_uri, &error)) {
    g_print ("Failed to set uri: %d %d: %s\n", error->domain, error->code, error->message);
  }
  g_print ("Requesting retuning\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_print ("Requested retuning\n");
#else

  /* Retune option two: set dvbsrc to null, change uri, set to playing */
#if 0
  gst_element_set_state (dvbsrc, GST_STATE_NULL);
  if (!gst_uri_handler_set_uri (GST_URI_HANDLER (dvbsrc), new_uri, &error)) {
    g_print ("Failed to set uri: %d %d: %s\n", error->domain, error->code, error->message);
  }
  g_print ("Requesting retuning\n");
  gst_element_set_state (dvbsrc, GST_STATE_PLAYING);
#else

  /* Retune option three: change the uri and call the "tune" action */
  if (!gst_uri_handler_set_uri (GST_URI_HANDLER (dvbsrc), new_uri, &error)) {
    g_print ("Failed to set uri: %d %d: %s\n", error->domain, error->code, error->message);
  }
  g_print ("Requesting retuning\n");
  g_signal_emit_by_name (dvbsrc, "tune", NULL);
  g_print ("Requested retuning\n");

#endif
#endif

  return G_SOURCE_CONTINUE;
}

static void
cb_newpad (GstElement *decodebin,
           GstPad     *pad,
           gpointer    data)
{
  GstCaps *caps;
  GstStructure *str;
  GstPad *out_pad;

  /* only care about video for now */
  caps = gst_pad_query_caps (pad, NULL);
  str = gst_caps_get_structure (caps, 0);
  if (g_strstr_len (gst_structure_get_name (str), -1, "video")) {
    out_pad = gst_element_get_static_pad (v_queue, "sink");
  } else if (g_strstr_len (gst_structure_get_name (str), -1, "audio")) {
    out_pad = gst_element_get_static_pad (a_queue, "sink");
  }
  gst_caps_unref (caps);

  if (out_pad == NULL || GST_PAD_IS_LINKED (out_pad)) {
    if (out_pad)
      gst_object_unref (out_pad);
    g_print ("Ignoring new pad\n");
    return;
  }

  /* TODO check me */
  gst_pad_link (pad, out_pad);
  g_object_unref (out_pad);
}

static gboolean
bus_message_handler (GstBus * bus, GstMessage * message, gpointer udata)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;

      gst_message_parse_error (message, &err, &dbg_info);
      g_printerr ("ERROR from element %s: %s\n",
         GST_OBJECT_NAME (message->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:{
      g_main_loop_quit (loop);
      g_print ("EOS.\n");
    }
    default:
      break;
  }

  return TRUE;
}

int main()
{
  GstStateChangeReturn ret;
  GstBus *bus;

  gst_init (NULL, NULL);
  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_pipeline_new (NULL);
  dvbsrc = gst_element_factory_make ("dvbbasebin", NULL);
  decodebin = gst_element_factory_make ("decodebin", NULL);
  v_queue = gst_element_factory_make ("queue", NULL);
  v_conv = gst_element_factory_make ("videoconvert", NULL);
  v_sink = gst_element_factory_make ("autovideosink", NULL);
  a_queue = gst_element_factory_make ("queue", NULL);
  a_conv = gst_element_factory_make ("audioconvert", NULL);
  a_sink = gst_element_factory_make ("autoaudiosink", NULL);

  g_object_set (G_OBJECT (v_queue), "max-size-bytes", 1000000000, NULL);
  g_object_set (G_OBJECT (a_queue), "max-size-bytes", 1000000000, NULL);

  gst_bin_add_many (GST_BIN (pipeline), dvbsrc, decodebin, NULL);
  gst_bin_add_many (GST_BIN (pipeline), v_queue, v_conv, v_sink, NULL);
  gst_bin_add_many (GST_BIN (pipeline), a_queue, a_conv, a_sink, NULL);
  if (!gst_element_link_pads (dvbsrc, "src", decodebin, "sink")) {
    g_print ("Linking failed\n");
    return -1;
  }
  if (!gst_element_link_many (v_queue, v_conv, v_sink, NULL)) {
    g_print ("Linking failed\n");
    return -1;
  }
  if (!gst_element_link_many (a_queue, a_conv, a_sink, NULL)) {
    g_print ("Linking failed\n");
    return -1;
  }

  g_signal_connect (decodebin, "pad-added", G_CALLBACK (cb_newpad), NULL);
  update_uri (dvbsrc);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_message_handler, NULL);

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_print ("State change failed\n");
    return -2;
  }

  g_timeout_add_seconds (20, (GSourceFunc) update_uri, dvbsrc);
  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
  return 0;
}
