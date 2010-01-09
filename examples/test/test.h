#include <spotify/api.h>

typedef void (SP_CALLCONV *test_callback)(sp_session *session, void *);

struct testrun {
        sp_session *session;

        char **name;
        test_callback *callbacks;
        void **arg;

        int current_test;
        int num_tests;

        int running;
};


void test_init(sp_session *session);
void test_add(char *name, test_callback callback, void *arg);
void test_start(void);
int test_run(void);
void test_finish(void);

extern struct testrun *test;
