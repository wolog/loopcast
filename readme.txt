Why ?
 We need to boot a large amount of computer (>500) in the minimum possible 
 time. All computers are started in the same second and are supposed to download
 the operating system from a central server.

 This software must compile against klibc, as it is intended to be used
 very early in the boot process.

How it works
 The sender read data from stdin, and store it in memory. The data is
 splitted in small chunks that are repeatedly send (multicasted) to the 
 network, as fast as possible (but a bandwidth limiter is available).

 The receiver listen to the network, grabing chunks and storing them in
 memory. If the receiver misses some chunks, it will wait the next loop.

 Once all chunks are validated, the receiver dump the content to stdout and 
 exit.

 A specific exit code can be specified from the server side, and used by the caller 
 of the client.

Todo:
 * Use linux crypto API to compute crc32 (if available)
