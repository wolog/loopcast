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

struct itimerval reftimer, timer;
network_t network;

void alarm_maxwait(int i)
{
	signal(SIGALRM, alarm_maxwait);
	timer = reftimer;
	setitimer(ITIMER_REAL, &timer, NULL);
	network_send_keepalive(&network);
}

int main(int argc, char *argv[])
{
	buffer_t buffer;
	message_t message;
	options_t options;
	uint8_t loop;
	int returnvalue;

	DEBUGP(("Calling options_init\n"));
	options_init(&options, RECEIVER, argc, argv);
	DEBUGP(("Calling network_init\n"));
	network_init(&options, &network);
	DEBUGP(("Calling buffer_init\n"));
	buffer_init(&options, &buffer, NULL);
	if (options.keepalives) {
		DEBUGP(("Start to send keepalives\n"));
		network_send_keepalive(&network);
		signal(SIGALRM, alarm_maxwait);
		reftimer = options.maxwait_itimer;
		timer = reftimer;
		setitimer(ITIMER_REAL, &timer, NULL);
	}
	loop = 0;
	while (1) {
		DEBUGP(("Start main receive loop\n"));
		if (network_recv(&options, &network, &message)) {
			if (buffer_recv(&options, &buffer, &message)) {
				if (options.exitonvalue) {
					signal(SIGALRM, SIG_IGN);
					network_clean(&network);
					returnvalue = message.chunk.returnvalue;
					buffer_clean(&buffer);
					if (options.verbose) {
						do_printf
						    ("Return code is now known (=%d), exiting\n",
						     returnvalue);
					}
					return returnvalue;
				}
				loop++;
				if (!(loop % ((2 * 1024 * 1024) / CHUNKSIZE))) {
					if (buffer_dump(&buffer, stdout)) {
						network_clean(&network);
						returnvalue =
						    buffer.
						    chunks[0].returnvalue;
						buffer_clean(&buffer);
						if (options.verbose) {
							do_printf
							    ("Successfully received\n");
						}
						break;
					}
				}
			}
		}
	}
	return returnvalue;
}
