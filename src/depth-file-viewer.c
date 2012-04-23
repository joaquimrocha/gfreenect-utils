#include <math.h>
#include <string.h>
#include <errno.h>
#include <glib-object.h>
#include <clutter/clutter.h>

static ClutterActor *info_text;
static ClutterActor *depth_tex;

#define POINT_SIZE 6

static void
grayscale_buffer_set_value (guchar *buffer, gint index, guchar value)
{
  buffer[index * 3] = value;
  buffer[index * 3 + 1] = value;
  buffer[index * 3 + 2] = value;
}

static void
draw_point (guchar *buffer,
            guint width,
            guint height,
            gchar *color_str,
            guint x,
            guint y)
{
  ClutterColor *color = clutter_color_new (0, 0, 0, 255);
  clutter_color_from_string (color, color_str);
  gint i, j;
  for (i = -POINT_SIZE; i < POINT_SIZE; i++)
    {
      for (j = -POINT_SIZE; j < POINT_SIZE; j++)
        {
          if (x + i < 0 || x + i >= width ||
              y + j < 0 || y + j >= height)
            continue;

          buffer[(width * (y + j) + x + i) * 3] = color->red;
          buffer[(width * (y + j) + x + i) * 3 + 1] = color->green;
          buffer[(width * (y + j) + x + i) * 3 + 2] = color->blue;
        }
    }

  clutter_color_free (color);
}

static guchar *
create_grayscale_buffer (guint16 *buffer, guint width, guint height)
{
  gint i,j;
  gint size;
  guchar *grayscale_buffer;

  size = width * height * sizeof (guchar) * 3;
  grayscale_buffer = g_slice_alloc (size);
  /*Paint it white*/
  memset (grayscale_buffer, 255, size);

  for (i = 0; i < width; i++)
    {
      for (j = 0; j < height; j++)
        {
          guint16 value = round (buffer[j * width + i]  * 256. / 3000.);
          if (value != 0)
            {
              gint index = j * width + i;
              grayscale_buffer_set_value (grayscale_buffer,
                                          index,
                                          value);
            }
        }
    }

  return grayscale_buffer;
}

static guint16 *
read_file_to_buffer (const gchar *name, gsize count, GError *e)
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

static gboolean
paint_texture (guchar *buffer, guint width, guint height)
{
  GError *error = NULL;
  if (! clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (depth_tex),
                                           buffer,
                                           FALSE,
                                           width, height,
                                           0,
                                           3,
                                           CLUTTER_TEXTURE_NONE,
                                           &error))
    {
      g_debug ("Error setting texture area: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  g_slice_free1 (width * height * sizeof (guchar) * 3, buffer);
  return TRUE;
}

static guchar *
load_image (const gchar *name, guint width, guint height)
{
  guchar *grayscale_buffer;
  guint16 *depth;
  gchar *contents;
  gsize len;
  GError *error = NULL;

  gsize count = width * height * sizeof (guint16);
  depth = read_file_to_buffer (name, count, error);

  if (error != NULL)
    {
      g_debug ("Error Opening: %s", error->message);
      return NULL;
    }

  if (depth == NULL)
    {
      return NULL;
    }

  grayscale_buffer = create_grayscale_buffer (depth, width, height);

  g_slice_free1 (width * height * sizeof (guint16), depth);
  return grayscale_buffer;
}

static void
set_info_text (const gchar *name)
{
  gchar *title;
  title = g_strdup_printf ("<b>File:</b> %s", name);
  clutter_text_set_markup (CLUTTER_TEXT (info_text), title);
  g_free (title);
}

static void
on_destroy (ClutterActor *actor, gpointer data)
{
  clutter_main_quit ();
}

static void
create_stage (guint width, guint height)
{
  ClutterActor *stage, *instructions;
  GError *error = NULL;

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Depth File Viewer");
  clutter_actor_set_size (stage, width, height + 100);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  g_signal_connect (stage, "destroy", G_CALLBACK (on_destroy), NULL);

  depth_tex = clutter_cairo_texture_new (width, height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), depth_tex);

  info_text = clutter_text_new ();
  clutter_actor_set_position (info_text, 50, height + 20);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), info_text);

  clutter_actor_show_all (stage);
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
  guchar *buffer;
  gchar *file_name;
  guint width, height;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return -1;

  signal (SIGINT, quit);

  if (argc < 2)
    {
      g_print ("Usage: %s DEPTH_FILE [COLOR_STRING POINT_X POINT_Y]\n",
               argv[0]);
      return 0;
    }

  width = 640;
  height = 480;

  file_name = argv[1];
  create_stage (width, height);
  set_info_text (file_name);

  buffer = load_image (file_name, width, height);
  if (buffer == NULL)
    return -1;

  if (argc > 2)
    {
      if ((argc - 2) % 3 == 0)
        {
          guint i;
          for (i = 2; i < argc; i+=3)
            {
              gchar *color;
              guint x, y;
              color = argv[i];
              x = g_ascii_strtod (argv[i + 1], NULL);
              y = g_ascii_strtod (argv[i + 2], NULL);
              if (errno == 0)
                {
                  draw_point (buffer, width, height, color, x, y);
                }
            }
        }
      else
        {
          g_print ("Wrong number of arguments...\n");
        }
    }

  if (!paint_texture (buffer, width, height))
    {
      return -1;
    }
  clutter_main ();

  return 0;
}
