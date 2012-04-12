#include <gfreenect.h>
#include <math.h>
#include <string.h>
#include <glib-object.h>
#include <clutter/clutter.h>
#include <clutter/clutter-keysyms.h>

static GFreenectDevice *kinect = NULL;
static ClutterActor *info_text;
static ClutterActor *depth_tex;
static ClutterActor *video_tex;

static guint THRESHOLD_BEGIN = 500;
/* Adjust this value to increase of decrease
   the threshold */
static guint THRESHOLD_END   = 1500;

static guint shot_timeout_id = 0;
static gboolean record_shot = FALSE;
static gint DEFAULT_SECONDS_TO_SHOOT = 2;
static gint seconds_to_shoot = 2;

typedef struct
{
  guint16 *reduced_buffer;
  gint width;
  gint height;
  gint reduced_width;
  gint reduced_height;
} BufferInfo;

static void
grayscale_buffer_set_value (guchar *buffer, gint index, guchar value)
{
  buffer[index * 3] = value;
  buffer[index * 3 + 1] = value;
  buffer[index * 3 + 2] = value;
}

static BufferInfo *
process_buffer (guint16 *buffer,
                guint width,
                guint height,
                guint dimension_factor,
                guint threshold_begin,
                guint threshold_end)
{
  BufferInfo *buffer_info;
  gint i, j, reduced_width, reduced_height;
  guint16 *reduced_buffer;

  g_return_val_if_fail (buffer != NULL, NULL);

  reduced_width = (width - width % dimension_factor) / dimension_factor;
  reduced_height = (height - height % dimension_factor) / dimension_factor;

  reduced_buffer = g_slice_alloc0 (reduced_width * reduced_height *
                                   sizeof (guint16));

  for (i = 0; i < reduced_width; i++)
    {
      for (j = 0; j < reduced_height; j++)
        {
          gint index;
          guint16 value;

          index = j * width * dimension_factor + i * dimension_factor;
          value = buffer[index];

          if (value < threshold_begin || value > threshold_end)
            {
              reduced_buffer[j * reduced_width + i] = 0;
              continue;
            }

          reduced_buffer[j * reduced_width + i] = value;
        }
    }

  buffer_info = g_slice_new0 (BufferInfo);
  buffer_info->reduced_buffer = reduced_buffer;
  buffer_info->reduced_width = reduced_width;
  buffer_info->reduced_height = reduced_height;
  buffer_info->width = width;
  buffer_info->height = height;

  return buffer_info;
}

static guchar *
create_grayscale_buffer (BufferInfo *buffer_info, gint dimension_reduction)
{
  gint i,j;
  gint size;
  guchar *grayscale_buffer;
  guint16 *reduced_buffer;

  reduced_buffer = buffer_info->reduced_buffer;

  size = buffer_info->width * buffer_info->height * sizeof (guchar) * 3;
  grayscale_buffer = g_slice_alloc (size);
  /*Paint is white*/
  memset (grayscale_buffer, 255, size);

  for (i = 0; i < buffer_info->reduced_width; i++)
    {
      for (j = 0; j < buffer_info->reduced_height; j++)
        {
          if (reduced_buffer[j * buffer_info->reduced_width + i] != 0)
            {
              gint index = j * dimension_reduction * buffer_info->width +
                i * dimension_reduction;
              grayscale_buffer_set_value (grayscale_buffer, index, 0);
            }
        }
    }

  return grayscale_buffer;
}

static guint16 *
read_file_to_buffer (gchar *name, gsize count, GError *e)
{
  GError *error = NULL;
  guint16 *depth = NULL;
  GFile *new_file = g_file_new_for_path (name);
  GFileInputStream *input_stream = g_file_read (new_file,
                                                NULL,
                                                &error);
  if (error != NULL)
    {
      g_debug ("ERROR: %s", error->message);
    }
  else
    {
      gsize bread = 0;
      depth = g_slice_alloc (count);
      g_input_stream_read_all ((GInputStream *) input_stream,
                               depth,
                               count,
                               &bread,
                               NULL,
                               &error);

      if (error != NULL)
        {
          g_debug ("ERROR: %s", error->message);
        }
    }
  return depth;
}

