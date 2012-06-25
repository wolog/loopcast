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

#ifndef _KLIBCAST_
#define _KLIBCAST_

#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if.h>

#define CHUNKSIZE 4096
#define MAXWAIT 5

#define IP_ADDR	"239.0.0.1"
#define IP_PORT 2121
#define RECEIVER 0
#define SENDER 1
#define STATUSCMD_LENGTH 256

#define ERROR(x) { debug_printf x; exit (1); }

#if defined( DEBUG )
#define DEBUGP(x) debug_printf x
#else
#define DEBUGP(x)
#endif

// runtime settings
typedef struct options_s {
	uint16_t maxchunks;
	int sender;
	int verbose;
	int maxwait;
	int clientsnumber;
	struct itimerval maxwait_itimer;
	int bwlimit;
	struct itimerval bwlimit_itimer;
	int keepalives;
	char statuscmd[STATUSCMD_LENGTH + 1];
	int statusstep;
	unsigned short int ip_port;
	unsigned long int ip_addr;
	char interface[IFNAMSIZ];
	char *output;
	uint8_t returnvalue;
	int exitonvalue;
} options_t;

// network data
typedef struct netsock_s {
	int sock, status;
	struct sockaddr_in saddr;
	struct ip_mreq imreq;
	struct in_addr iaddr;
	struct ifreq interface;
} netsock_t;

typedef struct network_s {
	int percent;
	uint32_t id;
	long received_packets;
	struct netsock_s data;
	struct netsock_s keepalive;
	struct keepalive_s *keepalives;
} network_t;

typedef struct keepalive_s {
	time_t time;
	uint8_t value;
} keepalive_t;

// elementary chunk of transfered data
typedef struct chunk_s {
	uint16_t n;
	uint8_t returnvalue;
	uint8_t data[CHUNKSIZE];
} chunk_t;

// the complete file is stored in memory
typedef struct buffer_s {
	uint32_t length;
	uint32_t maxchunks;
	uint32_t nchunks;
	uint32_t last_chunk_number;
	chunk_t *chunks;
} buffer_t;

// we transfer a chunk with its header
typedef struct message_s {
	uint32_t crc;
	uint32_t length;
	uint32_t nchunks;
	chunk_t chunk;
} message_t;

// basic 
int debug_printf(const char *fmt, ...);
uint32_t crc32(uint8_t * data, int len);

// print to stderr
int do_printf(const char *fmt, ...);

// parse the command line
int options_init(options_t * options, int sender, int argc, char **argv);

// manage communication 
int network_init(options_t * options, network_t * network);
int network_send(network_t * network, message_t * message);
int network_recv(options_t * options, network_t * network, message_t * message);
int network_send_keepalive(network_t * network);
int network_recv_keepalives(options_t * options, network_t * network,
			    time_t starttime);
int network_dump_keepalives(options_t * options, network_t * network);
int network_clean(network_t * network);

// manage buffer
int buffer_init(options_t * options, buffer_t * buffer, FILE * file);
int buffer_send(buffer_t * buffer, uint32_t chunk, message_t * message);
int buffer_recv(options_t * options, buffer_t * buffer, message_t * message);
int buffer_dump(buffer_t * buffer, FILE * file);
int buffer_clean(buffer_t * buffer);

#endif
