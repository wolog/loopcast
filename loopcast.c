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
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/sockios.h>

#include "loopcast.h"

#ifdef __KLIBC__
double difftime(time_t time1, time_t time0)
{
	return time1 - time0;
}
#endif

int debug_printf(const char *fmt, ...)
{
	int ret;

	va_list args;
	va_start(args, fmt);
	ret = vfprintf(stderr, fmt, args);
	va_end(args);
	return ret;
}

/* Let's use stderr as default not to pollute stdout
 * since it's used to expose the downloaded content */
int do_printf(const char *fmt, ...)
{
	int ret;

	va_list args;
	va_start(args, fmt);
	ret = vfprintf(stderr, fmt, args);
	va_end(args);
	return ret;
}

void do_statuscmd(options_t * options, int percent)
{
	char temp[STATUSCMD_LENGTH + 10];

	if (strlen(options->statuscmd)) {
		snprintf(temp, sizeof(temp), "%s %d", options->statuscmd,
			 percent);
		system(temp);
	}
}

void usage(options_t * options, char *me)
{
	do_printf("Usage:\n\t%s [options]\n", me);
	do_printf("\t  -h : this help screen\n");
	do_printf("\t  -i <ethernet interface name>\n");
	do_printf("\t  -d <multicast ip address>\n");
	do_printf
	    ("\t  -p <port number> : port number used to transmit data. If keepalive message are enabled,\n"
	     "\t\t<port number+1> is also used.\n");
	if (options->sender) {
		do_printf
		    ("\t  -k : activate keepalive messages, stop sending data if <maxwait> timeout is reached\n"
		     "\t\twithout receiving at least one keepalive message (default is %ds).\n",
		     MAXWAIT);
		do_printf
		    ("\t  -m <maxwait> : if 'keepalive' messages are activated (using -k), this is the maximum\n"
		     "\t\ttime to send data without receiving a 'keepalive' message.\n"
		     "\t\tIf 'keepalive' is not activated, this is a minimum duration of data sending before\n"
		     "\t\tleaving, 0 means infinite loop (default)\n");
	} else {
		do_printf
		    ("\t  -k : activate keepalive messages, send this if we need more loops from server.\n");
		do_printf
		    ("\t  -m <maxwait> : if 'keepalive' messages are activated (using -k), this is the time\n"
		     "\t\tto wait before sending a new keepalive message to the server.\n");
	}
	if (options->sender) {
		do_printf
		    ("\t  -N : wait this number of clients before sending data (implies -k).\n");
	} else {
		do_printf
		    ("\t  -N : force the client to use the specified id (default is last 2 bytes from ip address).\n");
	}
	do_printf
	    ("\t  -n <chunk numbers> : how many %dKB chunks are we able to %s.\n",
	     CHUNKSIZE / 1024, options->sender ? "send" : "receive");
	if (options->sender) {
		do_printf("\t  -o <filename> : output to given file\n");
		do_printf
		    ("\t  -r <return value> : this value will be returned by the receiver as exit code\n");
	} else {
		do_printf
		    ("\t  -r <value> : exit as soon as the exit code is known, no data is send to the exit file descriptor\n"
		     "\t\tIf keepalives are activated, the <value> is also send to the server.\n");
	}
	do_printf("\t  -v : be verbose\n");
	if (options->sender) {
		do_printf
		    ("\t  -w <bwlimit> in KiB/s : if defined, the data will not be send faster than the\n\t\tdefined speed (default unlimited).\n");
	}
	do_printf
	    ("\t  -x </path/to/some/app> : if defined, this app will be called at each <step>%% value.\n"
	     "\t\tIf <step> is not defined, the app will only be called at 1st data reception.\n"
	     "\t  -s <step value> : the <app> will be called each %%<step> of transfer completion\n"
	     "\t\t0%% and 100%% will always be treated as special values, and always be called\n"
	     "\t\t(this <step> feature is still a work in progress, not implemented yet)\n");
	exit(0);
}

int keepalives_init(options_t * options)
{
	options->keepalives = 1;
	if (!options->maxwait) {
		if (options->sender) {
			options->maxwait = MAXWAIT + 1;
		} else {
			options->maxwait = MAXWAIT;
		}
	}
	return 1;
}

