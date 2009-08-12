#include <config.h>

#include <signal.h>

#include <stdio.h>

#ifdef HAVE_X_WINDOWS

#include "lisp.h"
#include "blockinput.h"
#include "syssignal.h"

#include "xterm.h"
#include <X11/cursorfont.h>

#ifndef makedev
#include <sys/types.h>
#endif /* makedev */

#ifdef BSD_SYSTEM
#include <sys/ioctl.h>
#endif /* ! defined (BSD_SYSTEM) */

#include "systime.h"

#ifndef INCLUDED_FCNTL
#include <fcntl.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>

#include "charset.h"
#include "character.h"
#include "coding.h"
#include "ccl.h"
#include "frame.h"
#include "dispextern.h"
#include "fontset.h"
#include "termhooks.h"
#include "termopts.h"
#include "termchar.h"
#include "emacs-icon.h"
#include "disptab.h"
#include "buffer.h"
#include "window.h"
#include "keyboard.h"
#include "intervals.h"
#include "process.h"
#include "atimer.h"
#include "keymap.h"


#ifdef USE_X_TOOLKIT
#include <X11/Shell.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gtkutil.h"
#include "font.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "xwidget.h"
  
//just a fixed array of xwidgets for now
#define MAX_XWIDGETS 100
struct xwidget xwidgets[MAX_XWIDGETS];
  
static int once=0;


Lisp_Object Qxwidget;
Lisp_Object Qxwidget_id;
Lisp_Object Qtitle;
Lisp_Object Qxwidget_set_keyboard_grab;
Lisp_Object Qxwidget_embed_steal_window;

extern Lisp_Object QCdata, QCtype;
extern Lisp_Object QCwidth, QCheight;

static void buttonclick_handler( GtkWidget *widget,
                                 gpointer   data )
{
  struct xwidget* xw=(  struct xwidget*)data;
  Lisp_Object  frame;
  printf ("button clicked xw:%d id:%d\n",xw,xw->id);
    
  struct input_event event;
  EVENT_INIT (event);
  event.kind = XWIDGET_EVENT;
  
  FRAME_PTR f = (FRAME_PTR) g_object_get_data (G_OBJECT (xw->widget), XG_FRAME_DATA);
  XSETFRAME (frame, f);
  
  event.frame_or_window = Qnil;//frame; //how to get the frame here?


  event.arg=Qnil;
  event.arg = Fcons(make_number(xw->id),event.arg);
  event.arg=Fcons(intern("buttonclick"),event.arg);
  
  kbd_buffer_store_event (&event);


}


static void send_xembed_ready_event(int xwid,int xembedid){
  struct input_event event;
  EVENT_INIT (event);
  event.kind = XWIDGET_EVENT;
  event.frame_or_window = Qnil;//frame; //how to get the frame here?

  event.arg=Qnil;
  event.arg = Fcons(make_number(xembedid),event.arg);
  event.arg=Fcons(intern("xembed-ready"),event.arg);
  event.arg = Fcons(make_number(xwid), event.arg);

  
  kbd_buffer_store_event (&event);

}


/* xwidgets can only be "live" in exactly one window.
   that is, there is only ever 1 gtk widget belonging to a xwidget.
   then there might be phantom xwidgets, 
*/
struct window* xwidget_live_window=0;

void printnativerect(XRectangle rect){
  printf("xwidget clip:%d %d %d %d\n ",
         rect.x,rect.y,rect.width,rect.height);
}


void x_draw_xwidget_glyph_string (s)
     struct glyph_string *s;
{
  int box_line_hwidth = eabs (s->face->box_line_width);
  int box_line_vwidth = max (s->face->box_line_width, 0);
  int height = s->height;
  Pixmap pixmap = None;
  
  int drawing_in_selected_window=(XWINDOW(FRAME_SELECTED_WINDOW (s->f))) ==  (s->w); 

  //we assume the live window is the last one drawn in
  xwidget_live_window=s->w;
  
  printf("x_draw_xwidget_glyph_string: id:%d %d %d  (%d,%d,%d,%d) selected win:%d\n",
         s->xwidget_id, box_line_hwidth, box_line_vwidth, s->x,s->y,s->height,s->width, drawing_in_selected_window);
  //Screen *screen = FRAME_X_SCREEN (s->f); 
  //int depth = DefaultDepthOfScreen (screen); 
  ///////////////////////////////////////////////////////
  // stuff that affects x,y: menu, toolbar, scrollbar
  struct xwidget* xw=&xwidgets[s->xwidget_id];
  
