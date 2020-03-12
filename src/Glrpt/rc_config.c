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

#include "rc_config.h"
#include "../common/shared.h"

/*------------------------------------------------------------------------*/

/*  Load_Line()
 *
 *  Loads a line from a file, aborts on failure. Lines beginning
 *  with a '#' are ignored as comments. At the end of file EOF
 *  is returned. Lines assumed maximum 80 characters long.
 */

  static int
Load_Line( char *buff, FILE *pfile, const char *mesg )
{
  int
    num_chr, /* Number of characters read, excluding lf/cr */
    chr;     /* Character read by getc() */
  char error_mesg[MESG_SIZE];

  /* Prepare error message */
  snprintf( error_mesg, MESG_SIZE,
      _("Error reading %s\n"\
        "Premature EOF (End Of File)"), mesg );

  /* Clear buffer at start */
  buff[0] = '\0';
  num_chr = 0;

  /* Get next character, return error if chr = EOF */
  if( (chr = fgetc(pfile)) == EOF )
  {
    fprintf( stderr, "glrpt: %s\n", error_mesg );
    fclose( pfile );
    Show_Message( error_mesg, "red" );
    Error_Dialog();
    return( EOF );
  }

  /* Ignore commented lines and eol/cr and tab */
  while(
      (chr == '#') ||
      (chr == HT ) ||
      (chr == CR ) ||
      (chr == LF ) )
  {
    /* Go to the end of line (look for LF or CR) */
    while( (chr != CR) && (chr != LF) )
      /* Get next character, return error if chr = EOF */
      if( (chr = fgetc(pfile)) == EOF )
      {
        fprintf( stderr, "glrpt: %s\n", error_mesg );
        fclose( pfile );
        Show_Message( error_mesg, "red" );
        Error_Dialog();
        return( EOF );
      }

    /* Dump any CR/LF remaining */
    while( (chr == CR) || (chr == LF) )
      /* Get next character, return error if chr = EOF */
      if( (chr = fgetc(pfile)) == EOF )
      {
        fprintf( stderr, "glrpt: %s\n", error_mesg );
        fclose( pfile );
        Show_Message( error_mesg, "red" );
        Error_Dialog();
        return( EOF );
      }

  } /* End of while( (chr == '#') || ... */

  /* Continue reading characters from file till
   * number of characters = 80 or EOF or CR/LF */
  while( num_chr < 80 )
  {
    /* If LF/CR reached before filling buffer, return line */
    if( (chr == LF) || (chr == CR) ) break;

    /* Enter new character to line buffer */
    buff[num_chr++] = (char)chr;

    /* Get next character */
    if( (chr = fgetc(pfile)) == EOF )
    {
      /* Terminate buffer as a string if chr = EOF */
      buff[num_chr] = '\0';
      return( SUCCESS );
    }

    /* Abort if end of line not reached at 80 char. */
    if( (num_chr == 80) && (chr != LF) && (chr != CR) )
    {
      /* Terminate buffer as a string */
      buff[num_chr] = '\0';
      snprintf( error_mesg, MESG_SIZE,
          _("Error reading %s\n"\
            "Line longer than 80 characters"), mesg );
      fprintf( stderr, "glrpt: %s\n%s\n", error_mesg, buff );
      fclose( pfile );
      Show_Message( error_mesg, "red" );
      Error_Dialog();
      return( ERROR );
    }

  } /* End of while( num_chr < max_chr ) */

  /* Terminate buffer as a string */
  buff[num_chr] = '\0';

  return( SUCCESS );

} /* End of Load_Line() */

/*------------------------------------------------------------------------*/

static int found_cfg = 0;

/*  Load_Config()
 *
 *  Loads the glrptrc configuration file
 */

  gboolean
