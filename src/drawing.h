/*
 * MimamoriKanshi System Monitor - Drawing Header
 *
 * Cairo-based rendering of scrolling history graphs, latest-value bars,
 * and text overlays for each monitoring row.
 */

#ifndef MIMAMORIKANSHI_DRAWING_H
#define MIMAMORIKANSHI_DRAWING_H

#include <gtk/gtk.h>
#include <cairo/cairo.h>

/* Forward declaration */
typedef struct _MimamorikanshiPlugin MimamorikanshiPlugin;

/* Create / recreate per-row Cairo image surfaces.
   Call on startup and whenever dimensions change. */
void mimamorikanshi_drawing_ensure_surfaces(MimamorikanshiPlugin *mmk);

/* Destroy all per-row surfaces. */
void mimamorikanshi_drawing_free_surfaces(MimamorikanshiPlugin *mmk);

/* Write one new column of data into each row's circular buffer surface.
   Call once per timer tick after monitor_update(). */
void mimamorikanshi_drawing_update_columns(MimamorikanshiPlugin *mmk);

/* GTK "draw" signal callback for the drawing area widget. */
gboolean mimamorikanshi_drawing_draw(GtkWidget *widget,
                                      cairo_t   *cr,
                                      gpointer   user_data);

#endif /* MIMAMORIKANSHI_DRAWING_H */
