/****************************************************************************
 * This module is based on Twm, but has been siginificantly modified
 * by Rob Nation
 ****************************************************************************/
/*****************************************************************************/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**  Portions Copyright 1989 by the Massachusetts Institute of Technology   **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    names of Evans & Sutherland and M.I.T. not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND AND M.I.T. DISCLAIM ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND OR    **/
/**    M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/**********************************************************************
 *
 * Add a new window, put the titlbar and other stuff around
 * the window
 *
 **********************************************************************/
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fvwm.h"
#include "style.h"
#include <X11/Xatom.h>
#include "screen.h"
#include "misc.h"
#include "bindings.h"
#include "borders.h"
#ifdef SHAPE
#include <X11/extensions/shape.h>
#include <X11/Xresource.h>
#endif /* SHAPE */
#include "module_interface.h"
#include "session.h"
#include "stack.h"
#include "move_resize.h"
#include "colormaps.h"
#include "gnome.h"
#include "placement.h"
#include "builtins.h"
#include "decorations.h"
#include "libs/Colorset.h"
#include "style.h"

/* Used to parse command line of clients for specific desk requests. */
/* Todo: check for multiple desks. */
static XrmDatabase db;
static XrmOptionDescRec table [] = {
  /* Want to accept "-workspace N" or -xrm "fvwm*desk:N" as options
   * to specify the desktop. I have to include dummy options that
   * are meaningless since Xrm seems to allow -w to match -workspace
   * if there would be no ambiguity. */
    {"-workspacf",      "*junk",        XrmoptionSepArg, (caddr_t) NULL},
    {"-workspace",	"*desk",	XrmoptionSepArg, (caddr_t) NULL},
    {"-xrn",		NULL,		XrmoptionResArg, (caddr_t) NULL},
    {"-xrm",		NULL,		XrmoptionResArg, (caddr_t) NULL},
};

void FetchWmProtocols(FvwmWindow *);
void GetWindowSizeHints(FvwmWindow *);

/***********************************************************************/

Bool setup_window_structure(
  FvwmWindow **ptmp_win, Window w, FvwmWindow *ReuseWin)
{
  FvwmWindow save_state;
  FvwmWindow *savewin = NULL;

  memset(&save_state, '\0', sizeof(FvwmWindow));

  /*
      Allocate space for the FvwmWindow struct, or reuse an
      old one (on Recapture).
  */
  if (ReuseWin == NULL)
    {
      *ptmp_win = (FvwmWindow *)safemalloc(sizeof(FvwmWindow));
      if (*ptmp_win == (FvwmWindow *)0)
        {
          return False;
        }
    }
  else
    {
      *ptmp_win = ReuseWin;
      savewin = &save_state;
      memcpy(savewin, ReuseWin, sizeof(FvwmWindow));
    }

  /*
    RBW - 1999/05/28 - modify this when we implement the preserving of
    various states across a Recapture. The Destroy function in misc.c may
    also need tweaking, depending on what you want to preserve.
    For now, just zap any old information, except the desk.
  */
  memset(*ptmp_win, '\0', sizeof(FvwmWindow));
  (*ptmp_win)->w = w;
  if (savewin != NULL)
  {
    (*ptmp_win)->Desk = savewin->Desk;
  }

  (*ptmp_win)->cmap_windows = (Window *)NULL;
#ifdef MINI_ICONS
  (*ptmp_win)->mini_pixmap_file = NULL;
  (*ptmp_win)->mini_icon = NULL;
#endif

  return True;
}

void setup_window_name(FvwmWindow *tmp_win)
{
  XTextProperty text_prop;

  if ( XGetWMName(dpy, tmp_win->w, &text_prop) != 0 )
#ifdef I18N_MB
  {
    if (text_prop.value) {
      if (text_prop.encoding == XA_STRING) {
        /* STRING encoding, use this as it is */
        tmp_win->name = (char *)text_prop.value;
        tmp_win->name_list = NULL;
      } else {
        /* not STRING encoding, try to convert */
        if (XmbTextPropertyToTextList(dpy, &text_prop, &list, &num) >= Success
            && num > 0 && *list) {
          /* XXX: does not consider the conversion is REALLY succeeded */
          XFree(text_prop.value); /* return of XGetWMName() */
          tmp_win->name = *list;
          tmp_win->name_list = list;
        } else {
          if (list) XFreeStringList(list);
          XFree(text_prop.value); /* return of XGetWMName() */
          XGetWMName(dpy, tmp_win->w, &text_prop); /* XXX: read again ? */
          tmp_win->name = (char *)text_prop.value;
          tmp_win->name_list = NULL;
        }
      }
    } else {
      tmp_win->name = NoName;
    }
  }
#else
    tmp_win->name = (char *)text_prop.value;
#endif
  else
    tmp_win->name = NoName;
}

void setup_class_and_resource(FvwmWindow *tmp_win)
{
  /* removing NoClass change for now... */
  tmp_win->class.res_name = NoResource;
  tmp_win->class.res_class = NoClass;
  XGetClassHint(dpy, tmp_win->w, &tmp_win->class);
  if (tmp_win->class.res_name == NULL)
    tmp_win->class.res_name = NoResource;
  if (tmp_win->class.res_class == NULL)
    tmp_win->class.res_class = NoClass;

  FetchWmProtocols (tmp_win);
  FetchWmColormapWindows (tmp_win);
  if(!(XGetWindowAttributes(dpy,tmp_win->w,&(tmp_win->attr))))
    tmp_win->attr.colormap = Pcmap;
}

void setup_wm_hints(FvwmWindow *tmp_win)
{
  tmp_win->wmhints = XGetWMHints(dpy, tmp_win->w);
}

