/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include "xastir.h"
#include "globals.h"
#include "main.h"
#include "xa_config.h"
#include "db_funcs.h"
#include "draw_symbols.h"
#include "maps.h"  // for draw_vector prototype

#include <Xm/XmAll.h>
#include <X11/cursorfont.h>

// Must be last include file
#include "leak_detection.h"

// lesstif (at least as of version 0.94 in 2008), doesn't
// have full implementation of combo boxes.
#ifndef USE_COMBO_BOX
  #if (XmVERSION >= 2 && !defined(LESSTIF_VERSION))
    #define USE_COMBO_BOX 1
  #endif
#endif  // USE_COMBO_BOX

extern XmFontList fontlist1;    // Menu/System fontlist
extern void pos_dialog(Widget w);

int polygon_last_x = -1;        // Draw CAD Objects functions
int polygon_last_y = -1;        // Draw CAD Objects functions

Widget draw_CAD_objects_dialog = (Widget)NULL;
Widget cad_dialog = (Widget)NULL;
Widget cad_label_data,
       cad_comment_data,
       cad_probability_data,
       cad_line_style_data;
// Values entered in the cad_dialog
int draw_CAD_objects_flag = 0;
void Draw_All_CAD_Objects(Widget w);
void Save_CAD_Objects_to_file(void);
Widget cad_erase_dialog;
Widget list_of_existing_CAD_objects = (Widget)NULL;
Widget cad_list_dialog = (Widget)NULL;
Widget list_of_existing_CAD_objects_edit = (Widget)NULL;


void Draw_CAD_Objects_erase( Widget w, XtPointer clientData, XtPointer callData);
void Update_CAD_objects_list_dialog(void);
void CAD_object_set_raw_probability(CADRow *object_ptr, float probability, int as_percent);
void Draw_CAD_Objects_erase_dialog( Widget w, XtPointer clientData, XtPointer callData );
void Draw_CAD_Objects_list_dialog( Widget w, XtPointer clientData, XtPointer callData );
void Draw_CAD_Objects_erase_dialog_close(Widget w, XtPointer clientData, XtPointer callData);
void Draw_CAD_Objects_list_dialog_close(Widget w, XtPointer clientData, XtPointer callData);
void Format_area_for_output(double *area_km2, char *area_description, int sizeof_area_description);

int CAD_draw_objects = TRUE;
int CAD_show_label = TRUE;
int CAD_show_raw_probability = TRUE;
int CAD_show_comment = TRUE;
int CAD_show_area = TRUE;


#ifndef USE_COMBO_BOX
  int clsd_value;  // replacement value for cad line type combo box
#endif // !USE_COMBO_BOX

//////////////////// Draw CAD Objects Functions ////////////////////



//#define CAD_DEBUG

// Allocate a new vertice along the polygon.  If the vertice is very
// close to the first vertice, ask the operator if they wish to
// close the polygon.  If closing, ask for a raw probability?
//
// As each vertice is allocated, write it out to file?  We'd then
// need to edit the file and comment vertices out if we're deleting
// vertices in memory.  We could also write out an entire object
// when we select "Close Polygon".
//
void CAD_vertice_allocate(long latitude, long longitude)
{

#ifdef CAD_DEBUG
  fprintf(stderr,"Allocating a new vertice\n");
#endif

  // Check whether a line segment will cross another?

  // We use the CAD_list_head variable, as it will be pointing to
  // the top of the list, where the current object we're working
  // on will be placed.  Check whether that pointer is NULL
  // though, just in case.
  if (CAD_list_head)     // We have at least one object defined
  {
    VerticeRow *p_new;

    // Allocate area to hold the vertice
    p_new = (VerticeRow *)malloc(sizeof(VerticeRow));

    if (!p_new)
    {
      fprintf(stderr,"Couldn't allocate memory in CAD_vertice_allocate()\n");
      return;
    }

    p_new->latitude = latitude;
    p_new->longitude = longitude;

    // Link it in at the top of the vertice chain.
    p_new->next = CAD_list_head->start;
    CAD_list_head->start = p_new;
  }

  // Call redraw_symbols outside this function, as
  // verticies may be allocated both when loading lots of them from a file
  // and when the user is drawing objects in the user interface
  // Reload symbols/tracks/CAD objects
  //redraw_symbols(da);
}





// Allocate a struct for a new object and add one vertice to it.
// When do we name it and place the label?  Assign probability to
// it?  We should keep a pointer to the current polygon we're
// working on, so that we can modify it easily as we draw.
// Actually, it'll be pointed to by CAD_list_head, so we already
// have it!
//
// As each object is allocated, write it out to file?
//
// Compute a default label of date/time?
//
void CAD_object_allocate(long latitude, long longitude)
{
  CADRow *p_new;

#ifdef CAD_DEBUG
  fprintf(stderr,"Allocating a new CAD object\n");
#endif

  // Allocate memory and link it to the top of the singly-linked
  // list of CADRow objects.
  p_new = (CADRow *)malloc(sizeof(CADRow));

  if (!p_new)
  {
    fprintf(stderr,"Couldn't allocate memory in CAD_object_allocate()\n");
    return;
  }

  // Fill in default values
  p_new->creation_time = sec_now();
  p_new->start = NULL;
  p_new->line_color = colors[0x27];
  p_new->line_type = 2;  // LineOnOffDash;
  p_new->line_width = 4;
  p_new->computed_area = 0;
  CAD_object_set_raw_probability(p_new,0.0,FALSE);
  p_new->label_latitude = 0l;
  p_new->label_longitude = 0l;
  p_new->label[0] = '\0';
  p_new->comment[0] = '\0';

  // Allocate area to hold the first vertice

#ifdef CAD_DEBUG
  fprintf(stderr,"Allocating a new vertice\n");
#endif

  p_new->start = (VerticeRow *)malloc(sizeof(VerticeRow));
  if (!p_new->start)
  {
    fprintf(stderr,"Couldn't allocate memory in CAD_object_allocate(2)\n");
    free(p_new);
    return;
  }

  p_new->start->next = NULL;
  p_new->start->latitude = latitude;
  p_new->start->longitude = longitude;

  // Hook it into the linked list of objects
  p_new->next = CAD_list_head;
  CAD_list_head = p_new;

  /*
  //
  // Note:  It was too confusing to have these two dialogs close and
  // get redrawn when we click on the first vertice.  The net result
  // is that we may have two dialogs move on top of the drawing area
  // to the spot we're trying to draw.  Commented out this section due
  // to that.  We'll get the two dialogs updated when we click on
  // either the DONE or CANCEL button on the Close Polygon dialog.
  //
      // Here we update the erase cad objects dialog if it is up on
      // the screen.  We get rid of it and re-establish it, which will
      // usually make the dialog move, but this is better than having
      // it be out-of-date.
      //
      if (cad_erase_dialog != NULL) {
          Draw_CAD_Objects_erase_dialog_close(da, NULL, NULL);
          Draw_CAD_Objects_erase_dialog(da, NULL, NULL);
      }

      // Here we update the edit cad objects dialog by getting rid of
      // it and then re-establishing it if it is active when we start.
      // This will usually make the dialog move, but it's better than
      // having it be out-of-date.
      //
      if (cad_list_dialog!=NULL) {
          // Update the Edit CAD Objects list
          Draw_CAD_Objects_list_dialog_close(da, NULL, NULL);
          Draw_CAD_Objects_list_dialog(da, NULL, NULL);
      }
  */
}





// Delete all vertices associated with a CAD object and free the
// memory.  We really should pass a pointer to the object here
// instead of a vertice, and set the start pointer to NULL when
// done.
//
void CAD_vertice_delete_all(VerticeRow *v)
{
  VerticeRow *tmp;

  // Call CAD_vertice_delete() for each vertice, then unlink this
  // CAD object from the linked list and free its memory.

  // Iterate through each vertice, deleting as we go
  while (v != NULL)
  {
    tmp = v;
    v = v->next;
    free(tmp);

#ifdef CAD_DEBUG
    fprintf(stderr,"Free'ing a vertice\n");
#endif

  }
}





// Delete _all_ CAD objects and all associated vertices.  Loop
// through the entire list of CAD objects, calling
// CAD_vertice_delete_all() and then free'ing the CAD object.  When
// done, set the start pointer to NULL.
//
// We also need to wipe the persistent CAD object file.
//
void CAD_object_delete_all(void)
{
  CADRow *p = CAD_list_head;
  CADRow *tmp;

  while (p != NULL)
  {
    VerticeRow *v = p->start;

    // Remove all of the vertices
    if (v != NULL)
    {

      // Delete/free the vertices
      CAD_vertice_delete_all(v);
    }

    // Remove the object and free its memory
    tmp = p;
    p = p->next;
    free(tmp);

#ifdef CAD_DEBUG
    fprintf(stderr,"Free'ing an object\n");
#endif

  }

  // Zero the CAD linked list head
  CAD_list_head = NULL;
}





// Remove a vertice, thereby joining two segments into one?
//
// Recompute the raw probability if need be, or make it an invalid
// value so that we know we need to recompute it.
//
//void CAD_vertice_delete(CADrow *object) {
//    VerticeRow *v = object->start;

// Unlink the vertice from the linked list and free its memory.
// Allow removing a vertice in the middle or end of a chain.  If
// removing the vertice turns the polygon into an open polygon,
// alert the user of that fact and ask if they wish to close it.
//}





/* Test to see if a CAD object of the name (label) provided exists.
   Parameter: label, the label text to be checked.
   Returns 0 if no CAD object with a name matching the
   provided name is found.
   Returns 1 if a CAD object with a name matching the
   provided name is found.  */
int exists_CAD_object_by_label(char *label)
{
  CADRow *object_pointer = CAD_list_head;
  int result = 0;  // function return value
  int done = 0;    // flag to stop loop when a match is found
  while (object_pointer != NULL && done==0)
  {
    if (strcmp(object_pointer->label,label)==0)
    {
      // a matching name was found
      result = 1;
      done = 1;
    }
    object_pointer = object_pointer->next;
  }
  return result;
}