  int x=s->x;
  int y=s->y + (s->height/2) - (xw->height/2);
  int doingsocket=0;
  if(!xw->initialized){
    printf("once for xwidget %d\n",s->xwidget_id);
    xw->initialized=1;
    xw->id=s->xwidget_id;

    switch(xw->type){
    case 1:
      printf("mk button\n");
      xw->widget=gtk_button_new_with_label (xw->title); 
      g_signal_connect (G_OBJECT (xw->widget), "clicked", G_CALLBACK (buttonclick_handler), xw);
      break;
    case 2:
      printf("mk togglebutton\n");
      xw->widget=gtk_toggle_button_new_with_label (    xw->title);
      break;
    case 3:

      xw->widget=gtk_socket_new ();
      printf("mk socket\n");
      doingsocket=1;
      break;
    case 4:
      xw->widget=gtk_hscale_new (GTK_ADJUSTMENT(gtk_adjustment_new(0,0,100,1,1,0)));
      gtk_scale_set_draw_value(GTK_SCALE(xw->widget),FALSE); //i think its emacs role to show text and stuff, so disable the widgets own text
      printf("mk hscale\n");
    }

    //s->window   Window window; is an xwindows XID
    //    parent=gdk_window_foreign_new (s->window);
    //    g_assert(parent);
    //        xw->widgetwindow = GTK_WINDOW(gtk_window_new (GTK_WINDOW_POPUP));//GTK_WINDOW_TOPLEVEL)); //GTK_WINDOW_POPUP somehow works better than GTK_TOPLEVEL
    //    gtk_widget_set_size_request ( xw->widget ,s->background_width,s->height);

    //TODO mk container widget 1st, and put the widget inside
    //later, drawing should crop container window if necessary to handle case where xwidget is near bottom of emacs window
    xw->widgetwindow = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL,NULL));
    gtk_widget_set_size_request (GTK_WIDGET(xw->widgetwindow) ,xw->width,xw->height);
    gtk_scrolled_window_set_policy(xw->widgetwindow, GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_container_add(GTK_CONTAINER(xw->widgetwindow), xw->widget);
    
    gtk_widget_set_size_request (xw->widget ,xw->width,xw->height);    
    gtk_fixed_put(GTK_FIXED(s->f->gwfixed),GTK_WIDGET(xw->widgetwindow) ,x,y);
    gtk_widget_show_all (GTK_WIDGET(xw->widgetwindow) );

    if(doingsocket){
      printf("socket id:%x %d\n", gtk_socket_get_id (GTK_SOCKET(xw->widget)), gtk_socket_get_id (GTK_SOCKET(xw->widget)));
      send_xembed_ready_event(xw->id,gtk_socket_get_id (GTK_SOCKET(xw->widget)));
    }
  }


  //move doesnt seem to quite remove the previous drawing of the widget, so perhaps i should clear
  // the area before or after move?
  //or maybe emacs would handle erase better if i somehow communicated widget size better to emacs

  //x_draw_glyph_string_bg_rect (s, x, y, s->background_width, xw->height); //erase bg

  //ok, we are painting the xwidgets in non-selected window
  //we cant get real widgets here, so we draw a simple rectangle instead(for now)
  XGCValues xgcv;
  XGetGCValues (s->display, s->gc, GCForeground | GCBackground, &xgcv);
  XSetForeground (s->display, s->gc, xgcv.background);
  XFillRectangle (s->display, s->window, s->gc, x, y, xw->width, height);
  XSetForeground (s->display, s->gc, xgcv.foreground);
  
  if(drawing_in_selected_window){
    if(//drawing_in_selected_window && //only move the widget in selected window
       ( xw->x != x || xw->y != y)) //has it moved?
      {
        printf("xwidget %d moved\n",xw->id);
        //      gtk_widget_hide(xw->widget);
        //gdk_window_clear (      xw->widget);
        gtk_fixed_move(GTK_FIXED(s->f->gwfixed),xw->widgetwindow ,x,y);
        //      gtk_widget_show(xw->widget);


        
      }
    else
      {
        gtk_fixed_move(GTK_FIXED(s->f->gwfixed),xw->widgetwindow ,x,y);
      }
    //adjust size of the widget window if some parts happen to be outside drawable area
    //that is, we should clip
    //an emacs window is not a gtk window, a gtk window covers the entire frame
    int clipx=min(xw->width,WINDOW_RIGHT_EDGE_X((s->w))-x);
    int clipy=min(xw->height,WINDOW_BOTTOM_EDGE_Y((s->w))-WINDOW_MODE_LINE_HEIGHT(s->w)-y);
    gtk_widget_set_size_request (GTK_WIDGET(xw->widgetwindow) , clipx,   clipy);

  }else{
    //ok, we are painting the xwidgets in non-selected window
    //we cant get real widgets here, so we draw a simple rectangle instead(for now)
    XGCValues xgcv;
    XGetGCValues (s->display, s->gc, GCForeground | GCBackground, &xgcv);
    XFillRectangle (s->display, s->window, s->gc, x, y, xw->width, xw->height);
    //TODO write alternate text on top of rect here
    //TODO have a look at of-screen rendering of gtk widgets and use that here
  }
  
  xw->x=x;
  xw->y=y;
}