void setup_style_and_decor(FvwmWindow *tmp_win, window_style *pstyle)
{
#ifdef SHAPE
  if (ShapesSupported)
  {
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;

    XShapeSelectInput (dpy, tmp_win->w, ShapeNotifyMask);
    XShapeQueryExtents (dpy, tmp_win->w,
			&boundingShaped, &xws, &yws, &wws, &hws,
			&clipShaped, &xbs, &ybs, &wbs, &hbs);
    tmp_win->wShaped = boundingShaped;
  }
#endif /* SHAPE */

  /*  Assume that we'll decorate */
  SET_HAS_BORDER(tmp_win, 1);
  SET_HAS_TITLE(tmp_win, 1);

#ifdef USEDECOR
  /* search for a UseDecor tag in the style */
  tmp_win->fl = NULL;
  if (SGET_DECOR_NAME(*pstyle) != NULL)
  {
    FvwmDecor *fl = &Scr.DefaultDecor;

    for (; fl; fl = fl->next)
    {
      if (strcasecmp(SGET_DECOR_NAME(*pstyle), fl->tag) == 0)
      {
	tmp_win->fl = fl;
	break;
      }
    }
  }
  if (tmp_win->fl == NULL)
    tmp_win->fl = &Scr.DefaultDecor;
#endif

  tmp_win->title_g.height = GetDecor(tmp_win,TitleHeight);

  GetMwmHints(tmp_win);
  GetOlHints(tmp_win);

  SelectDecor(tmp_win, &pstyle->flags, SGET_BORDER_WIDTH(*pstyle),
	      SGET_HANDLE_WIDTH(*pstyle));

#ifdef SHAPE
  /* set boundary width to zero for shaped windows */
  if (tmp_win->wShaped)
    tmp_win->boundary_width = 0;
#endif /* SHAPE */
}

void setup_icon_boxes(FvwmWindow *tmp_win, window_style *pstyle)
{
  /* copy iconboxes ptr (if any) */
  if (SHAS_ICON_BOXES(&pstyle->flags))
    tmp_win->IconBoxes = SGET_ICON_BOXES(*pstyle);
  else
    tmp_win->IconBoxes = NULL;
}

void setup_layer(FvwmWindow *tmp_win, window_style *pstyle)
{
  /* FIXME: shouldn't transients inherit the layer ? */
  if (SUSE_LAYER(&pstyle->flags))
  {
    set_default_layer(tmp_win, SGET_LAYER(*pstyle));
    set_layer(tmp_win, SGET_LAYER(*pstyle));
  }
  else
  {
    set_default_layer(tmp_win, Scr.DefaultLayer);
    set_layer(tmp_win, Scr.DefaultLayer);
  }
}

void setup_frame_size(FvwmWindow *tmp_win, window_style *pstyle)
{
  GetWindowSizeHints (tmp_win);

  /* Tentative size estimate */
  tmp_win->frame_g.width = tmp_win->attr.width+2*tmp_win->boundary_width;
  tmp_win->frame_g.height = tmp_win->attr.height + tmp_win->title_g.height+
    2*tmp_win->boundary_width;

  if (SHAS_MAX_WINDOW_SIZE(&pstyle->flags))
  {
    tmp_win->max_window_width = SGET_MAX_WINDOW_WIDTH(*pstyle);
    tmp_win->max_window_height = SGET_MAX_WINDOW_HEIGHT(*pstyle);
  }
  else
  {
    tmp_win->max_window_width = DEFAULT_MAX_MAX_WINDOW_WIDTH;
    tmp_win->max_window_height = DEFAULT_MAX_MAX_WINDOW_HEIGHT;
  }
  ConstrainSize(tmp_win, &tmp_win->frame_g.width, &tmp_win->frame_g.height,
		0, 0, False);
}

int setup_window_placement(FvwmWindow *tmp_win, window_style *pstyle)
{
  int client_argc;
  char **client_argv = NULL;
  char *str_type;
  Bool status;
  XrmValue rm_value;
/*  RBW - 11/02/1998  */
  int tmpno1 = -1, tmpno2 = -1, tmpno3 = -1, spargs = 0;
/**/

  /* Find out if the client requested a specific desk on the command line. */
  /*  RBW - 11/20/1998 - allow a desk of -1 to work.  */
  if (XGetCommand (dpy, tmp_win->w, &client_argv, &client_argc))
  {
    XrmParseCommand(&db, table, 4, "fvwm", &client_argc, client_argv);
    XFreeStringList(client_argv);
    status = XrmGetResource(db, "fvwm.desk", "Fvwm.Desk",
			    &str_type, &rm_value);
    if ((status == True) && (rm_value.size != 0))
    {
      SGET_START_DESK(*pstyle) = atoi(rm_value.addr);
      /*  RBW - 11/20/1998  */
      if (SGET_START_DESK(*pstyle) > -1)
      {
	SSET_START_DESK(*pstyle, SGET_START_DESK(*pstyle) + 1);
      }
      pstyle->flags.use_start_on_desk = 1;
    }
/*  RBW - 11/02/1998  */
/*  RBW - 11/20/1998 - allow desk or page specs of -1 to work.  */
    /*  Handle the X Resource equivalent of StartsOnPage.  */
    status = XrmGetResource (db, "fvwm.page", "Fvwm.Page", &str_type,
			     &rm_value);
    if ((status == True) && (rm_value.size != 0))
    {
      spargs = sscanf (rm_value.addr, "%d %d %d", &tmpno1, &tmpno2, &tmpno3);
      switch (spargs)
      {
      case 1:
	pstyle->flags.use_start_on_desk = 1;
	SSET_START_DESK(*pstyle, (tmpno1 > -1) ? tmpno1 + 1 : tmpno1);
	break;
      case 2:
	pstyle->flags.use_start_on_desk = 1;
	SSET_START_PAGE_X(*pstyle, (tmpno1 > -1) ? tmpno1 + 1 : tmpno1);
	SSET_START_PAGE_Y(*pstyle, (tmpno2 > -1) ? tmpno2 + 1 : tmpno2);
	break;
      case 3:
	pstyle->flags.use_start_on_desk = 1;
	SSET_START_DESK(*pstyle, (tmpno1 > -1) ? tmpno1 + 1 : tmpno1);
	SSET_START_PAGE_X(*pstyle, (tmpno2 > -1) ? tmpno2 + 1 : tmpno2);
	SSET_START_PAGE_Y(*pstyle, (tmpno3 > -1) ? tmpno3 + 1 : tmpno3);
	break;
      default:
	break;
      }
    }

    XrmDestroyDatabase (db);
    db = NULL;
  }

  return PlaceWindow(tmp_win, &pstyle->flags, SGET_START_DESK(*pstyle),
		     SGET_START_PAGE_X(*pstyle), SGET_START_PAGE_Y(*pstyle));
}