Load_Config( gpointer data )
{
  char
    rc_fpath[MESG_SIZE], /* File path to glrptrc */
    line[LINE_LEN];      /* Buffer for Load_Line */

  /* Config file pointer */
  FILE *glrptrc;

  int idx;


  /* Setup file path to glrptrc and working dir */
  snprintf( rc_fpath, sizeof(rc_fpath),
      "%s/%s.cfg", rc_data.glrpt_dir, rc_data.satellite_name );

  /* Open glrptrc file */
  if( !found_cfg || !Open_File(&glrptrc, rc_fpath, "r") )
  {
    Show_Message( _("Failed to open configuration file"), "red" );
    Error_Dialog();
    return( FALSE );
  }

  /*** Read runtime configuration data ***/

  /*** SDR Receiver Configuration data ***/
  /* Read SDR Receiver SoapySDR Device Driver to use */
  if( Load_Line(line, glrptrc, _("SoapySDR Device Driver")) != SUCCESS )
    return( FALSE );
  Strlcpy( rc_data.device_driver, line, sizeof(rc_data.device_driver) );
  if( strcmp(rc_data.device_driver, "auto") == 0 )
    SetFlag( AUTO_DETECT_SDR );

  /* Read SoapySDR Device Index, abort if EOF */
  if( Load_Line(line, glrptrc, _("SDR Device Index")) != SUCCESS )
    return( FALSE );
  idx = atoi( line );
  if( (idx < 0) || (idx > 8) )
  {
    Show_Message(
        _("Invalid SoapySDR Device Index\n"\
          "Quit and correct glrptrc"), "red" );
    Error_Dialog();
    return( FALSE );
  }
  rc_data.device_index = (uint32_t)idx;

  /* Read Low Pass Filter Bandwidth, abort if EOF */
  if( Load_Line(line, glrptrc, _("Roofing Filter Bandwidth")) != SUCCESS )
    return( FALSE );
  rc_data.sdr_filter_bw = (uint32_t)( atoi(line) );

  /*** Read Device configuration data ***/
  /* Read Manual AGC Setting, abort if EOF */
  if( Load_Line(line, glrptrc, _("Manual Gain Setting")) != SUCCESS )
    return( FALSE );
  rc_data.tuner_gain = atof( line );
  if( rc_data.tuner_gain > 100.0 )
  {
    rc_data.tuner_gain = 100.0;
    Show_Message(
        _("Invalid Manual Gain Setting\n"\
          "Assuming a value of 100%"), "red" );
    Error_Dialog();
    return( FALSE );
  }

  /* Read Frequency Correction Factor, abort if EOF */
  if( Load_Line(line, glrptrc, _("Frequency Correction Factor")) != SUCCESS )
    return( FALSE );
  rc_data.freq_correction = atoi( line );
  if( abs(rc_data.freq_correction) > 100 )
  {
    Show_Message(
        _("Invalid Frequency Correction Factor\n"\
          "Quit and correct glrptrc"), "red" );
    Error_Dialog();
    return( FALSE );
  }

  /*** Image Decoding configuration data ***/
  /* Read Satellite Frequency in kHz, abort if EOF */
  if( Load_Line(line, glrptrc, _("Satellite Frequency kHz")) != SUCCESS )
    return( FALSE );
  rc_data.sdr_center_freq = (uint32_t)atoi( line ) * 1000;

  /* Read default decode duration, abort if EOF */
  if( Load_Line(line, glrptrc, _("Image Decoding Duration")) != SUCCESS )
    return( FALSE );
  rc_data.default_timer = (uint32_t)( atoi(line) );
  if( !rc_data.decode_timer )
    rc_data.decode_timer = rc_data.default_timer;

  /* Warn if decoding duration is too long */
  if( rc_data.decode_timer > MAX_OPERATION_TIME )
  {
    char mesg[MESG_SIZE];
    snprintf( mesg, sizeof(mesg),
        _("Default decoding duration specified\n"\
          "in glrptrc (%d sec) seems excessive\n"),
        rc_data.decode_timer );
    Show_Message( mesg, "red" );
  }

  /* Read LRPT image scale factor, abort if EOF */
  if( Load_Line(line, glrptrc, _("Image Scale Factor")) != SUCCESS )
    return( FALSE );
  rc_data.image_scale = (uint32_t)( atoi(line) );

  /* LRPT Demodulator Parameters */
  /* Read RRC Filter Order, abort if EOF */
  if( Load_Line(line, glrptrc, _("RRC Filter Order")) != SUCCESS )
    return( FALSE );
  rc_data.rrc_order = (uint32_t)( atoi(line) );

  /* Read RRC Filter alpha factor, abort if EOF */
  if( Load_Line(line, glrptrc, _("RRC Filter alpha factor")) != SUCCESS )
    return( FALSE );
  rc_data.rrc_alpha = atof( line );

  /* Read Costas PLL Loop Bandwidth, abort if EOF */
  if( Load_Line(line, glrptrc, _("Costas PLL Loop Bandwidth")) != SUCCESS )
    return( FALSE );
  rc_data.costas_bandwidth = atof( line );

  /* Read Costas PLL Locked Threshold, abort if EOF */
  if( Load_Line(line, glrptrc, _("Costas PLL Locked Threshold")) != SUCCESS )
    return( FALSE );
  rc_data.pll_locked   = atof( line );
  rc_data.pll_unlocked = rc_data.pll_locked * 1.03;

  /* Read Transmitter Modulation Mode, abort if EOF */
  if( Load_Line(line, glrptrc, _("Transmitter Modulation Mode")) != SUCCESS )
    return( FALSE );
  rc_data.psk_mode = (uint8_t)( atoi(line) );

  /* Read Transmitter QPSK Symbol Rate, abort if EOF */
  if( Load_Line(line, glrptrc, _("Transmitter QPSK Symbol Rate")) != SUCCESS )
    return( FALSE );
  rc_data.symbol_rate = (uint32_t)( atoi(line) );

  /* Read Demodulator Interpolation Factor, abort if EOF */
  if( Load_Line(line, glrptrc, _("Demodulator Interpolation Factor")) != SUCCESS )
    return( FALSE );
  rc_data.interp_factor = (uint32_t)( atoi(line) );

  /* Read LRPT Decoder Output Mode, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Output Mode")) != SUCCESS )
    return( FALSE );
  switch( atoi(line) )
  {
    case OUT_COMBO:
      SetFlag( IMAGE_OUT_COMBO );
      break;

    case OUT_SPLIT:
      SetFlag( IMAGE_OUT_SPLIT );
      break;

    case OUT_BOTH:
      SetFlag( IMAGE_OUT_COMBO );
      SetFlag( IMAGE_OUT_SPLIT );
      break;

    default:
      SetFlag( IMAGE_OUT_COMBO );
      SetFlag( IMAGE_OUT_SPLIT );
      Show_Message(
          _("Image Output Mode option invalid\n"
            "Assuming Both (Split and Combo)"), "red" );
  }

  /* Read LRPT Image Save file type, abort if EOF */
  if( Load_Line(line, glrptrc, _("Save As image file type")) != SUCCESS )
    return( FALSE );
  switch( atoi(line) )
  {
    case SAVEAS_JPEG:
      SetFlag( IMAGE_SAVE_JPEG );
      break;

    case SAVEAS_PGM:
      SetFlag( IMAGE_SAVE_PPGM );
      break;

    case SAVEAS_BOTH:
      SetFlag( IMAGE_SAVE_JPEG );
      SetFlag( IMAGE_SAVE_PPGM );
      break;

    default:
      SetFlag( IMAGE_SAVE_PPGM );
      SetFlag( IMAGE_SAVE_PPGM );
      Show_Message(
          _("Image Save As option invalid\n"
            "Assuming Both (JPEG and PGM)"), "red" );
  }

  /* Read JPEG Quality Factor, abort if EOF */
  if( Load_Line(line, glrptrc, _("JPEG Quality Factor")) != SUCCESS )
    return( FALSE );
  rc_data.jpeg_quality = (float)atof( line );

  /* Read LRPT Decoder Image Raw flag, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Image Raw flag")) != SUCCESS )
    return( FALSE );
  if( atoi(line) ) SetFlag( IMAGE_RAW );

  /* Read LRPT Decoder Image Normalize flag, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Image Normalize flag")) != SUCCESS )
    return( FALSE );
  if( atoi(line) ) SetFlag( IMAGE_NORMALIZE );

  /* Read LRPT Decoder Image CLAHE flag, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Image CLAHE flag")) != SUCCESS )
    return( FALSE );
  if( atoi(line) ) SetFlag( IMAGE_CLAHE );

  /* Read LRPT Decoder Image Rectify flag, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Image Rectify flag")) != SUCCESS )
    return( FALSE );
  rc_data.rectify_function = (uint8_t)atoi( line );
  if( rc_data.rectify_function > 2 )
  {
    Show_Message( _("Invalid Rectify Function. Assuming 1"), "red" );
    rc_data.rectify_function = 1;
  }
  if( rc_data.rectify_function )
    SetFlag( IMAGE_RECTIFY );

  /* Read LRPT Decoder Image Colorize flag, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Image Colorize flag")) != SUCCESS )
    return( FALSE );
  if( atoi(line) ) SetFlag( IMAGE_COLORIZE );

  /* Read LRPT Decoder Channel 0 APID, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Red APID")) != SUCCESS )
    return( FALSE );
  rc_data.apid[0] = (uint8_t)( atoi(line) );

  /* Read LRPT Decoder Channel 1 APID, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Green APID")) != SUCCESS )
    return( FALSE );
  rc_data.apid[1] = (uint8_t)( atoi(line) );

  /* Read LRPT Decoder Channel 2 APID, abort if EOF */
  if( Load_Line(line, glrptrc, _("LRPT Decoder Blue APID")) != SUCCESS )
    return( FALSE );
  rc_data.apid[2] = (uint8_t)( atoi(line) );

  /* Read LRPT Decoder Channels to be used for the combined
   * color image's red, green and blue channels, abort if EOF */
  if( Load_Line(line, glrptrc, _("Combined Color Image Channel Numbers")) != SUCCESS )
    return( FALSE );
  for( idx = 0; idx < CHANNEL_IMAGE_NUM; idx++ )
  {
    rc_data.color_channel[idx] = (uint8_t)( atoi(line + 2 * idx) );
    if( rc_data.color_channel[idx] >= CHANNEL_IMAGE_NUM )
    {
      Show_Message(
          "Channel Number for Combined\n"
          "Color Image out of Range", "red" );
      return( FALSE );
    }
  }

  /* Read image APIDs to invert palette, abort if EOF */
  if( Load_Line(line, glrptrc, _("Invert Palette APIDs")) != SUCCESS )
    return( FALSE );
  char *nptr = line, *endptr = NULL;
  for( idx = 0; idx < 3; idx++ )
  {
    rc_data.invert_palette[idx] = (uint32_t)( strtol(nptr, &endptr, 10) );
    nptr = ++endptr;
  }

  /* Read Red Channel Normalization Range, abort if EOF */
  if( Load_Line(line, glrptrc, _("Red Channel Normalization Range")) != SUCCESS )
    return( FALSE );
  rc_data.norm_range[RED][NORM_RANGE_BLACK] = (uint8_t)( atoi(line) );
  idx = 0;
  while( line[idx++] != '-' );
  rc_data.norm_range[RED][NORM_RANGE_WHITE] = (uint8_t)( atoi(&line[idx]) );

  /* Read Green Channel Normalization Range, abort if EOF */
  if( Load_Line(line, glrptrc, _("Green Channel Normalization Range")) != SUCCESS )
    return( FALSE );
  rc_data.norm_range[GREEN][NORM_RANGE_BLACK] = (uint8_t)atoi( line );
  idx = 0;
  while( line[idx++] != '-' );
  rc_data.norm_range[GREEN][NORM_RANGE_WHITE] = (uint8_t)( atoi(&line[idx]) );

  /* Read Blue Channel Normalization Range, abort if EOF */
  if( Load_Line(line, glrptrc, _("Blue Channel Normalization Range")) != SUCCESS )
    return( FALSE );
  rc_data.norm_range[BLUE][NORM_RANGE_BLACK] = (uint8_t)( atoi(line) );
  idx = 0;
  while( line[idx++] != '-' );
  rc_data.norm_range[BLUE][NORM_RANGE_WHITE] = (uint8_t)( atoi(&line[idx]) );

  /* Read Blue Channel min pixel value in pseudo-color image */
  if( Load_Line(line, glrptrc,
        _("Blue Channel min pixel value in pseudo-color image")) != SUCCESS )
    return( FALSE );
  rc_data.colorize_blue_min = (uint8_t)( atoi(line) );

  /* Read Blue Channel max pixel value to enhance in pseudo-color image */
  if( Load_Line(line, glrptrc,
        _("Blue Channel max pixel value in to enhance pseudo-color image")) != SUCCESS )
    return( FALSE );
  rc_data.colorize_blue_max = (uint8_t)( atoi(line) );

  /* Read Blue Channel pixel value above which we assume it is a cloudy area */
  if( Load_Line(line, glrptrc,
        _("Blue Channel cloud area pixel value threshold")) != SUCCESS )
    return( FALSE );
  rc_data.clouds_threshold = (uint8_t)( atoi(line) );

  /* Check low pass filter bandwidth. It should be at
   * least 100kHz and no more than about 200kHz */
  if( (rc_data.sdr_filter_bw < MIN_BANDWIDTH) ||
      (rc_data.sdr_filter_bw > MAX_BANDWIDTH) )
  {
    Show_Message(
        _("Invalid Roofing Filter Bandwidth\n"\
          "Quit and correct glrptrc"), "red" );
    Error_Dialog();
    return( FALSE );
  }

  /* Set Gain control buttons and slider */
  if( rc_data.tuner_gain > 0.0 )
  {
    GtkWidget *radiobtn =
      Builder_Get_Object( main_window_builder, "manual_agc_radiobutton" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(radiobtn), TRUE );
    ClearFlag( TUNER_GAIN_AUTO );
  }
  else
  {
    GtkWidget *radiobtn =
      Builder_Get_Object( main_window_builder, "auto_agc_radiobutton" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(radiobtn), TRUE );
    SetFlag( TUNER_GAIN_AUTO );
  }

  /* Initialize top window etc */
  Initialize_Top_Window();

  fclose( glrptrc );

  return( FALSE );
} /* End of Load_Config() */

