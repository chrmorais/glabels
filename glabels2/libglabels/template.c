/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  (LIBGLABELS) Template library for GLABELS
 *
 *  template.c:  template module
 *
 *  Copyright (C) 2001-2006  Jim Evins <evins@snaught.com>.
 *
 *  This file is part of the LIBGLABELS library.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 */
#include <config.h>

#include "template.h"

#include <glib/gi18n.h>
#include <glib/gmem.h>
#include <glib/gstrfuncs.h>
#include <glib/gdir.h>
#include <glib/gmessages.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libglabels-private.h"
#include "xml.h"
#include "xml-template.h"
#include "paper.h"

#define FULL_PAGE "Full-page"

/*===========================================*/
/* Private types                             */
/*===========================================*/

/*===========================================*/
/* Private globals                           */
/*===========================================*/

static GList *templates = NULL;

/*===========================================*/
/* Local function prototypes                 */
/*===========================================*/
static lglTemplate *template_full_page           (const gchar            *page_size);

static GList       *read_templates               (void);

static GList       *read_template_files_from_dir (GList                  *templates,
                                                  const gchar            *dirname);

static gint         compare_origins              (gconstpointer           a,
                                                  gconstpointer           b,
                                                  gpointer                user_data);

/**
 * lgl_template_init:
 *
 * Initialize libglabels template module by reading all paper definition
 * files located in system and user template directories.
 *
 * The end user would typically call lgl_init() instead.
 */
void
lgl_template_init (void)
{
	GList *page_sizes, *p;

	if (templates) {
		return; /* Already initialized */
	}

	templates = read_templates ();

	page_sizes = lgl_paper_get_id_list ();
	for ( p=page_sizes; p != NULL; p=p->next ) {
		if ( !lgl_paper_is_id_other (p->data) ) {
			templates = g_list_append (templates,
						   template_full_page (p->data));
		}
	}
	lgl_paper_free_id_list (page_sizes);
}

/**
 * lgl_template_register:
 * @template:  Pointer to a template structure to add to database.
 *
 * Register a template.  This function adds a template to the template database.
 * The template will be stored in an individual XML file in the user template directory.
 */
void
lgl_template_register (const lglTemplate  *template)
{
	GList       *p_tmplt, *pa1;
	lglTemplate *template1;

	if (!templates) {
		lgl_template_init ();
	}

	for (p_tmplt = templates; p_tmplt != NULL; p_tmplt = p_tmplt->next) {
		template1 = (lglTemplate *) p_tmplt->data;

		for (pa1=template1->aliases; pa1!=NULL; pa1=pa1->next) {
			
			if (g_strcasecmp (template->name, pa1->data) == 0) {

				/* FIXME: make sure templates are really identical */
				/*        if not, apply hash to name to make unique. */
				return;
			}
				
		}

	}

	if (lgl_paper_is_id_known (template->page_size)) {

		gchar *dir, *filename, *abs_filename;

		templates = g_list_append (templates,
					   lgl_template_dup (template));

		/* FIXME: make sure filename is unique */
		dir = LGL_USER_DATA_DIR;
		mkdir (dir, 0775); /* Try to make sure directory exists. */
		filename = g_strconcat (template->name, ".template", NULL);
		abs_filename = g_build_filename (dir, filename, NULL);
		lgl_xml_template_write_template_to_file (template, abs_filename);
		g_free (dir);
		g_free (filename);
		g_free (abs_filename);

	} else {
		g_message ("Cannot register new template with unknown page size.");
	}

}

/**
 * lgl_template_get_name_list_unique:
 * @page_size: If non NULL, limit results to given page size.
 * @category: If non NULL, limit results to given template category.
 *
 * Get a list of valid names of unique templates in the template database.  Results
 * can be filtered by page size and/or template category.  A list of valid page sizes
 * can be obtained using lgl_paper_get_id_list().  A list of valid template categories
 * can be obtained using lgl_category_get_id_list().
 *
 * This function differs from lgl_template_get_name_list_all(), because it does not
 * return multiple names for the same template.
 *
 * Returns: a list of template names.
 */
GList *
lgl_template_get_name_list_unique (const gchar *page_size,
                                   const gchar *category)
{
	GList       *p_tmplt;
	lglTemplate *template;
	GList       *names = NULL;

	if (!templates)
        {
		lgl_template_init ();
	}

	for (p_tmplt = templates; p_tmplt != NULL; p_tmplt = p_tmplt->next)
        {
		template = (lglTemplate *) p_tmplt->data;
		if (lgl_template_does_page_size_match (template, page_size) &&
                    lgl_template_does_category_match (template, category))
                {
                        names = g_list_insert_sorted (names, g_strdup (template->name),
                                                      (GCompareFunc)g_strcasecmp);
		}
	}

	return names;
}

