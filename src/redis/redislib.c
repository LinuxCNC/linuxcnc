// hiredis/LinuxCNC glue

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#include <errno.h>

#include "config.h"
#include "inifile.h"
#include "rtapi.h"		// only rtapi_print_msg
#include "hiredis.h"
#include "redislib.h"

#define MIN_TIMEOUT 0.5  // seconds to connect to redis server

static int emc_debug;

static int redis_port = 6379;
static const char *redis_host = "127.0.0.1";
static const char *redis_password;
static double redis_timeout = 1.5;

static redisContext *redis_context;

int redis_close()
{
    if (!redis_context) return -1;
    // disconnects from server
    redisFree(redis_context);
    return 0;
}

int redis_init(const char *inifile)
{
    FILE *fp;
    const char *s;
    struct timeval timeout = { 2, 0 };
    redisReply *reply;

    if (redis_context) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s:%d: already connected to server at %s:%d\n",
			__FILE__, __LINE__, redis_host, redis_port);
	return 0;
    }
    if (!inifile)
	inifile = getenv("INI_FILE_NAME");
    if (!inifile) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s:%d: no inifile, assuming redis server at %s:%d\n",
			__FILE__, __LINE__, redis_host, redis_port);
    } else {
	// see if overridden in ini
	if ((fp = fopen(inifile, "r")) != NULL) {
	    iniFindInt(fp, "DEBUG", "EMC", &emc_debug);
	    iniFindInt(fp, "PORT", "REDIS", &redis_port);
	    if (iniFindDouble(fp, "TIMEOUT", "REDIS", &redis_timeout) && redis_timeout > MIN_TIMEOUT) {
		timeout.tv_sec = floor(redis_timeout);
		timeout.tv_usec = (redis_timeout-timeout.tv_sec) * 1000000;
	    }
	    if ((s = iniFind(fp, "HOST", "REDIS"))) {
		redis_host = strdup(s);
	    }
	    if ((s = iniFind(fp, "PASSWORD", "REDIS"))) {
		redis_password = strdup(s);
	    }
	    fclose(fp);
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s:%d: cant open inifile '%s', assuming server at %s:%d\n",
			    __FILE__, __LINE__, inifile, redis_host, redis_port);
	}
    }

    // connect to redis server
    redis_context = redisConnectWithTimeout(redis_host, redis_port, timeout);
    if (redis_context->err) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s:%d: cant connect to Redis server %s:%d - %s\n",
			__FILE__, __LINE__, redis_host, redis_port, redis_context->errstr);
	return -1;
    }

    // authenticate if [REDIS]PASSWORD given
    if (redis_password) {
	reply = redisCommand(redis_context,"AUTH %s", redis_password);
	if (reply != NULL) {
	    freeReplyObject(reply);
	} else
	    return -2;
    }

    // make sure she talks to us
    reply = redisCommand(redis_context, "PING");
    freeReplyObject(reply);
    if (redis_context->err) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s:%d: Redis server doesnt answer to 'PING': %s\n",
			__FILE__, __LINE__, redis_context->errstr);
	return -3;
    }
    // redis_cmd("FLUSHALL"); // drops database (!)
    return 0;
}


// corresponds to REDIS_REPLY_STRING etc from hiredis.h
static char *redis_replytypes[] = {
    "unknown",
    "string",
    "array",
    "integer",
    "nil",
    "status",
    "error"
};

int redis_get_double(double *value,const char *format, ...)
{
    va_list ap;
    int status = -1;
    redisReply *reply;
    char *endptr;
    double d;

    if (!redis_context) return -1;

    va_start(ap,format);
    reply =  (redisReply *) redisvCommand(redis_context, format,  ap);
    va_end(ap);
    if (!reply) { return -1; }

    switch (reply->type) {
    case REDIS_REPLY_STRING:
	d =  strtod(reply->str, &endptr);
	if (endptr == reply->str) { // no conversion occured
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: could not convert '%s' to double - query was:\n",
			    __FUNCTION__,
			    reply->str);
	    va_start(ap,format);
	    rtapi_print_msg(RTAPI_MSG_ERR, format, ap);
	    va_end(ap);
	}
	if (reply->str + reply->len < endptr) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: trailing garbage after char %d: '%s'\n",
			    __FUNCTION__, endptr-reply->str,
			    reply->str);
	}
	*value = d;
	status = 0;
	break;

    default:
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: query returned a %s type reply instead of 'integer': %s, query was:\n",
			__FUNCTION__,
			redis_replytypes[reply->type],
			reply->str);
	va_start(ap,format);
	rtapi_print_msg(RTAPI_MSG_ERR, format, ap);
	va_end(ap);
	status = -reply->type;
	break;
    }
    freeReplyObject(reply);
    return status;
}

