/*  central.c
 *
 *  Copyright (C) 2002 Jasper Huijsmans <huysmans@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*  central.c
 *  ---------
 *  Contains functions for the central part of the panel, which consists of 
 *  the desktop switcher and four mini-buttons.
*/

#include "central.h"

#include "xfce.h"
#include "wmhints.h"
#include "callbacks.h"
#include "icons.h"
#include "settings.h"

/*  colors used for screen buttons in OLD_STYLE 
*/
static char *screen_class[] = {
    "gxfce_color2",
    "gxfce_color5",
    "gxfce_color4",
    "gxfce_color6"
};

GtkWidget *minibuttons[4];
GtkWidget *mini_tables[2]; /* LEFT and RIGHT */

GtkWidget *desktop_table;
ScreenButton *screen_buttons[NBSCREENS];
char *screen_names[NBSCREENS];

int current_screen = 0;

/*  screen names
*/
static char *get_default_screen_name(int index)
{
    char temp[3];

    g_snprintf(temp, 3, "%d", index+1);

    return g_strdup(temp);
}

void init_screen_names(void)
{
    int i;

    for (i = 0; i < NBSCREENS; i++)
	screen_names[i] = NULL;
}

/*  Screen buttons
 *  --------------
*/
struct _ScreenButton
{
    int index;

    int callback_id;

    GtkWidget *frame;
    GtkWidget *button;
    GtkWidget *label;
};

char *screen_button_get_name(ScreenButton *sb)
{
    return g_strdup(screen_names[sb->index]);
}

void screen_button_set_name(ScreenButton *sb, const char *name)
{
    g_free(screen_names[sb->index]);
    
    screen_names[sb->index] = g_strdup(name);
    gtk_label_set_text(GTK_LABEL(sb->label), name);
}

int screen_button_get_index(ScreenButton *sb)
{
    return sb->index;
}

static void screen_button_set_size(ScreenButton * sb, int size)
{
    int w, h;

    /* TODO:
     * calculation of height is very arbitrary. I just put here what looks 
     * good on my screen. Should probably be something a little more 
     * intelligent. */
    
    w = screen_button_width[size];
    
    switch (size)
    {
        case TINY:
            h = icon_size[TINY];
            break;
        case SMALL:
            h = -1;
            break;
        case LARGE:
            h = (top_height[LARGE] + icon_size[LARGE]) / 2 - 6;
            break;
        default:
            h = (top_height[MEDIUM] + icon_size[MEDIUM]) / 2 - 5;
    }

    gtk_widget_set_size_request(sb->button, w, h);
}

static void screen_button_set_style(ScreenButton * sb, int style)
{
    if(style == OLD_STYLE)
        gtk_widget_set_name(sb->button, screen_class[sb->index % 4]);
    else
        gtk_widget_set_name(sb->button, "gxfce_color4");
}

ScreenButton *create_screen_button(int index)
{
    ScreenButton *sb = g_new(ScreenButton, 1);

    sb->index = index;
    sb->callback_id = 0;

    sb->label = NULL;

    sb->frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(sb->frame), GTK_SHADOW_IN);
    gtk_widget_show(sb->frame);

    sb->button = gtk_toggle_button_new();
    gtk_button_set_relief(GTK_BUTTON(sb->button), GTK_RELIEF_HALF);
    gtk_widget_set_size_request(sb->button, screen_button_width[settings.size],
                                -1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sb->button), sb->index == 0);
    gtk_container_add(GTK_CONTAINER(sb->frame), sb->button);
    gtk_widget_show(sb->button);

    /* we only know the screens names after reading the config file */
    sb->label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(sb->label), 0.1, 0.5);
    gtk_container_add(GTK_CONTAINER(sb->button), sb->label);
    gtk_widget_show(sb->label);

    /* signals */
    /* we need the callback id to be able to block the handler to 
       prevent a race condition when changing screens */
    sb->callback_id =
        g_signal_connect(sb->button, "clicked", G_CALLBACK(screen_button_click),
                         sb);

    g_signal_connect(sb->button, "button-press-event",
                     G_CALLBACK(screen_button_pressed_cb), sb);

    screen_button_set_style(sb, settings.style);

    return sb;
};