/**
 * lgl_template_get_name_list_all:
 * @page_size: If non NULL, limit results to given page size.
 * @category: If non NULL, limit results to given template category.
 *
 * Get a list of valid names and aliases of unique templates in the template database.
 * Results can be filtered by page size and/or template category.  A list of valid page
 * sizes can be obtained using lgl_paper_get_id_list().  A list of valid template
 * categories can be obtained using lgl_category_get_id_list().
 *
 * This function differs from lgl_template_get_name_list_unique(), because it will
 * return multiple names for the same template.
 *
 * Returns: a list of template names and aliases.
 */
GList *
lgl_template_get_name_list_all (const gchar *page_size,
                                const gchar *category)
{
	GList       *p_tmplt, *p_alias;
	lglTemplate *template;
	gchar       *str;
	GList       *names = NULL;

	if (!templates)
        {
		lgl_template_init ();
	}

	for (p_tmplt = templates; p_tmplt != NULL; p_tmplt = p_tmplt->next)
        {
		template = (lglTemplate *) p_tmplt->data;
		if (lgl_template_does_page_size_match (template, page_size) &&
                    lgl_template_does_category_match (template, category))
                {
			for (p_alias = template->aliases; p_alias != NULL;
			     p_alias = p_alias->next)
                        {
				str = g_strdup ((gchar *) p_alias->data);
				names = g_list_insert_sorted (names, str,
							     (GCompareFunc)g_strcasecmp);
			}
		}
	}

	return names;
}

/**
 * lgl_template_free_name_list:
 * @names: List of template name strings to be freed.
 *
 * Free up all storage associated with a list of template names obtained with
 * lgl_template_get_name_list_all() or lgl_template_get_name_list_unique().
 *
 */
void
lgl_template_free_name_list (GList *names)
{
	GList *p_name;

	for (p_name = names; p_name != NULL; p_name = p_name->next) {
		g_free (p_name->data);
		p_name->data = NULL;
	}

	g_list_free (names);
}

/**
 * lgl_template_from_name:
 * @name: name string
 *
 * Lookup template in template database from name string.
 *
 * Returns: pointer to a newly allocated #lglTemplate structure.
 *
 */
lglTemplate *
lgl_template_from_name (const gchar *name)
{
	GList        *p_tmplt, *p_alias;
	lglTemplate  *template;

	if (!templates) {
		lgl_template_init ();
	}

	if (name == NULL) {
		/* If no name, return first template as a default */
		return lgl_template_dup ((lglTemplate *) templates->data);
	}

	for (p_tmplt = templates; p_tmplt != NULL; p_tmplt = p_tmplt->next) {
		template = (lglTemplate *) p_tmplt->data;
		for (p_alias = template->aliases; p_alias != NULL;
		     p_alias = p_alias->next) {
			if (g_strcasecmp (p_alias->data, name) == 0) {

				return lgl_template_dup (template);
			}
		}
	}

	/* No matching template has been found so return the first template */
	return lgl_template_dup ((lglTemplate *) templates->data);
}

/**
 * lgl_template_get_first_frame:
 * @template: #lglTemplate structure to query
 *
 * Lookup first #lglTemplateFrame in the given #lglTemplate.  The returned
 * value is simply a pointer to the given frame within the template and should
 * not be freed.
 *
 * Note: glabels currently only supports a single frame per template.
 *
 * Returns: pointer to #lglTemplateFrame structure.
 *
 */
const lglTemplateFrame *
lgl_template_get_first_frame (const lglTemplate *template)
{
	g_return_val_if_fail (template, NULL);

	return (lglTemplateFrame *)template->frames->data;
}

/**
 * lgl_template_frame_get_size:
 * @frame: #lglTemplateFrame structure to query
 * @w: pointer to location to receive width of frame
 * @h: pointer to location to receive height of frame
 *
 * Get size (width and height) of given #lglTemplateFrame in points.
 *
 */