static void
on_depth_frame (GFreenectDevice *kinect, gpointer user_data)
{
  gint width, height;
  guchar *grayscale_buffer;
  guint16 *depth;
  gchar *contents;
  BufferInfo *buffer_info;
  gsize len;
  GError *error = NULL;
  GFreenectFrameMode frame_mode;

  depth = (guint16 *) gfreenect_device_get_depth_frame_raw (kinect,
                                                            &len,
                                                            &frame_mode);
  if (error != NULL)
    {
      g_debug ("ERROR Opening: %s", error->message);
    }


  width = frame_mode.width;
  height = frame_mode.height;

  buffer_info = process_buffer (depth,
                                width,
                                height,
                                1,
                                THRESHOLD_BEGIN,
                                THRESHOLD_END);

  grayscale_buffer = create_grayscale_buffer (buffer_info,
                                              1);

  if (record_shot)
    {
      g_debug ("Taking shot...");
      GError *error = NULL;
      gchar *name = g_strdup_printf ("./depth-data-%d", g_get_real_time ());
      g_file_set_contents (name, (gchar *) buffer_info->reduced_buffer,
                           width * height * sizeof (guint16), &error);
      if (error != NULL)
        {
          g_debug ("ERROR: %s", error->message);
        }
      else
        {
          g_print ("Created file: %s\n", name);
        }

      record_shot = FALSE;
    }

  if (! clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (depth_tex),
                                           grayscale_buffer,
                                           FALSE,
                                           width, height,
                                           0,
                                           3,
                                           CLUTTER_TEXTURE_NONE,
                                           &error))
    {
      g_debug ("Error setting texture area: %s", error->message);
      g_error_free (error);
    }
  g_slice_free1 (width * height * sizeof (guchar) * 3, grayscale_buffer);
}

static void
on_video_frame (GFreenectDevice *kinect, gpointer user_data)
{
  guchar *buffer;
  GError *error = NULL;
  GFreenectFrameMode frame_mode;

  buffer = gfreenect_device_get_video_frame_rgb (kinect, NULL, &frame_mode);

  if (! clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (video_tex),
                                           buffer,
                                           FALSE,
                                           frame_mode.width, frame_mode.height,
                                           0,
                                           frame_mode.bits_per_pixel / 8,
                                           CLUTTER_TEXTURE_NONE,
                                           &error))
    {
      g_debug ("Error setting texture area: %s", error->message);
      g_error_free (error);
    }
}

static void
set_info_text (gint seconds)
{
  gchar *title, *threshold;
  gchar *record_status = NULL;
  threshold = g_strdup_printf ("<b>Threshold:</b> %d",
                               THRESHOLD_END);
  if (seconds == 0)
    {
      record_status = g_strdup (" <b>SAVING DEPTH FILE!</b>");
    }
  else if (seconds > 0)
    {
      record_status = g_strdup_printf (" <b>Taking shot in:</b> %d seconds",
                                       seconds);
    }

  title = g_strconcat (threshold, record_status, NULL);
  clutter_text_set_markup (CLUTTER_TEXT (info_text), title);
  g_free (title);
  g_free (threshold);
  g_free (record_status);
}

static void
set_threshold (gint difference)
{
  gint new_threshold = THRESHOLD_END + difference;
  if (new_threshold >= THRESHOLD_BEGIN + 300 &&
      new_threshold <= 4000)
    THRESHOLD_END = new_threshold;
}

static void
set_tilt_angle (GFreenectDevice *kinect, gdouble difference)
{
  GError *error = NULL;
  gdouble angle;
  angle = gfreenect_device_get_tilt_angle_sync (kinect, NULL, &error);
  if (error != NULL)
    {
      g_error_free (error);
      return;
    }

  if (angle >= -31 && angle <= 31)
    gfreenect_device_set_tilt_angle (kinect,
                                     angle + difference,
                                     NULL,
                                     NULL,
                                     NULL);
}

static gboolean
decrease_time_to_take_shot (gpointer data)
{
  gboolean call_again = TRUE;
  set_info_text (seconds_to_shoot);
  if (seconds_to_shoot < 0)
    {
      seconds_to_shoot = DEFAULT_SECONDS_TO_SHOOT;
      call_again = FALSE;
    }
  else if (seconds_to_shoot == 0)
    {
      record_shot = TRUE;
    }
  seconds_to_shoot--;
  return call_again;
}

