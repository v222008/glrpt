/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 *
 *  http://www.gnu.org/copyleft/gpl.txt
 */

#include "main.h"
#include "../common/shared.h"
#include "../lrpt_decode/rectify_meteor.h"

/* Signal handler */
static void sig_handler( int signal );

/*------------------------------------------------------------------------*/

  int
main( int argc, char *argv[] )
{
  /* Command line option returned by getopt() */
  int option;

  /* New and old actions for sigaction() */
  struct sigaction sa_new, sa_old;

  /* Initialize new actions */
  sa_new.sa_handler = sig_handler;
  sigemptyset( &sa_new.sa_mask );
  sa_new.sa_flags = 0;

  /* Register function to handle signals */
  sigaction( SIGINT,  &sa_new, &sa_old );
  sigaction( SIGSEGV, &sa_new, 0 );
  sigaction( SIGFPE,  &sa_new, 0 );
  sigaction( SIGTERM, &sa_new, 0 );
  sigaction( SIGABRT, &sa_new, 0 );
  sigaction( SIGCONT, &sa_new, 0 );
  sigaction( SIGALRM, &sa_new, 0 );

  /* Process command line options. Defaults below. */
  while( (option = getopt(argc, argv, "hv") ) != -1 )
    switch( option )
    {
      case 'h': /* Print usage and exit */
        Usage();
        exit(0);

      case 'v': /* Print version */
        puts( PACKAGE_STRING );
        exit(0);

      default: /* Print usage and exit */
        Usage();
        exit(-1);
    } /* End of switch( option ) */

  /* Start GTK+ */
  gtk_init( &argc, &argv );

  /* Defaults/initialization */
  rc_data.decode_timer  = 0;
  rc_data.ifft_decimate = IFFT_DECIMATE;
  rc_data.satellite_name[0] = '\0';

  /* Create glrpt window */
  size_t s = sizeof(rc_data.glrpt_glade);
  Strlcpy( rc_data.glrpt_glade, getenv("HOME"), s );
  Strlcat( rc_data.glrpt_glade, "/glrpt/glrpt.glade", s );
  main_window = create_main_window( &main_window_builder );
  gtk_window_set_title( GTK_WINDOW(main_window), PACKAGE_STRING );
  gtk_widget_show( main_window );

  /* Create the text view scroller */
  text_scroller = Builder_Get_Object( main_window_builder, "text_scrolledwindow" );

  /* Get text buffer */
  text_buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW(
        Builder_Get_Object(main_window_builder, "message_textview")) );

  /* Get waterfall and scope widgets */
  qpsk_drawingarea = Builder_Get_Object( main_window_builder, "qpsk_drawingarea" );
  ifft_drawingarea = Builder_Get_Object( main_window_builder, "ifft_drawingarea" );

  /* Get Receive Status widgets */
  start_togglebutton    = Builder_Get_Object( main_window_builder, "start_togglebutton" );
  pll_ave_entry         = Builder_Get_Object( main_window_builder, "pll_ave_entry" );
  pll_freq_entry        = Builder_Get_Object( main_window_builder, "pll_freq_entry" );
  pll_lock_icon         = Builder_Get_Object( main_window_builder, "pll_lock_icon" );
  agc_gain_entry        = Builder_Get_Object( main_window_builder, "agc_gain_entry" );
  sig_level_entry       = Builder_Get_Object( main_window_builder, "sig_level_entry" );
  frame_icon            = Builder_Get_Object( main_window_builder, "frame_icon" );
  status_icon           = Builder_Get_Object( main_window_builder, "status_icon" );
  sig_quality_entry     = Builder_Get_Object( main_window_builder, "sig_quality_entry" );
  packet_cnt_entry      = Builder_Get_Object( main_window_builder, "packet_cnt_entry" );
  ob_time_entry         = Builder_Get_Object( main_window_builder, "ob_time_entry" );
  sig_level_drawingarea = Builder_Get_Object( main_window_builder, "sig_level_drawingarea" );
  sig_qual_drawingarea  = Builder_Get_Object( main_window_builder, "sig_qual_drawingarea" );
  agc_gain_drawingarea  = Builder_Get_Object( main_window_builder, "agc_gain_drawingarea" );
  pll_ave_drawingarea   = Builder_Get_Object( main_window_builder, "pll_ave_drawingarea" );

  /* Create some rendering tags */
  gtk_text_buffer_create_tag( text_buffer, "black",
      "foreground", "black", NULL);
  gtk_text_buffer_create_tag( text_buffer, "red",
      "foreground", "red", NULL);
  gtk_text_buffer_create_tag( text_buffer, "orange",
      "foreground", "orange", NULL);
  gtk_text_buffer_create_tag( text_buffer, "green",
      "foreground", "darkgreen", NULL);
  gtk_text_buffer_create_tag( text_buffer, "bold",
      "weight", PANGO_WEIGHT_BOLD, NULL);

  /* Get sizes of displays and initialize */
  GtkAllocation alloc;
  gtk_widget_get_allocation( ifft_drawingarea, &alloc );
  Fft_Drawingarea_Size_Alloc( &alloc );
  gtk_widget_get_allocation( qpsk_drawingarea, &alloc );
  Qpsk_Drawingarea_Size_Alloc( &alloc );

  char ver[28];
  snprintf( ver, sizeof(ver), _("Welcome to %s"), PACKAGE_STRING );
  Show_Message( ver, "bold" );

  /* Find configuration file(s) and open the first as default */
  g_idle_add( Find_Config_Files, NULL );
  g_idle_add( Load_Config, NULL );

  gtk_main();

  return( 0 );
} /* main() */