DEFUN("xwidget-embed-steal-window", Fxwidget_embed_steal_window, Sxwidget_embed_steal_window,
      2,2,0,
      doc: /* tell existing embed xwidget to steal other window id.*/)
     (xwidget_id,window_id)
     Lisp_Object xwidget_id, window_id;      
{
  struct xwidget *xw;
  int xid=XFASTINT(xwidget_id);
  xw=&xwidgets[xid];
  int iwindow_id=XFASTINT(window_id);
  printf("  gtk_socket_add_id: %d %d\n",xid,iwindow_id);
  //  gtk_socket_steal(GTK_SOCKET(xw->widget),iwindow_id);
  //try adding proper gtk plugs instead, i never once had "steal" work
  gtk_socket_add_id(GTK_SOCKET(xw->widget),iwindow_id);
  //add_id annoyingly odesnt work either. the only working option
  //seems to be clients that plug into the sockets, and so far only emacs and mplayer
  //oenvrml
  return Qnil;
}


int  xwidget_owns_kbd=0;
DEFUN("xwidget-set-keyboard-grab", Fxwidget_set_keyboard_grab,Sxwidget_set_keyboard_grab,
      2,2,0,
      doc: /* set unset kbd grab for xwidget.*/)
     (xwidget_id,kbd_grab)
     Lisp_Object xwidget_id, kbd_grab;      
{
  struct xwidget *xw;
  int xid=XFASTINT(xwidget_id);
  xw=&xwidgets[xid];
  int kbd_flag=XFASTINT(kbd_grab);
  printf("kbd grab: %d %d\n",xid,kbd_flag);
  if (kbd_flag)
    {
      //int rv=gtk_widget_activate(xw->widget); //ok, but how deactivate?
      //printf("activation:%d\n",rv);
      //      gtk_window_present(GTK_WINDOW(xw->widget));
      //gtk_widget_grab_focus(xw->widget);
      //      _gtk_socket_windowing_update_active (xw->widget,1);
      //      GDK_WINDOW_XWINDOW (GTK_WIDGET (socket)->window)
      //FRAME_X_OUTPUT (f)->widget
      //      gdk_keyboard_grab(xw->widget,TRUE,GDK_CURRENT_TIME);
      
      GtkWidget* parent=gtk_widget_get_parent(xw->widget);
      GtkWidget* lastparent;
      for(lastparent=parent; parent=gtk_widget_get_parent(parent); parent==NULL);

      gtk_container_set_focus_child(GTK_CONTAINER(lastparent),xw->widget);

      xwidget_owns_kbd=TRUE;
    }
  else
    {
      xwidget_owns_kbd=FALSE;
    }
  /*
    gdk_keyboard_grab(xw->widget,TRUE,GDK_CURRENT_TIME);
    else
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  */
  return Qnil;
}
      