void
lgl_template_frame_get_size (const lglTemplateFrame *frame,
                             gdouble                *w,
                             gdouble                *h)
{
	g_return_if_fail (frame);

	switch (frame->shape) {
	case LGL_TEMPLATE_FRAME_SHAPE_RECT:
		*w = frame->rect.w;
		*h = frame->rect.h;
		break;
	case LGL_TEMPLATE_FRAME_SHAPE_ROUND:
		*w = 2.0 * frame->round.r;
		*h = 2.0 * frame->round.r;
		break;
	case LGL_TEMPLATE_FRAME_SHAPE_CD:
		if (frame->cd.w == 0.0) {
			*w = 2.0 * frame->cd.r1;
		} else {
			*w = frame->cd.w;
		}
		if (frame->cd.h == 0.0) {
			*h = 2.0 * frame->cd.r1;
		} else {
			*h = frame->cd.h;
		}
		break;
	default:
		*w = 0.0;
		*h = 0.0;
		break;
	}
}

/**
 * lgl_template_frame_get_n_labels:
 * @frame: #lglTemplateFrame structure to query
 *
 * Get total number of labels per sheet corresponding to the given frame.
 *
 * Returns: number of labels per sheet.
 *
 */
gint
lgl_template_frame_get_n_labels (const lglTemplateFrame *frame)
{
	gint               n_labels = 0;
	GList             *p;
	lglTemplateLayout *layout;

	g_return_val_if_fail (frame, 0);

	for ( p=frame->all.layouts; p != NULL; p=p->next ) {
		layout = (lglTemplateLayout *)p->data;

		n_labels += layout->nx * layout->ny;
	}

	return n_labels;
}

/**
 * lgl_template_frame_get_origins:
 * @frame: #lglTemplateFrame structure to query
 *
 * Get an array of label origins for the given frame.  These origins represent the
 * upper left hand corner of each label on a page corresponding to the given frame.
 * The array should be freed using g_free().
 *
 * Returns: A newly allocated array of #lglTemplateOrigin structures.
 *
 */
lglTemplateOrigin *
lgl_template_frame_get_origins (const lglTemplateFrame *frame)
{
	gint               i_label, n_labels, ix, iy;
	lglTemplateOrigin *origins;
	GList             *p;
	lglTemplateLayout *layout;

	g_return_val_if_fail (frame, NULL);

	n_labels = lgl_template_frame_get_n_labels (frame);
	origins = g_new0 (lglTemplateOrigin, n_labels);

	i_label = 0;
	for ( p=frame->all.layouts; p != NULL; p=p->next ) {
		layout = (lglTemplateLayout *)p->data;

		for (iy = 0; iy < layout->ny; iy++) {
			for (ix = 0; ix < layout->nx; ix++, i_label++) {
				origins[i_label].x = ix*layout->dx + layout->x0;
				origins[i_label].y = iy*layout->dy + layout->y0;
			}
		}
	}

	g_qsort_with_data (origins, n_labels, sizeof(lglTemplateOrigin),
			   compare_origins, NULL);

	return origins;
}

/**
 * lgl_template_new:
 *   @name:         Template name
 *   @description:  Template descriptions
 *   @page_size:    Page size id
 *   @page_width:   Page width in points, set to zero unless page_size="Other"
 *   @page_height:  Page height in points, set to zero unless page_size="Other"
 *
 * Create a new template structure, with the given top-level attributes.  The
 * created template will have no initial aliases, categories, or frames
 * associated with it.  See lgl_template_add_alias(), lgl_template_add_category(),
 * and lgl_template_add_frame() to add these.
 *
 * Returns: pointer to a newly allocated #lglTemplate structure.
 *
 */
lglTemplate *
lgl_template_new (const gchar         *name,
                  const gchar         *description,
                  const gchar         *page_size,
                  gdouble              page_width,
                  gdouble              page_height)
{
	lglTemplate *template;

	template = g_new0 (lglTemplate,1);

	template->name        = g_strdup (name);
	template->description = g_strdup (description);
	template->page_size   = g_strdup (page_size);
	template->page_width  = page_width;
	template->page_height = page_height;

	/* Always include primary name in alias list. */
	template->aliases = NULL;
	template->aliases = g_list_append (template->aliases, g_strdup (name));

	return template;
}

/**
 * lgl_template_does_page_size_match:
 *   @template:  Pointer to template structure to test
 *   @page_size: Page size ID string
 *
 * This function tests if the page size of the template matches the given ID.
 *
 * Returns:  TRUE if the template matches the given page size ID.
 *
 */
gboolean
lgl_template_does_page_size_match (const lglTemplate  *template,
                                   const gchar        *page_size)
{
	g_return_val_if_fail (template, FALSE);

        /* NULL matches everything. */
        if (page_size == NULL)
        {
                return TRUE;
        }

        return g_strcasecmp(page_size, template->page_size) == 0;
}

