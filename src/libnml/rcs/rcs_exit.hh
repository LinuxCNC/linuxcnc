#ifndef RCS_EXIT_HH
#define RCS_EXIT_HH

extern "C" {
    void rcs_cleanup(int code);
    void rcs_exit(int code);
}
#endif