void get_default_window_background(
  FvwmWindow *tmp_win, unsigned long *pvaluemask,
  XSetWindowAttributes *pattributes)
{
  if(Pdepth < 2)
  {
    *pvaluemask = CWBackPixmap;
    if(IS_STICKY(tmp_win))
      pattributes->background_pixmap = Scr.sticky_gray_pixmap;
    else
      pattributes->background_pixmap = Scr.light_gray_pixmap;
  }
  else
  {
    *pvaluemask = CWBackPixel;
    pattributes->background_pixel = tmp_win->BackPixel;
    pattributes->background_pixmap = None;
  }
}

void setup_frame_window(
  FvwmWindow *tmp_win, int valuemask, XSetWindowAttributes *pattributes)
{
#if defined(PIXMAP_BUTTONS) && defined(BORDERSTYLE)
  Pixmap TexturePixmap = None;
  Pixmap TexturePixmapSave;

  /* stash valuemask bits in case BorderStyle TiledPixmap overwrites */
  TexturePixmapSave = pattributes->background_pixmap;
#endif

  valuemask |= CWCursor|CWColormap|CWBorderPixel|CWEventMask;
  pattributes->cursor = Scr.FvwmCursors[CRS_DEFAULT];
  pattributes->colormap = Pcmap;
  pattributes->border_pixel = 0;
  pattributes->event_mask = (SubstructureRedirectMask | ButtonPressMask
			     | ButtonReleaseMask | EnterWindowMask
			     | LeaveWindowMask | ExposureMask
			     | VisibilityChangeMask);

#if defined(PIXMAP_BUTTONS) && defined(BORDERSTYLE)
  if (DFS_FACE_TYPE(GetDecor(tmp_win, BorderStyle.inactive.style)) ==
      TiledPixmapButton)
    TexturePixmap = GetDecor(tmp_win,BorderStyle.inactive.u.p->picture);
  if (TexturePixmap)
  {
    pattributes->background_pixmap = TexturePixmap;
    valuemask = (valuemask & ~CWBackPixel) | CWBackPixmap;
  }
#endif

  /* create the frame window, child of root, grandparent of client */
  tmp_win->frame = XCreateWindow(dpy, Scr.Root, tmp_win->frame_g.x,
				 tmp_win->frame_g.y, tmp_win->frame_g.width,
				 tmp_win->frame_g.height, 0, Pdepth,
				 InputOutput, Pvisual, valuemask, pattributes);
  XSaveContext(dpy, tmp_win->w, FvwmContext, (caddr_t) tmp_win);
  XSaveContext(dpy, tmp_win->frame, FvwmContext, (caddr_t) tmp_win);

#if defined(PIXMAP_BUTTONS) && defined(BORDERSTYLE)
  /* restore background */
  pattributes->background_pixmap = TexturePixmapSave;
#endif
}

void setup_title_window(
  FvwmWindow *tmp_win, int valuemask, XSetWindowAttributes *pattributes)
{
  valuemask |= CWCursor|CWColormap|CWBorderPixel|CWEventMask;
  pattributes->cursor = Scr.FvwmCursors[CRS_DEFAULT];
  pattributes->colormap = Pcmap;
  pattributes->border_pixel = 0;
  pattributes->event_mask = (ButtonPressMask | ButtonReleaseMask
			     | EnterWindowMask | LeaveWindowMask
			     | ExposureMask);

  tmp_win->title_g.x = tmp_win->boundary_width + tmp_win->title_g.height + 1;
  tmp_win->title_g.y = tmp_win->boundary_width;
  pattributes->cursor = Scr.FvwmCursors[CRS_TITLE];
  tmp_win->title_w = XCreateWindow(dpy, tmp_win->frame,
				   tmp_win->title_g.x,
				   tmp_win->title_g.y,
				   tmp_win->title_g.width,
				   tmp_win->title_g.height, 0,
				   CopyFromParent,
				   InputOutput, CopyFromParent, valuemask,
				   pattributes);
  XSaveContext(dpy, tmp_win->title_w, FvwmContext, (caddr_t) tmp_win);
}

void setup_button_windows(
  FvwmWindow *tmp_win, int valuemask, XSetWindowAttributes *pattributes)
{
  int i;

  /* restore valuemask to remember background */
  valuemask |= CWCursor|CWColormap|CWBorderPixel|CWEventMask;
  pattributes->cursor = Scr.FvwmCursors[CRS_SYS];
  pattributes->colormap = Pcmap;
  pattributes->border_pixel = 0;
  pattributes->event_mask = (ButtonPressMask | ButtonReleaseMask
			   | EnterWindowMask | LeaveWindowMask
			   | ExposureMask);

  for(i = 4; i >= 0; i--)
  {
    if((i < Scr.nr_left_buttons) && (tmp_win->left_w[i] > 0))
    {
      tmp_win->left_w[i] = XCreateWindow(dpy, tmp_win->frame,
					 tmp_win->title_g.height * i, 0,
					 tmp_win->title_g.height,
					 tmp_win->title_g.height, 0,
					 CopyFromParent, InputOutput,
					 CopyFromParent, valuemask,
					 pattributes);
      XSaveContext(dpy, tmp_win->left_w[i], FvwmContext, (caddr_t) tmp_win);
    }
    else
    {
      tmp_win->left_w[i] = None;
    }

    if((i < Scr.nr_right_buttons) && (tmp_win->right_w[i] > 0))
    {
      tmp_win->right_w[i] = XCreateWindow(dpy, tmp_win->frame,
					  tmp_win->title_g.width
					  - tmp_win->title_g.height * (i+1),
					  0, tmp_win->title_g.height,
					  tmp_win->title_g.height, 0,
					  CopyFromParent, InputOutput,
					  CopyFromParent, valuemask,
					  pattributes);
      XSaveContext(dpy, tmp_win->right_w[i], FvwmContext, (caddr_t) tmp_win);
    }
    else
    {
      tmp_win->right_w[i] = None;
    }
  }
}