void
syms_of_xwidget ()
{
  int i;

  Qxwidget_set_keyboard_grab=intern("xwidget-set-keyboard-grab");
  staticpro(&Qxwidget_set_keyboard_grab);
  defsubr(&Sxwidget_set_keyboard_grab);

  Qxwidget_embed_steal_window=intern("xwidget-embed-steal-window");
  staticpro(&Qxwidget_embed_steal_window);
  defsubr(&Sxwidget_embed_steal_window);
  
  Qxwidget = intern ("xwidget");
  staticpro (&Qxwidget);
  
  Qxwidget_id = intern (":xwidget-id");
  staticpro (&Qxwidget_id);
  
  Qtitle = intern (":title");
  staticpro (&Qtitle);

  Fprovide (intern ("xwidget"), Qnil); //proudly announce xwidget support
  
  for(i=0;i<MAX_XWIDGETS;i++)
    xwidgets[i].initialized=0;
}


/* Value is non-zero if OBJECT is a valid Lisp xwidget specification.  A
   valid xwidget specification is a list whose car is the symbol
   `xwidget', and whose rest is a property list.  The property list must
   contain a value for key `:type'.  That value must be the name of a
   supported xwidget type.  The rest of the property list depends on the
   xwidget type.  */

int valid_xwidget_p (object) Lisp_Object object;
{
  int valid_p = 0;

  if (XWIDGETP (object))
    {
      /* Lisp_Object tem; */

      /* for (tem = XCDR (object); CONSP (tem); tem = XCDR (tem)) */
      /*   if (EQ (XCAR (tem), QCtype)) */
      /*     { */
      /*       tem = XCDR (tem); */
      /*       if (CONSP (tem) && SYMBOLP (XCAR (tem))) */
      /*         { */
      /*   	struct xwidget_type *type; */
      /*   	type = lookup_xwidget_type (XCAR (tem)); */
      /*   	if (type) */
      /*   	  valid_p = type->valid_p (object); */
      /*         } */

      /*       break; */
      /*     } */
      //never mind type support for now
      valid_p = 1;
    }

  return valid_p;
}

//type support nevermind for now

/* /\* List of supported image types.  Use define_image_type to add new */
/*    types.  Use lookup_image_type to find a type for a given symbol.  *\/ */

/* static struct wxidget_type *wxidget_types; */

/* /\* Look up xwidget type SYMBOL, and return a pointer to its xwidget_type */
/*    structure.  Value is null if SYMBOL is not a known image type.  *\/ */

/* static INLINE struct xwidget_type *lookup_xwidget_type (Lisp_Object symbol) */
/* { */
/*   struct xwidget_type *type; */

/*   for (type = xwidget_types; type; type = type->next) */
/*     if (EQ (symbol, *type->type)) */
/*       break; */

/*   return type; */
/* } */


void xwidget_hide(struct xwidget* xw){
  printf("xwidget %d hidden\n",xw->id);
  gtk_widget_hide(xw->widget);
}

void xwidget_show(struct xwidget* xw){
  printf("xwidget %d shown\n",xw->id);
  gtk_widget_show(xw->widget);
}


Lisp_Object
xwidget_spec_value (spec, key, found)
     Lisp_Object spec, key;
     int *found;
{
  Lisp_Object tail;

  xassert (valid_xwidget_p (spec));

  for (tail = XCDR (spec);
       CONSP (tail) && CONSP (XCDR (tail));
       tail = XCDR (XCDR (tail)))
    {
      if (EQ (XCAR (tail), key))
	{
	  if (found)
	    *found = 1;
	  return XCAR (XCDR (tail));
	}
    }

  if (found)
    *found = 0;
  return Qnil;
}

void assert_valid_xwidget_id(int id,char *str){
  if(id < 0 || id> MAX_XWIDGETS){
    printf("broken xwidgetid:%d %s\n",id,str);
    abort();
  }
}

struct xwidget* xwidget_from_id(int id){
  assert_valid_xwidget_id(id,"xwidget_from_id");
  return &xwidgets[id];
}

int
lookup_xwidget (spec)
     Lisp_Object spec;
{
  
  int found=0,found1=0,found2=0;
  Lisp_Object value;
  value= xwidget_spec_value(spec, Qxwidget_id, &found1);
  int id=INTEGERP (value) ? XFASTINT (value) : 0; //id 0 by default, but id must be unique so this is dumb
      
  struct xwidget* xw=&xwidgets[id];
  value=xwidget_spec_value(spec, QCtype,&found);
  xw->type=INTEGERP (value) ? XFASTINT (value) : 1; //default to button
  value=xwidget_spec_value(spec, Qtitle, &found2);
  xw->title=STRINGP(value)?(char*)SDATA(value):"?";  //funky cast FIXME

  value=xwidget_spec_value(spec, QCheight,NULL);
  xw->height=INTEGERP (value) ? XFASTINT (value) : 50; //ok
  value=xwidget_spec_value(spec, QCwidth,NULL);
  xw->width=INTEGERP (value) ? XFASTINT (value) : 50; //ok

  
  printf("xwidget_id:%d type:%d found:%d %d %d title:%s (%d,%d)\n",id,  xw->type, found,found1,found2, xw->title,xw->height,xw->width);


  assert_valid_xwidget_id(id,"lookup_xwidget");

  return id;
}
//////////////////////////////////
int region_modified=0;