/**
 * lgl_template_does_category_match:
 *   @template:  Pointer to template structure to test
 *   @category:  Category ID string
 *
 * This function tests if the given template belongs to the given category ID.
 *
 * Returns:  TRUE if the template matches the given category ID.
 *
 */
gboolean
lgl_template_does_category_match  (const lglTemplate  *template,
                                   const gchar        *category)
{
        GList *p;

	g_return_val_if_fail (template, FALSE);

        /* NULL matches everything. */
        if (category == NULL)
        {
                return TRUE;
        }

        for ( p=template->categories; p != NULL; p=p->next )
        {
                if (g_strcasecmp(category, p->data) == 0)
                {
                        return TRUE;
                }
        }

        return FALSE;
}

/**
 * lgl_template_add_frame:
 *   @template:  Pointer to template structure
 *   @frame:     Pointer to frame structure
 *
 * This function adds the given frame structure to the template.  Once added,
 * the frame structure belongs to the given template; do not attempt to free
 * it.
 *
 * Note: Currently glabels only supports a single frame per template.
 *
 */
void
lgl_template_add_frame (lglTemplate      *template,
                        lglTemplateFrame *frame)
{
	g_return_if_fail (template);
	g_return_if_fail (frame);

	template->frames = g_list_append (template->frames, frame);
}
 
/**
 * lgl_template_add_category:
 *   @template:  Pointer to template structure
 *   @category:  Category ID string
 *
 * This function adds the given category ID to a templates category list.
 *
 */
void
lgl_template_add_category (lglTemplate         *template,
                           const gchar         *category)
{
	g_return_if_fail (template);
	g_return_if_fail (category);

	template->categories = g_list_append (template->categories,
                                              g_strdup (category));
}
 
/**
 * lgl_template_add_alias:
 *   @template:  Pointer to template structure
 *   @alias:     Alias string
 *
 * This function adds the given alias to a templates list of aliases.
 *
 */
void
lgl_template_add_alias (lglTemplate         *template,
                        const gchar         *alias)
{
	g_return_if_fail (template);
	g_return_if_fail (alias);

	template->aliases = g_list_append (template->aliases,
					   g_strdup (alias));
}
 
/**
 * lgl_template_frame_rect_new:
 *   @id:      ID of frame.  (This should currently always be "0").
 *   @w:       width of frame in points.
 *   @h:       height of frame in points.
 *   @r:       radius of rounded corners in points.  (Should be 0 for square corners.)
 *   @x_waste: Amount of overprint to allow in the horizontal direction.
 *   @y_waste: Amount of overprint to allow in the vertical direction.
 *
 * This function creates a new template frame for a rectangular label or card.
 *
 * Returns: Pointer to newly allocated #lglTemplateFrame structure.
 *
 */
lglTemplateFrame *
lgl_template_frame_rect_new  (const gchar         *id,
                              gdouble              w,
                              gdouble              h,
                              gdouble              r,
                              gdouble              x_waste,
                              gdouble              y_waste)
{
	lglTemplateFrame *frame;

	frame = g_new0 (lglTemplateFrame, 1);

	frame->shape = LGL_TEMPLATE_FRAME_SHAPE_RECT;
	frame->rect.id = g_strdup (id);

	frame->rect.w = w;
	frame->rect.h = h;
	frame->rect.r = r;
	frame->rect.x_waste = x_waste;
	frame->rect.y_waste = y_waste;

	return frame;
}

/**
 * lgl_template_frame_round_new:
 *   @id:      ID of frame.  (This should currently always be "0").
 *   @r:       radius of label in points.
 *   @waste:   Amount of overprint to allow.
 *
 * This function creates a new template frame for a round label.
 *
 * Returns: Pointer to newly allocated #lglTemplateFrame structure.
 *
 */
lglTemplateFrame *
lgl_template_frame_round_new (const gchar         *id,
                              gdouble              r,
                              gdouble              waste)
{
	lglTemplateFrame *frame;

	frame = g_new0 (lglTemplateFrame, 1);

	frame->shape = LGL_TEMPLATE_FRAME_SHAPE_ROUND;
	frame->round.id = g_strdup (id);

	frame->round.r = r;
	frame->round.waste = waste;

	return frame;
}
                                                                               
/**
 * lgl_template_frame_cd_new:
 *   @id:      ID of frame.  (This should currently always be "0").
 *   @r1:      outer radius of label in points.
 *   @r2:      radius of center hole in points.
 *   @w:       clip width of frame in points for business card CDs.  Should be 0 for no clipping.
 *   @h:       clip height of frame in points for business card CDs.  Should be 0 for no clipping.
 *   @waste:   Amount of overprint to allow.
 *
 * This function creates a new template frame for a CD/DVD label.
 *
 * Returns: Pointer to newly allocated #lglTemplateFrame structure.
 *
 */