void setup_parent_window(FvwmWindow *tmp_win)
{
  unsigned long valuemask;
  XSetWindowAttributes attributes;

  /* make sure this does not have a BackPixel or BackPixmap so that
     that when the client dies there is no flash of BackPixel/BackPixmap */
  /* may look odd with shaped windows if fvwm has shapes disabled */
  valuemask = CWBackingStore | CWBackPixmap | CWBorderPixel | CWColormap
	      | CWCursor | CWEventMask;
  attributes.backing_store = NotUseful;
  attributes.background_pixmap = None;
  attributes.border_pixel = 0;
  attributes.colormap = Pcmap;
  attributes.cursor = Scr.FvwmCursors[CRS_DEFAULT];
  attributes.event_mask = SubstructureRedirectMask;
  tmp_win->Parent = XCreateWindow(
    dpy, tmp_win->frame, tmp_win->boundary_width,
    tmp_win->boundary_width + tmp_win->title_g.height,
    (tmp_win->frame_g.width - 2 * tmp_win->boundary_width),
    (tmp_win->frame_g.height - 2 * tmp_win->boundary_width -
     tmp_win->title_g.height),
    0, CopyFromParent, InputOutput, CopyFromParent, valuemask, &attributes);

  XSaveContext(dpy, tmp_win->Parent, FvwmContext, (caddr_t) tmp_win);
}

void setup_resize_handle_windows(FvwmWindow *tmp_win)
{
  unsigned long valuemask;
  XSetWindowAttributes attributes;
  int i;

  /* sides and corners are input only */
  /* title and buttons maybe one day */
  valuemask = CWCursor | CWEventMask;
  attributes.event_mask = (ButtonPressMask | ButtonReleaseMask
			   | EnterWindowMask | LeaveWindowMask);
  if(HAS_BORDER(tmp_win))
  {
    /* Just dump the windows any old place and let SetupFrame take
     * care of the mess */
    for(i = 0; i < 4; i++)
    {
      attributes.cursor = Scr.FvwmCursors[CRS_TOP_LEFT+i];
      tmp_win->corners[i] = XCreateWindow (dpy, tmp_win->frame, 0, 0,
					   tmp_win->corner_width,
					   tmp_win->corner_width, 0, 0,
					   InputOnly,
					   DefaultVisual(dpy, Scr.screen),
					   valuemask, &attributes);
      XLowerWindow(dpy, tmp_win->corners[i]);

      attributes.cursor = Scr.FvwmCursors[CRS_TOP+i];
      tmp_win->sides[i] = XCreateWindow (dpy, tmp_win->frame, 0, 0,
					 tmp_win->boundary_width,
					 tmp_win->boundary_width, 0, 0,
					 InputOnly,
					 DefaultVisual(dpy, Scr.screen),
					 valuemask, &attributes);
      XSaveContext(dpy, tmp_win->sides[i], FvwmContext, (caddr_t) tmp_win);
      XSaveContext(dpy, tmp_win->corners[i],FvwmContext, (caddr_t) tmp_win);
    }
  }
}

void setup_icon(FvwmWindow *tmp_win, window_style *pstyle)
{
  XTextProperty text_prop;

  /* find a suitable icon pixmap */
  if((tmp_win->wmhints) && (tmp_win->wmhints->flags & IconWindowHint))
  {
    if (SHAS_ICON(&pstyle->flags) &&
	SICON_OVERRIDE(&pstyle->flags) == ICON_OVERRIDE)
      tmp_win->icon_bitmap_file = SGET_ICON_NAME(*pstyle);
    else
      tmp_win->icon_bitmap_file = NULL;
 }
  else if((tmp_win->wmhints) && (tmp_win->wmhints->flags & IconPixmapHint))
  {
    if (SHAS_ICON(&pstyle->flags) &&
	SICON_OVERRIDE(&pstyle->flags) != NO_ICON_OVERRIDE)
      tmp_win->icon_bitmap_file = SGET_ICON_NAME(*pstyle);
    else
      tmp_win->icon_bitmap_file = NULL;
  }
  else if(SHAS_ICON(&pstyle->flags))
  {
    /* an icon was specified */
    tmp_win->icon_bitmap_file = SGET_ICON_NAME(*pstyle);
  }
  else
  {
    /* use default icon */
    tmp_win->icon_bitmap_file = Scr.DefaultIcon;
  }

  /* icon name */
  if (XGetWMIconName(dpy, tmp_win->w, &text_prop))
  {
    tmp_win->icon_name = (char *)text_prop.value;
  }
  else
  {
    tmp_win->icon_name = NULL;
  }
  if(tmp_win->icon_name==(char *)NULL)
    tmp_win->icon_name = tmp_win->name;

  /* wait until the window is iconified and the icon window is mapped
   * before creating the icon window
   */
  tmp_win->icon_w = None;
}

#ifdef MINI_ICONS
void setup_mini_icon(FvwmWindow *tmp_win, window_style *pstyle)
{
  if (SHAS_MINI_ICON(&pstyle->flags))
  {
    tmp_win->mini_pixmap_file = SGET_MINI_ICON_NAME(*pstyle);
  }
  else
  {
    tmp_win->mini_pixmap_file = NULL;
  }

  if (tmp_win->mini_pixmap_file)
  {
    tmp_win->mini_icon = CachePicture(dpy, Scr.NoFocusWin, NULL,
				      tmp_win->mini_pixmap_file,
				      Scr.ColorLimit);
  }
  else
  {
    tmp_win->mini_icon = NULL;
  }
}
#endif

