#include <stdlib.h>
#include <string.h>
#include <spotify/api.h>

#include "debug.h"
#include "test.h"

struct testrun *test;


void test_init(sp_session *session) {
	test = malloc(sizeof(struct testrun));
	memset(test, 0, sizeof(struct testrun));
	test->session = session;
}


void test_add(char *name, test_callback *callback, void *arg) {
	test->name = realloc(test->name, sizeof(char *) * (1 + test->num_tests));
	test->name[test->num_tests] = strdup(name);

	test->callbacks = realloc(test->callbacks, sizeof(callback) * (1 + test->num_tests));
	test->callbacks[test->num_tests] = callback;

	test->arg = realloc(test->arg, sizeof(void *) * (1 + test->num_tests));
	test->arg[test->num_tests] = arg;

	test->num_tests++;
}


void test_start(void) {
	test->running = 1;
}


int test_run(void) {
	test_callback *callback;

	if(!test->running)
		return 0;

	if(test->current_test == test->num_tests)
		return -1;


	DSFYDEBUG("Running test '%s'\n", test->name[test->current_test]);
	callback = test->callbacks[test->current_test];
	callback(test->session, test->arg[test->current_test]);

	return 0;
}


void test_finish(void) {
	test->current_test++;
}