void screen_button_pack(ScreenButton * sb, GtkWidget * table)
{
    int pos = sb->index;

    if (pos % 2 == 0 || settings.size <= SMALL)
    {
	gtk_table_attach(GTK_TABLE(table), sb->frame, pos, pos + 1, 0, 1,
                         GTK_EXPAND, GTK_EXPAND, 0, 0);
    }
    else
    {
	gtk_table_attach(GTK_TABLE(table), sb->frame, pos - 1, pos, 1, 2,
                         GTK_EXPAND, GTK_EXPAND, 0, 0);
    }
}

void screen_button_free(ScreenButton * sb)
{
    g_free(screen_names[sb->index]);
    g_free(sb);
}

/*  Central panel
 *  -------------
*/
static void create_minibuttons(void)
{
    GdkPixbuf *pb[4];
    GtkWidget *im;
    GtkWidget *button;
    int i;

    pb[0] = get_system_pixbuf(MINILOCK_ICON);
    pb[1] = get_system_pixbuf(MINIINFO_ICON);
    pb[2] = get_system_pixbuf(MINIPALET_ICON);
    pb[3] = get_system_pixbuf(MINIPOWER_ICON);

    for(i = 0; i < 4; i++)
    {
        button = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
        gtk_widget_show(button);

        im = gtk_image_new_from_pixbuf(pb[i]);
        gtk_widget_show(im);
        g_object_unref(pb[i]);
        gtk_container_add(GTK_CONTAINER(button), im);

        minibuttons[i] = button;
    }

    /* fixed tooltips, since the user can't change the icon anyway */
    add_tooltip(minibuttons[0], _("Lock the screen"));
    add_tooltip(minibuttons[1], _("Info..."));
    add_tooltip(minibuttons[2], _("Setup..."));
    add_tooltip(minibuttons[3], _("Exit"));

    /* signals */
    g_signal_connect(minibuttons[0], "clicked",
                     G_CALLBACK(mini_lock_cb), NULL);

    g_signal_connect(minibuttons[1], "clicked",
                     G_CALLBACK(mini_info_cb), NULL);

    g_signal_connect(minibuttons[2], "clicked",
                     G_CALLBACK(mini_palet_cb), NULL);

    g_signal_connect(minibuttons[3], "clicked",
                     G_CALLBACK(mini_power_cb), NULL);
}

static void add_mini_table(int side, GtkBox * hbox)
{
    GtkWidget *table;
    int first;

    mini_tables[side] = gtk_table_new(2, 2, FALSE);

    if (settings.show_minibuttons)
	gtk_widget_show(mini_tables[side]);

    if(side == LEFT)
        first = 0;
    else
        first = 2;

    if (settings.size > SMALL)
    {
	gtk_table_attach(GTK_TABLE(mini_tables[side]), minibuttons[first],
			 0, 1, 0, 1, GTK_SHRINK, GTK_EXPAND, 0, 0);
	gtk_table_attach(GTK_TABLE(mini_tables[side]), minibuttons[first + 1],
			 0, 1, 1, 2, GTK_SHRINK, GTK_EXPAND, 0, 0);

	gtk_box_pack_start(hbox, mini_tables[side], TRUE, TRUE, 0);
    }
    else
    {
	gtk_table_attach(GTK_TABLE(mini_tables[side]), minibuttons[first],
			 0, 1, 0, 1, GTK_SHRINK, GTK_EXPAND, 0, 0);
	gtk_table_attach(GTK_TABLE(mini_tables[side]), minibuttons[first + 1],
			 1, 2, 0, 1, GTK_SHRINK, GTK_EXPAND, 0, 0);

	gtk_box_pack_start(hbox, mini_tables[side], TRUE, TRUE, 0);
    }
}

static void add_desktop_table(GtkBox * hbox)
{
    GtkWidget *table;
    int i;

    table = gtk_table_new(2, NBSCREENS, FALSE);

    if(settings.show_desktop_buttons)
        gtk_widget_show(table);

    desktop_table = table;

    for(i = 0; i < settings.num_screens; i++)
    {
        ScreenButton *sb = screen_buttons[i];

        screen_button_pack(sb, table);
    }

    gtk_box_pack_start(hbox, table, TRUE, TRUE, 0);
}