void setup_focus_policy(FvwmWindow *tmp_win)
{
  if(HAS_CLICK_FOCUS(tmp_win) || Scr.go.MouseFocusClickRaises)
  {
    int button;

    /* need to grab all buttons for window */
    for(button = 1; button <= 3; button++)
    {
      XGrabButton(dpy, button, 0, tmp_win->frame, True, ButtonPressMask,
		  GrabModeSync, GrabModeAsync, None,
		  Scr.FvwmCursors[CRS_SYS]);
      if(GetUnusedModifiers() != 0)
      {
	register unsigned int mods;
	register unsigned int max = GetUnusedModifiers();
	register unsigned int living_modifiers = ~max;

	/* handle all bindings for the dead modifiers */
	for (mods = 1; mods <= max; mods++)
	{
	  /* Since mods starts with 1 we don't need to test if mods
	   * contains a dead modifier. Otherwise both, dead and living
	   * modifiers would be zero ==> mods == 0 */
	  if (mods & living_modifiers)
	    continue;
	  XGrabButton(dpy, button, mods, tmp_win->frame, True,
		      ButtonPressMask, GrabModeSync, GrabModeAsync, None,
		      Scr.FvwmCursors[CRS_SYS]);
	}
      } /* if */
    } /* for */
  } /* if */
}

/***********************************************************************
 *
 *  Procedure:
 *	AddWindow - add a new window to the fvwm list
 *
 *  Returned Value:
 *	(FvwmWindow *) - pointer to the FvwmWindow structure
 *
 *  Inputs:
 *	w	- the window id of the window to add
 *	iconm	- flag to tell if this is an icon manager window
 *
 ***********************************************************************/