int options_init(options_t * options, int sender, int argc, char **argv)
{
	int optc, dummy;
	char *opt_send = "d:hi:km:n:N:o:p:r:vw:";
	char *opt_recv = "d:hi:km:n:N:p:s:r:Rvx:";
	char *opt_mode;

	DEBUGP(("options_init: Enter\n"));
	memset(options, 0, sizeof(options_t));
	options->maxchunks = 50000;
#if defined( DEBUG )
	options->verbose = 1;
#else
	options->verbose = 0;
#endif
	options->sender = sender;
	options->ip_port = IP_PORT;
	options->maxwait_itimer.it_value.tv_sec = MAXWAIT;
	strcpy(options->interface, "eth0");
	if (options->sender) {
		opt_mode = opt_send;
	} else {
		opt_mode = opt_recv;
	}
	options->ip_addr = inet_addr(IP_ADDR);

	while ((optc = getopt(argc, argv, opt_mode)) != EOF) {
		switch (optc) {
		case 'd':
			options->ip_addr = inet_addr(optarg);
			if (!options->ip_addr) {
				do_printf("'%s' is not a valid ip address\n",
					  optarg);
				options->ip_addr = inet_addr(IP_ADDR);
			}
			break;
		case 'h':
			usage(options, argv[0]);
			break;
		case 'i':
			strncpy(options->interface, optarg,
				sizeof(options->interface) - 1);
			if (options->verbose) {
				do_printf("interface set to '%s'\n", optarg);
			}
			break;
		case 'k':
			keepalives_init(options);
			break;
		case 'm':
			dummy = atoi(optarg);
			if (dummy > 0) {
				options->maxwait =
				    options->maxwait_itimer.it_value.tv_sec =
				    dummy;
				if (options->verbose) {
					do_printf
					    ("max wait duration set to '%s' seconds\n",
					     optarg);
				}
			} else {
				do_printf
				    ("'%s' is not a valid maximum loop number\n",
				     optarg);
			}
			break;
		case 'n':
			dummy = atoi(optarg);
			if (dummy > 0) {
				options->maxchunks = dummy;
				if (options->verbose) {
					do_printf("chunk number set to '%s'\n",
						  optarg);
				}
			} else {
				do_printf("'%s' is not a valid chunk number\n",
					  optarg);
			}
			break;
		case 'N':
			dummy = atoi(optarg);
			if ((dummy >= 0) && (dummy < 65535)) {
				options->clientsnumber = dummy;
				keepalives_init(options);
				if (options->verbose) {
					if (options->sender) {
						do_printf
						    ("Will wait '%s' clients to start\n",
						     optarg);
					} else {
						do_printf
						    ("client's id forced to '%s'\n",
						     optarg);
					}
				}
			} else {
				if (options->sender) {
					do_printf
					    ("'%s' is not a valid clients number\n",
					     optarg);
				} else {
					do_printf
					    ("'%s' is not a valid client id\n",
					     optarg);
				}
			}
			break;
		case 'o':
			options->output = strdup(optarg);
			if (!options->output) {
				do_printf
				    ("unable to allocate options->output\n");
				exit(255);
			}
			if (options->verbose) {
				do_printf("output set to '%s'\n", optarg);
			}
			break;
		case 'p':
			dummy = atoi(optarg);
			if ((dummy > 0) && (dummy < 65535)) {
				options->ip_port = dummy;
				if (options->verbose) {
					do_printf
					    ("ip port number set to '%s'\n",
					     optarg);
				}
			} else {
				do_printf
				    ("'%s' is not a valid ip port number\n",
				     optarg);
			}
			break;
		case 'r':
			options->returnvalue = atoi(optarg);
			keepalives_init(options);
			if (options->verbose) {
				do_printf("return value set to '%s'\n", optarg);
			}
			break;
		case 'R':
			options->exitonvalue = 1;
			if (options->verbose) {
				do_printf("exit on value set to true\n");
			}
			break;
		case 's':
			dummy = atoi(optarg);
			if ((dummy > 0) && (dummy <= 100)) {
				options->statusstep = dummy;
				if (options->verbose) {
					do_printf
					    ("status step value set to '%s'\n",
					     optarg);
				}
			} else {
				do_printf
				    ("'%s' is not a valid status step value (0<n<=100)\n",
				     optarg);
			}
			break;
		case 'v':
			options->verbose = 1;
			break;
		case 'w':
			dummy = atoi(optarg);
			if (dummy > 0) {
				options->bwlimit = dummy;
				if (options->verbose) {
					do_printf("bwlimit set to '%s' KiB/s\n",
						  optarg);
				}
			} else {
				do_printf
				    ("'%s' is not a valid bandwidth limit\n",
				     optarg);
			}
			break;
		case 'x':
			strncpy(options->statuscmd, optarg, STATUSCMD_LENGTH);
			break;
		}
	}

	if (options->bwlimit) {
		unsigned long sleep;
		sleep = (10240 * options->bwlimit) / sizeof(message_t);
		sleep = 10000000L / sleep;
		options->bwlimit_itimer.it_value.tv_sec = sleep / 1000000L;
		options->bwlimit_itimer.it_value.tv_usec =
		    sleep -
		    (options->bwlimit_itimer.it_value.tv_sec * 1000000L);;
		if (options->verbose) {
			do_printf
			    ("bwlimit sleep value set to %ds, %dµs\n",
			     options->bwlimit_itimer.it_value.tv_sec,
			     options->bwlimit_itimer.it_value.tv_usec);
		}
		if ((options->bwlimit_itimer.it_value.tv_sec +
		     options->bwlimit_itimer.it_value.tv_usec) == 0) {
			options->bwlimit = 0;
			if (options->verbose) {
				do_printf
				    ("Wait duration is 0µs, bwlimit deactivated\n");
			}
		}
	}
	DEBUGP(("options_init: Exit\n"));
	return 1;
}

