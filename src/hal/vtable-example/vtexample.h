
// example methods exported via vtable

typedef int (*vte_hello_t)(const char *);
typedef int (*vte_goodbye_t)(const char *);

typedef struct {
    vte_hello_t   hello;
    vte_goodbye_t goodbye;
} vtexample_t;