/* Counts to see how many CAD objects of the name (label) provided exist.
   Parameter: label, the label text to be checked.
   Returns 0 if no CAD object with a name matching the
   provided name is found.
   Returns count of the number of CAD objects with a matching label if
   one or more is found.  */
int count_CAD_object_with_matching_label(char *label)
{
  CADRow *object_pointer = CAD_list_head;
  int result = 0;
  while (object_pointer != NULL)
  {
    // iterate through all CAD objects
    if (strcmp(object_pointer->label,label)==0)
    {
      // a matching name was found
      result++;
      object_pointer = object_pointer->next;
    }
  }
  return result;
}





/* Delete one CAD object and all of its vertices. */
void CAD_object_delete(CADRow *object)
{
  CADRow *all_objects_ptr = CAD_list_head;
  CADRow *previous_object_ptr = CAD_list_head;
  VerticeRow *v = object->start;
  int done = 0;

#ifdef CAD_DEBUG
  fprintf(stderr,"Deleting CAD object %s\n",object->label);
#endif
  // check to see if the object we were given was the first object
  if (object==all_objects_ptr)
  {
#ifdef CAD_DEBUG
    fprintf(stderr,"Deleting first CAD object %s\n",object->label);
#endif
    CAD_vertice_delete_all(v); // Frees the memory also

    // Unlink the object from the chain and free the memory.
    CAD_list_head = object->next;  // Unlink
    free(object);   // Free the object memory
  }
  else
  {
#ifdef CAD_DEBUG
    fprintf(stderr,"Deleting other than first CAD object %s\n",object->label);
#endif
    // walk through the list and delete the object when found
    while (all_objects_ptr != NULL && done==0)
    {
      if (object==all_objects_ptr)
      {
        v = object->start;
        CAD_vertice_delete_all(v);
        previous_object_ptr->next = object->next;
        free(object);
        done = 1;
      }
      else
      {
        all_objects_ptr = all_objects_ptr->next;
      }
    }
  }
}





// Split an existing CAD object into two objects.  Can we trigger
// this by drawing a line across a closed polygon?
void CAD_object_split_existing(void)
{
}

// Join two existing polygons into one larger polygon.
void CAD_object_join_two(void)
{
}

// Move an entire CAD object, with all it's vertices, somewhere
// else.  Move the label along with it as well.
void CAD_object_move(void)
{
}





// Determine if a CAD object is a closed polygon.
//
// Takes a pointer to a CAD object as an argument.
// Returns 1 if the object is closed.
// Returns 0 if the object is not closed.
//
int is_CAD_object_open(CADRow *cad_object)
{
  VerticeRow *vertex_pointer;
  int vertex_count = 0;
  int result = 1;
  int atleast_one_different = 0;
  long start_lat, start_long;
  long stop_lat, stop_long;

  vertex_pointer = cad_object->start;
  if (vertex_pointer!=NULL)
  {
    // greater than zero points, get first point.
    start_lat = vertex_pointer->latitude;
    start_long = vertex_pointer->longitude;
    stop_lat = vertex_pointer->latitude;
    stop_long = vertex_pointer->longitude;
    vertex_pointer = vertex_pointer->next;
    while (vertex_pointer != NULL)
    {
      //greater than one point, get current point.
      stop_lat = vertex_pointer->latitude;
      stop_long = vertex_pointer->longitude;
      if (stop_lat!=start_lat || stop_long!=start_long)
      {
        atleast_one_different = 1;
      }
      vertex_pointer = vertex_pointer->next;
      vertex_count++;
    }
    if (vertex_count>2 && start_lat==stop_lat && start_long==stop_long && atleast_one_different > 0)
    {
      // more than two points, and they aren't in the same place
      result = 0;
    }
  }
  return result;
}





// Compute the area enclosed by a CAD object.  Check that it is a
// closed, non-intersecting polygon first.
//
double CAD_object_compute_area(CADRow *CAD_list_head)
{
  VerticeRow *tmp;
  double area;
  char temp_course[20];
  // Walk the linked list, computing the area of the
  // polygon.  Greene's Theorem is how we can compute the area of
  // a polygon using the vertices.  We could also compute whether
  // we're going clockwise or counter-clockwise around the polygon
  // using Greene's Theorem.  In fact I think we do that for
  // Shapefile hole polygons.  Remember that here we're walking
  // around the vertices backwards due to the ordering of the
  // list.  Shouldn't matter for our purposes though.
  //
  area = 0.0;
  tmp = CAD_list_head->start;
  if (is_CAD_object_open(CAD_list_head)==0)
  {
    // Only compute the area if CAD object is a closed polygon,
    // that is, not an open polygon.
    while (tmp->next != NULL)
    {
      double dx0, dy0, dx1, dy1;

      // Because lat/long units can vary drastically w.r.t. real
      // units, we need to multiply the terms by the real units in
      // order to get real area.

      // Compute real distances from a fixed point.  Convert to
      // the current measurement units.  We'll use the starting
      // vertice as our fixed point.
      //
      dx0 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              CAD_list_head->start->latitude,
              tmp->longitude,
              temp_course,
              sizeof(temp_course));

      if (tmp->longitude < CAD_list_head->start->longitude)
      {
        dx0 = -dx0;
      }

      dy0 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              tmp->latitude,
              CAD_list_head->start->longitude,
              temp_course,
              sizeof(temp_course));

      if (tmp->latitude < CAD_list_head->start->latitude)
      {
        dx0 = -dx0;
      }

      dx1 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              CAD_list_head->start->latitude,
              tmp->next->longitude,
              temp_course,
              sizeof(temp_course));

      if (tmp->next->longitude < CAD_list_head->start->longitude)
      {
        dx0 = -dx0;
      }

      dy1 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              tmp->next->latitude,
              CAD_list_head->start->longitude,
              temp_course,
              sizeof(temp_course));

      // Add the minus signs back in, if any
      if (tmp->longitude < CAD_list_head->start->longitude)
      {
        dx0 = -dx0;
      }
      if (tmp->latitude < CAD_list_head->start->latitude)
      {
        dy0 = -dy0;
      }
      if (tmp->next->longitude < CAD_list_head->start->longitude)
      {
        dx1 = -dx1;
      }
      if (tmp->next->latitude < CAD_list_head->start->latitude)
      {
        dy1 = -dy1;
      }

      // Greene's Theorem:  Summation of the following, then
      // divide by two:
      //
      // A = X Y    - X   Y
      //  i   i i+1    i+1 i
      //
      area += (dx0 * dy1) - (dx1 * dy0);

      tmp = tmp->next;
    }
    area = 0.5 * area;
  }

  if (area < 0.0)
  {
    area = -area;
  }

//fprintf(stderr,"Square nautical miles: %f\n", area);

  return area;

}





// Allocate a label for an object, and place it according to the
// user's requests.  Keep track of where from the origin to place
// the label, font to use, color, etc.
void CAD_object_allocate_label(void)
{
}





// Set the probability for an object.  We should probably allocate
// the raw probability to small "buckets" within the closed polygon.
// This will allow us to split/join polygons later without messing
// up the probablity assigned to each area originally.  Check that
// it is a closed polygon first.
// if as_percent==TRUE, then probability is treated as a percent
// (expected to be a value between 0 and 100).
// otherwise, then probability is treated as a probability
// (expected to be a value between 0 and 1).
//
void CAD_object_set_raw_probability(CADRow *object_ptr, float probability, int as_percent)
{
  // initial implementation just assigns a single raw probability to the whole polygon.
  // internal storage is as a probability between 0 and 1
  // users will usually want to manipulate this as a percent (between 0 and 100)
  // thus the get and set functions are aware of both internal storage and
  // the user's request and return an appropriately scaled value.
  if (as_percent==TRUE)
  {
    // convert from a percent to a probability between 0 and 1
    object_ptr->raw_probability = (probability/100.00);
  }
  else
  {
    // treat as in internal storage form
    object_ptr->raw_probability = probability;
  }
}





// Get the raw probability for an object.  Sum up the raw
// probability "buckets" contained within the closed polygon.  Check
// that it _is_ a closed polygon first.
//
float CAD_object_get_raw_probability(CADRow *object_ptr, int as_percent)
{
  float result = 0.0;
  // not checking yet for closure
  if (object_ptr != NULL)
  {
    // initial implementation returns just the single raw probability
    result = object_ptr->raw_probability;
    if (as_percent > 0)
    {
      // raw probability is a probability between 0 an 1,
      // this may be desired as a percent.
      result = result * 100;
    }
  }
#ifdef CAD_DEBUG
  fprintf(stderr,"Getting Probability: %01.5f\n",result);
#endif
  return result;
}





void CAD_object_set_line_width(void)
{
}





void CAD_object_set_color(void)
{
}





void CAD_object_set_linetype(void)
{
}





// Used to break a line segment into two.  Can then move the vertice
// if needed.  Recompute the raw probability if need be, or make it
// an invalid value so that we know we need to recompute it.
void CAD_vertice_insert_new(void)
{
  // Check whether a line segment will cross another?
}





// Move an existing vertice.  Recompute the raw probability if need
// be, or make it an invalid value so that we know we need to
// recompute it.
void CAD_vertice_move(void)
{
  // Check whether a line segment will cross another?
}





