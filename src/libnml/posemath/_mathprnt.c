
/*
  Modification History:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "posemath.h"
#include "mathprnt.h"

/*
   returns ptr to the NULL at the end of a string
*/
static char *END(char *s)
{
    return &s[strlen(s)];
}

static int ft_string(char *string, char *from, char *to)
{
    int count;

    count = to - from + 1;
    if (count > 256) {
	printf("ft_string:  String segment too long!\n");
	return -1;
    }

    if (count < 0) {
	printf("ft_string:  End pointer less than start pointer.\n");
	return -1;
    }

    strncpy(string, from, count);
    *(string + count) = '\0';

    return 0;
}

static int _pmSprintf(char *string, const char *f, va_list ap)
{
    char *format;
    char *ptr;
    char *conv_ptr;
    enum { SCAN, CONV, DONE } state;
    char fmt[256], fmt2[256];

    format = (char *) f;	/* discards const */

    /* scan format string for print options */
    state = SCAN;
    ptr = format;
    conv_ptr = format;
    *string = '\0';

    while (state != DONE) {
	switch (state) {
	case SCAN:
	    switch (*ptr) {
	    case '%':
		conv_ptr = ptr;
		state = CONV;
		break;

	    case '\0':
		sprintf(END(string), format);
		state = DONE;
		break;
	    }
	    break;

	case CONV:
	    switch (*ptr) {
	    case '%':
		state = SCAN;
		break;

	    case 'd':		/* integer formats */
	    case 'o':
	    case 'x':
	    case 'u':
		ft_string(fmt, format, ptr);
		sprintf(END(string), fmt, va_arg(ap, int));
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'e':		/* floating point formats */
	    case 'f':
	    case 'g':
		ft_string(fmt, format, ptr);
		sprintf(END(string), fmt, va_arg(ap, double));
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'c':		/* character format */
		ft_string(fmt, format, ptr);
		sprintf(END(string), fmt, va_arg(ap, int));
		format = ptr + 1;
		state = SCAN;
		break;

	    case 's':		/* string formats */
	    case 'b':
		ft_string(fmt, format, ptr);
		sprintf(END(string), fmt, va_arg(ap, char *));
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'v':		/* VECTOR format */
		ft_string(fmt, format, conv_ptr - 1);
		sprintf(END(string), fmt);
		{
		    PmCartesian vecky;	/* vector is a reserved word in new
					   G4 compiler */

		    vecky = va_arg(ap, PmCartesian);
		    ft_string(fmt, conv_ptr, ptr - 1);
		    sprintf(fmt2, "%sf %sf %sf", fmt, fmt, fmt);
		    sprintf(END(string), fmt2, vecky.x, vecky.y, vecky.z);
		}
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'q':		/* QUATERNION format */
		ft_string(fmt, format, conv_ptr - 1);
		sprintf(END(string), fmt);
		{
		    PmQuaternion quat;

		    quat = va_arg(ap, PmQuaternion);
		    ft_string(fmt, conv_ptr, ptr - 1);
		    sprintf(fmt2, "%sf %sf %sf %sf", fmt, fmt, fmt, fmt);
		    sprintf(END(string), fmt2, quat.s, quat.x, quat.y,
			quat.z);
		}
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'Q':		/* QUATERNION/MATRIX format */
		ft_string(fmt, format, conv_ptr - 1);
		sprintf(END(string), fmt);
		{
		    PmQuaternion quat;
		    PmRotationMatrix mat;

		    quat = va_arg(ap, PmQuaternion);
		    ft_string(fmt, conv_ptr, ptr - 1);
		    pmQuatMatConvert(quat, &mat);
		    sprintf(fmt2, "\n%sf %sf %sf", fmt, fmt, fmt);
		    sprintf(END(string), fmt2, mat.x.x, mat.y.x, mat.z.x);
		    sprintf(END(string), fmt2, mat.x.y, mat.y.y, mat.z.y);
		    sprintf(END(string), fmt2, mat.x.z, mat.y.z, mat.z.z);
		}
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'm':		/* MATRIX format */
		ft_string(fmt, format, conv_ptr - 1);
		sprintf(END(string), fmt);
		{
		    PmRotationMatrix mat;

		    mat = va_arg(ap, PmRotationMatrix);
		    ft_string(fmt, conv_ptr, ptr - 1);
		    sprintf(fmt2, "\n%sf %sf %sf ", fmt, fmt, fmt);
		    sprintf(END(string), fmt2, mat.x.x, mat.y.x, mat.z.x);
		    sprintf(END(string), fmt2, mat.x.y, mat.y.y, mat.z.y);
		    sprintf(END(string), fmt2, mat.x.z, mat.y.z, mat.z.z);
		}
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'p':		/* POSE format */
		ft_string(fmt, format, conv_ptr - 1);
		sprintf(END(string), fmt);
		{
		    PmPose pose;

		    pose = va_arg(ap, PmPose);
		    ft_string(fmt, conv_ptr, ptr - 1);
		    sprintf(fmt2, "%sf %sf %sf  %sf %sf %sf %sf",
			fmt, fmt, fmt, fmt, fmt, fmt, fmt);
		    sprintf(END(string), fmt2,
			pose.tran.x, pose.tran.y, pose.tran.z,
			pose.rot.s, pose.rot.x, pose.rot.y, pose.rot.z);

		}
		format = ptr + 1;
		state = SCAN;
		break;

	    case 'P':		/* POSE/MATRIX format */
		ft_string(fmt, format, conv_ptr - 1);
		sprintf(END(string), fmt);
		{
		    PmPose pose;
		    PmRotationMatrix mat;

		    pose = va_arg(ap, PmPose);
		    ft_string(fmt, conv_ptr, ptr - 1);
		    pmQuatMatConvert(pose.rot, &mat);
		    sprintf(fmt2, "\n%sf %sf %sf  %sf", fmt, fmt, fmt, fmt);
		    sprintf(END(string), fmt2, mat.x.x, mat.y.x, mat.z.x,
			pose.tran.x);
		    sprintf(END(string), fmt2, mat.x.y, mat.y.y, mat.z.y,
			pose.tran.y);
		    sprintf(END(string), fmt2, mat.x.z, mat.y.z, mat.z.z,
			pose.tran.z);
		}
		format = ptr + 1;
		state = SCAN;
		break;

	    case '-':		/* conversion modifiers */
	    case '+':
	    case ' ':
	    case '#':
	    case '.':
	    case 'l':
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		break;

	    default:
		printf("Invalid conversion specification:  %s\n", format);
		return -1;
	    }
	    break;

	default:
	    printf("Programming error in math_sprintf.\n");
	    return -1;
	}			/* end switch( state ) */
	ptr++;
    }				/* end while */
    return 0;
}

void pmSprintf(char *string, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _pmSprintf(string, fmt, ap);
    va_end(ap);
}

void pmPrintf(const char *fmt, ...)
{
    va_list ap;
    char string[512];

    va_start(ap, fmt);
    _pmSprintf(string, fmt, ap);
    printf("%s", string);
    va_end(ap);
}

void pmFprintf(FILE * fp, const char *fmt, ...)
{
    va_list ap;
    char string[512];

    va_start(ap, fmt);
    _pmSprintf(string, fmt, ap);
    fprintf(fp, "%s", string);
    va_end(ap);
}
