#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>

#include <inifile.h>

void myerror(const char *pfx, int err)
{
	fprintf(stderr, "%s: ", pfx);
	switch(err) {
	case -EINVAL: fprintf(stderr, "Invalid argument or ini-file"); break;
	case -ENOSPC: fprintf(stderr, "Buffer too small"); break;
	case -ENOENT: fprintf(stderr, "Entry not found"); break;
	default:      fprintf(stderr, "Unknown error: %d", err); break;
	}
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	int lose = 0;
	int optc;
	const char *var = NULL;
	const char *sec = NULL;
	char typ = 's';

	while(-1 != (optc = getopt(argc, argv, "v:s:t:"))) {
		switch(optc) {
		case 'v':
			var = strdup(optarg);
			break;
		case 's':
			sec = strdup(optarg);
			break;
		case 't':
			switch(*optarg) {
			case 'b':
			case 'i':
			case 'n':
			case 'u':
			case 'r':
			case 's':
				typ = *optarg;
				break;
			default:
				fprintf(stderr, "Invalid type specifier '%c'\n", isprint(*optarg & 0xff) ? *optarg : '?');
				lose++;
				break;
			}
			break;
		default:
			lose++;
			break;
		}
	}

	if(optind >= argc) {
		fprintf(stderr, "Missing ini file\n");
		lose++;
	}

	if(lose)
		return 1;

	int rv;
	char buf[PATH_MAX];
	double r;
	int n;
	rtapi_s64 i;
	rtapi_u64 u;
	bool b;

	switch(typ) {
	case 'b':
		rv = iniFindBool(argv[optind], var, sec, &b);
		if(!rv) {
			printf("%s\n", b ? "true" : "false");
		} else {
			myerror("iniFindBool", rv);
			return -rv;
		}
		break;
	case 'n':
		rv = iniFindInt(argv[optind], var, sec, &n);
		if(!rv) {
			printf("%d\n", n);
		} else {
			myerror("iniFindInt", rv);
			return -rv;
		}
		break;
	case 'i':
		rv = iniFindSInt(argv[optind], var, sec, &i);
		if(!rv) {
			printf("%ld\n", i);
		} else {
			myerror("iniFindSInt", rv);
			return -rv;
		}
		break;
	case 'u':
		rv = iniFindUInt(argv[optind], var, sec, &u);
		if(!rv) {
			printf("%lu\n", u);
		} else {
			myerror("iniFindUInt", rv);
			return -rv;
		}
		break;
	case 'r':
		rv = iniFindDouble(argv[optind], var, sec, &r);
		if(!rv) {
			printf("%.15lg\n", r);
		} else {
			myerror("iniFindDouble", rv);
			return -rv;
		}
		break;
	case 's':
		rv = iniFindString(argv[optind], var, sec, buf, sizeof(buf));
		if(!rv) {
			printf("%s\n", buf);
		} else {
			myerror("iniFindString", rv);
			return -rv;
		}
		break;
	default:
		return 1;
	}
	
	return 0;
}
// vim: ts=4 sw=4