// Set the location for drawing the label of an area to the center
// of the area.  Takes a pointer to a CAD object as a parameter.
// Sets the label_latitude and label_longitude attributes of the CAD
// object to the center of the region described by the vertices of
// the object.
//
void CAD_object_set_label_at_centroid(CADRow *CAD_object)
{
  // *** current implementation approximates the center as the
  // average of the largest and smallest of each of latitude
  // and longitude rather than correctly computing the centroid,
  // that is, it places the label at the centroid of a bounding
  // box for the area.  ***
  // We can't use a simple x=sum(x)/n, y=sum(y)/n as the
  // points on the outline shouldn't be weighted equally.
  // Ideal would be to place the label at the central point within
  // the area itslef, apparently this is a hard prbolem.
  // alternative would be to use the centroid, which like the
  // average of maximum and minimum values may lie outside of
  // the area.
  VerticeRow *vertex_pointer;
  long min_lat, min_long;
  long max_lat, max_long;
  // Walk the linked list and compute the centroid of the bounding box.
  vertex_pointer = CAD_object->start;
  min_lat = 0.0;
  min_long = 0.0;

  // Set the latitude and longitude of the label to the
  // centroid of the bounding box.
  // Start by setting lat and long of label to first point.
  CAD_object->label_latitude = vertex_pointer->latitude;
  CAD_object->label_longitude = vertex_pointer->longitude;
  if (vertex_pointer != NULL)
  {
    // Iterate through the vertices and calculate the center x and y position
    // based on an average of the largest and smallest latitudes and longitudes.
    min_lat = vertex_pointer->latitude;
    min_long = vertex_pointer->longitude;
    max_lat = vertex_pointer->latitude;
    max_long = vertex_pointer->longitude;
    while (vertex_pointer != NULL)
    {
      if (vertex_pointer->next != NULL)
      {
        if (vertex_pointer->longitude < min_long )
        {
          min_long = vertex_pointer->longitude;
        }
        if (vertex_pointer->latitude < min_lat )
        {
          min_lat = vertex_pointer->latitude;
        }
        if (vertex_pointer->longitude > max_long )
        {
          max_long = vertex_pointer->longitude;
        }
        if (vertex_pointer->latitude > max_lat )
        {
          max_lat = vertex_pointer->latitude;
        }
      }
      vertex_pointer = vertex_pointer->next;
    }
    CAD_object->label_latitude = (max_lat + min_lat)/2.0;
    CAD_object->label_longitude = (max_long + min_long)/2.0;
  }
}





// This is the callback for the CAD objects parameters dialog.  It
// takes the values entered in the dialog and stores them in the
// most recently created object.
//
void Set_CAD_object_parameters (Widget widget,
                                XtPointer clientData,
                                XtPointer calldata)
{

  float probability = 0.0;
  CADRow *target_object = NULL;
  int cb_selected;
  // need to find out object to edit from clientData rather than
  // using the first object in list as the one to edit.
  //target_object = CAD_list_head;
  target_object = (CADRow *)clientData;

  // set label, comment, and probability for area
  xastir_snprintf(target_object->label,
                  sizeof(target_object->label),
                  "%s", XmTextGetString(cad_label_data)
                 );
  xastir_snprintf(target_object->comment,
                  sizeof(target_object->comment),
                  "%s", XmTextGetString(cad_comment_data)
                 );
  // Is more error checking needed?  atof appears to correctly handle
  // empty input, reasonable probability values, and text (0.00).
  // User side probabilities are expressed as percent.
  probability = atof(XmTextGetString(cad_probability_data));
  CAD_object_set_raw_probability(target_object, probability, TRUE);

  // Use the selected line type, default is dashed
  cb_selected = FALSE;

#ifdef USE_COMBO_BOX
  XtVaGetValues(cad_line_style_data,
                XmNselectedPosition,
                &cb_selected,
                NULL);
#else
  cb_selected = clsd_value;
#endif // USE_COMBO_BOX        

  if (cb_selected)
  {
    target_object->line_type = cb_selected;
  }
  else
  {
    target_object->line_type = 2; // LineOnOffDash
  }

  if (cad_list_dialog)
  {
    Update_CAD_objects_list_dialog();
  }

  // close object_parameters dialog
  XtPopdown(cad_dialog);
  XtDestroyWidget(cad_dialog);
  cad_dialog = (Widget)NULL;

  Save_CAD_Objects_to_file();
  // Reload symbols/tracks/CAD objects so that object name will show on map.
  redraw_symbols(da);

  // Here we update the erase cad objects dialog if it is up on
  // the screen.  We get rid of it and re-establish it, which will
  // usually make the dialog move, but this is better than having
  // it be out-of-date.
  //
  if (cad_erase_dialog != NULL)
  {
    Draw_CAD_Objects_erase_dialog_close(widget,clientData,calldata);
    Draw_CAD_Objects_erase_dialog(widget,clientData,calldata);
  }

  // Here we update the edit cad objects dialog by getting rid of
  // it and then re-establishing it if it is active when we start.
  // This will usually make the dialog move, but it's better than
  // having it be out-of-date.
  //
  if (cad_list_dialog!=NULL)
  {
    // Update the Edit CAD Objects list
    Draw_CAD_Objects_list_dialog_close(widget, clientData, calldata);
    Draw_CAD_Objects_list_dialog(widget, clientData, calldata);
  }
}





// Update the list of existing CAD objects on the cad list dialog to
// reflect the current list of objects.
//
void Update_CAD_objects_list_dialog(void)
{
  CADRow *object_ptr = CAD_list_head;
  int counter = 1;
  XmString cb_item;

  if (list_of_existing_CAD_objects_edit!=NULL && cad_list_dialog)
  {
    XmListDeleteAllItems(list_of_existing_CAD_objects_edit);

    // iterate through list of objects to populate scrolled list
    while (object_ptr != NULL)
    {

      //  If no label, use the string "<Empty Label>" instead
      if (object_ptr->label[0] == '\0')
      {
        cb_item = XmStringCreateLtoR("<Empty Label>", XmFONTLIST_DEFAULT_TAG);
      }
      else
      {
        cb_item = XmStringCreateLtoR(object_ptr->label, XmFONTLIST_DEFAULT_TAG);
      }

      XmListAddItem(list_of_existing_CAD_objects_edit,
                    cb_item,
                    counter);
      counter++;
      XmStringFree(cb_item);
      object_ptr = object_ptr->next;
    }
  }
}





void close_object_params_dialog(Widget widget, XtPointer clientData, XtPointer calldata)
{
  XtPopdown(cad_dialog);
  XtDestroyWidget(cad_dialog);
  cad_dialog = (Widget)NULL;

  // Here we update the erase cad objects dialog if it is up on
  // the screen.  We get rid of it and re-establish it, which will
  // usually make the dialog move, but this is better than having
  // it be out-of-date.
  //
  if (cad_erase_dialog != NULL)
  {
    Draw_CAD_Objects_erase_dialog_close(widget,clientData,calldata);
    Draw_CAD_Objects_erase_dialog(widget,clientData,calldata);
  }

  // Here we update the edit cad objects dialog by getting rid of
  // it and then re-establishing it if it is active when we start.
  // This will usually make the dialog move, but it's better than
  // having it be out-of-date.
  //
  if (cad_list_dialog!=NULL)
  {
    // Update the Edit CAD Objects list
    Draw_CAD_Objects_list_dialog_close(widget, clientData, calldata);
    Draw_CAD_Objects_list_dialog(widget, clientData, calldata);
  }
}


#ifndef USE_COMBO_BOX
void clsd_menuCallback(Widget widget, XtPointer ptr, XtPointer callData)
{
  //XmPushButtonCallbackStruct *data = (XmPushButtonCallbackStruct *)callData;
  XtPointer userData;

  XtVaGetValues(widget, XmNuserData, &userData, NULL);

  //clsd_menu is zero based, cad_line_style_data constants are one based.
  clsd_value = (int)userData + 1;
  if (debug_level & 1)
  {
    fprintf(stderr,"Selected value on cad line type pulldown: %d\n",clsd_value);
  }
}
#endif  // !USE_COMBO_BOX