// crc32 code adapted from http://www.cl.cam.ac.uk/research/srg/bluebook/21/crc/node6.html
#define QUOTIENT 0x04c11db7
uint32_t crctab[256];
int crctabinit = 1;
inline uint32_t crc32(uint8_t * data, int len)
{
	uint32_t result;
	int i;
	uint8_t dummy[4];

	if (crctabinit) {
		uint16_t i, j;
		uint32_t crc;

		DEBUGP(("crc32: init: "));
		for (i = 0; i < 256; i++) {
			DEBUGP(("."));
			crc = i << 24;
			for (j = 0; j < 8; j++) {
				if (crc & 0x80000000)
					crc = (crc << 1) ^ QUOTIENT;
				else
					crc = crc << 1;
			}
			crctab[i] = crc;
		}
		crctabinit = 0;
		DEBUGP(("\n"));
	}

	if (len < 4) {
		for (i = 0; i < len; i++) {
			dummy[i] = data[i];
		}
		for (; i < 4; i++) {
			dummy[i] = 0;
		}
		data = dummy;
		len = 4;
	}

	result = *data++ << 24;
	result |= *data++ << 16;
	result |= *data++ << 8;
	result |= *data++;
	result = ~result;
	len -= 4;

	for (i = 0; i < len; i++) {
		result = (result << 8 | *data++) ^ crctab[result >> 24];
	}

	DEBUGP(("crc32: (crc is 0x%08.8X)\n", ~result));
	return ~result;
}

