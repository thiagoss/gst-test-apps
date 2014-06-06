#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <gst/gst.h>

static GMainLoop *loop;
static GstElement *pipeline;
static GstElement *src = NULL;
static GstElement *sink = NULL;

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

static void 
handlecommand (gchar *sz) { 
  g_printf ("Setting freq: %s", sz); 
  gint f = atoi (sz);

  if (f == 0) {
    gst_element_send_event (pipeline, gst_event_new_eos ());
  } else {
    if (src)
      g_object_set (src, "frequency", f, NULL); 
  }
} 

static gboolean 
mycallback (GIOChannel *channel, GIOCondition cond, gpointer data) 
{ 
  gchar *str_return; 
  gsize length, terminator_pos; 
  GError *error = NULL; 
                
  if (g_io_channel_read_line (channel, &str_return, &length, &terminator_pos, &error) == G_IO_STATUS_ERROR) 
    g_warning("Something went wrong"); 

  if (error != NULL) { 
    g_printf (error->message); 
    exit (1); 
  } 

  handlecommand (str_return); 
  g_free (str_return); 
  return TRUE; 
}	


int main(int argc, char ** argv)
{
  GstBus *bus;
  glong bus_watch_id;
  GIOChannel *channel;

  gst_init (NULL, NULL);

  if (argc < 3) {
    g_print ("Usage: ./dvbsrc-capture-freq-switch <output-file-path>\n");
    return 0;
  }

  pipeline = gst_parse_launch ("dvbsrc name=src ! gdppay ! filesink name=sink", NULL);
  if (pipeline == NULL) {
    g_print ("Failed to create pipeline\n");
    return -1;
  }
  loop = g_main_loop_new (NULL, FALSE);

  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

  g_object_set (sink, "location", argv[1], NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, NULL);
  gst_object_unref (bus);

#ifdef G_OS_WIN32 
  channel = g_io_channel_win32_new_fd (STDIN_FILENO); 
#else 
  channel = g_io_channel_unix_new (STDIN_FILENO); 
#endif 

  g_io_add_watch (channel, G_IO_IN, mycallback, NULL);

  if (gst_element_set_state (pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE) {
    g_main_loop_run (loop);
  } else {
    g_print ("Failed to put pipeline to playing state\n");
    return -2;
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (sink);
  gst_object_unref (src);
  gst_object_unref (pipeline);
  g_io_channel_unref (channel);

  return 0;
}