// Create a dialog to obtain information about a newly created CAD
// object from the user.  Values of probability, name, and comment
// are initially blank.  Takes as a parameter a string describing
// the area of the object.  There is a single button with a callback
// to Set_CAD_object_parameters, which stores values from the dialog
// in the object's struct.  Should be generalized to allow editing
// of a pre-existing CAD object (except for the name).  Parameter
// should be a pointer to the object.
//
void Set_CAD_object_parameters_dialog(char *area_description, CADRow *CAD_object)
{
  Widget cad_pane, cad_form,
         cad_label,
         cad_comment,
         cad_probability,
         cad_line_style,
         button_done,
         button_cancel;
  char   probability_string[5];
  int i;  // loop counters
  //XmString cb_item;  // used to create picklist of line styles
  XmString cb_items[3];
#ifndef USE_COMBO_BOX
  Widget clsd_menuPane;
  Widget clsd_button;
  Widget clsd_buttons[3];
  Widget clsd_menu;
  char buf[18];
  int x;
  Arg args[12]; // available for XtSetArguments
#endif // !USE_COMBO_BOX
  Widget clsd_widget;


  if (cad_dialog)
  {
    (void)XRaiseWindow(XtDisplay(cad_dialog), XtWindow(cad_dialog));
  }
  else
  {

    // Area Object"
    cad_dialog = XtVaCreatePopupShell(langcode("CADPUD001"),
                                      xmDialogShellWidgetClass,
                                      appshell,
                                      XmNdeleteResponse,          XmDESTROY,
                                      XmNdefaultPosition,         FALSE,
                                      XmNfontList, fontlist1,
                                      NULL);

    cad_pane = XtVaCreateWidget("Set_Del_Object pane",
                                xmPanedWindowWidgetClass,
                                cad_dialog,
                                MY_FOREGROUND_COLOR,
                                MY_BACKGROUND_COLOR,
                                NULL);

    cad_form =  XtVaCreateWidget("Set_Del_Object ob_form",
                                 xmFormWidgetClass,
                                 cad_pane,
                                 XmNfractionBase,            2,
                                 XmNautoUnmanage,            FALSE,
                                 XmNshadowThickness,         1,
                                 MY_FOREGROUND_COLOR,
                                 MY_BACKGROUND_COLOR,
                                 NULL);
    // Area of polygon, already scaled and internationalized.
    cad_label = XtVaCreateManagedWidget(area_description,
                                        xmLabelWidgetClass,
                                        cad_form,
                                        XmNtopAttachment,           XmATTACH_FORM,
                                        XmNtopOffset,               10,
                                        XmNbottomAttachment,        XmATTACH_NONE,
                                        XmNleftAttachment,          XmATTACH_FORM,
                                        XmNleftOffset,              10,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);
    // "Area Label:"
    cad_label = XtVaCreateManagedWidget(langcode("CADPUD002"),
                                        xmLabelWidgetClass,
                                        cad_form,
                                        XmNtopAttachment,           XmATTACH_FORM,
                                        XmNtopOffset,               50,
                                        XmNbottomAttachment,        XmATTACH_NONE,
                                        XmNleftAttachment,          XmATTACH_FORM,
                                        XmNleftOffset,              10,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);
    // label text field
    cad_label_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                     xmTextFieldWidgetClass,
                     cad_form,
                     XmNeditable,                TRUE,
                     XmNcursorPositionVisible,   TRUE,
                     XmNsensitive,               TRUE,
                     XmNshadowThickness,         1,
                     XmNcolumns,                 20,
                     XmNmaxLength,               CAD_LABEL_MAX_SIZE - 1,
                     XmNtopAttachment,           XmATTACH_FORM,
                     XmNtopOffset,               50,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_WIDGET,
                     XmNleftWidget,              cad_label,
                     XmNrightAttachment,         XmATTACH_NONE,
                     XmNbackground,              colors[0x0f],
                     XmNfontList, fontlist1,
                     NULL);
    // "Comment"
    cad_comment = XtVaCreateManagedWidget(langcode("CADPUD003"),
                                          xmLabelWidgetClass,
                                          cad_form,
                                          XmNtopAttachment,           XmATTACH_FORM,
                                          XmNtopOffset,               90,
                                          XmNbottomAttachment,        XmATTACH_NONE,
                                          XmNleftAttachment,          XmATTACH_FORM,
                                          XmNleftOffset,              10,
                                          XmNrightAttachment,         XmATTACH_NONE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);
    // comment text field
    cad_comment_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                       xmTextFieldWidgetClass,
                       cad_form,
                       XmNeditable,                TRUE,
                       XmNcursorPositionVisible,   TRUE,
                       XmNsensitive,               TRUE,
                       XmNshadowThickness,         1,
                       XmNcolumns,                 40,
                       XmNmaxLength,               CAD_COMMENT_MAX_SIZE - 1,
                       XmNtopAttachment,           XmATTACH_FORM,
                       XmNtopOffset,               90,
                       XmNbottomAttachment,        XmATTACH_NONE,
                       XmNleftAttachment,          XmATTACH_WIDGET,
                       XmNleftWidget,              cad_comment,
                       XmNrightAttachment,         XmATTACH_NONE,
                       XmNbackground,              colors[0x0f],
                       XmNfontList, fontlist1,
                       NULL);
    // "Probability (as %)"
    cad_probability = XtVaCreateManagedWidget(langcode("CADPUD004"),
                      xmLabelWidgetClass,
                      cad_form,
                      XmNtopAttachment,           XmATTACH_FORM,
                      XmNtopOffset,               130,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNleftAttachment,          XmATTACH_FORM,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);
    // probability field
    cad_probability_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                           xmTextFieldWidgetClass,
                           cad_form,
                           XmNeditable,                TRUE,
                           XmNcursorPositionVisible,   TRUE,
                           XmNsensitive,               TRUE,
                           XmNshadowThickness,         1,
                           XmNcolumns,                 5,
                           XmNmaxLength,               5,
                           XmNtopAttachment,           XmATTACH_FORM,
                           XmNtopOffset,               130,
                           XmNbottomAttachment,        XmATTACH_NONE,
                           XmNleftAttachment,          XmATTACH_WIDGET,
                           XmNleftWidget,              cad_probability,
                           XmNrightAttachment,         XmATTACH_NONE,
                           XmNbackground,              colors[0x0f],
                           XmNfontList, fontlist1,
                           NULL);
    // Boundary Line Type
    cad_line_style = XtVaCreateManagedWidget("Line Type:",
                     xmLabelWidgetClass,
                     cad_form,
                     XmNtopAttachment,           XmATTACH_WIDGET,
                     XmNtopWidget,               cad_probability_data,
                     XmNtopOffset,               5,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_FORM,
                     XmNleftOffset,              10,
                     XmNrightAttachment,         XmATTACH_NONE,
                     MY_FOREGROUND_COLOR,
                     MY_BACKGROUND_COLOR,
                     XmNfontList, fontlist1,
                     NULL);

    // lesstif as of 0.95 in 2008 doesn't fully support combo boxes
    //
    // Need to replace combo boxes with a pull down menu when lesstif is used.
    // See xpdf's  XPDFViewer.cc/XPDFViewer.h for an example.
    //cb_items = (XmString *) XtMalloc ( sizeof (XmString) * 4 );
    // Solid
    cb_items[0] = XmStringCreateLtoR( langcode("CADPUD012"), XmFONTLIST_DEFAULT_TAG);
    // Dashed
    cb_items[1] = XmStringCreateLtoR( langcode("CADPUD013"), XmFONTLIST_DEFAULT_TAG);
    // Double Dash
    cb_items[2] = XmStringCreateLtoR( langcode("CADPUD014"), XmFONTLIST_DEFAULT_TAG);

    clsd_widget = cad_line_style_data;

#ifdef USE_COMBO_BOX
    // Combo box to pick line style
    cad_line_style_data = XtVaCreateManagedWidget("select line style",
                          xmComboBoxWidgetClass,
                          cad_form,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               cad_probability_data,
                          XmNtopOffset,               5,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              cad_line_style,
                          XmNleftOffset,              10,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNnavigationType,          XmTAB_GROUP,
                          XmNcomboBoxType,            XmDROP_DOWN_LIST,
                          XmNpositionMode,            XmONE_BASED,
                          XmNvisibleItemCount,        3,
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          XmNfontList, fontlist1,
                          NULL);
    XmComboBoxAddItem(cad_line_style_data,cb_items[0],1,1);
    XmComboBoxAddItem(cad_line_style_data,cb_items[1],2,1);
    XmComboBoxAddItem(cad_line_style_data,cb_items[2],3,1);

    clsd_widget = cad_line_style_data;
#else
    // menu replacement for combo box when using lesstif
    x = 0;
    XtSetArg(args[x], XmNmarginWidth, 0);
    ++x;
    XtSetArg(args[x], XmNmarginHeight, 0);
    ++x;
    XtSetArg(args[x], XmNfontList, fontlist1);
    ++x;
    clsd_menuPane = XmCreatePulldownMenu(cad_form,"sddd_menuPane", args, x);
    //sddd_menu is zero based, constants for database types are one based.
    //sddd_value is set to match constants in callback.
    for (i=0; i<3; i++)
    {
      x = 0;
      XtSetArg(args[x], XmNlabelString, cb_items[i]);
      x++;
      XtSetArg(args[x], XmNuserData, (XtPointer)i);
      x++;
      XtSetArg(args[x], XmNfontList, fontlist1);
      ++x;
      sprintf(buf,"button%d",i);
      clsd_button = XmCreatePushButton(clsd_menuPane, buf, args, x);
      XtManageChild(clsd_button);
      XtAddCallback(clsd_button, XmNactivateCallback, clsd_menuCallback, Set_CAD_object_parameters_dialog);
      clsd_buttons[i] = clsd_button;
    }
    x = 0;
    XtSetArg(args[x], XmNleftAttachment, XmATTACH_WIDGET);
    ++x;
    XtSetArg(args[x], XmNleftWidget, cad_line_style);
    ++x;
    XtSetArg(args[x], XmNtopAttachment, XmATTACH_WIDGET);
    ++x;
    XtSetArg(args[x], XmNtopWidget, cad_probability_data);
    ++x;
    XtSetArg(args[x], XmNmarginWidth, 0);
    ++x;
    XtSetArg(args[x], XmNmarginHeight, 0);
    ++x;
    XtSetArg(args[x], XmNtopOffset, 5);
    ++x;
    XtSetArg(args[x], XmNleftOffset, 10);
    ++x;
    XtSetArg(args[x], XmNsubMenuId, clsd_menuPane);
    ++x;
    XtSetArg(args[x], XmNfontList, fontlist1);
    ++x;
    clsd_menu = XmCreateOptionMenu(cad_form, "sddd_Menu", args, x);
    XtManageChild(clsd_menu);
    clsd_value = 2;   // set a default value (line on off dash)
    clsd_widget = clsd_menu;