lglTemplateFrame *
lgl_template_frame_cd_new (const gchar         *id,
                           gdouble              r1,
                           gdouble              r2,
                           gdouble              w,
                           gdouble              h,
                           gdouble              waste)
{
	lglTemplateFrame *frame;

	frame = g_new0 (lglTemplateFrame, 1);

	frame->shape = LGL_TEMPLATE_FRAME_SHAPE_CD;
	frame->cd.id = g_strdup (id);

	frame->cd.r1 = r1;
	frame->cd.r2 = r2;
	frame->cd.w  = w;
	frame->cd.h  = h;
	frame->cd.waste = waste;

	return frame;
}

/**
 * lgl_template_frame_add_layout:
 *   @frame:  Pointer to template frame to add layout to.
 *   @layout: Pointer to layout structure to add to frame.
 *
 * This function adds a layout structure to the given template frame.
 *
 */
void
lgl_template_frame_add_layout (lglTemplateFrame   *frame,
                               lglTemplateLayout  *layout)
{
	g_return_if_fail (frame);
	g_return_if_fail (layout);

	frame->all.layouts = g_list_append (frame->all.layouts, layout);
}
 
/**
 * lgl_template_frame_add_markup:
 *   @frame:  Pointer to template frame to add markup to.
 *   @markup: Pointer to markup structure to add to frame.
 *
 * This function adds a markup structure to the given template frame.
 *
 */
void
lgl_template_frame_add_markup (lglTemplateFrame   *frame,
                               lglTemplateMarkup  *markup)
{
	g_return_if_fail (frame);
	g_return_if_fail (markup);

	frame->all.markups = g_list_append (frame->all.markups, markup);
}
 
/**
 * lgl_template_layout_new:
 *   @nx:  Number of labels across.
 *   @ny:  Number of labels down.
 *   @x0:  X coordinate of the top-left corner of the top-left label in the layout in points.
 *   @y0:  Y coordinate of the top-left corner of the top-left label in the layout in points.
 *   @dx:  Horizontal pitch in points.  This is the distance from left-edge to left-edge.
 *   @dy:  Vertical pitch in points.  This is the distance from top-edge to top-edge.
 *
 * This function creates a new layout structure with the given parameters.
 *
 * Returns: a newly allocated #lglTemplateLayout structure.
 *
 */
lglTemplateLayout *
lgl_template_layout_new (gint    nx,
                         gint    ny,
                         gdouble x0,
                         gdouble y0,
                         gdouble dx,
                         gdouble dy)
{
	lglTemplateLayout *layout;

	layout = g_new0 (lglTemplateLayout, 1);

	layout->nx = nx;
	layout->ny = ny;
	layout->x0 = x0;
	layout->y0 = y0;
	layout->dx = dx;
	layout->dy = dy;

	return layout;
}

/**
 * lgl_template_markup_margin_new:
 *   @size: margin size in points.
 *
 * This function creates a new margin markup structure.
 *
 * Returns: a newly allocated #lglTemplateMarkup structure.
 *
 */
lglTemplateMarkup *
lgl_template_markup_margin_new (gdouble size)
{
	lglTemplateMarkup *markup;

	markup = g_new0 (lglTemplateMarkup, 1);

	markup->type        = LGL_TEMPLATE_MARKUP_MARGIN;
	markup->margin.size = size;

	return markup;
}

/**
 * lgl_template_markup_line_new:
 *   @x1: x coordinate of first endpoint.
 *   @y1: y coordinate of first endpoint.
 *   @x2: x coordinate of second endpoint.
 *   @y2: y coordinate of second endpoint.
 *
 * This function creates a new line markup structure.
 *
 * Returns: a newly allocated #lglTemplateMarkup structure.
 *
 */
lglTemplateMarkup *
lgl_template_markup_line_new (gdouble x1,
                              gdouble y1,
                              gdouble x2,
                              gdouble y2)
{
	lglTemplateMarkup *markup;

	markup = g_new0 (lglTemplateMarkup, 1);

	markup->type        = LGL_TEMPLATE_MARKUP_LINE;
	markup->line.x1     = x1;
	markup->line.y1     = y1;
	markup->line.x2     = x2;
	markup->line.y2     = y2;

	return markup;
}