FvwmWindow *AddWindow(Window w, FvwmWindow *ReuseWin)
{
  /* new fvwm window structure */
  register FvwmWindow *tmp_win = NULL;
  FvwmWindow *tmptmp_win = NULL;
  /* mask for create windows */
  unsigned long valuemask;
  unsigned long valuemask_save = 0;
  /* attributes for create windows */
  XSetWindowAttributes attributes;
  /* area for merged styles */
  window_style style;
  /* used for faster access */
  style_flags *sflags;
  int width,height;
  int a,b;
  extern FvwmWindow *colormap_win;
  extern Boolean PPosOverride;
#ifdef I18N_MB
  char **list;
  int num;
#endif
  int do_shade = 0;
  int do_maximize = 0;
  int x_max, y_max;
  unsigned int w_max, h_max;
  Bool do_resize_too = False;

  /****** init window structure ******/

  if (!setup_window_structure(&tmptmp_win, w, ReuseWin))
    return NULL;
  tmp_win = tmptmp_win;

  /****** safety check ******/

  if(!PPosOverride &&
     XGetGeometry(dpy, tmp_win->w, &JunkRoot, &JunkX, &JunkY,
		  &JunkWidth, &JunkHeight, &JunkBW, &JunkDepth) == 0)
  {
    free((char *)tmp_win);
    return NULL;
  }

  /****** window name ******/

  setup_window_name(tmp_win);

  /****** window class and resource ******/

  setup_class_and_resource(tmp_win);

  /****** WM hints ******/

  setup_wm_hints(tmp_win);

  /****** transient ******/

  SET_TRANSIENT(tmp_win, !!XGetTransientForHint(dpy, tmp_win->w,
						&tmp_win->transientfor));

  /****** old border width ******/

  tmp_win->old_bw = tmp_win->attr.border_width;

  /****** basic style and decor ******/

  /* If the window is in the NoTitle list, or is a transient, dont decorate it.
   * If its a transient, and DecorateTransients was specified, decorate anyway.
   */

  /* get merged styles */
  lookup_style(tmp_win, &style);
  sflags = SGET_FLAGS_POINTER(style);
  setup_style_and_decor(tmp_win, &style);
  memcpy(&(FW_COMMON_FLAGS(tmp_win)), &(sflags->common),
	 sizeof(common_flags_type));

  /****** icon boxes ******/

  setup_icon_boxes(tmp_win, &style);

  /****** disabled buttons ******/

  tmp_win->buttons = SIS_BUTTON_DISABLED(sflags);

  /****** layer ******/

  setup_layer(tmp_win, &style);

  /****** calculate frame size ******/

  setup_frame_size(tmp_win, &style);

  /****** window placement ******/

  switch (setup_window_placement(tmp_win, &style))
  {
  case 0:
    /* failed */
    return NULL;
  case 1:
    /* ok */
    do_resize_too = False;
    break;
  case 2:
  default:
    /* resize window too */
    do_resize_too = True;
    break;
  }

  /****** safety check ******/

  /*
   * Make sure the client window still exists.  We don't want to leave an
   * orphan frame window if it doesn't.  Since we now have the server
   * grabbed, the window can't disappear later without having been
   * reparented, so we'll get a DestroyNotify for it.  We won't have
   * gotten one for anything up to here, however.
   */
  MyXGrabServer(dpy);
  if(XGetGeometry(dpy, w, &JunkRoot, &JunkX, &JunkY,
		  &JunkWidth, &JunkHeight,
		  &JunkBW,  &JunkDepth) == 0)
  {
    free((char *)tmp_win);
    MyXUngrabServer(dpy);
    return NULL;
  }

  /****** border width ******/

  XSetWindowBorderWidth (dpy, tmp_win->w,0);

  /******  ******/

  SET_ICONIFIED(tmp_win, 0);
  SET_ICON_UNMAPPED(tmp_win, 0);
  SET_MAXIMIZED(tmp_win, 0);

  /****** window colors ******/

  update_window_color_style(tmp_win, &style);

  /****** window list ******/

  /* add the window to the end of the fvwm list */
  tmp_win->next = Scr.FvwmRoot.next;
  tmp_win->prev = &Scr.FvwmRoot;
  while (tmp_win->next != NULL)
  {
    tmp_win->prev = tmp_win->next;
    tmp_win->next = tmp_win->next->next;
  }
  /* tmp_win->prev points to the last window in the list, tmp_win->next is
   * NULL. Now fix the last window to point to tmp_win */
  tmp_win->prev->next = tmp_win;

  /****** stacking order ******/

  /*
      RBW - 11/13/1998 - add it into the stacking order chain also.
      This chain is anchored at both ends on Scr.FvwmRoot, there are
      no null pointers.
  */
  add_window_to_stack_ring_after(tmp_win, &Scr.FvwmRoot);

  /****** session management ******/

  /*
      MatchWinToSM changes tmp_win->attr and the stacking order.
      Thus it is important have this call *after* PlaceWindow and the
      stacking order initialization.
  */
  MatchWinToSM(tmp_win, &x_max, &y_max, &w_max, &h_max, &do_shade,
	       &do_maximize);

  /****** geometry ******/

  /* set up geometry */
  tmp_win->frame_g.x = tmp_win->attr.x + tmp_win->old_bw;
  tmp_win->frame_g.y = tmp_win->attr.y + tmp_win->old_bw;
  tmp_win->frame_g.width = tmp_win->attr.width+2*tmp_win->boundary_width;
  tmp_win->frame_g.height = tmp_win->attr.height + tmp_win->title_g.height
			  + 2 * tmp_win->boundary_width;
  ConstrainSize(tmp_win, &tmp_win->frame_g.width, &tmp_win->frame_g.height,
		0, 0, False);
  tmp_win->title_g.x = tmp_win->title_g.y = 0;
  tmp_win->title_w = 0;
  tmp_win->title_g.width = tmp_win->frame_g.width - 2*tmp_win->boundary_width;
  if(tmp_win->title_g.width < 1)
    tmp_win->title_g.width = 1;

  /****** create windows ******/

  get_default_window_background(tmp_win, &valuemask_save, &attributes);

  /****** frame window ******/

  setup_frame_window(tmp_win, valuemask_save, &attributes);

  /****** title window ******/

  if (HAS_TITLE(tmp_win))
  {
    setup_title_window(tmp_win, valuemask_save, &attributes);
  }

  /****** button windows ******/

  if (HAS_TITLE(tmp_win))
  {
    setup_button_windows(tmp_win, valuemask_save, &attributes);
  }

  /****** resize handle windows ******/

  setup_resize_handle_windows(tmp_win);

  /****** parent of the client window ******/

  setup_parent_window(tmp_win);

  /****** icon ******/

  setup_icon(tmp_win, &style);

  /****** mini icon ******/

#ifdef MINI_ICONS
  setup_mini_icon(tmp_win, &style);
#endif

  /******  ******/

  XMapSubwindows (dpy, tmp_win->frame);
  XRaiseWindow(dpy, tmp_win->Parent);
  XReparentWindow(dpy, tmp_win->w, tmp_win->Parent, 0, 0);

  /******  ******/

  valuemask = CWEventMask | CWDontPropagate;
  attributes.event_mask = (StructureNotifyMask | PropertyChangeMask |
			   EnterWindowMask | LeaveWindowMask |
			   ColormapChangeMask | FocusChangeMask);

  attributes.do_not_propagate_mask = ButtonPressMask | ButtonReleaseMask;

  XChangeWindowAttributes(dpy, tmp_win->w, valuemask, &attributes);

  /******  ******/

  XAddToSaveSet(dpy, tmp_win->w);

  /******  ******/

  /*
   * Reparenting generates an UnmapNotify event, followed by a MapNotify.
   * Set the map state to FALSE to prevent a transition back to
   * WithdrawnState in HandleUnmapNotify.  Map state gets set corrected
   * again in HandleMapNotify.
   */
  SET_MAPPED(tmp_win, 0);
  width = tmp_win->frame_g.width;
  tmp_win->frame_g.width = 0;
  height = tmp_win->frame_g.height;
  tmp_win->frame_g.height = 0;

  /******  ******/

  SetupFrame(tmp_win, tmp_win->frame_g.x, tmp_win->frame_g.y,width,height,
	     True, False);

  /****** maximize ******/

  if (do_maximize)
  {
    /* This is essentially Maximize, only we want the given dimensions */
    SET_MAXIMIZED(tmp_win, 1);
    ConstrainSize(tmp_win, &w_max, &h_max, 0, 0, False);
    tmp_win->maximized_ht = h_max;
    SetupFrame(tmp_win, x_max, y_max, w_max, h_max, TRUE, False);
    SetBorder(tmp_win, Scr.Hilite == tmp_win, True, True, None);
    /* fix orig values to not change page on unmaximize  */
    if (tmp_win->orig_g.x >= Scr.MyDisplayWidth)
      tmp_win->orig_g.x = tmp_win->orig_g.x % Scr.MyDisplayWidth;
    if (tmp_win->orig_g.y >= Scr.MyDisplayHeight)
      tmp_win->orig_g.y = tmp_win->orig_g.y % Scr.MyDisplayHeight;
  }

  /****** windowshade ******/

  if (do_shade)
  {
    WindowShade(&Event, tmp_win->w, tmp_win, C_WINDOW, "", 0);
  }

  /****** grab keys and buttons ******/

  GrabAllWindowButtons(dpy, tmp_win->frame, Scr.AllBindings, C_WINDOW,
		       GetUnusedModifiers(), Scr.FvwmCursors[CRS_DEFAULT],
		       True);
  GrabAllWindowKeys(dpy, tmp_win->frame, Scr.AllBindings,
		    C_WINDOW|C_TITLE|C_RALL|C_LALL|C_SIDEBAR,
		    GetUnusedModifiers(), True);

  /******  ******/

  if (!position_new_window_in_stack_ring(tmp_win, SDO_START_LOWERED(sflags)))
  {
    XWindowChanges xwc;
    xwc.sibling = get_next_window_in_stack_ring(tmp_win)->frame;
    xwc.stack_mode = Above;
    XConfigureWindow(dpy, tmp_win->frame, CWSibling|CWStackMode, &xwc);
  }

  /******  ******/

  MyXUngrabServer(dpy);

  /****** store window offset caused by decorating ******/

  XGetGeometry(dpy, tmp_win->w, &JunkRoot, &JunkX, &JunkY,
		   &JunkWidth, &JunkHeight, &JunkBW, &JunkDepth);
  XTranslateCoordinates(dpy,tmp_win->frame,Scr.Root,JunkX,JunkY,
			&a,&b,&JunkChild);
  tmp_win->xdiff -= a;
  tmp_win->ydiff -= b;

  /****** grab buttons for focus policy ******/

  setup_focus_policy(tmp_win);

  /****** inform modules of new window ******/

  BroadcastConfig(M_ADD_WINDOW,tmp_win);
  BroadcastName(M_WINDOW_NAME,tmp_win->w,tmp_win->frame,
		(unsigned long)tmp_win,tmp_win->name);
  BroadcastName(M_ICON_NAME,tmp_win->w,tmp_win->frame,
		(unsigned long)tmp_win,tmp_win->icon_name);
   if (tmp_win->icon_bitmap_file != NULL &&
       tmp_win->icon_bitmap_file != Scr.DefaultIcon)
     BroadcastName(M_ICON_FILE,tmp_win->w,tmp_win->frame,
		   (unsigned long)tmp_win,tmp_win->icon_bitmap_file);
  BroadcastName(M_RES_CLASS,tmp_win->w,tmp_win->frame,
		(unsigned long)tmp_win,tmp_win->class.res_class);
  BroadcastName(M_RES_NAME,tmp_win->w,tmp_win->frame,
		(unsigned long)tmp_win,tmp_win->class.res_name);
#ifdef MINI_ICONS
  if (tmp_win->mini_icon != NULL)
    BroadcastMiniIcon(M_MINI_ICON,
                      tmp_win->w, tmp_win->frame, (unsigned long)tmp_win,
                      tmp_win->mini_icon->width,
                      tmp_win->mini_icon->height,
                      tmp_win->mini_icon->depth,
                      tmp_win->mini_icon->picture,
                      tmp_win->mini_icon->mask,
                      tmp_win->mini_pixmap_file);
#endif

  /******  ******/

  FetchWmProtocols (tmp_win);
  FetchWmColormapWindows (tmp_win);

  /****** colormap ******/

  if(!(XGetWindowAttributes(dpy,tmp_win->w,&(tmp_win->attr))))
    tmp_win->attr.colormap = Pcmap;

  /****** resize window ******/

  if(do_resize_too)
    {
      XWarpPointer(dpy, Scr.Root, Scr.Root, 0, 0, Scr.MyDisplayWidth,
		   Scr.MyDisplayHeight,
		   tmp_win->frame_g.x + (tmp_win->frame_g.width>>1),
		   tmp_win->frame_g.y + (tmp_win->frame_g.height>>1));
      Event.xany.type = ButtonPress;
      Event.xbutton.button = 1;
      Event.xbutton.x_root = tmp_win->frame_g.x + (tmp_win->frame_g.width>>1);
      Event.xbutton.y_root = tmp_win->frame_g.y + (tmp_win->frame_g.height>>1);
      Event.xbutton.x = (tmp_win->frame_g.width>>1);
      Event.xbutton.y = (tmp_win->frame_g.height>>1);
      Event.xbutton.subwindow = None;
      Event.xany.window = tmp_win->w;
      resize_window(&Event , tmp_win->w, tmp_win, C_WINDOW, "", 0);
    }

  /****** window colormap ******/

  InstallWindowColormaps(colormap_win);

  /****** gnome setup ******/

#ifdef GNOME
  /* set GNOME hints on the window from flags set on tmp_win */
  GNOME_SetHints(tmp_win);
  GNOME_SetLayer(tmp_win);
  GNOME_SetDesk(tmp_win);
#endif

  return tmp_win;
}