#endif  // USE_COMBO_BOX
    // free up space from combo box strings
    for (i=0; i<3; i++)
    {
      XmStringFree(cb_items[i]);
    }


    // "OK"
    button_done = XtVaCreateManagedWidget(langcode("CADPUD005"),
                                          xmPushButtonGadgetClass,
                                          cad_form,
                                          XmNtopAttachment,     XmATTACH_WIDGET,
                                          XmNtopWidget,         clsd_widget,
                                          XmNtopOffset,         5,
                                          XmNbottomAttachment,  XmATTACH_FORM,
                                          XmNbottomOffset,      5,
                                          XmNleftAttachment,    XmATTACH_POSITION,
                                          XmNleftPosition,      0,
                                          XmNrightAttachment,   XmATTACH_POSITION,
                                          XmNrightPosition,     1,
                                          XmNnavigationType,    XmTAB_GROUP,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

    // "Cancel"
    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                                            xmPushButtonGadgetClass,
                                            cad_form,
                                            XmNtopAttachment,     XmATTACH_WIDGET,
                                            XmNtopWidget,         clsd_widget,
                                            XmNtopOffset,         5,
                                            XmNbottomAttachment,  XmATTACH_FORM,
                                            XmNbottomOffset,      5,
                                            XmNleftAttachment,    XmATTACH_POSITION,
                                            XmNleftPosition,      1,
                                            XmNrightAttachment,   XmATTACH_POSITION,
                                            XmNrightPosition,     2,
                                            XmNnavigationType,    XmTAB_GROUP,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);


    // callback depends on whether this is a new or old object
    //XtAddCallback(button_done, XmNactivateCallback, Set_CAD_object_parameters, Set_CAD_object_parameters_dialog);

    if (CAD_object!=NULL)
    {
      XtAddCallback(button_done, XmNactivateCallback, Set_CAD_object_parameters, (XtPointer *)CAD_object);
    }
    else
    {
      // called to get information for a newly created cad object
      // pass pointer to the head of the list, which contains
      // the most recently created cad object.
      XtAddCallback(button_done, XmNactivateCallback, Set_CAD_object_parameters, CAD_list_head);
    }

    XtAddCallback(button_cancel, XmNactivateCallback, close_object_params_dialog, NULL);

    pos_dialog(cad_dialog);
    XmInternAtom(XtDisplay(cad_dialog),"WM_DELETE_WINDOW", FALSE);

    XtManageChild(cad_form);
    XtManageChild(cad_pane);

    XtPopup(cad_dialog,XtGrabNone);
  } // end if ! caddialog

  if (CAD_object!=NULL)
  {
    XmString tempSelection;

    // given an existing object, fill form with its information
    XmTextFieldSetString(cad_label_data,CAD_object->label);
    XmTextFieldSetString(cad_comment_data,CAD_object->comment);
    xastir_snprintf(probability_string,
                    sizeof(probability_string),
                    "%01.2f",
                    CAD_object_get_raw_probability(CAD_object,1));
    XmTextFieldSetString(cad_probability_data,probability_string);

    switch(CAD_object->line_type)
    {

      case 1: // Solid
#ifndef USE_COMBO_BOX
        i = 0;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD012"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;

      case 2: // Dashed
#ifndef USE_COMBO_BOX
        i = 1;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD013"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;

      case 3: // Double Dash
#ifndef USE_COMBO_BOX
        i = 2;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD014"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;

      default:
#ifndef USE_COMBO_BOX
        i = 1;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD013"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;
    }
#ifdef USE_COMBO_BOX
    XmComboBoxSelectItem(cad_line_style_data, tempSelection);
#else
    clsd_value = i+1;
    //clsd_menu is zero based, line types are one based.
    //clsd_value matches line types (1-3).
    XtVaSetValues(clsd_menu, XmNmenuHistory, clsd_buttons[i], NULL);
#endif // USE_COMBO_BOX
    XmStringFree(tempSelection);
  }
}





static Cursor cs_CAD = (Cursor)NULL;

void free_cs_CAD(void)
{
  XFreeCursor(XtDisplay(da), cs_CAD);
  cs_CAD = (Cursor)NULL;
}

// This is the callback for the Draw togglebutton
//
void Draw_CAD_Objects_mode( Widget UNUSED(widget),
                            XtPointer UNUSED(clientData),
                            XtPointer callData)
{

  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;


  if(state->set)
  {
    draw_CAD_objects_flag = 1;

    // Create the "pencil" cursor so we know what mode we're in.
    //
    if(!cs_CAD)
    {
      cs_CAD=XCreateFontCursor(XtDisplay(da),XC_pencil);
      atexit(free_cs_CAD);
    }

    // enable the close polygon button on an open CAD menu
    if (CAD_close_polygon_menu_item)
    {
      XtSetSensitive(CAD_close_polygon_menu_item,TRUE);
    }

    (void)XDefineCursor(XtDisplay(da),XtWindow(da),cs_CAD);
    (void)XFlush(XtDisplay(da));

    draw_CAD_objects_flag = 1;
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position
  }
  else
  {
    draw_CAD_objects_flag = 0;
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position

    Save_CAD_Objects_to_file();

    // Remove the special "pencil" cursor.
    (void)XUndefineCursor(XtDisplay(da),XtWindow(da));
    (void)XFlush(XtDisplay(da));

    // disable the close polygon button on an open CAD menu.
    if (CAD_close_polygon_menu_item)
    {
      XtSetSensitive(CAD_close_polygon_menu_item,FALSE);
    }
  }
}





// Called when we complete a new CAD object.  Save the object to
// disk so that we can recover in the case of a crash or power
// failure.  Save any old file to a backup file.  Perhaps write them
// to numbered backup files so that we keep several on-hand?
//
void Save_CAD_Objects_to_file(void)
{
  FILE *f;
  char *file;
  CADRow *object_ptr = CAD_list_head;
  char temp_file_path[MAX_VALUE];

  fprintf(stderr,"Saving CAD objects to file\n");

  // Save in ~/.xastir/config/CAD_object.log
  file = get_user_base_dir("config/CAD_object.log", temp_file_path, sizeof(temp_file_path));
  f = fopen(file,"w+");

  if (f == NULL)
  {
    fprintf(stderr,
            "Couldn't open config/CAD_object.log file for writing!\n");
    return;
  }

  while (object_ptr != NULL)
  {
    VerticeRow *vertice = object_ptr->start;

    // Write out the main object info:
    fprintf(f,"\nCAD_Object\n");
    fprintf(f,"creation_time:   %lu\n",(unsigned long)object_ptr->creation_time);
    fprintf(f,"line_color:      %d\n",object_ptr->line_color);
    fprintf(f,"line_type:       %d\n",object_ptr->line_type);
    fprintf(f,"line_width:      %d\n",object_ptr->line_width);
    fprintf(f,"computed_area:   %f\n",object_ptr->computed_area);
    fprintf(f,"raw_probability: %f\n",CAD_object_get_raw_probability(object_ptr,TRUE));
    fprintf(f,"label_latitude:  %lu\n",object_ptr->label_latitude);
    fprintf(f,"label_longitude: %lu\n",object_ptr->label_longitude);
    fprintf(f,"label: %s\n",object_ptr->label);
    if (strlen(object_ptr->comment)>1)
    {
      fprintf(f,"comment: %s\n",object_ptr->comment);
    }
    else
    {
      fprintf(f,"comment: NULL\n");
    }

    // Iterate through the vertices:
    while (vertice != NULL)
    {

      fprintf(f,"Vertice: %lu %lu\n",
              vertice->latitude,
              vertice->longitude);

      vertice = vertice->next;
    }
    object_ptr = object_ptr->next;
  }
  (void)fclose(f);
}





// Called by main() when we start Xastir.  Restores CAD objects
// created in earlier Xastir sessions.
//
void Restore_CAD_Objects_from_file(void)
{
  FILE *f;
  char *file;
  char line[MAX_FILENAME];
  char temp_file_path[MAX_VALUE];
#ifdef CAD_DEBUG
  fprintf(stderr,"Restoring CAD objects from file\n");
#endif

  // Restore from ~/.xastir/config/CAD_object.log
  file = get_user_base_dir("config/CAD_object.log", temp_file_path, sizeof(temp_file_path));
  f = fopen(file,"r");

  if (f == NULL)
  {
#ifdef CAD_DEBUG
    fprintf(stderr,
            "Couldn't open config/CAD_object.log file for reading!\n");
#endif
    return;
  }

  while (!feof (f))
  {
    (void)get_line(f, line, MAX_FILENAME);
    if (strncasecmp(line,"CAD_Object",10) == 0)
    {
      // Found a new CAD Object declaration!

      //fprintf(stderr,"Found CAD_Object\n");

      // Malloc a new object, add it to the linked list, start
      // filling in the fields.
      //
      // This gives us a default object with one vertice.  We
      // can replace all of the fields in it as we parse them.
      CAD_object_allocate(0l, 0l);

      // Remove the one vertice from the newly allocated
      // object so that we don't end up with one too many
      // vertices when all done.
      CAD_vertice_delete_all(CAD_list_head->start);
      CAD_list_head->start = NULL;

    }
    else if (strncasecmp(line,"creation_time:",14) == 0)
    {
      //fprintf(stderr,"Found creation_time:\n");
      unsigned long temp_time;
      if (1 != sscanf(line+15, "%lu",&temp_time))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [creation_time]\n");
      }
      CAD_list_head->creation_time=(time_t)temp_time;
    }
    else if (strncasecmp(line,"line_color:",11) == 0)
    {
      //fprintf(stderr,"Found line_color:\n");
      if (1 != sscanf(line+12,"%d",
                      &CAD_list_head->line_color))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [line_color]\n");
      }
    }
    else if (strncasecmp(line,"line_type:",10) == 0)
    {
      //fprintf(stderr,"Found line_type:\n");
      if (1 != sscanf(line+11,"%d",
                      &CAD_list_head->line_type))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [line_type]\n");
      }
    }
    else if (strncasecmp(line,"line_width:",11) == 0)
    {
      //fprintf(stderr,"Found line_width:\n");
      if (1 != sscanf(line+12,"%d",
                      &CAD_list_head->line_width))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [line_width]\n");
      }
    }
    else if (strncasecmp(line,"computed_area:",14) == 0)
    {
      //fprintf(stderr,"Found computed_area:\n");
      if (1 != sscanf(line+15,"%f",
                      &CAD_list_head->computed_area))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [computed_area]\n");
      }
    }
    else if (strncasecmp(line,"raw_probability:",16) == 0)
    {
      //fprintf(stderr,"Found raw_probability:\n");
      if (1 != sscanf(line+17,"%f",
                      &CAD_list_head->raw_probability))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [raw_probability]\n");
      }
      else
      {
        // External storage is as percent, need to make sure that this
        // fits the expected internal storage format.
        // Thus take given value and store using method that knows
        // how to handle percents
        CAD_object_set_raw_probability(CAD_list_head,CAD_list_head->raw_probability,TRUE);
      }
    }
    else if (strncasecmp(line,"label_latitude:",15) == 0)
    {
      //fprintf(stderr,"Found label_latitude:\n");
      if (1 != sscanf(line+16,"%lu",
                      (unsigned long *)&CAD_list_head->label_latitude))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [label_latitude]\n");
      }
    }
    else if (strncasecmp(line,"label_longitude:",16) == 0)
    {
      //fprintf(stderr,"Found label_longitude:\n");
      if (1 != sscanf(line+17,"%lu",
                      (unsigned long *)&CAD_list_head->label_longitude))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [label_longitude]\n");
      }
    }
    else if (strncasecmp(line,"label:",6) == 0)
    {
      //fprintf(stderr,"Found label:\n");
      xastir_snprintf(CAD_list_head->label,
                      sizeof(CAD_list_head->label),
                      "%s",
                      line+7);
    }
    else if (strncasecmp(line,"comment:",8) == 0)
    {
      //fprintf(stderr,"Found comment:\n");
      xastir_snprintf(CAD_list_head->comment,
                      sizeof(CAD_list_head->comment),
                      "%s",
                      line+9);

      if (strcmp(CAD_list_head->comment,"NULL")==0)
      {
        xastir_snprintf(CAD_list_head->comment,
                        sizeof(CAD_list_head->comment),
                        "%c",
                        '\0'
                       );
      }
    }
    else if (strncasecmp(line,"Vertice:",8) == 0)
    {
      long latitude, longitude;

      //fprintf(stderr,"Found Vertice:\n");
      if (2 != sscanf(line+9,"%lu %lu",
                      (unsigned long *)&latitude,
                      (unsigned long *)&longitude))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [vertex]\n");
      }
      CAD_vertice_allocate(latitude,longitude);
    }
    else
    {
      // Else not recognized, do nothing with it!
      //fprintf(stderr,"Found unrecognized line\n");
    }
  }
  (void)fclose(f);
  // Reload symbols/tracks/CAD objects to draw the loaded objects
  redraw_symbols(da);
}