int network_init(options_t * options, network_t * network)
{
	unsigned char ttl = 3;
	unsigned char one = 1;

	memset(network, 0, sizeof(network_t));

	network->data.sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (network->data.sock < 0) {
		ERROR(("network_init: Error creating data socket"));
	}
	network->data.saddr.sin_family = PF_INET;
	if (options->sender) {
		network->data.saddr.sin_port = htons(0);
	} else {
		network->data.saddr.sin_port = htons(options->ip_port);
	}
	network->data.saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	network->data.status =
	    bind(network->data.sock, (struct sockaddr *)&network->data.saddr,
		 sizeof(struct sockaddr_in));
	if (network->data.status < 0) {
		ERROR(("network_init: Error binding data socket to interface"));
	}

	if (options->keepalives) {
		network->keepalive.sock =
		    socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (network->keepalive.sock < 0) {
			ERROR(("network_init: Error creating keepalive socket"));
		}
		if (fcntl(network->keepalive.sock, F_SETFL, O_NONBLOCK) == -1) {
			ERROR(("network_init: Error setting keepalive socket O_NONBLOCK"));
		}
		network->keepalive.saddr.sin_family = PF_INET;

		if (options->sender) {
			network->keepalives =
			    malloc(sizeof(keepalive_t) * 256 * 256);
			if (!network->keepalives) {
				do_printf
				    ("Unable to allocate keepalives table, exiting...");
				exit(255);
			}
			memset(network->keepalives, 0,
			       sizeof(keepalive_t) * 256 * 256);
			network->keepalive.saddr.sin_port =
			    htons(options->ip_port + 1);
		} else {
			network->keepalive.saddr.sin_port = htons(0);
			if (!options->clientsnumber) {
				strncpy(network->keepalive.interface.ifr_name,
					options->interface, IFNAMSIZ - 1);
				ioctl(network->keepalive.sock, SIOCGIFADDR,
				      &network->keepalive.interface);
				DEBUGP(("interface %s has address %s (%d)\n",
					options->interface,
					inet_ntoa(((struct sockaddr_in *)
						   &network->
						   keepalive.interface.
						   ifr_addr)->sin_addr),
					((struct sockaddr_in *)
					 &network->keepalive.interface.
					 ifr_addr)->sin_addr.s_addr));
				network->id = htonl((((struct sockaddr_in *)
						      &network->keepalive.
						      interface.ifr_addr)->
						     sin_addr.s_addr >> 24) +
						    (((((struct sockaddr_in *)
							&network->
							keepalive.interface.
							ifr_addr)->
						       sin_addr.s_addr >> 16) &
						      0xFF) * 256) +
						    options->returnvalue *
						    65536);
			} else {
				network->id =
				    htonl(options->clientsnumber +
					  options->returnvalue * 65536);
			}
			DEBUGP(("client number is %d\n", ntohl(network->id)));
		}
		network->keepalive.saddr.sin_addr.s_addr = htonl(INADDR_ANY);
		network->keepalive.status =
		    bind(network->keepalive.sock,
			 (struct sockaddr *)&network->keepalive.saddr,
			 sizeof(struct sockaddr_in));
		if (network->keepalive.status < 0) {
			ERROR(("network_init: Error binding keepalive socket to interface"));
		}
	}

	if (options->sender) {
		/* data socket in send mode */
		setsockopt(network->data.sock, IPPROTO_IP, IP_MULTICAST_IF,
			   &network->data.iaddr, sizeof(struct in_addr));

		setsockopt(network->data.sock, IPPROTO_IP, IP_MULTICAST_TTL,
			   &ttl, sizeof(unsigned char));

		setsockopt(network->data.sock, IPPROTO_IP, IP_MULTICAST_LOOP,
			   &one, sizeof(unsigned char));

		network->data.saddr.sin_addr.s_addr = options->ip_addr;
		network->data.saddr.sin_port = htons(options->ip_port);

		/* keepalive socket in receive mode */
		if (options->keepalives) {
			network->keepalive.imreq.imr_multiaddr.s_addr =
			    options->ip_addr;
			network->keepalive.imreq.imr_interface.s_addr =
			    INADDR_ANY;

			setsockopt(network->keepalive.sock, IPPROTO_IP,
				   IP_ADD_MEMBERSHIP,
				   (const void *)&network->keepalive.imreq,
				   sizeof(struct ip_mreq));
		}

	} else {
		/* data socket in receive mode */
		network->data.imreq.imr_multiaddr.s_addr = options->ip_addr;
		network->data.imreq.imr_interface.s_addr = INADDR_ANY;

		setsockopt(network->data.sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			   (const void *)&network->data.imreq,
			   sizeof(struct ip_mreq));

		/* keepalive socket in send mode */
		if (options->keepalives) {
			setsockopt(network->keepalive.sock, IPPROTO_IP,
				   IP_MULTICAST_IF, &network->keepalive.iaddr,
				   sizeof(struct in_addr));

			setsockopt(network->keepalive.sock, IPPROTO_IP,
				   IP_MULTICAST_TTL, &ttl,
				   sizeof(unsigned char));

			setsockopt(network->keepalive.sock, IPPROTO_IP,
				   IP_MULTICAST_LOOP, &one,
				   sizeof(unsigned char));

			network->keepalive.saddr.sin_addr.s_addr =
			    options->ip_addr;
			network->keepalive.saddr.sin_port =
			    htons(options->ip_port + 1);
		}

	}
	DEBUGP(("network_init: Exit\n"));
	return 1;
}

