/**
 * Copyright (C) 2012 hejian <hejian.he@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "skin-whitening-images.h"

#define PLUG_IN_PROC   "plug-in-skin-whitening"
#define PLUG_IN_BINARY "skin-whitening"
#define PLUG_IN_ROLE   "gimp-skin-whitening"

#define PREVIEW_SIZE  480
#define THUMBNAIL_SIZE  80

typedef enum
{
  WHITENING_EFFECT_LITTLE_WHITENING,
  WHITENING_EFFECT_MODERATE_WHITENING,
  WHITENING_EFFECT_HIGH_WHITENING,
} WhiteningEffectType;

static const WhiteningEffectType effects[] =
{
  WHITENING_EFFECT_LITTLE_WHITENING,
  WHITENING_EFFECT_MODERATE_WHITENING,
  WHITENING_EFFECT_HIGH_WHITENING,
};

static void     query    (void);
static void     run      (const gchar      *name,
                          gint              nparams,
                          const GimpParam  *param,
                          gint             *nreturn_vals,
                          GimpParam       **return_vals);

static void     skin_whitening (GimpDrawable *drawable);

static gboolean skin_whitening_dialog (gint32        image_ID,
                                       GimpDrawable *drawable);

static void     preview_update (GtkWidget *preview);

static GtkWidget* effects_box_new ();
static GtkWidget* effect_icon_new (WhiteningEffectType effect);
static gboolean   effect_select (GtkWidget *event_box, GdkEventButton *event, WhiteningEffectType effect);

const GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static gint32     image_ID         = 0;
static gint       width;
static gint       height;

static GtkWidget *preview          = NULL;
static gint32     preview_image    = 0;

MAIN ()

static void
query (void)
{
  static const GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run-mode",   "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
    { GIMP_PDB_IMAGE,    "image",      "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",   "Input drawable" },
  };

  gimp_install_procedure (PLUG_IN_PROC,
                          "the easiest way for skin whitening.",
                          "the easiest way for skin whitening.",
                          "Hejian <hejian.he@gmail.com>",
                          "Hejian <hejian.he@gmail.com>",
                          "2012",
                          "_Skin whitening...",
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register (PLUG_IN_PROC, "<Image>/Tools/Beautify");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[2];
  GimpDrawable      *drawable;
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  image_ID = param[1].data.d_image;
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  width = gimp_image_width (image_ID);
  height = gimp_image_height (image_ID);

  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      if (! skin_whitening_dialog (image_ID, drawable))
        return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      break;

    default:
      break;
  }

  if ((status == GIMP_PDB_SUCCESS) &&
      (gimp_drawable_is_rgb(drawable->drawable_id) ||
       gimp_drawable_is_gray(drawable->drawable_id)))
    {
      /* Run! */
      gimp_image_undo_group_start (image_ID);
      skin_whitening (drawable);
      gimp_image_undo_group_end (image_ID);

      /* If run mode is interactive, flush displays */
      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        gimp_displays_flush ();

      /* Store data */
      /*if (run_mode == GIMP_RUN_INTERACTIVE)
        gimp_set_data (PLUG_IN_PROC, &bvals, sizeof (BeautifyValues));*/

    }

  gimp_drawable_detach (drawable);
}

static void
skin_whitening (GimpDrawable *drawable)
{
  gint32 source = gimp_image_get_active_layer (preview_image);
  gimp_edit_copy (source);
  gint32 floating_sel = gimp_edit_paste (drawable->drawable_id, FALSE);
  gimp_floating_sel_anchor (floating_sel);
}