// popdown and destroy the cad_erase_dialog.
//
void Draw_CAD_Objects_erase_dialog_close ( Widget UNUSED(w),
    XtPointer UNUSED(clientData),
    XtPointer UNUSED(callData) )
{

  if (cad_erase_dialog!=NULL)
  {
    // close cad_erase_dialog
    XtPopdown(cad_erase_dialog);
    XtDestroyWidget(cad_erase_dialog);
    cad_erase_dialog = (Widget)NULL;
  }

}





// Call back for delete selected button on
// Draw_CAD_Objects_erase_dialog.  Iterates through the list of
// selected CAD objects and deletes them.
//
void Draw_CAD_Objects_erase_selected ( Widget w,
                                       XtPointer clientData,
                                       XtPointer callData)
{
  int itemCount;       // number of items in list of CAD objects.
  XmString *listItems; // names of CAD objects on list
  char *cadName;       // the text name of a CAD object
  Position x;          // position on list
  char *selectedName;  // the text name of a selected CAD object
  CADRow *object_ptr = CAD_list_head;  // pointer to the linked list of CAD objects
  int done = 0;        // has a cad object with a name matching the current selection been found

  // For more than a few objects this loop/save/redraw will need to move
  // off to a separate thread.

  XtVaGetValues(list_of_existing_CAD_objects,
                XmNitemCount,&itemCount,
                XmNitems,&listItems,
                NULL);
  // iterate through list and delete each first object with a name matching
  // those that are selected on the list.
  //
  // *** Note: If names are not unique the results may not be what the user expects.
  // The first match to a selection will be deleted, not necessarily the selection.
  //
  for (x=1; x<=itemCount; x++)
  {

    if (done)
    {
      break;
    }

    if (XmListPosSelected(list_of_existing_CAD_objects,x))
    {
      int no_label = 0;

      XmStringGetLtoR(listItems[(x-1)],XmFONTLIST_DEFAULT_TAG,&selectedName);

      // Check for our own definition of no label for the CAD
      // objects, which is "<Empty Label>"
      if (strcmp(selectedName,"<Empty Label>") == 0)
      {
        no_label++;
      }

      object_ptr = CAD_list_head;
      done = 0;

      while (object_ptr != NULL && done == 0)
      {

        cadName = object_ptr->label;

        if (strcmp(cadName,selectedName)==0
            || ( (cadName == NULL || cadName[0] == '\0') && no_label) )
        {
          // delete CAD object matching the selected name
          CAD_object_delete(object_ptr);
          done = 1;
        }
        else
        {
          object_ptr = object_ptr->next;
        }
      }
    }
  }

  Draw_CAD_Objects_erase_dialog_close(w,clientData,callData);

  // Save the altered list to file.
  Save_CAD_Objects_to_file();
  // Reload symbols/tracks/CAD objects
  redraw_symbols(da);

  // Here we update the edit cad objects dialog by getting rid of it and
  // then re-establishing it if it is active when we start.  This will
  // usually make the dialog move, but it's better than having it be
  // out-of-date.
  //
  if (cad_list_dialog!=NULL)
  {
    // Update the Edit CAD Objects list
    Draw_CAD_Objects_list_dialog_close(w, clientData, callData);
    Draw_CAD_Objects_list_dialog(w, clientData, callData);
  }
}





// Callback for delete CAD objects menu option.  Dialog to allow
// users to delete all CAD objects or select individual CAD objects
// to delete.
//
void Draw_CAD_Objects_erase_dialog( Widget UNUSED(w),
                                    XtPointer UNUSED(clientData),
                                    XtPointer UNUSED(callData) )
{

  Widget cad_erase_pane, cad_erase_form, cad_erase_label,
         button_delete_all, button_delete_selected, button_cancel;
  Arg al[100];       /* Arg List */
  unsigned int ac;   /* Arg Count */
  CADRow *object_ptr = CAD_list_head;
  int counter = 1;
  XmString cb_item;

  if (cad_erase_dialog)
  {
    (void)XRaiseWindow(XtDisplay(cad_erase_dialog), XtWindow(cad_erase_dialog));
  }
  else
  {

    // Delete CAD Objects
    cad_erase_dialog = XtVaCreatePopupShell("Delete CAD Objects",
                                            xmDialogShellWidgetClass,
                                            appshell,
                                            XmNdeleteResponse,          XmDESTROY,
                                            XmNdefaultPosition,         FALSE,
                                            XmNfontList, fontlist1,
                                            NULL);

    cad_erase_pane = XtVaCreateWidget("CAD erase Object pane",
                                      xmPanedWindowWidgetClass,
                                      cad_erase_dialog,
                                      MY_FOREGROUND_COLOR,
                                      MY_BACKGROUND_COLOR,
                                      NULL);

    cad_erase_form =  XtVaCreateWidget("Cad erase Object form",
                                       xmFormWidgetClass,
                                       cad_erase_pane,
                                       XmNfractionBase,            3,
                                       XmNautoUnmanage,            FALSE,
                                       XmNshadowThickness,         1,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       NULL);

    // heading: Delete CAD Objects
    cad_erase_label = XtVaCreateManagedWidget(langcode("CADPUD009"),
                      xmLabelWidgetClass,
                      cad_erase_form,
                      XmNtopAttachment,           XmATTACH_FORM,
                      XmNtopOffset,               10,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNleftAttachment,          XmATTACH_FORM,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);

    // *** need to handle the special case of no CAD objects ? ***

    // scrolled pick list to allow selection of current objects
    /*set args for list */
    ac=0;
    XtSetArg(al[ac], XmNvisibleItemCount, 11);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmEXTENDED_SELECT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNtopWidget, cad_erase_label);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE);
    ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNrightOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNleftOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
    ac++;
    //XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, colors[0x0f]);
    ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1);
    ac++;

    list_of_existing_CAD_objects = XmCreateScrolledList(cad_erase_form,
                                   "CAD objects for deletion scrolled list",
                                   al,
                                   ac);
    // make sure list is empty
    XmListDeleteAllItems(list_of_existing_CAD_objects);

    // iterate through list of objects to populate scrolled list
    while (object_ptr != NULL)
    {

      //  If no label, use the string "<Empty Label>" instead
      if (object_ptr->label[0] == '\0')
      {
        cb_item = XmStringCreateLtoR("<Empty Label>", XmFONTLIST_DEFAULT_TAG);
      }
      else
      {
        cb_item = XmStringCreateLtoR(object_ptr->label, XmFONTLIST_DEFAULT_TAG);
      }


      XmListAddItem(list_of_existing_CAD_objects,
                    cb_item,
                    counter);
      counter++;
      XmStringFree(cb_item);
      object_ptr = object_ptr->next;
    }

    // "Delete All"
    button_delete_all = XtVaCreateManagedWidget(langcode("CADPUD010"),
                        xmPushButtonGadgetClass,
                        cad_erase_form,
                        XmNtopAttachment,     XmATTACH_WIDGET,
                        XmNtopWidget,         list_of_existing_CAD_objects,
                        XmNtopOffset,         5,
                        XmNbottomAttachment,  XmATTACH_FORM,
                        XmNbottomOffset,      5,
                        XmNleftAttachment,    XmATTACH_FORM,
                        XmNleftOffset,        5,
                        XmNnavigationType,    XmTAB_GROUP,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        XmNfontList, fontlist1,
                        NULL);
    XtAddCallback(button_delete_all, XmNactivateCallback, Draw_CAD_Objects_erase, Draw_CAD_Objects_erase_dialog);

    // "Delete Selected"
    button_delete_selected = XtVaCreateManagedWidget(langcode("CADPUD011"),
                             xmPushButtonGadgetClass,
                             cad_erase_form,
                             XmNtopAttachment,     XmATTACH_WIDGET,
                             XmNtopWidget,         list_of_existing_CAD_objects,
                             XmNtopOffset,         5,
                             XmNbottomAttachment,  XmATTACH_FORM,
                             XmNbottomOffset,      5,
                             XmNleftAttachment,    XmATTACH_WIDGET,
                             XmNleftWidget,        button_delete_all,
                             XmNleftOffset,        10,
                             XmNnavigationType,    XmTAB_GROUP,
                             MY_FOREGROUND_COLOR,
                             MY_BACKGROUND_COLOR,
                             XmNfontList, fontlist1,
                             NULL);
    XtAddCallback(button_delete_selected, XmNactivateCallback, Draw_CAD_Objects_erase_selected, Draw_CAD_Objects_erase_dialog);

    // "Cancel"
    button_cancel = XtVaCreateManagedWidget(langcode("CADPUD008"),
                                            xmPushButtonGadgetClass,
                                            cad_erase_form,
                                            XmNtopAttachment,     XmATTACH_WIDGET,
                                            XmNtopWidget,         list_of_existing_CAD_objects,
                                            XmNtopOffset,         5,
                                            XmNbottomAttachment,  XmATTACH_FORM,
                                            XmNbottomOffset,      5,
                                            XmNleftAttachment,    XmATTACH_WIDGET,
                                            XmNleftWidget,        button_delete_selected,
                                            XmNleftOffset,        10,
                                            XmNnavigationType,    XmTAB_GROUP,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);
    XtAddCallback(button_cancel, XmNactivateCallback, Draw_CAD_Objects_erase_dialog_close, Draw_CAD_Objects_erase_dialog);
    pos_dialog(cad_erase_dialog);
    XmInternAtom(XtDisplay(cad_erase_dialog),"WM_DELETE_WINDOW", FALSE);

    XtManageChild(cad_erase_form);
    XtManageChild(list_of_existing_CAD_objects);
    XtManageChild(cad_erase_pane);

    XtPopup(cad_erase_dialog,XtGrabNone);
  }
}





