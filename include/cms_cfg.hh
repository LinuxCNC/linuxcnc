/********************************************************************
* Description: cms_cfg.hh
*   cms_config -- function which reads configuration file.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#ifndef CMS_CFG_HH
#define CMS_CFG_HH

class CMS;

/* Config File Definitions. */
#ifndef CMS_CONFIG_LINELEN
#define CMS_CONFIG_LINELEN 200
#endif
#ifndef CMS_CONFIG_COMMENTCHAR
#define CMS_CONFIG_COMMENTCHAR '#'
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Function Prototypes. */
    extern int cms_config(CMS ** c, const char *b, const char *p, const char *f,
	int set_to_server = 0, int set_to_master = 0);

    extern int cms_copy(CMS ** dest, CMS * src,
	int set_to_server = 0, int set_to_master = 0);

    extern int cms_create_from_lines(CMS ** cms, const char *buffer_line,
	const char *proc_line, int set_to_server = 0, int set_to_master = 0);

    extern int cms_create(CMS ** cms, const char *buf_line, const char *proc_line,
	const char *buffer_type, const char *proc_type,
	int set_to_server = 0, int set_to_master = 0);

    extern int load_nml_config_file(const char *file);
    extern int unload_nml_config_file(const char *file);
//    extern int print_loaded_nml_config_file_list();
//    extern int unload_all_nml_config_files();
    extern char *get_buffer_line(const char *buf, const char *file);
    extern int hostname_matches_bufferline(char *bufline);

#ifdef __cplusplus
}
#endif
#endif				/* !defined(CMS_CFG_HH) */
