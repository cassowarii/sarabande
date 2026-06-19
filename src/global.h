#ifdef DEBUG
#define PANIC(msg) do { fprintf(stderr, "panic: " __FILE__ ":%d: " msg "\n", __LINE__); abort(); } while (0)
#else
#define PANIC(x) 0
#endif