static gboolean
skin_whitening_dialog (gint32        image_ID,
                 GimpDrawable *drawable)
{
  GtkWidget *dialog;
  GtkWidget *main_hbox;
  GtkWidget *left_vbox;
  GtkWidget *middle_vbox;
  GtkWidget *right_vbox;

  gimp_ui_init (PLUG_IN_BINARY, FALSE);

  dialog = gimp_dialog_new ("Skin Whitening", PLUG_IN_ROLE,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_PROC,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  gtk_widget_show (dialog);

  main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_hbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      main_hbox, TRUE, TRUE, 0);
  gtk_widget_show (main_hbox);

  left_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (left_vbox), 12);
  gtk_box_pack_start (GTK_BOX (main_hbox), left_vbox, TRUE, TRUE, 0);
  gtk_widget_show (left_vbox);

  middle_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (middle_vbox), 12);
  gtk_box_pack_start (GTK_BOX (main_hbox), middle_vbox, TRUE, TRUE, 0);
  gtk_widget_show (middle_vbox);

  right_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (right_vbox), 12);
  gtk_box_pack_start (GTK_BOX (main_hbox), right_vbox, TRUE, TRUE, 0);
  gtk_widget_show (right_vbox);

  /* preview */
  preview_image = gimp_image_duplicate (image_ID);

  preview = gtk_image_new();
  preview_update (preview);

  gtk_box_pack_start (GTK_BOX (middle_vbox), preview, TRUE, TRUE, 0);
  gtk_widget_show (preview);

  /* effects */
  GtkWidget *label = gtk_label_new ("One click whitening");
  gtk_box_pack_start (GTK_BOX (right_vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  GtkWidget *effects = effects_box_new ();
  gtk_box_pack_start (GTK_BOX (right_vbox), effects, FALSE, FALSE, 0);
  gtk_widget_show (effects);

  gboolean run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
}

static void
preview_update (GtkWidget *preview)
{
  gint preview_size = PREVIEW_SIZE;
  gint max_size = height;
  if (height < width)
    max_size = width;
  if (preview_size > max_size)
    preview_size = max_size;
  GdkPixbuf *pixbuf = gimp_image_get_thumbnail (preview_image, preview_size, preview_size, GIMP_PIXBUF_SMALL_CHECKS);
  gtk_image_set_from_pixbuf (GTK_IMAGE(preview), pixbuf);
}

static GtkWidget *
effects_box_new ()
{
  gint rows = 5;
  gint cols = 3;
  GtkWidget *table = gtk_table_new (rows, cols, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);

  gint row = 1;
  gint col = 1;

  gint i;
  for (i = 0; i < G_N_ELEMENTS (effects); i++) {
    GtkWidget *icon = effect_icon_new (effects[i]);
    gtk_table_attach_defaults (GTK_TABLE (table), icon, col - 1, col, row - 1, row);
    gtk_widget_show (icon);

    col++;
    if (col > cols)
    {
      row++;
      col = 1;
    }
  }

  return table;
}

static GtkWidget *
effect_icon_new (WhiteningEffectType effect)
{
  const guint8 *data;
  gchar  *title;

  switch (effect) {
    case WHITENING_EFFECT_LITTLE_WHITENING:
      data = skin_whitening_1;
      title = "Little W";
      break;
    case WHITENING_EFFECT_MODERATE_WHITENING:
      data = skin_whitening_2;
      title = "Middle W";
      break;
    case WHITENING_EFFECT_HIGH_WHITENING:
      data = skin_whitening_3;
      title = "High W";
      break;
  }

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);

  /* image */
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_inline (-1, data, FALSE, NULL);
  GtkWidget *image = gtk_image_new_from_pixbuf (pixbuf);
  GtkWidget *event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), image);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (box), event_box, FALSE, FALSE, 0);
  gtk_widget_show (event_box);

  g_signal_connect (event_box, "button_press_event", G_CALLBACK (effect_select), (gpointer)effect);

  /* label */
  GtkWidget *label = gtk_label_new (title);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  return box;
}

static gboolean
effect_select (GtkWidget *event_box, GdkEventButton *event, WhiteningEffectType effect)
{
  gint32 layer = gimp_image_get_active_layer (preview_image);
  switch (effect)
  {
    case WHITENING_EFFECT_LITTLE_WHITENING:
    {
      guint8 pts[] = {
        0.000000 * 255, 0.007843 * 255,
        0.121569 * 255, 0.160784 * 255,
        0.247059 * 255, 0.317647 * 255,
        0.372549 * 255, 0.462745 * 255,
        0.498039 * 255, 0.592157 * 255,
        0.623529 * 255, 0.713725 * 255,
        0.749020 * 255, 0.819608 * 255,
        0.874510 * 255, 0.913725 * 255,
        1.000000 * 255, 0.996078 * 255,
      };
      gimp_curves_spline (layer, GIMP_HISTOGRAM_RED, 18, pts);
      gimp_curves_spline (layer, GIMP_HISTOGRAM_GREEN, 18, pts);
      gimp_curves_spline (layer, GIMP_HISTOGRAM_BLUE, 18, pts);
      break;
    }
    case WHITENING_EFFECT_MODERATE_WHITENING:
    {
      guint8 pts[] = {
        0.000000 * 255, 0.007843 * 255,
        0.121569 * 255, 0.192157 * 255,
        0.247059 * 255, 0.372549 * 255,
        0.372549 * 255, 0.529412 * 255,
        0.498039 * 255, 0.666667 * 255,
        0.623529 * 255, 0.784314 * 255,
        0.749020 * 255, 0.874510 * 255,
        0.874510 * 255, 0.945098 * 255,
        1.000000 * 255, 0.996078 * 255,
      };
      gimp_curves_spline (layer, GIMP_HISTOGRAM_RED, 18, pts);
      gimp_curves_spline (layer, GIMP_HISTOGRAM_GREEN, 18, pts);
      gimp_curves_spline (layer, GIMP_HISTOGRAM_BLUE, 18, pts);
      break;
    }
    case WHITENING_EFFECT_HIGH_WHITENING:
    {
      guint8 pts[] = {
        0.000000 * 255, 0.007843 * 255,
        0.121569 * 255, 0.223529 * 255,
        0.247059 * 255, 0.427451 * 255,
        0.372549 * 255, 0.600000 * 255,
        0.498039 * 255, 0.741176 * 255,
        0.623529 * 255, 0.854902 * 255,
        0.749020 * 255, 0.933333 * 255,
        0.874510 * 255, 0.980392 * 255,
        1.000000 * 255, 0.996078 * 255,
      };
      gimp_curves_spline (layer, GIMP_HISTOGRAM_RED, 18, pts);
      gimp_curves_spline (layer, GIMP_HISTOGRAM_GREEN, 18, pts);
      gimp_curves_spline (layer, GIMP_HISTOGRAM_BLUE, 18, pts);
      break;
    }
  }

  preview_update (preview);
}
