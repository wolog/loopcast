/*
    loopcast is a small client/server utility to distribute data or simple
    orders to a high number of clients through a multicast network socket.

    Copyright (C) 2010  Olivier Guerrier <olivier@guerrier.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "loopcast.h"

volatile int lock = 0;
struct itimerval reftimer, timer;

void sigalarm_unlock(int i)
{
	lock = 0;
	signal(SIGALRM, sigalarm_unlock);
	timer = reftimer;
	setitimer(ITIMER_REAL, &timer, NULL);
}

volatile int waitclients = 1;
void sigusr1_dontwait(int i)
{
	waitclients = 0;
	signal(SIGUSR1, sigusr1_dontwait);
}

void sigusr1_dummy(int i)
{
	signal(SIGUSR1, sigusr1_dummy);
}

network_t *sigusr2_network;
options_t *sigusr2_options;
void sigusr2_dumpclients(int i)
{
	network_dump_keepalives(sigusr2_options, sigusr2_network);
	signal(SIGUSR2, sigusr2_dumpclients);
}

void sigusr2_dummy(int i)
{
	signal(SIGUSR2, sigusr2_dummy);
}

int main(int argc, char *argv[])
{
	buffer_t buffer;
	message_t message;
	network_t network;
	options_t options;
	uint32_t i;
	uint32_t loop = 0;
	time_t starttime;
	int clients;

	signal(SIGUSR1, sigusr1_dummy);
	signal(SIGUSR2, sigusr2_dummy);
	DEBUGP(("Calling options_init\n"));
	options_init(&options, SENDER, argc, argv);
	DEBUGP(("Calling network_init\n"));
	network_init(&options, &network);
	DEBUGP(("Calling buffer_init\n"));
	buffer_init(&options, &buffer, stdin);
	message.length = buffer.length;
	signal(SIGUSR1, sigusr1_dontwait);
	sigusr2_options = &options;
	sigusr2_network = &network;
	signal(SIGUSR2, sigusr2_dumpclients);
	if (options.clientsnumber) {
		starttime = time(NULL);
		do {
			sleep(1);
			clients =
			    network_recv_keepalives(&options, &network,
						    starttime);
			if (options.verbose) {
				do_printf("Expecting %d clients, found %d\n",
					  options.clientsnumber, clients);
			}
		} while (clients < options.clientsnumber && waitclients);
		if (options.verbose && !waitclients) {
			do_printf
			    ("SIGUSR1 received, starting with %d clients, where %d were expected\n",
			     clients, options.clientsnumber);
		}
	}
	if (options.verbose) {
		printf("Start main loop\n");
	}
	if (options.output) {
		network_dump_keepalives(&options, &network);
	}
	if (options.bwlimit) {
		lock = 1;
		signal(SIGALRM, sigalarm_unlock);
		reftimer = options.bwlimit_itimer;
		timer = reftimer;
		setitimer(ITIMER_REAL, &timer, NULL);
	}
	starttime = time(NULL);
	while (1) {
		loop++;
		if (options.verbose) {
			printf("Loop %u :", loop);
		}
		for (i = 0; i < buffer.nchunks; i++) {
			buffer_send(&buffer, i, &message);
			network_send(&network, &message);
			if (options.bwlimit) {
				lock = 1;
				while (lock) {
					pause();
				}	/* wait here until sigalarm release the lock */
			}
			if (options.keepalives) {
				if (!network_recv_keepalives
				    (&options, &network, starttime)) {
					break;
				}
			}
		}
		if (options.verbose) {
			printf("done\n");
		}
		if (options.keepalives) {
			if (!network_recv_keepalives
			    (&options, &network, starttime)) {
				if (options.verbose) {
					do_printf
					    ("no keepalive received, stop sending\n");
				}
				break;
			}
		} else {
			if (options.maxwait
			    && (difftime(time(NULL), starttime) >
				options.maxwait)) {
				if (options.verbose) {
					do_printf
					    ("Max time reached after %ld seconds, stop sending\n",
					     difftime(time(NULL), starttime));
				}
				break;
			}
		}
	}
	network_clean(&network);
	buffer_clean(&buffer);
	return 0;
}