void central_panel_init(GtkBox *hbox)
{
    int i;

    init_screen_names();

    create_minibuttons();
    

    for(i = 0; i < NBSCREENS; i++)
    {
	if (i < settings.num_screens)
	    screen_buttons[i] = create_screen_button(i);
	else
	    screen_buttons[i] = NULL;
    }
    
    add_mini_table(LEFT, hbox);
    add_desktop_table(hbox);
    add_mini_table(RIGHT, hbox);
}

void central_panel_set_from_xml(xmlNodePtr node)
{
    xmlNodePtr child = NULL;
    xmlChar *value;
    int i;

    if (node)
	child = node->children;

    for(i = 0; i < NBSCREENS; i++)
    {
        ScreenButton *sb = screen_buttons[i];

        if(!child || !xmlStrEqual(child->name, (const xmlChar *)"Screen"))
	    value = NULL;
	else
	    value = DATA(child);

        if(value)
            screen_names[i] = (char *)value;
        else
            screen_names[i] = get_default_screen_name(i);

	if (i < settings.num_screens)
	    gtk_label_set_text(GTK_LABEL(sb->label), screen_names[i]);
	
        if(child)
            child = child->next;
    }
}

void central_panel_write_xml(xmlNodePtr root)
{
    xmlNodePtr node;
    int i;

    node = xmlNewTextChild(root, NULL, "Central", NULL);

    for(i = 0; i < NBSCREENS; i++)
    {
        xmlNewTextChild(node, NULL, "Screen", screen_names[i]);
    }
}

void central_panel_cleanup()
{
    int i;

    for(i = 0; i < NBSCREENS; i++)
    {
	if (screen_buttons[i])
	    screen_button_free(screen_buttons[i]);
    }
}

static void reorder_minitable(int side, int size)
{
    GtkWidget *table = mini_tables[side];
    int n;
    GList *child;
    GtkTableChild *tc;
    int hpos, vpos;

    if(side == LEFT)
        n = 1;
    else
        n = 3;

    /* when size == SMALL put second button in second column first row
     * otherwise put second button in first column second row */
    for(child = GTK_TABLE(table)->children;
        child && child->data; child = child->next)
    {
        tc = (GtkTableChild *) child->data;

        if(tc->widget == minibuttons[n])
            break;
    }

    if(size == SMALL || size == TINY)
    {
        hpos = 1;
        vpos = 0;
    }
    else
    {
        hpos = 0;
        vpos = 1;
    }

    tc->left_attach = hpos;
    tc->right_attach = hpos + 1;
    tc->top_attach = vpos;
    tc->bottom_attach = vpos + 1;

}

static void reorder_desktop_table(int size)
{
    GtkWidget *table;
    GList *child;
    GtkTableChild *tc;
    int hpos, vpos;

    table = desktop_table;

    for(child = GTK_TABLE(table)->children; child && child->data;
        child = child->next)
    {
        ScreenButton *sb;
        int i;

        tc = (GtkTableChild *) child->data;

        for(i = 0; i < NBSCREENS; i++)
        {
            sb = screen_buttons[i];

            if(sb && tc->widget == sb->frame)
                break;
        }

        if((i % 2) == 0)
        {
            if(size <= SMALL)
            {
                hpos = i;
                vpos = 0;
            }
            else
            {
                hpos = i / 2;
                vpos = 0;
            }
        }
        else
        {
            if(size <= SMALL)
            {
                hpos = i;
                vpos = 0;
            }
            else
            {
                hpos = (i - 1) / 2;
                vpos = 1;
            }
        }

        tc->left_attach = hpos;
        tc->right_attach = hpos + 1;
        tc->top_attach = vpos;
        tc->bottom_attach = vpos + 1;
    }
}