/**
 * lgl_template_markup_circle_new:
 *   @x0: x coordinate of center of circle.
 *   @y0: y coordinate of center of circle.
 *   @r:  radius of circle.
 *
 * This function creates a new circle markup structure.
 *
 * Returns: a newly allocated #lglTemplateMarkup structure.
 *
 */
lglTemplateMarkup *
lgl_template_markup_circle_new (gdouble x0,
                                gdouble y0,
                                gdouble r)
{
	lglTemplateMarkup *markup;

	markup = g_new0 (lglTemplateMarkup, 1);

	markup->type        = LGL_TEMPLATE_MARKUP_CIRCLE;
	markup->circle.x0   = x0;
	markup->circle.y0   = y0;
	markup->circle.r    = r;

	return markup;
}

/**
 * lgl_template_markup_rect_new:
 *   @x1: x coordinate of top-left corner of rectangle.
 *   @y1: y coordinate of top-left corner of rectangle.
 *   @w:  width of rectangle.
 *   @h:  height of rectangle.
 *   @r:  radius of rounded corner.
 *
 * This function creates a new rectangle markup structure.
 *
 * Returns: a newly allocated #lglTemplateMarkup structure.
 *
 */
lglTemplateMarkup *
lgl_template_markup_rect_new (gdouble x1,
                              gdouble y1,
                              gdouble w,
                              gdouble h,
                              gdouble r)
{
	lglTemplateMarkup *markup;

	markup = g_new0 (lglTemplateMarkup, 1);

	markup->type        = LGL_TEMPLATE_MARKUP_RECT;
	markup->rect.x1     = x1;
	markup->rect.y1     = y1;
	markup->rect.w      = w;
	markup->rect.h      = h;
	markup->rect.r      = r;

	return markup;
}


/**
 * lgl_template_dup:
 *   @orig_template: Template to duplicate.
 *
 * This function duplicates a template structure.
 *
 * Returns:  a newly allocated #lglTemplate structure.
 *
 */
lglTemplate *
lgl_template_dup (const lglTemplate *orig_template)
{
	lglTemplate         *template;
	GList               *p;
	lglTemplateFrame    *frame;

	g_return_val_if_fail (orig_template, NULL);

	template = lgl_template_new (orig_template->name,
                                     orig_template->description,
                                     orig_template->page_size,
                                     orig_template->page_width,
                                     orig_template->page_height);

	for ( p=orig_template->categories; p != NULL; p=p->next ) {

                lgl_template_add_category (template, p->data);

	}

	for ( p=orig_template->frames; p != NULL; p=p->next ) {

		frame = (lglTemplateFrame *)p->data;

		lgl_template_add_frame (template, lgl_template_frame_dup (frame));
	}

	for ( p=orig_template->aliases; p != NULL; p=p->next ) {

		if (g_strcasecmp (template->name, p->data) != 0) {
			lgl_template_add_alias (template, p->data);
		}

	}

	return template;
}

/**
 * lgl_template_free:
 *   @template: Template to free.
 *
 * This function frees all memory associated with given template structure.
 *
 */
void
lgl_template_free (lglTemplate *template)
{
	GList            *p;
	lglTemplateFrame *frame;

	if ( template != NULL ) {

		g_free (template->name);
		template->name = NULL;

		g_free (template->description);
		template->description = NULL;

		g_free (template->page_size);
		template->page_size = NULL;

		for ( p=template->categories; p != NULL; p=p->next ) {

			g_free (p->data);
			p->data = NULL;

		}
		g_list_free (template->categories);
		template->categories = NULL;

		for ( p=template->frames; p != NULL; p=p->next ) {

			frame = (lglTemplateFrame *)p->data;

			lgl_template_frame_free (frame);
			p->data = NULL;
		}
		g_list_free (template->frames);
		template->frames = NULL;

		for ( p=template->aliases; p != NULL; p=p->next ) {

			g_free (p->data);
			p->data = NULL;

		}
		g_list_free (template->aliases);
		template->aliases = NULL;

		g_free (template);

	}

}

/**
 * lgl_template_frame_dup:
 *   @orig_frame: Frame to duplicate.
 *
 * This function duplicates a template frame structure.
 *
 * Returns:  a newly allocated #lglTemplateFrame structure.
 *
 */
