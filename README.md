# http-server
Comparing performance characteristics of different server architecture strategies 

Authors
--------
Kesi Neblett, Padraic Quinn, and Jonathan Voss

Part 0
------
Got familiar with the http-server.c code, netcat, and apache benchmark. It took
1.033 seconds to handle 100 requests sent 5 at a time using apache benchmark.

Part 1
------
The code works as specified in the assignment. The parent process does not need
the client socket and closes it immediately. The child closes the client socket
once it has successfully handled the request. The child process does not need
the server socket and should close it. However, the parent process never closes
the server socket as it continues on in an infinite loop so the only effect not
closing the server socket would have is causing more work for the OS once the 
program has stopped operating. The OS would need to close two defunct socket
descriptors rather than just one. The child processes are handled as quickly as
possible given the specifications for the assignment. Our first implementation 
made the server socket nonblocking so that we could essentially infinitely loop
between calling accept and waitpid. However, having a nonblocking server socket was not the right
implementation. Therefore, all zombie children are cleaned up after another
request is accepted, as the program blocks on accept. This is done by calling 
waitpid in a nonblocking fashion on all of its children (by passing a pid of -1
and placing it within a while loop so that all defunct children can be
handled each time execution reaches this stage. It took 0.937 seconds to handle
100 requests sent 5 at a time using apache benchmark. It was interesting to note
that concurrent requests on small files were actually handled slower in part 1
than part 0. This is likely because the overhead of creating a new process
outweighs being able to concurrently handle many HTTP requests, all of which can
be handled quickly.

Part 2
------
The code works as specified in the assignment. A named semaphore was the better
choice in this situation because it needed to be accessible from multiple
processes. The semaphore struct is only in the child process since the child
process handles requests. A binary semaphore is used because only one process
can have access to the statistics at one time to avoid race conditions. The only
slow system call that is used is sem_wait(). However, a child process should
never get stuck here as sem_post() is called in a while loop until it succeeds.
It is worth noting that the permissions for our semaphore is 0666. It did not
specify in the spec what they should be and we were unsure if we should change
the permissions to make them more secure. This is also noted in the comments of
the code.

Part 3
------
The code works as specified in the assignment. Statistics are still handled.

Part 5
------
Non Thread-Safe Functions
strtok() and inet_ntoa() are not thread safe as they store information in static
memory. strtok() has a reentrant function strtok_r() that stores information in
the variable specified in the arguments. inet_ntoa() also has a reenterant 
function inet_ntoa_r() but it is not available in Linux. However, inet_ntoa also
returns the modified string. We used strcpy to save the return value of
inet_ntoa in a variable declared on each threads local stack. 

Design Decisions
We used a struct, tdata, to store the client socket, thread id, a sockaddr_in
struct, and, and the root of the path name to the html file. We pass this struct
to every new thread created. We chose to malloc the struct tdata and malloc the
pthread_t *thread id because if we stored the thread id on the stack of the 
parent thread, then it would go out of scope before the child thread would 
detach. We use pthread_detach to terminate the thread since we do not need any
informaton from the child thread and we also do not want to block the process to
wait for a thread to finish becuase that is pointless. We free the thread id and
struct after the child thread detaches.

Benchmark Testing
Tested using apache bench with n = 100 and c = 5
The data below are results of the runs with the lowest request per second out
of 10 runs with apache bench on each server (part0, part1, part5).

Part 0 Results

Concurrency Level:      5
Time taken for tests:   1.162 seconds
Complete requests:      100
Failed requests:        0
Requests per second:    86.09 [#/sec] (mean)
Time per request:       58.077 [ms] (mean)
Time per request:       11.615 [ms] (mean, across all concurrent requests)
Range Total Time        103-131

Part 1 Results

Concurrency Level:      5
Time taken for tests:   1.199 seconds
Complete requests:      100
Failed requests:        0
Requests per second:    83.37 [#/sec] (mean)
Time per request:       59.972 [ms] (mean)
Time per request:       11.994 [ms] (mean, across all concurrent requests)
Transfer rate:          297224.47 [Kbytes/sec] received
Time Total Range        122-180

Part 5 Results

Concurrency Level:      5
Time taken for tests:   1.017 seconds
Complete requests:      100
Failed requests:        0
Requests per second:    98.30 [#/sec] (mean)
Time per request:       50.865 [ms] (mean)
Time per request:       10.173 [ms] (mean, across all concurrent requests)
Range Total Time        144-222


Conclusion

The server in part5 processes request at 98.30 request per second. This is
faster than the servers implemented in part0 (86.09 r/s) and part1 (83.37 r/s). 

Part 6
-------

Total Time Range        107-223
(10 runs)

Concurrency Level:      5
Time taken for tests:   0.852 seconds
Complete requests:      100
Failed requests:        0
Total transferred:      365059900 bytes
HTML transferred:       365058000 bytes
Requests per second:    117.38 [#/sec] (mean)
Time per request:       42.596 [ms] (mean)
Time per request:       8.519 [ms] (mean, across all concurrent requests)

Interrupt signal is handled and successfully deallocates memory between
client connections. 

Part 7
-------

Total Time Range        239 -342

Concurrency Level:      5
Time taken for tests:   1.049 seconds
Complete requests:      100
Failed requests:        0
Total transferred:      365059900 bytes
HTML transferred:       365058000 bytes
Requests per second:    95.35 [#/sec] (mean)
Time per request:       52.440 [ms] (mean)
Time per request:       10.488 [ms] (mean, across all concurrent requests)

We used signal_broadcast() because the fasted thread will reach the lock
first and the other thread must continue to wait for the next broadcast

We handled SIGINT in part6 to detach threads but did not handle the interrupt
signal in this section in the same manner

Part 8
-------
Tested with 5 ab calls, each call was on a different port. Each ab call was set
to n = 10 and c = 2. The muliserver script shows test values.

Data from one of the 5 calls

Concurrency Level:      2
Time taken for tests:   0.550 seconds
Complete requests:      10
Failed requests:        0
Total transferred:      36505990 bytes
HTML transferred:       36505800 bytes
Requests per second:    18.19 [#/sec] (mean)
Time per request:       109.923 [ms] (mean)
Time per request:       54.961 [ms] (mean, across all concurrent requests)

Interrupt on SIGINT only destroys the queue and exits. It does not detach
threads


Part 9
------

Interrupt on SIGINT only destroys the queue and exits. It does not detach
threads

The two signals returned by accept that we handled are EAGAIN and
EWOULDBLOCK


Part 10
-------
The code works as specified in the assignment. For Task 3, when the signal
is sent to the child process the server prints "400 Bad Request" and the
child process terminates. This is because when the signal is sent to the
child process it interrupts the process and causes an error, then the
process closes the connection, which the server responds to with a 400 Bad
Request message.

Part 12
-------
The code works as specified in the assignment.

Part 13
-------
The code works as specified in the assignment. The time to do 100 requests 5 at
a time was much worse than in part 7.

