#ifndef INETFILE_HH
#define INETFILE_HH


#ifdef __cplusplus
class INET_FILE;
#else
#define INET_FILE void
#endif

#ifdef __cplusplus
extern "C" {
#endif

    int inet_file_init(const char *, char *, int debug);
    int inet_file_exit();
    INET_FILE *inet_file_open(const char *url, char *);
    int inet_file_close(INET_FILE *);
    char *inet_file_gets(char *, int, INET_FILE *);
    int inet_file_eof(INET_FILE *);
    int inet_file_rewind(INET_FILE *);

#ifdef __cplusplus
}
#endif
#endif