lglTemplateFrame *
lgl_template_frame_dup (const lglTemplateFrame *orig_frame)
{
	lglTemplateFrame    *frame;
	GList               *p;
	lglTemplateLayout   *layout;
	lglTemplateMarkup   *markup;

	g_return_val_if_fail (orig_frame, NULL);

	switch (orig_frame->shape) {

	case LGL_TEMPLATE_FRAME_SHAPE_RECT:
		frame =
			lgl_template_frame_rect_new (orig_frame->all.id,
                                                     orig_frame->rect.w,
                                                     orig_frame->rect.h,
                                                     orig_frame->rect.r,
                                                     orig_frame->rect.x_waste,
                                                     orig_frame->rect.y_waste);
		break;

	case LGL_TEMPLATE_FRAME_SHAPE_ROUND:
		frame =
			lgl_template_frame_round_new (orig_frame->all.id,
                                                      orig_frame->round.r,
                                                      orig_frame->round.waste);
		break;

	case LGL_TEMPLATE_FRAME_SHAPE_CD:
		frame =
			lgl_template_frame_cd_new (orig_frame->all.id,
                                                   orig_frame->cd.r1,
                                                   orig_frame->cd.r2,
                                                   orig_frame->cd.w,
                                                   orig_frame->cd.h,
                                                   orig_frame->cd.waste);
		break;

	default:
		return NULL;
		break;
	}

	for ( p=orig_frame->all.layouts; p != NULL; p=p->next ) {

		layout = (lglTemplateLayout *)p->data;

		lgl_template_frame_add_layout (frame, lgl_template_layout_dup (layout));
	}

	for ( p=orig_frame->all.markups; p != NULL; p=p->next ) {

		markup = (lglTemplateMarkup *)p->data;

		lgl_template_frame_add_markup (frame, lgl_template_markup_dup (markup));
	}

	return frame;
}

/**
 * lgl_template_frame_free:
 *   @frame: Frame to free.
 *
 * This function frees all memory associated with given template frame structure.
 *
 */
void
lgl_template_frame_free (lglTemplateFrame *frame)
{
	GList                *p;
	lglTemplateLayout    *layout;
	lglTemplateMarkup    *markup;

	if ( frame != NULL ) {

		g_free (frame->all.id);
		frame->all.id = NULL;

		for ( p=frame->all.layouts; p != NULL; p=p->next ) {

			layout = (lglTemplateLayout *)p->data;

			lgl_template_layout_free (layout);
			p->data = NULL;
		}
		g_list_free (frame->all.layouts);
		frame->all.layouts = NULL;

		for ( p=frame->all.markups; p != NULL; p=p->next ) {

			markup = (lglTemplateMarkup *)p->data;

			lgl_template_markup_free (markup);
			p->data = NULL;
		}
		g_list_free (frame->all.markups);
		frame->all.markups = NULL;

		g_free (frame);

	}

}

/**
 * lgl_template_layout_dup:
 *   @orig_layout: Layout to duplicate.
 *
 * This function duplicates a template layout structure.
 *
 * Returns:  a newly allocated #lglTemplateLayout structure.
 *
 */
lglTemplateLayout *
lgl_template_layout_dup (const lglTemplateLayout *orig_layout)
{
	lglTemplateLayout *layout;

	g_return_val_if_fail (orig_layout, NULL);

	layout = g_new0 (lglTemplateLayout, 1);

	/* copy contents */
	*layout = *orig_layout;

	return layout;
}

/**
 * lgl_template_layout_free:
 *   @layout: Layout to free.
 *
 * This function frees all memory associated with given template layout structure.
 *
 */
void
lgl_template_layout_free (lglTemplateLayout *layout)
{
	g_free (layout);
}

/**
 * lgl_template_markup_dup:
 *   @orig_markup: Markup to duplicate.
 *
 * This function duplicates a template markup structure.
 *
 * Returns:  a newly allocated #lglTemplateMarkup structure.
 *
 */
lglTemplateMarkup *
lgl_template_markup_dup (const lglTemplateMarkup *orig_markup)
{
	lglTemplateMarkup *markup;

	g_return_val_if_fail (orig_markup, NULL);

	markup = g_new0 (lglTemplateMarkup, 1);

	*markup = *orig_markup;

	return markup;
}

/**
 * lgl_template_markup_free:
 *   @markup: Markup to free.
 *
 * This function frees all memory associated with given template markup structure.
 *
 */
