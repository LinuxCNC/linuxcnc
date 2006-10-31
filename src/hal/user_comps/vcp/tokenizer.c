/** This file, 'tokenizer.c', contains functions that can read tokens
    from a file.  See 'tokenizer.h' for documentation for these functions.
*/

/** Copyright (C) 2005 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tokenizer.h"

token_file_t *tf_open(char *name, int max_token_len, char *delims,
                      char escape, char comment, char quote)
{
    token_file_t *tf;

    
    if ( max_token_len < 2 ) {
	/* negative or zero would be bad, 2 avoids possible 
	   strangeness with 1 */
	max_token_len = 2;
    }
    /* allocate the structure */
    tf = (token_file_t *)malloc(sizeof(token_file_t));
    if ( tf == NULL ) {
        printf( "token_file_open(): unable to allocate memory\n" );
        return NULL;
    }
    /* init fields */
    tf->fp = NULL;
    tf->fname = NULL;
    tf->buffer = NULL;
    tf->delims = NULL;
    tf->escape = '\0';
    tf->comment = '\0';
    tf->quote = '\0';
    tf->maxtokenlen = max_token_len;
    tf->tokennum = 0;
    tf->linenum = 1;
    tf->tokenline = 1;
    tf->colnum = 0;
    tf->tokencol = 0;
    tf->lastc = 0;
    /* allocate space and save filename */
    tf->fname = (char *)malloc(strlen(name)+1);
    if ( tf->fname == NULL ) {
        printf( "token_file_open(): unable to allocate memory\n" );
        tf_close(tf);
        return NULL;
    }
    strcpy(tf->fname, name);
    /* allocate space and save delimiters */
    if ( delims != NULL ) {
        tf->delims = (char *)malloc(strlen(delims)+1);
        if ( tf->delims == NULL ) {
            printf( "token_file_open(): unable to allocate memory\n" );
            tf_close(tf);
            return NULL;
        }
        strcpy(tf->delims, delims);
    }
    /* save other special characters */
    tf->escape = escape;
    tf->comment = comment;
    tf->quote = quote;
    /* allocate space for buffer */
    tf->buffer = (char *)malloc(tf->maxtokenlen+1);
    if ( tf->buffer == NULL ) {
        printf( "token_file_open(): unable to allocate memory\n" );
        tf_close(tf);
        return NULL;
    }
    /* open file */
    tf->fp = fopen(name, "r");
    if ( tf->fp == NULL ) {
        printf( "token_file_open(): unable to open file '%s'\n", name );
        tf_close(tf);
        return NULL;
    }
    return tf;
}

void tf_close(token_file_t *tf)
{
    if ( tf == NULL ) {
        return;
    }
    if ( tf->fname != NULL ) {
        free(tf->fname);
    }
    if ( tf->delims != NULL ) {
        free(tf->delims);
    }
    if ( tf->buffer != NULL ) {
        free(tf->buffer);
    }
    if ( tf->fp != NULL ) {
        fclose(tf->fp);
    }
    free(tf);
}

char *tf_get_token(token_file_t *tf)
{
    char *rv;
    int c, len, escape;
    enum { IN_TOKEN,
	   IN_QUOTED_TOKEN,
	   SKIP_WS,
	   SKIP_COMMENT,
	   START_TOKEN,
	   DONE } state;

    len = 0;
    tf->buffer[len] = '\0';
    escape = 0;
    state = SKIP_WS;
    while ( state != DONE ) {
        /* get a character */
        if ( tf->lastc == '\0' ) {
            /* get a new character */
            c = fgetc(tf->fp);
            /* update column count */
            tf->colnum++;
            /* update line count */
            if ( c == '\n' ) {
                tf->linenum++;
                tf->colnum = 0;
            }
        } else {
            /* get character left over from last pass */
            c = tf->lastc;
            tf->lastc = '\0';
        }
        /* handle EOF? */
        if ( c == EOF ) {
            if ( state == IN_QUOTED_TOKEN ) {
                printf ( "token_file_get_token(): End of file in quoted string\n" );
                return NULL;
            } else if ( state == IN_TOKEN ) {
                state = DONE;
            } else {
                return NULL;
            }
        }
        /* main state machine */
        switch ( state ) {
        case IN_TOKEN:
            if ( escape ) {
                /* add next char to token, regardless of what it is */
                 if ( len < tf->maxtokenlen ) {
                    tf->buffer[len++] = c;
                }
                escape = 0;
            } else if ( c == tf->escape ) {
                escape = 1;
            } else if ( c == tf->comment ) {
                state = DONE;
                tf->lastc = c;
            } else if ( isspace(c) ) {
                state = DONE;
                tf->lastc = c;
            } else if ( strchr(tf->delims, c) ) {
                state = DONE;
                tf->lastc = c;
            } else {
                if ( len < tf->maxtokenlen ) {
                    tf->buffer[len++] = c;
                }
            }
            break;
        case IN_QUOTED_TOKEN:
            if ( escape ) {
                /* add next char to token, regardless of what it is */
                 if ( len < tf->maxtokenlen ) {
                    tf->buffer[len++] = c;
                }
                escape = 0;
            } else if ( c == tf->escape ) {
                escape = 1;
            } else if ( c == tf->quote ) {
                state = DONE;
            } else {
                if ( len < tf->maxtokenlen ) {
                    tf->buffer[len++] = c;
                }
            }
            break;
        case SKIP_WS:
            if ( !isspace(c) ) {
                state = START_TOKEN;
                tf->lastc = c;
            }
            break;
        case SKIP_COMMENT:
            if ( c == '\n' ) {
                state = SKIP_WS;
            }
            break;
        case START_TOKEN:
            if ( c == tf->comment ) {
                state = SKIP_COMMENT;
            } else if ( c == tf->quote ) {
                state = IN_QUOTED_TOKEN;
                tf->tokennum++;
                tf->tokenline = tf->linenum;
                tf->tokencol = tf->colnum;
            } else if ( c == tf->escape ) {
                state = IN_TOKEN;
                escape = 1;
                tf->tokennum++;
                tf->tokenline = tf->linenum;
                tf->tokencol = tf->colnum;
            } else if ( strchr(tf->delims, c) ) {
                /* first char is a delimiter, we're done */
                state = DONE;
                tf->buffer[len++] = c;
                tf->tokennum++;
                tf->tokenline = tf->linenum;
                tf->tokencol = tf->colnum;
            } else {
                state = IN_TOKEN;
                if ( len < tf->maxtokenlen ) {
                    tf->buffer[len++] = c;
                }
                tf->tokennum++;
                tf->tokenline = tf->linenum;
                tf->tokencol = tf->colnum;
            }
            break;
        default:
            break;
        }
    }
    /* terminate buffer */
    tf->buffer[len++] = '\0';
    /* allocate space for returned token */
    rv = (char *)malloc(len);
    if ( rv == NULL ) {
        printf( "token_file_get_token(): unable to allocate memory\n" );
        return NULL;
    }
    strcpy(rv, tf->buffer);
    return rv;
}