/*------------------------------------------------------------------------*/

/*  Initialize_Top_Window()
 *
 *  Initializes glrpt's top window
 */

  void
Initialize_Top_Window( void )
{
  /* The scrolled window image container */
  gchar text[32];

  /* Show current satellite */
  GtkLabel *label = GTK_LABEL(
      Builder_Get_Object(main_window_builder, "satellite_label") );
  snprintf( text, sizeof(text), "%s LRPT", rc_data.satellite_name );
  gtk_label_set_text( label, text );

  /* Show Center_Freq to frequency entry */
  Enter_Center_Freq( rc_data.sdr_center_freq );

  /* Show Bandwidth to B/W entry */
  Enter_Filter_BW();

  /* Kill existing pixbuf */
  if( scaled_image_pixbuf != NULL )
  {
    g_object_unref( scaled_image_pixbuf );
    scaled_image_pixbuf = NULL;
  }

  /* Create new pixbuff for scaled images (+3 for white separator lines) */
  scaled_image_width   = (3 * METEOR_IMAGE_WIDTH) / (int)rc_data.image_scale + 3;
  scaled_image_height  = (int)rc_data.decode_timer * IMAGE_LINES_PERSEC;
  scaled_image_height /= (int)rc_data.image_scale;

  /* Create a pixbuf for the LRPT image display */
  scaled_image_pixbuf = gdk_pixbuf_new(
      GDK_COLORSPACE_RGB, FALSE, 8, scaled_image_width, scaled_image_height );

  /* Error, not enough memory */
  if( scaled_image_pixbuf == NULL)
  {
    Show_Message( _("Memory allocation for pixbuf failed - Quit"), "red" );
    Error_Dialog();
    return;
  }

  /* Get details of pixbuf */
  scaled_image_pixel_buf  = gdk_pixbuf_get_pixels(     scaled_image_pixbuf );
  scaled_image_rowstride  = gdk_pixbuf_get_rowstride(  scaled_image_pixbuf );
  scaled_image_n_channels = gdk_pixbuf_get_n_channels( scaled_image_pixbuf );

  /* Fill pixbuf with background color */
  gdk_pixbuf_fill( scaled_image_pixbuf, 0xaaaaaaff );

  /* Globalize image to be displayed */
  lrpt_image = Builder_Get_Object( main_window_builder, "lrpt_image" );

  /* Set lrpt image from pixbuff */
  gtk_image_set_from_pixbuf( GTK_IMAGE(lrpt_image), scaled_image_pixbuf );
  gtk_widget_show( lrpt_image );

  /* Set window size as required (minimal) */
  gtk_window_resize( GTK_WINDOW(main_window), 10, 10 );

} /* Initialize_Top_Window() */

/*------------------------------------------------------------------------*/

/*  sig_handler()
 *
 *  Signal Action Handler function
 */

static void sig_handler( int signal )
{
  if( signal == SIGALRM )
  {
    Alarm_Action();
    return;
  }

  /* Internal wakeup call */
  if( signal == SIGCONT ) return;

  ClearFlag( STATUS_RECEIVING );
  fprintf( stderr, "\n" );
  switch( signal )
  {
    case SIGINT:
      fprintf( stderr, "%s\n", _("glrpt: Exiting via User Interrupt") );
      exit(-1);

    case SIGSEGV:
      fprintf( stderr, "%s\n", _("glrpt: Segmentation Fault") );
      exit(-1);

    case SIGFPE:
      fprintf( stderr, "%s\n", _("glrpt: Floating Point Exception") );
      exit(-1);

    case SIGABRT:
      fprintf( stderr, "%s\n", _("glrpt: Abort Signal received") );
      exit(-1);

    case SIGTERM:
      fprintf( stderr, "%s\n", _("glrpt: Termination Request received") );
      exit(-1);
  }

} /* End of sig_handler() */

/*------------------------------------------------------------------------*/