int network_send_keepalive(network_t * network)
{
	network->keepalive.status =
	    sendto(network->keepalive.sock, (void *)&network->id,
		   sizeof(network->id), 0,
		   (struct sockaddr *)&network->keepalive.saddr,
		   sizeof(struct sockaddr_in));
	return 1;
}

int network_send(network_t * network, message_t * message)
{
	network->data.status =
	    sendto(network->data.sock, (void *)message, sizeof(message_t), 0,
		   (struct sockaddr *)&network->data.saddr,
		   sizeof(struct sockaddr_in));
	DEBUGP(("network_send: message %ld\n", message->chunk.n));
	return 1;
}

int network_dump_keepalives(options_t * options, network_t * network)
{
	int k, keepalives;
	keepalive_t *ktable;
	FILE *fd = NULL;

	if (!network->keepalives) {
		do_printf("No keepalives\n");
		return 0;
	}
	ktable = network->keepalives;
	if (options->output) {
		fd = fopen(options->output, "w");
	}
	if (!fd) {
		fd = stderr;
	}
	for (k = 0; k < 256 * 256; k++) {
		if (ktable[k].time) {
			fprintf(fd, "client: %d.%d value: %d\n", k / 256,
				k % 256, ktable[k].value);
			keepalives++;
		}
	}
	if (fd != stderr) {
		fclose(fd);
	}
	return keepalives;
}

int network_recv_keepalives(options_t * options, network_t * network,
			    time_t starttime)
{
	socklen_t socklen = sizeof(struct sockaddr_in);
	uint32_t id;
	int k, keepalives;
	time_t ref;
	keepalive_t *ktable;

	do {
		network->keepalive.status =
		    recvfrom(network->keepalive.sock, (void *)&id, sizeof(id),
			     0, (struct sockaddr *)&network->keepalive.saddr,
			     &socklen);
		if (network->keepalive.status > 0) {
			id = ntohl(id);
			if (options->verbose) {
				do_printf
				    ("Received keepalive (%d) from client %d.%d, with value %d\n",
				     id, (id % 65536) / 256, id % 256,
				     id / 65536);
			}
			network->keepalives[id % 65536].time = time(NULL);
			network->keepalives[id % 65536].value = id / 65536;
		}
	}
	while (network->keepalive.status > 0);

	keepalives = 0;
	ref = time(NULL) - (options->maxwait);
	if (difftime(starttime, ref) > 0) {
		keepalives++;
	}
	ktable = network->keepalives;
	for (k = 0; k < 256 * 256; k++) {
		if (difftime(ktable[k].time, ref) > 0) {
			keepalives++;
		} else {
			ktable[k].time = 0;
		}
	}

#ifdef DEBUG
	if (keepalives) {
		DEBUGP(("network_recv_keepalives: %d\n", keepalives));
	}
#endif
	return (keepalives);
}

int network_recv(options_t * options, network_t * network, message_t * message)
{
	socklen_t socklen = sizeof(struct sockaddr_in);

	network->data.status =
	    recvfrom(network->data.sock, (void *)message, sizeof(message_t), 0,
		     (struct sockaddr *)&network->data.saddr, &socklen);

	if (!network->received_packets) {
		/* This is the first packet we received, Call statuscmd */
		do_statuscmd(options, 0);
	}

	network->received_packets++;
	DEBUGP(("network_recv: message %ld\n", message->chunk.n));
	return 1;
}