static void
take_shot (void)
{
  if (shot_timeout_id != 0)
    {
      g_source_remove (shot_timeout_id);
      seconds_to_shoot = DEFAULT_SECONDS_TO_SHOOT;
    }
  shot_timeout_id = g_timeout_add_seconds (1,
                                           decrease_time_to_take_shot,
                                           NULL);
}

static gboolean
on_key_release (ClutterActor *actor,
                ClutterEvent *event,
                gpointer data)
{
  GFreenectDevice *kinect;
  gint seconds = -1;
  gdouble angle;
  guint key;
  g_return_val_if_fail (event != NULL, FALSE);

  kinect = GFREENECT_DEVICE (data);

  key = clutter_event_get_key_symbol (event);
  switch (key)
    {
    case CLUTTER_KEY_space:
      seconds = 3;
      take_shot ();
      break;
    case CLUTTER_KEY_plus:
      set_threshold (100);
      break;
    case CLUTTER_KEY_minus:
      set_threshold (-100);
      break;
    case CLUTTER_KEY_Up:
      set_tilt_angle (kinect, 5);
      break;
    case CLUTTER_KEY_Down:
      set_tilt_angle (kinect, -5);
      break;
    }
  set_info_text (seconds);
  return TRUE;
}

static ClutterActor *
create_instructions (void)
{
  ClutterActor *text;

  text = clutter_text_new ();
  clutter_text_set_markup (CLUTTER_TEXT (text),
                         "<b>Instructions:</b>\n"
                         "\tTake shot and save:  \tSpace bar\n"
                         "\tSet tilt angle:  \t\t\t\tUp/Down Arrows\n"
                         "\tIncrease threshold:  \t\t\t+/-");
  return text;
}

static void
on_destroy (ClutterActor *actor, gpointer data)
{
  GFreenectDevice *device = GFREENECT_DEVICE (data);
  gfreenect_device_stop_depth_stream (device, NULL);
  gfreenect_device_stop_video_stream (device, NULL);
  clutter_main_quit ();
}

static void
on_new_kinect_device (GObject      *obj,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  ClutterActor *stage, *instructions;
  GError *error = NULL;
  gint width = 640;
  gint height = 480;

  kinect = gfreenect_device_new_finish (res, &error);
  if (kinect == NULL)
    {
      g_debug ("Failed to created kinect device: %s", error->message);
      g_error_free (error);
      clutter_main_quit ();
      return;
    }

  g_debug ("Kinect device created!");

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Kinect Test");
  clutter_actor_set_size (stage, width * 2, height + 200);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  g_signal_connect (stage, "destroy", G_CALLBACK (on_destroy), kinect);
  g_signal_connect (stage,
                    "key-release-event",
                    G_CALLBACK (on_key_release),
                    kinect);

  depth_tex = clutter_cairo_texture_new (width, height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), depth_tex);

  video_tex = clutter_cairo_texture_new (width, height);
  clutter_actor_set_position (video_tex, width, 0.0);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), video_tex);

  info_text = clutter_text_new ();
  set_info_text (-1);
  clutter_actor_set_position (info_text, 50, height + 20);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), info_text);

  instructions = create_instructions ();
  clutter_actor_set_position (instructions, 50, height + 70);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), instructions);

  clutter_actor_show_all (stage);

  g_signal_connect (kinect,
                    "depth-frame",
                    G_CALLBACK (on_depth_frame),
                    NULL);

  g_signal_connect (kinect,
                    "video-frame",
                    G_CALLBACK (on_video_frame),
                    NULL);

  gfreenect_device_set_tilt_angle (kinect, 0, NULL, NULL, NULL);

  gfreenect_device_start_depth_stream (kinect,
                                       GFREENECT_DEPTH_FORMAT_MM,
                                       NULL);

  gfreenect_device_start_video_stream (kinect,
                                       GFREENECT_RESOLUTION_MEDIUM,
                                       GFREENECT_VIDEO_FORMAT_RGB, NULL);
}

static void
quit (gint signale)
{
  signal (SIGINT, 0);

  clutter_main_quit ();
}

int
main (int argc, char *argv[])
{
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return -1;

  gfreenect_device_new (0,
                        GFREENECT_SUBDEVICE_CAMERA,
                        NULL,
                        on_new_kinect_device,
                        NULL);

  signal (SIGINT, quit);

  clutter_main ();

  if (kinect != NULL)
    g_object_unref (kinect);

  return 0;
}