int redis_get_int(int *value, const char *format, ...)
{
    va_list ap;
    int status = -1;
    redisReply *reply;
    char *endptr;
    long val;

    if (!redis_context) return -1;

    va_start(ap,format);
    reply =  (redisReply *) redisvCommand(redis_context, format,  ap);
    va_end(ap);
    if (!reply) { return -1; }

    switch (reply->type) {
    case REDIS_REPLY_INTEGER:
	*value = reply->integer;
	status = 0;
	break;

    case REDIS_REPLY_STRING:
	errno = 0;    // To distinguish success/failure after call
	val = strtol(reply->str, &endptr, 10);

           /* Check for various possible errors */

	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
	    || (errno != 0 && val == 0)) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: couldnt convert '%s' to an integer\n",
			    __FUNCTION__,
			    reply->str);
	    status = -1;
	    break;
	}

	if (endptr == reply->str) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: integer conversion  failed '%s'\n",
			    __FUNCTION__,
			    reply->str);
	    status = -1;
	    break;
	}
	*value = val;
	status = 0;
	break;

    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: query returned a %s type reply instead of 'integer': %s\n",
			__FUNCTION__,
			redis_replytypes[reply->type],
			reply->str);
	va_start(ap,format);
	rtapi_print_msg(RTAPI_MSG_ERR, format, ap);
	va_end(ap);
	status = -reply->type;
	break;
    }
    freeReplyObject(reply);
    return status;
}

int redis_get_string(char *buf, int bufsize, const char *format, ...)
{
    va_list ap;
    int status = -1;
    redisReply *reply;

    if (!redis_context) return -1;

    va_start(ap,format);
    reply =  (redisReply *) redisvCommand(redis_context, format,  ap);
    va_end(ap);
    if (!reply) { return -1; }

    switch (reply->type) {
    case REDIS_REPLY_STRING:
	if (reply->len >= bufsize) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: buffer size %d too small for reply '%s'\n",
			    __FUNCTION__, bufsize, reply->str);
	    status = -1;
	    break;
	}
	strcpy(buf,reply->str);
	status = 0;
	break;

    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: query returned a %s type reply instead of 'integer': %s\n",
			__FUNCTION__,
			redis_replytypes[reply->type],
			reply->str);
	va_start(ap,format);
	rtapi_print_msg(RTAPI_MSG_ERR, format, ap);
	va_end(ap);
	status = -reply->type;
	break;
    }
    freeReplyObject(reply);
    return status;
}

int redis_cmd(const char *format, ...)
{
    va_list ap;
    int status = -1;
    redisReply *reply;
    va_start(ap,format);
    reply =  (redisReply *) redisvCommand(redis_context, format,  ap);
    va_end(ap);
    if (!reply) { return -1; }

    switch (reply->type) {
    case REDIS_REPLY_INTEGER:
	status = reply->integer;
	break;

    case REDIS_REPLY_STATUS:
	if (!strncmp(reply->str, "OK", 3)) {
	    status = 0;
	    break;
	}
	if (!strncmp(reply->str, "QUEUED", 7)) {
	    status = 0;
	    break;
	}
	// fall through
    case REDIS_REPLY_ERROR:
	rtapi_print_msg(RTAPI_MSG_DBG, "%s:%d: redis  %s %s\n",
			__FILE__, __LINE__,
			reply->type == REDIS_REPLY_ERROR ? "error" : "status",
			reply->str);
	break;
    }
    freeReplyObject(reply);
    return status;
}


#if 0

 int base;

           if (argc < 2) {
               fprintf(stderr, "Usage: %s str [base]\n", argv[0]);
               exit(EXIT_FAILURE);
           }

           str = argv[1];
           base = (argc > 2) ? atoi(argv[2]) : 10;

           errno = 0;    /* To distinguish success/failure after call */
           val = strtol(str, &endptr, base);

           /* Check for various possible errors */

           if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                   || (errno != 0 && val == 0)) {
               perror("strtol");
               exit(EXIT_FAILURE);
           }

           if (endptr == str) {
               fprintf(stderr, "No digits were found\n");
               exit(EXIT_FAILURE);
           }

           /* If we got here, strtol() successfully parsed a number */

#endif