void
lgl_template_markup_free (lglTemplateMarkup *markup)
{
	g_free (markup);
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Make a template for a full page of the given page size.        */
/*--------------------------------------------------------------------------*/
static lglTemplate *
template_full_page (const gchar *page_size)
{
	lglPaper              *paper = NULL;
	lglTemplate           *template = NULL;
	lglTemplateFrame      *frame = NULL;
	gchar                 *name;

	g_return_val_if_fail (page_size, NULL);

	paper = lgl_paper_from_id (page_size);
	if ( paper == NULL ) {
		return NULL;
	}

	name         = g_strdup_printf (_("Generic %s full page"), page_size);

	template = lgl_template_new (name,
                                     FULL_PAGE,
                                     page_size,
                                     paper->width,
                                     paper->height);


	frame = lgl_template_frame_rect_new ("0",
                                             paper->width,
                                             paper->height,
                                             0.0,
                                             0.0,
                                             0.0);
	lgl_template_add_frame (template, frame);

	lgl_template_frame_add_layout (frame, lgl_template_layout_new (1, 1, 0., 0., 0., 0.));

	lgl_template_frame_add_markup (frame, lgl_template_markup_margin_new (9.0));

	g_free (name);
	name = NULL;
	lgl_paper_free (paper);
	paper = NULL;

	return template;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Read templates from various  files.                            */
/*--------------------------------------------------------------------------*/
static GList *
read_templates (void)
{
	gchar *data_dir;
	GList *templates = NULL;

	data_dir = LGL_SYSTEM_DATA_DIR;
	templates = read_template_files_from_dir (templates, data_dir);
	g_free (data_dir);

	data_dir = LGL_USER_DATA_DIR;
	templates = read_template_files_from_dir (templates, data_dir);
	g_free (data_dir);

	if (templates == NULL) {
		g_critical (_("Unable to locate any template files.  Libglabels may not be installed correctly!"));
	}

	return templates;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Read all template files from given directory.  Append to list. */
/*--------------------------------------------------------------------------*/
static GList *
read_template_files_from_dir (GList       *templates,
			      const gchar *dirname)
{
	GDir        *dp;
	const gchar *filename, *extension, *extension2;
	gchar       *full_filename = NULL;
	GError      *gerror = NULL;
	GList       *new_templates = NULL;

	if (dirname == NULL)
		return templates;

	if (!g_file_test (dirname, G_FILE_TEST_EXISTS)) {
		return templates;
	}

	dp = g_dir_open (dirname, 0, &gerror);
	if (gerror != NULL) {
	        g_message ("cannot open data directory: %s", gerror->message );
		return templates;
	}

	while ((filename = g_dir_read_name (dp)) != NULL) {

		extension = strrchr (filename, '.');
		extension2 = strrchr (filename, '-');

		if ( (extension && (g_strcasecmp (extension, ".template") == 0)) ||
		     (extension2 && (g_strcasecmp (extension2, "-templates.xml") == 0)) ) {

			full_filename = g_build_filename (dirname, filename, NULL);
			new_templates =
				lgl_xml_template_read_templates_from_file (full_filename);
			g_free (full_filename);

			templates = g_list_concat (templates, new_templates);
			new_templates = NULL;
		}

	}

	g_dir_close (dp);

	return templates;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Sort origins comparison function, first by y then by x.        */
/*--------------------------------------------------------------------------*/
static gint
compare_origins (gconstpointer a,
		 gconstpointer b,
		 gpointer      user_data)
{
	const lglTemplateOrigin *a_origin = a, *b_origin = b;

	if ( a_origin->y < b_origin->y ) {
		return -1;
	} else if ( a_origin->y > b_origin->y ) {
		return +1;
	} else {
		if ( a_origin->x < b_origin->x ) {
			return -1;
		} else if ( a_origin->x > b_origin->x ) {
			return +1;
		} else {
			return 0; /* hopefully 2 labels won't have the same origin */
		}
	}
}

/**
 * lgl_template_print_known_templates:
 *
 * Print all known templates (for debugging purposes).
 *
 */
void
lgl_template_print_known_templates (void)
{
	GList       *p;
	lglTemplate *template;

	g_print ("%s():\n", __FUNCTION__);
	for (p=templates; p!=NULL; p=p->next) {
		template = (lglTemplate *)p->data;

		g_print("TEMPLATE name=\"%s\", description=\"%s\"\n",
			template->name, template->description);

	}
	g_print ("\n");

}

/**
 * lgl_template_print_aliases:
 *   @template: template
 *
 * Print all aliases of a template (for debugging purposes).
 *
 */
void
lgl_template_print_aliases (const lglTemplate *template)
{
	GList *p;

	g_print ("%s():\n", __FUNCTION__);
	for (p=template->aliases; p!=NULL; p=p->next) {
		
		g_print("Alias = \"%s\"\n", (gchar *)p->data);

	}
	g_print ("\n");

}