int network_clean(network_t * network)
{
	shutdown(network->data.sock, 2);
	close(network->data.sock);
	return 1;
}

int buffer_init(options_t * options, buffer_t * buffer, FILE * file)
{
	uint32_t length;
	uint16_t i;
	int lr;

	length = 0;
	i = 0;
	buffer->last_chunk_number = 0;
	buffer->chunks = malloc(sizeof(chunk_t) * options->maxchunks);
	if (buffer->chunks) {
		memset(buffer->chunks, 0, sizeof(chunk_t) * options->maxchunks);
		buffer->maxchunks = options->maxchunks;
		if (file) {
			DEBUGP(("buffer_init: Start to read file\n"));
			do {
				lr = fread(buffer->chunks[i].data, 1, CHUNKSIZE,
					   file);
				if (lr) {
					if (i > (options->maxchunks - 1)) {
						ERROR(("buffer_init: Too much data, stop reading after %ld chunks\n", i));
					}
					buffer->chunks[i].n = i + 1;
					buffer->chunks[i].returnvalue =
					    options->returnvalue;
					i++;
					length += lr;
				}
			} while (lr == CHUNKSIZE);
		}
		buffer->nchunks = i;
		buffer->length = length;
		DEBUGP(("buffer_init: Exit\n"));
		return 1;
	} else {
		ERROR(("buffer_init: Not enough memory"));
	}
}

int buffer_recv(options_t * options, buffer_t * buffer, message_t * message)
{
	uint32_t crc, crcmsg;

	if ((message->chunk.n > 0) && (message->chunk.n <= buffer->maxchunks)) {
		if (buffer->chunks[message->chunk.n - 1].n) {
			DEBUGP(("buffer_recv: chunk %d already here\n",
				message->chunk.n));
			return 2;
		}
	}
	crcmsg = message->crc;
	message->crc = 0;
	crc = crc32((uint8_t *) message, sizeof(message_t));
	if (crc == crcmsg) {
		if (message->chunk.n < 1
		    || message->chunk.n > buffer->maxchunks) {
			ERROR(("buffer_recv: Unexpected chunk number"));
		}
		if ((message->chunk.n < buffer->last_chunk_number)) {
			if (options->verbose) {
				do_printf
				    ("Entering a new receive loop from sender\n");
			}
		}
		buffer->length = message->length;
		buffer->nchunks = message->nchunks;
		buffer->chunks[message->chunk.n - 1] = message->chunk;
		DEBUGP(("buffer_recv: Exit (chunk %ld ok)\n",
			message->chunk.n));
		return 1;
	}
	DEBUGP(("buffer_recv: Exit (message failed)\n"));
	return 0;
}

int buffer_send(buffer_t * buffer, uint32_t chunk, message_t * message)
{
	uint32_t crc;

	message->crc = 0;
	message->length = buffer->length;
	message->nchunks = buffer->nchunks;
	message->chunk = buffer->chunks[chunk];
	crc = crc32((uint8_t *) message, sizeof(message_t));
	message->crc = crc;
	DEBUGP(("buffer_send: Exit\n"));
	return 1;
}

int buffer_dump(buffer_t * buffer, FILE * file)
{
	uint32_t length;
	uint16_t i;

	length = 0;
	for (i = 0; i < buffer->nchunks; i++) {
		if (!buffer->chunks[i].n) {
			DEBUGP(("buffer_dump: Exit (buffer not ready)\n"));
			return 0;
		}
		length += CHUNKSIZE;
	}
	length = 0;
	for (i = 0; i < buffer->nchunks; i++) {
		length += CHUNKSIZE;
		if (length > buffer->length) {
			fwrite(buffer->chunks[i].data, 1,
			       CHUNKSIZE - length + buffer->length, file);
		} else {
			fwrite(buffer->chunks[i].data, 1, CHUNKSIZE, file);
		}
	}

	DEBUGP(("buffer_dump: Exit (buffer dumped, %ld)\n", buffer->length));
	return 1;
}

int buffer_clean(buffer_t * buffer)
{
	if (buffer->chunks) {
		free(buffer->chunks);
		buffer->chunks = NULL;
		return 1;
	}
	return 0;
}
