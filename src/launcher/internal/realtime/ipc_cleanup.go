// ipc_cleanup.go — this file is intentionally empty.
// SysV shared memory is cleaned up in-process by halpr_rtapi_app_exit()
// via rtapi_shmem_delete() which calls shmdt/shmctl(IPC_RMID).
package realtime
