/*
** Small test for signals handling
*/


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <syscalls.h>


void sig_handler(int);
void alarm_handler(int);
void install_handler(int, __signal_func_ptr);
int32 thread(void *);
void usage(void);


void
sig_handler(int sig)
{
	int i;
	
	printf("sig_test (sig_handler): Received signal #%d...\n", sig);
	printf("Counting for fun: ");
	for (i=0; i<10; i++)
		printf("%d ", i);
	printf("\n");
}


void
alarm_handler(int sig)
{
	printf("Alarm!\n");
}


void
install_handler(int sig, __signal_func_ptr handler)
{
	struct sigaction newa;
	struct sigaction olda;
	
	memset(&newa, 0, sizeof(newa));
	
	newa.sa_handler = handler;
	newa.sa_flags = SA_NOMASK | SA_RESTART;
	
	if (sys_sigaction(sig, &newa, &olda) < 0) {
		printf("Failed installing handler for sig #%d!\n", sig);
	}
}


int32
thread(void *data)
{
	printf("Started snoozing thread...\n");
	while (1) {
		if (snooze(1000000000L) == B_INTERRUPTED)
			printf("sig_test (thread): snooze was interrupted!\n");
	}
	return 0;
}


void
usage()
{
	printf("usage: sig_test <mode>\n"
		"where `mode' is one of:\n"
		"\t1\tInstalls one handler for all signals\n"
		"\t2\tSame as 1 but also spawns 5 threads with no active handlers\n"
		"\t3\tSets an alarm via set_alarm\n"
		"\n");
	exit(1);
}


int
main(int argc, char **argv)
{
	int i;
	
	if (argc != 2)
		usage();
	
	switch (argv[1][0]) {
		case '1':
			printf("Spawning some threads...\n");
			for (i=0; i<5; i++)
				resume_thread(spawn_thread(thread, "sig_test aux thread", B_NORMAL_PRIORITY, NULL));
		case '2':
			printf("Installing signal handlers...\n");
			for (i=1; i<=NSIG; i++)
				install_handler(i, sig_handler);
			break;
		case '3':
			install_handler(SIGALRM, alarm_handler);
			sys_set_alarm(5000000, B_PERIODIC_ALARM);
			break;
	}
	printf("Done. Entering sleep mode...\n");
	while (1) {
		if (sys_snooze(1000000000L) == B_INTERRUPTED)
			// this never gets called as the syscall is always restarted
			printf("sig_test (main): snooze was interrupted!\n");
	}
	
	return 0;
}