/*set up detection of touched xwidget*/
void   xwidget_start_redisplay(){
  int i;
  for(i=0;i<MAX_XWIDGETS;i++)
    xwidgets[i].redisplayed=0;

}

/* the xwidget was touched during redisplay, so it isnt a candidate for deletion*/
void xwidget_touch(struct xwidget* xw){
  printf("touch xwidget %d\n", xw->id);
  xw->redisplayed=1;
}

/* redisplay has ended, now we should delete untouched xwidgets*/
void  xwidget_end_redisplay(struct glyph_matrix* matrix){
  int i;
  struct xwidget* xw;
  /*  for(i=0;i<MAX_XWIDGETS;i++){
      xw=&xwidgets[i];
      if(!xw->redisplayed && xw->initialized && region_modified)
      {
      printf("xwidget %d would have been deleted\n",xw->id);
      //xwidget_delete(xw);
      }
      }*/
  region_modified=0;
  xwidget_start_redisplay();
  //iterate desired glyph matrix of "live" window here, hide gtk widgets
  //not in the desired matrix.
  int area;
  //  xwidget_live_window=XWINDOW (selected_window);
  // the live window needs to be a window with actual xwidgets
  //the window pointer also ought to be checked for validity
  //also, the current scheme will fail on the case of several buffers showing xwidgets
  
  //  struct glyph_matrix* matrix=xwidget_live_window->desired_matrix;
  //  dump_glyph_matrix(matrix, 2);
  for (i = 0; i < matrix->nrows; ++i){
    //    dump_glyph_row (MATRIX_ROW (matrix, i), i, glyphs);
    struct glyph_row *row;
    row=MATRIX_ROW (matrix, i);
    if(row->enabled_p!=0){
      for (area = LEFT_MARGIN_AREA; area < LAST_AREA; ++area)
        {
          struct glyph *glyph = row->glyphs[area];
          struct glyph *glyph_end = glyph + row->used[area];
          for (; glyph < glyph_end; ++glyph){
            if (glyph->type == XWIDGET_GLYPH){
              printf("(%d)",glyph->u.xwidget_id);
              //here the id sometimes sucks, so maybe the desired glyph matrix isnt ready here?
              //also, it appears the desired matrix is not the entire window, but only the changed part. wtf?
              int id=glyph->u.xwidget_id;
              if(id < 0 || id> MAX_XWIDGETS){
                printf("glyph matrix contains crap, abort xwidget handling and wait for better times\n ");
                //dump_glyph_matrix(matrix, 2);
                return;
              }
              else
                printf("row %d not enabled\n", i);
              
              xwidget_touch(&xwidgets[glyph->u.xwidget_id]);
            }
          }
        }
    }
  }
  
  for(i=0;i<MAX_XWIDGETS;i++){
    xw=&xwidgets[i];
    if(xw->initialized)
      {
        if(xw->redisplayed)
          xwidget_show(xw);
        else
          xwidget_hide(xw);
      }
  }
}

/* some type of modification was made to the buffers*/
void xwidget_modify_region(){
  region_modified=1;
}

////////////////////////////////////////////////////////////////

/* delete the xwidget and its native widget peer */
void xwidget_delete(struct xwidget* xw){
  printf("xwidget %d deleted\n",xw->id);
  xw->initialized=0;
  gtk_widget_destroy(xw->widget);

}





void xwidget_invalidate(){
  int i;
  struct xwidget* xw;
  printf("invalidate ");
  for(i=0;i<MAX_XWIDGETS;i++){
    xw=&xwidgets[i];
    if(xw->initialized)
      {
        printf("%d,",i);
        gtk_widget_queue_draw_area(xw->widget,0,0,xw->width,xw->height);
      }
  }
  printf("\n");
}