// popdown and destroy the cad_list_dialog
//
void Draw_CAD_Objects_list_dialog_close ( Widget UNUSED(w),
    XtPointer UNUSED(clientData),
    XtPointer UNUSED(callData) )
{

  if (cad_list_dialog!=NULL)
  {
    // close cad_list_dialog
    XtPopdown(cad_list_dialog);
    XtDestroyWidget(cad_list_dialog);
    cad_list_dialog = (Widget)NULL;
  }

}





// Show details for selected CAD object.  Callback for the show/edit
// details button on the Draw_CAD_Objects_list dialog.
//
void Show_selected_CAD_object_details ( Widget UNUSED(w),
                                        XtPointer UNUSED(clientData),
                                        XtPointer UNUSED(callData) )
{

  static int sizeof_area_description = 200;
  int itemCount;       // number of items in list of CAD objects.
  XmString *listItems; // names of CAD objects on list
  char *cadName;       // the text name of a CAD object
  Position x;          // position on list
  char *selectedName;  // the text name of a selected CAD object
  CADRow *object_ptr = CAD_list_head;  // pointer to the linked list of CAD objects
  int done = 0;        // has a cad object with a name matching the current selection been found
  double area;
  char area_description[sizeof_area_description];
  xastir_snprintf(area_description, sizeof_area_description, "Area");

  if (cad_list_dialog!=NULL)
  {
    // get the selected object
    XtVaGetValues(list_of_existing_CAD_objects_edit,
                  XmNitemCount,&itemCount,
                  XmNitems,&listItems,
                  NULL);
    // iterate through list and find each object with a name
    // matching one selected on the list.
    //
    // *** Note: If names are not unique the results may not be what the user expects.
    // The first match to a selection will be used, not necessarily the selection.
    //
    for (x=1; x<=itemCount; x++)
    {
      if (XmListPosSelected(list_of_existing_CAD_objects_edit,x))
      {
        int no_label = 0;

        XmStringGetLtoR(listItems[(x-1)],XmFONTLIST_DEFAULT_TAG,&selectedName);

        // Check for our own definition of no label for the CAD
        // objects, which is "<Empty Label>"
        if (strcmp(selectedName,"<Empty Label>") == 0)
        {
          no_label++;
        }

        object_ptr = CAD_list_head;
        done = 0;

        while (object_ptr != NULL && done == 0)
        {

          cadName = object_ptr->label;

          if (strcmp(cadName,selectedName)==0
              || ( (cadName == NULL || cadName[0] == '\0') && no_label) )
          {

            // get the area for the CAD object matching the selected name
            // and format it as a localized string.
            area = object_ptr->computed_area;
            Format_area_for_output(&area, area_description,sizeof_area_description);
            // open the CAD object details dialog for the matching CAD object
            Set_CAD_object_parameters_dialog(area_description,object_ptr);
            done = 1;
          }
          else
          {
            object_ptr = object_ptr->next;
          }
        }
      }
    }

    // leave the list dialog open
  }
}





// Callback for edit CAD objects menu option.  Dialog to allow users
// to select individual CAD objects in order to edit their metadata.
//
void Draw_CAD_Objects_list_dialog( Widget UNUSED(w),
                                   XtPointer UNUSED(clientData),
                                   XtPointer UNUSED(callData) )
{

  Widget cad_list_pane, cad_list_form, cad_list_label,
         button_list_selected, button_close;
  Arg al[100];       /* Arg List */
  unsigned int ac;   /* Arg Count */
  CADRow *object_ptr = CAD_list_head;
  int counter = 1;
  XmString cb_item;

  if (cad_list_dialog)
  {
    (void)XRaiseWindow(XtDisplay(cad_list_dialog), XtWindow(cad_list_dialog));
  }
  else
  {

    // List CAD Objects
    cad_list_dialog = XtVaCreatePopupShell("List CAD Objects",
                                           xmDialogShellWidgetClass,
                                           appshell,
                                           XmNdeleteResponse,          XmDESTROY,
                                           XmNdefaultPosition,         FALSE,
                                           XmNfontList, fontlist1,
                                           NULL);

    cad_list_pane = XtVaCreateWidget("CAD list Object pane",
                                     xmPanedWindowWidgetClass,
                                     cad_list_dialog,
                                     MY_FOREGROUND_COLOR,
                                     MY_BACKGROUND_COLOR,
                                     XmNfontList, fontlist1,
                                     NULL);

    cad_list_form =  XtVaCreateWidget("Cad list Object form",
                                      xmFormWidgetClass,
                                      cad_list_pane,
                                      XmNfractionBase,            3,
                                      XmNautoUnmanage,            FALSE,
                                      XmNshadowThickness,         1,
                                      MY_FOREGROUND_COLOR,
                                      MY_BACKGROUND_COLOR,
                                      XmNfontList, fontlist1,
                                      NULL);

    // heading: CAD Objects
    cad_list_label = XtVaCreateManagedWidget(langcode("CADPUD006"),
                     xmLabelWidgetClass,
                     cad_list_form,
                     XmNtopAttachment,           XmATTACH_FORM,
                     XmNtopOffset,               10,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_FORM,
                     XmNleftOffset,              10,
                     XmNrightAttachment,         XmATTACH_NONE,
                     MY_FOREGROUND_COLOR,
                     MY_BACKGROUND_COLOR,
                     XmNfontList, fontlist1,
                     NULL);

    // *** need to handle the special case of no CAD objects ? ***

    // scrolled pick list to allow selection of current objects
    /*set args for list */
    ac=0;
    XtSetArg(al[ac], XmNvisibleItemCount, 11);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNtopWidget, cad_list_label);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE);
    ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNrightOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNleftOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
    ac++;
    //XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, colors[0x0f]);
    ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1);
    ac++;

    list_of_existing_CAD_objects_edit = XmCreateScrolledList(cad_list_form,
                                        "CAD objects for deletion scrolled list",
                                        al,
                                        ac);
    // make sure list is empty
    XmListDeleteAllItems(list_of_existing_CAD_objects_edit);

    // iterate through list of objects to populate scrolled list
    while (object_ptr != NULL)
    {

      //  If no label, use the string "<Empty Label>" instead
      if (object_ptr->label[0] == '\0')
      {
        cb_item = XmStringCreateLtoR("<Empty Label>", XmFONTLIST_DEFAULT_TAG);
      }
      else
      {
        cb_item = XmStringCreateLtoR(object_ptr->label, XmFONTLIST_DEFAULT_TAG);
      }

      XmListAddItem(list_of_existing_CAD_objects_edit,
                    cb_item,
                    counter);
      counter++;
      XmStringFree(cb_item);
      object_ptr = object_ptr->next;
    }

    // "Show/edit details"
    button_list_selected = XtVaCreateManagedWidget(langcode("CADPUD007"),
                           xmPushButtonGadgetClass,
                           cad_list_form,
                           XmNtopAttachment,     XmATTACH_WIDGET,
                           XmNtopWidget,         list_of_existing_CAD_objects_edit,
                           XmNtopOffset,         5,
                           XmNbottomAttachment,  XmATTACH_FORM,
                           XmNbottomOffset,      5,
                           XmNleftAttachment,    XmATTACH_FORM,
                           XmNleftOffset,        10,
                           XmNnavigationType,    XmTAB_GROUP,
                           MY_FOREGROUND_COLOR,
                           MY_BACKGROUND_COLOR,
                           XmNfontList, fontlist1,
                           NULL);
    XtAddCallback(button_list_selected, XmNactivateCallback, Show_selected_CAD_object_details, Draw_CAD_Objects_list_dialog);

    // "Close"
    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                                           xmPushButtonGadgetClass,
                                           cad_list_form,
                                           XmNtopAttachment,     XmATTACH_WIDGET,
                                           XmNtopWidget,         list_of_existing_CAD_objects_edit,
                                           XmNtopOffset,         5,
                                           XmNbottomAttachment,  XmATTACH_FORM,
                                           XmNbottomOffset,      5,
                                           XmNleftAttachment,    XmATTACH_WIDGET,
                                           XmNleftWidget,        button_list_selected,
                                           XmNleftOffset,        10,
                                           XmNnavigationType,    XmTAB_GROUP,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);
    XtAddCallback(button_close, XmNactivateCallback, Draw_CAD_Objects_list_dialog_close, Draw_CAD_Objects_erase_dialog);
    pos_dialog(cad_list_dialog);
    XmInternAtom(XtDisplay(cad_list_dialog),"WM_DELETE_WINDOW", FALSE);

    XtManageChild(cad_list_form);
    XtManageChild(list_of_existing_CAD_objects_edit);
    XtManageChild(cad_list_pane);

    XtPopup(cad_list_dialog,XtGrabNone);
  }
}