/***********************************************************************
 *
 *  Procedure:
 *	FetchWMProtocols - finds out which protocols the window supports
 *
 *  Inputs:
 *	tmp - the fvwm window structure to use
 *
 ***********************************************************************/
void FetchWmProtocols (FvwmWindow *tmp)
{
  Atom *protocols = NULL, *ap;
  int i, n;
  Atom atype;
  int aformat;
  unsigned long bytes_remain,nitems;

  if(tmp == NULL) return;
  /* First, try the Xlib function to read the protocols.
   * This is what Twm uses. */
  if (XGetWMProtocols (dpy, tmp->w, &protocols, &n))
    {
      for (i = 0, ap = protocols; i < n; i++, ap++)
	{
	  if (*ap == (Atom)_XA_WM_TAKE_FOCUS)
	    SET_WM_TAKES_FOCUS(tmp, 1);
	  if (*ap == (Atom)_XA_WM_DELETE_WINDOW)
	    SET_WM_DELETES_WINDOW(tmp, 1);
	}
      if (protocols)
	XFree((char *)protocols);
    }
  else
    {
      /* Next, read it the hard way. mosaic from Coreldraw needs to
       * be read in this way. */
      if ((XGetWindowProperty(dpy, tmp->w, _XA_WM_PROTOCOLS, 0L, 10L, False,
			      _XA_WM_PROTOCOLS, &atype, &aformat, &nitems,
			      &bytes_remain,
			      (unsigned char **)&protocols))==Success)
	{
	  for (i = 0, ap = protocols; i < nitems; i++, ap++)
	    {
	      if (*ap == (Atom)_XA_WM_TAKE_FOCUS)
		SET_WM_TAKES_FOCUS(tmp, 1);
	      if (*ap == (Atom)_XA_WM_DELETE_WINDOW)
		SET_WM_DELETES_WINDOW(tmp, 1);
	    }
	  if (protocols)
	    XFree((char *)protocols);
	}
    }
  return;
}



/***********************************************************************
 *
 *  Procedure:
 *	GetWindowSizeHints - gets application supplied size info
 *
 *  Inputs:
 *	tmp - the fvwm window structure to use
 *
 ***********************************************************************/

