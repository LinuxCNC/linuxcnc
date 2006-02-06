#ifndef TOKENIZER_H
#define TOKENIZER_H

/** This file, 'tokenizer.h', contains declarations and prototypes 
    for functions that can read tokens from a file.
*/

/** Copyright (C) 2005 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
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

typedef struct {
    FILE *fp;		/* source file */
    char *fname;	/* name of file */
    char *buffer;	/* buffer for reading */
    char *delims;	/* string of delimiter chars */
    char escape;	/* escape character */
    char comment;	/* comment character */
    char quote;		/* quote character */
    int maxtokenlen;	/* maximum token length */
    int linenum;	/* current line number */
    int colnum;		/* current column number */
    int tokennum;	/* current token number */
    int tokenline;	/* line on which current token starts */
    int tokencol;	/* column on which current token starts */
    int lastc;		/* previously read but unused character */
} token_file_t;

/** 'tf_open()' is used to open a file to be tokenized, and to initialize
    the tokenizer.

    'name' is the name of the file to be opened.
    'max_token_len' is the maximum token length, tokens longer than
       this will be truncated (the remainder will be the next token).
    'delims' is a string containing all the legal delimiter characters.
    'escape' is the escape character, used to ignore the special meaning
       of delimiters, command characters, and quotes.  It is also used
       to invoke alternate meanings of some characters, such as (escape)n,
       which is a newline.
    'comment' is the character that introduces a comment, all text from the
       comment character to the end of a line is ignored.
    'quote' is the character used to mark the beginning of a quoted string.
       The string ends at the next unescaped quote character.  Inside a 
       quoted string, whitespace, delimiters, and comment characters have
       no special meaning and do not need to be escaped.  Alternate meanings
       for escaped characters, such as (escape)n for newline, are still
       recognized inside quoted strings.
*/
   
token_file_t *tf_open(char *name, int max_token_len, char *delims,
                      char escape, char comment, char quote);

/** 'tf_get_token()' returns the next token from the file.  It is returned
    as a malloc'ed string, and should be freed when the program is done 
    with it.  The return values consists either of a delimiter from the
    'delims' string passed when the file was opened, or a string.  Tokens
    are separated by whitespace or delimiters.  Tokens can contain whitespace
    or delimiters if those characters are escaped with the escape character,
    of if the token is a quoted string.
*/

char *tf_get_token(token_file_t *tf);

/** 'tf_close()' closes a token file that was opened by tf_open().
*/

void tf_close(token_file_t *tf);

#endif /* TOKENIZER_H */