// Free the object and vertice lists then do a screen update.
// callback from delete all button on cad_erase_dialog.
//
void Draw_CAD_Objects_erase( Widget w,
                             XtPointer clientData,
                             XtPointer callData)
{

  // if we were called from the cad_erase_dialog, make sure it is closed properly
  if (cad_erase_dialog)
  {
    Draw_CAD_Objects_erase_dialog_close(w,clientData,callData);
  }

  CAD_object_delete_all();
  polygon_last_x = -1;    // Invalid position
  polygon_last_y = -1;    // Invalid position

  // Save the empty list out to file
  Save_CAD_Objects_to_file();

  // Reload symbols/tracks/CAD objects
  redraw_symbols(da);
}



// Add an ending vertice that is the same as the starting vertice.
// Best not to use the screen coordinates we captured first, as the
// user may have zoomed or panned since then.  Better to copy the
// first vertice that we recorded in our linked list.
//
// Compute the area of the closed polygon.  Write it out to STDERR,
// the computed_area field in the Object, and to a dialog that pops
// up on the screen.
//
void Draw_CAD_Objects_close_polygon( Widget UNUSED(widget),
                                     XtPointer UNUSED(clientData),
                                     XtPointer UNUSED(callData) )
{
  static int sizeof_area_description = 200;
  VerticeRow *tmp;
  double area;
  int n;
  //char temp_course[20];
  char area_description[sizeof_area_description];
  xastir_snprintf(area_description, sizeof_area_description, "Area");

  // Check whether we're currently working on a polygon.  If not,
  // get out of here.
  if (polygon_last_x == -1 || polygon_last_y == -1)
  {

    // Tell the code that we're starting a new polygon by wiping
    // out the first position.
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position

    return;
  }

  // Find the last vertice in the linked list.  That will be the
  // first vertice we recorded for the object.

  // Check for at least three vertices.  We don't need to check
  // that the first/last point are equal:  We force it below by
  // copying the first vertice to the last.
  //
  n = 0;
  if (CAD_list_head != NULL)
  {

    // Walk the linked list.  Stop at the last record.
    tmp = CAD_list_head->start;
    if (tmp != NULL)
    {
      n++;
      while (tmp->next != NULL)
      {
        tmp = tmp->next;
        n++;
      }
      if (n > 2)
      {
        // We have more than a point or a line, therefore
        // can copy the first point to the last, closing the
        // polygon.
        CAD_vertice_allocate(tmp->latitude, tmp->longitude);
      }
    }
  }
  // Reload symbols/tracks/CAD objects and redraw the polygon
  redraw_symbols(da);

#ifdef CAD_DEBUG
  fprintf(stderr,"Points in closed polygon: n = %d\n",n);
#endif

  if (n < 3)
  {
    // Not enough points to compute an area.

    // Tell the code that we're starting a new polygon by wiping
    // out the first position.
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position

    return;
  }
  area =  CAD_object_compute_area(CAD_list_head);

  // Save it in the object.  Convert nautical square miles to
  // square kilometers because that's what "Format_area_for_output"
  // requires.
  area = area * 3.429903999977917; // Now in km squared
//fprintf(stderr,"SQUARE KM: %f\n", area);
  CAD_list_head->computed_area = area;

  Format_area_for_output(&area, area_description, sizeof_area_description);

#ifdef CAD_DEBUG
  // Also write the area to stderr
  fprintf(stderr,"New CAD object %s\n",area_description);
#endif

  // Tell the code that we're starting a new polygon by wiping out
  // the first position.
  polygon_last_x = -1;    // Invalid position
  polygon_last_y = -1;    // Invalid position

  CAD_object_set_label_at_centroid(CAD_list_head);
  // CAD object vertices are ready, needs associated data
  // obtain label, comment, and probability for this polygon
  // from user through a dialog.
  Set_CAD_object_parameters_dialog(area_description,NULL);
}





// Function called by UpdateTime when doing screen refresh.  Draws
// all CAD objects onto the screen again.
//
void Draw_All_CAD_Objects(Widget w)
{
  CADRow *object_ptr = CAD_list_head;
  long x_long, y_lat;
  long x_offset, y_offset;
  float probability;
  char probability_string[8];
  VerticeRow *vertice;
  double area;
  int actual_line_type = LineOnOffDash;
  static int sizeof_area_description = 50; // define here as local static to limit size of display on map
  // independent of size as shown on form
  char area_description[sizeof_area_description];
  char dash[2];

  // Start at CAD_list_head, iterate through entire linked list,
  // drawing as we go.  Respect the line
  // width/line_color/line_type variables for each object.

//fprintf(stderr,"Drawing CAD objects\n");
  if (CAD_draw_objects==TRUE)
  {
    while (object_ptr != NULL)
    {
      probability = CAD_object_get_raw_probability(object_ptr,1);
      xastir_snprintf(probability_string,
                      sizeof(probability_string),
                      "%01.1f%%",
                      probability);

      // find point at which to draw label and other descriptive text
      x_long = object_ptr->label_longitude;
      y_lat = object_ptr->label_latitude;
#ifdef CAD_DEBUG
      fprintf(stderr,"Drawing object %s\n", (object_ptr->label) ? object_ptr->label : "NULL" );
#endif
      //fprintf(stderr,"Lat: %d\n", y_lat);
      //fprintf(stderr,"Long: %d\n", x_long);
//            if ((x_long+10>=0) && (x_long-10<=129600000l)) {      // 360 deg

//                if ((y_lat+10>=0) && (y_lat-10<=64800000l)) {     // 180 deg

      if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude))
      {

        if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude))
        {

          // ok to draw label and assocated data, point is on screen
          x_offset=((x_long-NW_corner_longitude)/scale_x)-(10);
          y_offset=((y_lat -NW_corner_latitude) /scale_y)-(10);
          // ****** ?? use -10 or point ??
          x_offset=((x_long-NW_corner_longitude)/scale_x);
          y_offset=((y_lat -NW_corner_latitude) /scale_y);

          if (((int)strlen(object_ptr->label)>0) & (CAD_show_label==TRUE))
          {
            // Draw Label
            // 0x08 is background color
            // 0x40 is foreground color (yellow)
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,object_ptr->label,0x08,0x40,strlen(object_ptr->label));

            x_offset=x_offset+12;
            y_offset=y_offset+15;
          }

          if (CAD_show_raw_probability==TRUE)
          {
            // draw probability
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,probability_string,0x08,0x40,strlen(probability_string));
            y_offset=y_offset+15;
          }

          if ((CAD_show_comment==TRUE) & ((int)strlen(object_ptr->comment)>0))
          {
            // draw comment
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,object_ptr->comment,0x08,0x40,strlen(object_ptr->comment));
            y_offset=y_offset+15;
          }


          if (CAD_show_area==TRUE)
          {
            area = object_ptr->computed_area;
            Format_area_for_output(&area, area_description, sizeof_area_description);
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,area_description,0x08,0x40,strlen(area_description));
            y_offset=y_offset+15;
          }

        }
      }
//                }
//            }

      // Iterate through the vertices and draw the lines
      vertice = object_ptr->start;

      switch (object_ptr->line_type)
      {

        case 1:
          actual_line_type = LineSolid;
          break;

        case 2:
          actual_line_type = LineOnOffDash;
          dash[0] = dash[1]  = 8;
          break;

        case 3:
          actual_line_type = LineDoubleDash;
          dash[0] = dash[1]  = 16;
          break;

        default:
          actual_line_type = LineOnOffDash;
          dash[0] = dash[1]  = 8;
          break;
      }

      // Set up line color/width/type here
      (void)XSetLineAttributes (XtDisplay (da),
                                gc_tint,
                                object_ptr->line_width,
                                actual_line_type,
                                CapButt,
                                JoinMiter);

      if (object_ptr->line_type  != 1)
      {
        (void)XSetDashes (XtDisplay (da),
                          gc_tint,
                          0,       // dash offset
                          dash,    // dash list[]
                          2);      // elements in dash lista
      }

      (void)XSetForeground (XtDisplay (da),
                            gc_tint,
                            object_ptr->line_color);

      (void)XSetFunction (XtDisplay (da),
                          gc_tint,
                          GXxor);

      while (vertice != NULL)
      {
        if (vertice->next != NULL)
        {
          // Use the draw_vector function from maps.c
          draw_vector(w,
                      vertice->longitude,
                      vertice->latitude,
                      vertice->next->longitude,
                      vertice->next->latitude,
                      gc_tint,
                      pixmap_final,
                      0);
        }
        vertice = vertice->next;
      }
      object_ptr = object_ptr->next;
    }
  }
}




// Formats an area as a string in english (square miles) or metric units
// (square kilometers). Switches to square feet or square meters if the
// area is less than 0.1 of the units.
//
// Parameters:
//     area: an area in square kilometers.
//     area_description: area reformatted as a localized text string.
//     sizeof_area_description: array length of area_description.
//
void Format_area_for_output(double *area_km2, char *area_description, int sizeof_area_description)
{
  double area;
  // Format it for output and dump it out.  We're using square terms, so
  // apply the conversion factor twice to convert from square kilometers
  // to the units of interest. The result here is squared meters or
  // squared feet.
//fprintf(stderr,"Square km: %f\n", *area_km2);

  area = *area_km2 * 1000.0 * 1000.0 * cvt_m2len * cvt_m2len;

  // We could be measuring a very small or a very large object.
  // In the case of very small, convert it to square feet or
  // square meters.

  if (english_units)   // Square feet
  {
//fprintf(stderr,"Square feet: %f\n", area);

    if (area < 2787840.0)   // Switch at 0.5 miles squared
    {
      // Smaller area: Output in feet squared
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("POPUPMA053") );   // ft
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
    else
    {
      // Larger area: Output in miles squared
      area = area / 27878400.0;
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("POPUPMA055") );   // mi
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
  }
  else    // Square meters
  {
//fprintf(stderr,"Square meters: %f\n", area);

    if (area < 100000.0)   // Switch at 0.1 km squared
    {
      // Smaller area: Output in meters squared
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("POPUPMA054") );   // meters
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
    else
    {
      // Larger ara: Output in kilometers squared
      area = area / 1000000.0;
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("UNIOP00005") );   // km
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
  }
}








