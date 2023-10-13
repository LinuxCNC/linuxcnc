
/*
 * Togl - a Tk OpenGL widget
 * Version 1.6
 * Copyright (C) 1996-1998  Brian Paul and Ben Bederson
 * See the LICENSE-Togl file for copyright details.
 */

#ifndef TOGL_H
#define TOGL_H

#if defined(WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN
#   if defined(_MSC_VER)
#	define EXPORT(a,b) __declspec(dllexport) a b
#	define DllEntryPoint DllMain
#   else
#	if defined(__BORLANDC__)
#	    define EXPORT(a,b) a _export b
#	else
#	    define EXPORT(a,b) a b
#	endif
#   endif
#else
#   define EXPORT(a,b) a b
#endif /* WIN32 */

#ifdef macintosh
# ifndef MAC_TCL
#   define MAC_TCL
# endif
#endif

#include <tcl.h>
#include <tk.h>
#include <GL/gl.h>
#ifdef TOGL_X11
#include <X11/Xlib.h>
#endif

#ifdef __sgi
#include <GL/glx.h>
#include <X11/extensions/SGIStereo.h>
#endif


#ifndef NULL
#define NULL    0
#endif


#ifdef __cplusplus
extern "C" {
#endif



#define TOGL_VERSION "1.6"
#define TOGL_MAJOR_VERSION 1
#define TOGL_MINOR_VERSION 6



/*
 * "Standard" fonts which can be specified to Togl_LoadBitmapFont()
 */
#define TOGL_BITMAP_8_BY_13		((char *) 1)
#define TOGL_BITMAP_9_BY_15		((char *) 2)
#define TOGL_BITMAP_TIMES_ROMAN_10	((char *) 3)
#define TOGL_BITMAP_TIMES_ROMAN_24	((char *) 4)
#define TOGL_BITMAP_HELVETICA_10	((char *) 5)
#define TOGL_BITMAP_HELVETICA_12	((char *) 6)
#define TOGL_BITMAP_HELVETICA_18	((char *) 7)
 

/*
 * Normal and overlay plane constants
 */
#define TOGL_NORMAL	1
#define TOGL_OVERLAY	2



struct Togl;


typedef void (Togl_Callback) (struct Togl *togl);
typedef int  (Togl_CmdProc) (struct Togl *togl, int argc, char *argv[]);
  
  EXPORT(int,Togl_Init)(Tcl_Interp *interp);

/*
 * Default/initial callback setup functions
 */

extern void Togl_CreateFunc( Togl_Callback *proc );

extern void Togl_DisplayFunc( Togl_Callback *proc );

extern void Togl_ReshapeFunc( Togl_Callback *proc );

extern void Togl_DestroyFunc( Togl_Callback *proc );

extern void Togl_TimerFunc( Togl_Callback *proc );

extern void Togl_ResetDefaultCallbacks( void );


/*
 * Change callbacks for existing widget
 */

extern void Togl_SetCreateFunc( struct Togl *togl, Togl_Callback *proc );

extern void Togl_SetDisplayFunc( struct Togl *togl, Togl_Callback *proc );

extern void Togl_SetReshapeFunc( struct Togl *togl, Togl_Callback *proc );

extern void Togl_SetDestroyFunc( struct Togl *togl, Togl_Callback *proc );

extern void Togl_SetTimerFunc( struct Togl *togl, Togl_Callback *proc );


/*
 * Miscellaneous
 */

extern int Togl_Configure( Tcl_Interp *interp, struct Togl *togl, 
                           int argc, char *argv[], int flags );

extern void Togl_MakeCurrent( const struct Togl *togl );

extern void Togl_CreateCommand( char *cmd_name,
                                Togl_CmdProc *cmd_proc );

extern void Togl_PostRedisplay( struct Togl *togl );

extern void Togl_SwapBuffers( const struct Togl *togl );


/*
 * Query functions
 */

extern char *Togl_Ident( const struct Togl *togl );

extern int Togl_Width( const struct Togl *togl );

extern int Togl_Height( const struct Togl *togl );

extern Tcl_Interp *Togl_Interp( const struct Togl *togl );

extern Tk_Window Togl_TkWin( const struct Togl *togl );


/*
 * Color Index mode
 */

extern unsigned long Togl_AllocColor( const struct Togl *togl,
                                      float red, float green, float blue );

extern void Togl_FreeColor( const struct Togl *togl, unsigned long index );

extern void Togl_SetColor( const struct Togl *togl, unsigned long index,
                           float red, float green, float blue );


/*
 * Bitmap fonts
 */

extern GLuint Togl_LoadBitmapFont( const struct Togl *togl,
                                   const char *fontname );

extern void Togl_UnloadBitmapFont( const struct Togl *togl, GLuint fontbase );
extern int Togl_BitmapFontMetrics( const struct Togl *togl,
                                   const char *fontname,
                                   int *charwidth, int *linespace);


/*
 * Overlay functions
 */

extern void Togl_UseLayer( struct Togl *togl, int layer );

extern void Togl_ShowOverlay( struct Togl *togl );

extern void Togl_HideOverlay( struct Togl *togl );

extern void Togl_PostOverlayRedisplay( struct Togl *togl );

extern void Togl_OverlayDisplayFunc( Togl_Callback *proc );

extern int Togl_ExistsOverlay( const struct Togl *togl );

extern int Togl_GetOverlayTransparentValue( const struct Togl *togl );

extern int Togl_IsMappedOverlay( const struct Togl *togl );

extern unsigned long Togl_AllocColorOverlay( const struct Togl *togl,
                                             float red, float green, 
                                             float blue );

extern void Togl_FreeColorOverlay( const struct Togl *togl, 
                                   unsigned long index );

/*
 * User client data
 */

extern void Togl_ClientData( ClientData clientData );

extern ClientData Togl_GetClientData( const struct Togl *togl );

extern void Togl_SetClientData( struct Togl *togl, ClientData clientData );


/*
 * X11-only commands.
 * Contributed by Miguel A. De Riera Pasenau (miguel@DALILA.UPC.ES)
 */

#ifdef TOGL_X11
extern Display *Togl_Display( const struct Togl *togl );
extern Screen *Togl_Screen( const struct Togl *togl );
extern int Togl_ScreenNumber( const struct Togl *togl );
extern Colormap Togl_Colormap( const struct Togl *togl );
#endif


/*
 * SGI stereo-only commands.
 * Contributed by Ben Evans (Ben.Evans@anusf.anu.edu.au)
 */

#ifdef __sgi
extern void Togl_StereoDrawBuffer( GLenum mode );
extern void Togl_StereoFrustum( GLfloat left, GLfloat right,
                                GLfloat bottom, GLfloat top,
                                GLfloat near, GLfloat far,
                                GLfloat eyeDist, GLfloat eyeOffset );
extern void Togl_StereoClear( GLbitfield mask );
#endif


/*
 * Generate EPS file.
 * Contributed by Miguel A. De Riera Pasenau (miguel@DALILA.UPC.ES)
 */

extern int Togl_DumpToEpsFile( const struct Togl *togl,
                               const char *filename,
                               int inColor,
                               void (*user_redraw)(const struct Togl *) );



/* Mac-specific setup functions */
#ifdef macintosh
int Togl_MacInit(void);
int Togl_MacSetupMainInterp(Tcl_Interp *interp);
#endif

#ifdef __cplusplus
}
#endif


#endif
