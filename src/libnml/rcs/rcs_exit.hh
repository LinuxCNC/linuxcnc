#ifndef RCS_EXIT_HH
#define RCS_EXIT_HH

#ifdef __cplusplus
extern "C" {
#endif

    int attach_rcs_exit_list(void (*fptr) (int));
    void rcs_cleanup(int code);
    void rcs_exit(int code);

#ifdef __cplusplus
}
#endif
#endif