void GetWindowSizeHints(FvwmWindow *tmp)
{
  long supplied = 0;
  int broken_hints = 0;

  if (HAS_OVERRIDE_SIZE_HINTS(tmp) ||
      !XGetWMNormalHints (dpy, tmp->w, &tmp->hints, &supplied))
    tmp->hints.flags = 0;

  /* Beat up our copy of the hints, so that all important field are
   * filled in! */
  if (tmp->hints.flags & PResizeInc)
    {
      if (tmp->hints.width_inc <= 0)
        {
          tmp->hints.width_inc = 1;
	  broken_hints = 1;
        }
      if (tmp->hints.height_inc <= 0)
        {
          tmp->hints.height_inc = 1;
	  broken_hints = 2;
        }
    }
  else
    {
      tmp->hints.width_inc = 1;
      tmp->hints.height_inc = 1;
    }

  if(tmp->hints.flags & PMinSize)
    {
      if (tmp->hints.min_width <= 0)
        {
	  if (tmp->hints.min_width < 0)
	    broken_hints = 3;
          tmp->hints.min_width = 1;
        }
      if (tmp->hints.min_height <= 0)
        {
	  if (tmp->hints.min_height < 0)
	    broken_hints = 4;
          tmp->hints.min_height = 1;
        }
    }
  else
    {
      if(tmp->hints.flags & PBaseSize)
	{
	  tmp->hints.min_width = tmp->hints.base_width;
	  tmp->hints.min_height = tmp->hints.base_height;
	}
      else
	{
	  tmp->hints.min_width = 1;
	  tmp->hints.min_height = 1;
	}
    }

  if(tmp->hints.flags & PMaxSize)
    {
      if(tmp->hints.max_width < tmp->hints.min_width)
        {
	  tmp->hints.max_width = DEFAULT_MAX_MAX_WINDOW_WIDTH;
          broken_hints = 5;
        }
      if(tmp->hints.max_height < tmp->hints.min_height)
        {
	  tmp->hints.max_height = DEFAULT_MAX_MAX_WINDOW_HEIGHT;
          broken_hints = 6;
        }
    }
  else
    {
      tmp->hints.max_width = DEFAULT_MAX_MAX_WINDOW_WIDTH;
      tmp->hints.max_height = DEFAULT_MAX_MAX_WINDOW_HEIGHT;
    }

  /*
   * ICCCM says that PMinSize is the default if no PBaseSize is given,
   * and vice-versa.
   */

  if(tmp->hints.flags & PBaseSize)
    {
      if (tmp->hints.base_width < 0)
        {
          tmp->hints.base_width = 0;
          broken_hints = 7;
        }
      if (tmp->hints.base_height < 0)
        {
          tmp->hints.base_height = 0;
          broken_hints = 8;
        }
      if ((tmp->hints.base_width > tmp->hints.min_width) ||
          (tmp->hints.base_height > tmp->hints.min_height))
        {
          /* In this case, doing the aspect ratio calculation
	     for window_size - base_size as prescribed by the
             ICCCM is going to fail.
             Resetting the flag disables the use of base_size
	     in aspect ratio calculation while it is still used
	     for grid sizing.
           */
          tmp->hints.flags &= ~PBaseSize;
#if 0
 	/* Keep silent about this, since the Xlib manual actually
  	   recommends making min <= base <= max ! */
          broken_hints = 0;
#endif
        }
    }
  else
    {
      if(tmp->hints.flags & PMinSize)
	{
	  tmp->hints.base_width = tmp->hints.min_width;
	  tmp->hints.base_height = tmp->hints.min_height;
	}
      else
	{
	  tmp->hints.base_width = 0;
	  tmp->hints.base_height = 0;
	}
    }

  if(!(tmp->hints.flags & PWinGravity))
    {
      tmp->hints.win_gravity = NorthWestGravity;
    }

  if (tmp->hints.flags & PAspect)
  {
    /*
    ** check to make sure min/max aspect ratios look valid
    */
#define maxAspectX tmp->hints.max_aspect.x
#define maxAspectY tmp->hints.max_aspect.y
#define minAspectX tmp->hints.min_aspect.x
#define minAspectY tmp->hints.min_aspect.y


    /*
    ** The math looks like this:
    **
    **   minAspectX    maxAspectX
    **   ---------- <= ----------
    **   minAspectY    maxAspectY
    **
    ** If that is multiplied out, this must be satisfied:
    **
    **   minAspectX * maxAspectY <=  maxAspectX * minAspectY
    **
    ** So, what to do if this isn't met?  Ignoring it entirely
    ** seems safest.
    **
    */

    /* We also ignore people who put negative entries into
     * their aspect ratios. They deserve it.
     *
     * We cast to double here, since the values may be large.
     */
    if ((maxAspectX < 0) || (maxAspectY < 0) ||
        (minAspectX < 0) || (minAspectY < 0) ||
        (((double)minAspectX * (double)maxAspectY) >
         ((double)maxAspectX * (double)minAspectY)))
     {
        broken_hints = 9;
        tmp->hints.flags &= ~PAspect;
        fvwm_msg (WARN, "GetWindowSizeHints",
                 "%s window %#lx has broken aspect ratio: %d/%d - %d/%d\n",
                  tmp->name, tmp->w, minAspectX, minAspectY, maxAspectX,
	  	maxAspectY);
     }
    else
     {
       /* protect against overflow */
       if ((maxAspectX > 65536) || (maxAspectY > 65536))
         {
            double ratio = (double) maxAspectX / (double) maxAspectY;
            if (ratio > 1.0)
              {
                 maxAspectX = 65536;
                 maxAspectY = 65536 / ratio;
              }
            else
              {
                 maxAspectX = 65536 * ratio;
                 maxAspectY = 65536;
              }
         }
       if ((minAspectX > 65536) || (minAspectY > 65536))
         {
            double ratio = (double) minAspectX / (double) minAspectY;
            if (ratio > 1.0)
              {
                 minAspectX = 65536;
                 minAspectY = 65536 / ratio;
              }
            else
              {
                 minAspectX = 65536 * ratio;
                 minAspectY = 65536;
              }
         }
     }
  }

  if (broken_hints)
    {
      XSizeHints orig_hints;
      XGetWMNormalHints (dpy, tmp->w, &orig_hints, &supplied);
      fvwm_msg (WARN, "GetWindowSizeHints",
               "%s window %#lx has broken (%d) size hints\n"
               "Please report this to fvwm-workers@fvwm.org\n"
               "%ld %d %d %d %d %d %d %d/%d %d/%d %d %d %d\n",
                tmp->name, tmp->w, broken_hints,
                orig_hints.flags,
                orig_hints.min_width, orig_hints.min_height,
                orig_hints.max_width, orig_hints.max_height,
                orig_hints.width_inc, orig_hints.height_inc,
                orig_hints.min_aspect.x, orig_hints.min_aspect.y,
                orig_hints.max_aspect.x, orig_hints.max_aspect.y,
                orig_hints.base_width, orig_hints.base_height,
                orig_hints.win_gravity);
    }
}

