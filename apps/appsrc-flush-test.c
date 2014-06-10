#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>

static GMainLoop *loop;
static GstElement *pipeline;
static GstElement *src = NULL;

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static GstBuffer *
_create_buf (gint ts, guint8 fill)
{
  GstBuffer *buf;
  guint8 *data;
  gsize size = 16 * 16 *3;


  data = g_malloc (size);
  memset (data, fill, size);

  buf = gst_buffer_new_wrapped (data, size);
  GST_BUFFER_PTS (buf) = GST_SECOND * ts;
  GST_BUFFER_DURATION (buf) = GST_SECOND * 5;
  return buf;
}

int main(int argc, char ** argv)
{
  GstBus *bus;
  glong bus_watch_id;
  GstVideoInfo info;

  gst_init (NULL, NULL);
  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_parse_launch ("appsrc name=src ! videoconvert ! autovideosink",
      NULL);

  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, NULL);
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_RGB, 16, 16);

  gst_app_src_set_caps (GST_APP_SRC (src), gst_video_info_to_caps (&info));
  gst_app_src_push_buffer (GST_APP_SRC (src), _create_buf (0, 0));
  gst_app_src_push_buffer (GST_APP_SRC (src), _create_buf (5, 0));
  gst_app_src_push_buffer (GST_APP_SRC (src), _create_buf (10, 0));

  g_print ("Flushing\n");
  gst_element_send_event (pipeline, gst_event_new_flush_start ());
  gst_element_send_event (pipeline, gst_event_new_flush_stop (TRUE));
  g_print ("Flushing done\n");

  gst_app_src_push_buffer (GST_APP_SRC (src), _create_buf (15, 255));
  gst_app_src_push_buffer (GST_APP_SRC (src), _create_buf (20, 255));
  gst_app_src_push_buffer (GST_APP_SRC (src), _create_buf (25, 255));

  gst_app_src_end_of_stream (GST_APP_SRC (src));

  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_object_unref (src);

  return 0;
}