/*------------------------------------------------------------------*/

/* Find_Config_Files()
 *
 * Searches glrpt's home directory for per-satellite configuration
 * files and sets up the "Select Satellite" menu item accordingly
 */
  gboolean
Find_Config_Files( gpointer data )
{
  char *ext;
  DIR *glrpt_dir = NULL;
  struct dirent **file_list;
  int num_files, idx;
  GtkWidget *sat_menu;


  /* Setup directory path to glrpt's working directory */
  snprintf( rc_data.glrpt_dir,
      sizeof(rc_data.glrpt_dir), "%s/glrpt/", getenv("HOME") );

  /* Build "Select Satellite" Menu item */
  if( !popup_menu )
    popup_menu = create_popup_menu( &popup_menu_builder );
  sat_menu = Builder_Get_Object( popup_menu_builder, "select_satellite" );

  /* Look for files with a .cfg extention */
  errno = 0;
  found_cfg = 0;
  num_files = scandir( rc_data.glrpt_dir, &file_list, NULL, alphasort );
  for( idx = 0; idx < num_files; idx++ )
  {
    /* Look for files with a ".cfg" extention */
    if( (ext = strstr(file_list[idx]->d_name, ".cfg")) )
    {
      /* Cut off file extention to create a satellite name */
      *ext = '\0';

      /* Append new child items to Select Satellite menu */
      GtkWidget *menu_item =
        gtk_menu_item_new_with_label( file_list[idx]->d_name );
      g_signal_connect( menu_item, "activate",
          G_CALLBACK( on_satellite_menuitem_activate ), NULL );
      gtk_widget_show(menu_item);
      gtk_menu_shell_append( GTK_MENU_SHELL(sat_menu), menu_item );

      /* Make first entry the default */
      if( rc_data.satellite_name[0] == '\0' )
        Strlcpy( rc_data.satellite_name,
            file_list[idx]->d_name, sizeof(rc_data.satellite_name) );

      found_cfg++;
    } /* if( (ext = strstr(file_list[idx]->d_name, ".cfg")) ) */
  } /* for( idx = 0; idx < num_files; idx++ ) */

  /* Check for number of config files found */
  if( !found_cfg )
  {
    Show_Message( _("No configuration file(s) found"), "red" );
    Error_Dialog();
    return( FALSE );
  }

  closedir( glrpt_dir );
  return( FALSE );
} /* Find_Config_Files() */

/*------------------------------------------------------------------*/