void central_panel_set_size(int size)
{
    GdkPixbuf *pb[4];
    GtkWidget *im;
    GtkWidget *button;
    GtkRequisition req;
    int w, h, i;

    pb[0] = get_system_pixbuf(MINILOCK_ICON);
    pb[1] = get_system_pixbuf(MINIINFO_ICON);
    pb[2] = get_system_pixbuf(MINIPALET_ICON);
    pb[3] = get_system_pixbuf(MINIPOWER_ICON);

    for(i = 0; i < 4; i++)
    {
        button = minibuttons[i];

        im = gtk_bin_get_child(GTK_BIN(button));
        gtk_image_set_from_pixbuf(GTK_IMAGE(im), pb[i]);
        g_object_unref(pb[i]);
    }

    /* mini tables */
    reorder_minitable(LEFT, size);
    reorder_minitable(RIGHT, size);

    /* screen buttons */
    for(i = 0; i < NBSCREENS; i++)
    {
        ScreenButton *sb = screen_buttons[i];

	if (sb)
	    screen_button_set_size(sb, size);
    }

    /* desktop table */
    reorder_desktop_table(size);

    /* set all minibuttons to the same size */
    for(i = 0; i < 4; i++)
        gtk_widget_set_size_request(minibuttons[i], -1, -1);

    w = h = 0;

    for(i = 0; i < 4; i++)
    {
        gtk_widget_size_request(minibuttons[i], &req);

        if(req.width > w)
            w = req.width;
        if(req.height > h)
            h = req.height;
    }

    for(i = 0; i < 4; i++)
        gtk_widget_set_size_request(minibuttons[i], w, h);
}

void central_panel_set_style(int style)
{
    int i;

    for(i = 0; i < NBSCREENS; i++)
    {
        ScreenButton *sb = screen_buttons[i];

	if (sb)
	    screen_button_set_style(sb, style);
    }
}

void central_panel_set_theme(const char *theme)
{
    GdkPixbuf *pb[4];
    GtkWidget *im;
    GtkWidget *button;
    int i;

    pb[0] = get_system_pixbuf(MINILOCK_ICON);
    pb[1] = get_system_pixbuf(MINIINFO_ICON);
    pb[2] = get_system_pixbuf(MINIPALET_ICON);
    pb[3] = get_system_pixbuf(MINIPOWER_ICON);

    for(i = 0; i < 4; i++)
    {
        button = minibuttons[i];

        im = gtk_bin_get_child(GTK_BIN(button));
        gtk_image_set_from_pixbuf(GTK_IMAGE(im), pb[i]);
        g_object_unref(pb[i]);
    }
}

void central_panel_set_current(int n)
{
    int i;

    if(n < 0)
        return;

    if(n > NBSCREENS)
    {
        /* try to force our maximum number of desktops */
        request_net_number_of_desktops(NBSCREENS);
        request_net_current_desktop(NBSCREENS);
        return;
    }

    if(n > settings.num_screens)
        central_panel_set_num_screens(n);

    for(i = 0; i < NBSCREENS; i++)
    {
        ScreenButton *sb = screen_buttons[i];

	if (!sb)
	    continue;
	
        /* Prevent signal handler to run; we're being called
         * as a reaction to a change by another program
         */
        g_signal_handler_block(sb->button, sb->callback_id);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sb->button), i == n);
        g_signal_handler_unblock(sb->button, sb->callback_id);
    }
}

void central_panel_set_num_screens(int n)
{
    int i;

    for(i = 0; i < NBSCREENS; i++)
    {
        ScreenButton *sb = screen_buttons[i];

	if (!sb)
	{
	    sb = create_screen_button(i);
	    screen_button_pack(sb, desktop_table);
	    gtk_label_set_text(GTK_LABEL(sb->label), screen_names[i]);
	}
	
        if(i < n)
            gtk_widget_show(sb->frame);
        else
            gtk_widget_hide(sb->frame);
    }

    if(n == 1)
        gtk_widget_hide(desktop_table);
    else if(settings.show_desktop_buttons)
        gtk_widget_show(desktop_table);
}

void central_panel_set_show_desktop_buttons(gboolean show)
{
    if(show)
        gtk_widget_show(desktop_table);
    else
        gtk_widget_hide(desktop_table);
}

void central_panel_set_show_minibuttons(gboolean show)
{
    if(show)
    {
        gtk_widget_show(mini_tables[LEFT]);
        gtk_widget_show(mini_tables[RIGHT]);
    }
    else
    {
        gtk_widget_hide(mini_tables[LEFT]);
        gtk_widget_hide(mini_tables[RIGHT]);
    }
}


