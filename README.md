# GetFile-cached-proxy-server
# Aim and design
To implement cache interface with the proxy server using shared memory IPCs.

the client interfaces with the proxy server using the gf protocol. The cache daemon provides the caching services using shared memory IPC. The system V shared memory IPC was chosen with system V semaphores for synchronization.
The shared memory is used to buffer the data found in the cache to the proxy server which in turn transfers it to the client.
The semaphores were used to signify when the shared memory was full for the server to start reading and for the cache to know when to start putting more data into it.
The cache daemon initializes a number of threads and creates a number of shared memory segments as detailed by the server.
The server and the cache communicate using the system V message passing interface. There are three types of channels used. Two common channels for the server and the client, and multiple unique channels which are referenced to a unique shared memory segment.
The server maintains a queue of free shared memory segments. It provides this information with its request to the cache in a general server channel. The cache replies back in a unique cache channel which it chooses based on the free segment id.
flow of control description: the proxy server initializes a number of handler threads and the shared memory segments based on the input arguments. It sends this information to the cache daemon using the message queue. The cache then initializes its shared memory segments and creates a number of cache handler threads.
Upon receiving request from the client the server handler identifies a free shared memory segment and sends this information to the cache. The cache then responds with the status of the file. If the file is found, the transfer is initiated. Semaphores synchronize the transfer operation. The server transfers the data to the client using the gf protocol.
# Implementation and testing
Implementation consisted of working with the different IPC mechanisms of message passing, shared memory, multithreading, and the associated synchronization mechanisms using the mutex and semaphores.
The shared memory segments were stored as an array of pointers with their index identifying them uniquely. Upon a request form the client, the server will send this information about the free segment to the client. A message channel is created based on the same unique id of the segment which is used to communicate back to the server.
The testing consisted of multithreaded loads with many servers using the same set of caches serving simultaneous request from the clients
# References
The system V IPC were used. They were mainly referred from their documentation on docs.oracle.com and some example code from tutorials.
