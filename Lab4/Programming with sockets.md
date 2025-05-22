### ==Client-Server Model==

#### **Basic Concept**
- Implements **asymmetric communication** between two processes:
	- **Server**: Waits passively at a _well-known address_ for client requests. Service and protocol are known to clients.
	- **Client**: Actively initiates communication, sends requests, and may receive responses.

#### **Roles & Architecture**

- A single process can act as both client and server in different relationships → supports **multi-tier architecture**.
- A single process can offer **multiple services**.

#### **Communication Types**

- **Connection-oriented (Stream)**: Requires a persistent connection (e.g., TCP).
- **Connectionless (Datagram)**: No ongoing connection needed (e.g., UDP).
    
#### **Server Request Handling**

- **Iterative**: Handles one request at a time (used in connectionless communication).
- **Concurrent**: Handles multiple requests simultaneously (used in connection-oriented communication).
    
#### **Communication State Management**

- Some services require stateful sessions:

1. **Server maintains state** → e.g., **FTP**
	The server keeps track of the session state (like login info, ongoing transfers, etc.).

	**✔ Pros:**
	- Simplifies the client: client doesn't need to track session info.
	- Suitable for complex interactions that require context (e.g., authentication, file transfers).

	**✘ Cons:**
	- Higher memory and resource usage on the server (state per client).
	- Harder to scale (each connection takes resources).
	- If the server crashes, **all session states are lost** → clients must reconnect and restart the session.

	**Crash scenario:**
	- **Server crash**: session data is lost; clients need to reconnect and reauthenticate.
	- **Client crash**: server may keep the session alive, wasting resources until timeout.
	  

2. **Client maintains state (server is stateless)** → e.g., **NFS**
	The client maintains session state; the server is stateless and responds to each request independently.

	**✔ Pros:**
	- Server is lightweight and scalable.
	- Easier to recover from server crashes (no session info lost).
	- More robust in distributed systems.

	**✘ Cons:**
	- Puts more responsibility on the client (must manage context).
	- Less efficient for complex, interactive sessions.

	**Crash scenario:**
	- **Server crash**: No state is lost; client can retry requests once the server is back.
	- **Client crash**: It loses its own state, so it must recover from local storage or restart.
### ==Peer-to-Peer (P2P) Model==

#### **Core Concepts**

- **No distinction** between client and server:
    - All nodes are **peers**
    - Each node can act as a **client**, **server**, or **both**.

- Nodes must **join the P2P network**:
    - **Centralized registration** (e.g., Napster): peer registers its services.
    - **Decentralized discovery**: peers use a **discovery protocol** to find others (e.g., Gnutella, Freenet).
#### **Advantages over Client-Server**

- **Less vulnerable** to single points of failure (e.g., server crashes or overload).
- **Potential for better scalability** as more peers join (but this depends on implementation).

#### **Disadvantages**

- **Security issues**: harder to enforce security policies across decentralized nodes.
- **Unpredictable availability**: peers may go offline without notice, leading to inconsistent access.
- **Data coherence/versioning problems**: multiple copies of data may become outdated or inconsistent.

### ==Service Quality Criteria==

#### 1. **Exactness of Fulfilling Service Specifications**

- Measures how accurately a service performs the tasks it promises.
- Ensures that the service behaves **as specified in its documentation or contract**.
- Example: A file transfer service must transfer data without corruption.

#### 2. **Response Time**

- **Average**: Mean time the system takes to respond under normal conditions.
- **Maximum**: Longest acceptable wait time before response.
- **Variability**: How consistent or unpredictable the response time is (also known as jitter).
- Important in real-time systems (e.g., streaming or online games).

#### 3. **Security**

- The ability to **protect data and operations** from unauthorized access and attacks.
- Includes:
    - Authentication
    - Authorization
    - Data encryption
    - Secure communication protocols
- Crucial in banking, healthcare, or any system with sensitive data.

#### 4. **Scalability**

- How well the service handles growth:
    - Can it maintain performance as user load or data volume increases?
    - Horizontal (more nodes) or vertical (more powerful machines) scaling.
- Important for cloud-based services and distributed systems.

#### 5. **Reliability and Availability (HA – High Availability)**

- **Reliability**: How consistently the service performs without failures.
- **Availability**: Percentage of time the service is accessible and operational (e.g., 99.9% uptime).
- High availability often includes fault tolerance and failover mechanisms.

#### 6. **Other Considerations**

- **Open vs. Proprietary**
    - Open: Follows public standards, may promote interoperability and reduce vendor lock-in.
    - Proprietary: Vendor-specific, may offer unique features but reduce compatibility.
    
- **Heterogeneous Environment Support**:
    - Can the service operate across different platforms, hardware, operating systems, and network conditions?
    - Vital in enterprise environments with mixed systems.

### ==Handling Multiple Communication Channels==

When a program deals with multiple I/O sources (like network connections, files, or devices), it needs a strategy to manage them efficiently. Main strategies are:

### 1. **Blocking I/O (Default Behavior)** 

- The program **waits** (or "blocks") until I/O is ready.
- **Two main ways to handle multiple channels in this model:**

#### a) **Working Threads**

- Each I/O task runs in a **separate thread**.
- **Pros:** Lightweight, easy to share memory.
- **Cons:**
    - Needs **synchronization** (e.g., mutexes).
    - **One thread crash can crash the whole process** (since threads share memory).
#### b) **Working Sub-Processes**

- Use **separate processes** for each I/O task.
- **Pros:** More **robust** – crash of one sub-process won’t affect others.
- **Cons:** Higher **overhead** due to:
    - Process creation
    - Inter-process communication (IPC)
### 2. **Synchronous I/O Multiplexing**

- Uses mechanisms like `select()`, `poll()`, or `epoll()` in Unix/Linux.
- A **single thread** watches multiple I/O channels and acts on the ones ready.
- **Efficient and common** in server applications.
- No need to block on every I/O call.

### 3. **Non-blocking I/O**

- I/O functions return **immediately**, even if the operation is not complete.
- The program must **check back** or be notified later.
- **More complex** and requires **advanced programming skills**.
- Often used in **event-driven** systems (like GUI apps or high-performance servers).

### 4. **Signal-Controlled I/O**

- OS sends a **signal** to notify when I/O is ready.
- **Rarely used**:
    - Hard to **program correctly**
    - Considered **unreliable**

### ==Blocking I/O – Notes & Explanation==

- **Blocking I/O** means that functions like `recvfrom`, `read`, and `recv` will **pause the program** until certain conditions are met.

### **When Does a Blocking Read Return?**

1. **Error or interruption:**
    - If there's an error (e.g., socket closed), the function returns `-1` and `errno` contains the error code.
    - If interrupted by a signal, `errno == EINTR`.

2. **Data arrives** (e.g., a datagram on a UDP socket).
    
3. **Receive buffer fills to a certain level:**
    - When **at least `SO_RCVLOWAT` bytes** are available.
    - Default: `1` byte.
    - You can **change this** threshold using the `setsockopt()` system call.

### **Writing Functions (`sendto`, `write`, `send`) Block Until:**

1. **An error occurs**, or function is interrupted by a signal :
	Returns `-1`, with `errno == EINTR`.
2. **For datagram sockets** (like UDP):
    - Data is copied to the **output queue** of the protocol stack.
3. **For connection-oriented sockets** (like TCP):
    - All user data is copied to the **socket's output buffer**.

### **`accept()` Function Blocks Until:**

1. A **connection request** is extracted from the queue.
    - A new socket is created and a file descriptor reffering to 
     that socket is returned.
2. An **error occurs**, or it is interrupted by a signal.

### **`connect()` Function Blocks Until:**

1. The connection to the **server is established**.
2. An **error occurs**
	- Use `getsockopt()` with `SO_ERROR` to get the actual error code or success.
3. It is **interrupted by a signal**.
	- In this case, the connection may **continue in the background**.
    - Use `select()` to check when the socket becomes writable.

### **Notes:**

1. **Default behavior** of sockets: reading functions are **blocking**.
2. You can make interrupted functions **automatically restart** by setting 
   the **`SA_RESTART`** flag when defining signal handlers.

### ==Synchronous I/O Multiplexing with select() and poll()==

#### **What Is I/O Multiplexing?**

- A technique to **monitor multiple file descriptors (e.g., sockets)** at once to see if **any are ready** for I/O.
- Avoids blocking on just one descriptor; allows a single thread to manage many I/O sources.

### **select()` Function Overview**

The `select()` system call checks multiple file descriptors to see if any are:

- Ready to **read**
- Ready to **write**
- Have **exceptions** (e.g., out-of-band data)

```C
int select(int maxfdp1, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
```

- `maxfdp1`: Highest descriptor number + 1 (used for efficiency)
- `readfds`: Set of descriptors to monitor for readability
- `writefds`: Set of descriptors to monitor for writability
- `exceptfds`: set of descriptors to be ready for reading OOB (e.g., OOB data)
- `timeout`: How long to wait:
    - `NULL` = wait forever
    - `0` = don’t wait at all (non-blocking)
    - Struct = wait for specified time

- `FD_ZERO(fd_set *mask)`: Clear all bits in the mask
- `FD_SET(fd, fd_set *mask)`: Add a descriptor to the set
- `FD_CLR(fd, fd_set *mask)`: Remove a descriptor
- `FD_ISSET(fd, fd_set *mask)`: Check if descriptor is set
### **poll()` Function Overview**

`poll()` is a **system call** that lets a program **monitor multiple file descriptors** to see if they are ready for I/O (like reading or writing). It's an **alternative to `select()`** and solves some of its limitations.

```C
int poll(struct pollfd fds[], nfds_t nfds, int timeout);
```

- `fds[]`: An array of `pollfd` structures (each describing one file descriptor and what you're watching for).
- `nfds`: Number of elements in the array.
- `timeout`: How long to wait (in milliseconds):
    - `0` = return immediately (non-blocking)
    - `-1` = wait indefinitely
    - `> 0` = wait up to that many milliseconds
### **Conditions for a descriptor to become _ready for reading_**:

1. **Receive buffer has enough data**:
    - At least `SO_RCVLOWAT` bytes are available in the buffer (default: 1 byte).
    - Applies to **TCP** and **UDP** sockets.
    
2. **Remote peer has closed the connection**:
    - This causes **EOF (end-of-file)** to be available, which can be read.

3. **Listening socket** has at least **one pending connection**:
    - The `accept()` call can return immediately.

4. **Socket error occurred**:
	The socket has encountered an error (e.g., broken pipe, connection reset, etc.).
    - Detected by `getsockopt()` with `SO_ERROR`.
    - If you call `read()` on this socket **and there's no data to read**, it:
		- Fails immediately.
		- Sets `errno = so_error` (e.g., `ECONNRESET`).
		- Resets the socket’s internal `so_error = 0` (because you “acknowledged” it by reading).
    - - If there **is data available**:
	    - The `read()` **succeeds** and returns the data.
		- The **error is not cleared** — it stays in `so_error` until the next `read()` when the buffer is empty.

### **Conditions for a descriptor to become _ready for writing_**:

1. **Output buffer has enough space**:
    - At least `SO_SNDLOWAT` bytes of space (default: 2048 bytes).
    - Allows `write()` to proceed.

2. **Socket error occurred**:
    - Detected via `getsockopt()` with `SO_ERROR`.
    - `write()` fails, `errno = so_error`.
    - If `so_error == 0`, writing is possible.

### **Conditions to become _ready for reading OOB (Out-of-Band) data_**:

1. **There is OOB data waiting**:
    - Used rarely in modern systems (e.g., TCP urgent data).
    
2. **Stream pointer is at the OOB mark**:
    - Meaning it's the right time to read the urgent data.
    - The **stream pointer** refers to the internal read position in the TCP stream.
	- The **OOB mark** is the point where the urgent data lives.
	- This condition means:
		You're at the correct position in the stream to **read the urgent byte** using `recv()` with the `MSG_OOB` flag.

### **`pselect()` – POSIX Synchronous I/O Multiplexing**

```C
int pselect(int maxfd1, fd_set *rdset, fd_set *wrset, fd_set *exset, 
const struct timespec *timeout, const sigset_t *sigmask);      
```

- **`maxfd1`**: Highest-numbered file descriptor in the sets, plus one.
- **`rdset`, `wrset`, `exset`**: Sets of descriptors to monitor.
- **`timeout`**: How long to wait (precision: seconds + nanoseconds).
    - `NULL` = wait indefinitely.
- **`sigmask`**: Optional — defines signals to block **only while waiting**.

**Why is `pselect()` Useful?**

It lets you **change the signal mask** **atomically** (in one step) with the call to `pselect()`.
This prevents **race conditions** where a signal could be delivered **just before** you call `select()`, causing you to miss it or be interrupted too early.

 **Equivalent to This (but safer):**
 
```C
sigset_t orgmask;
sigprocmask(SIG_SETMASK, &sigmask, &orgmask);  // Block signals
ready = select(...);                           // Monitor descriptors
sigprocmask(SIG_SETMASK, &orgmask, NULL);      // Restore old mask
```

### ==Non-blocking I/O==

### **How to enable Non-Blocking mode:**

 **1. `ioctl()` – Old-style**

```C
int flag = 1;
ioctl(fd, FIONBIO, &flag);
```

**2.`fcntl()` – More Flexible (POSIX preferred)**

```C
int oldflag = fcntl(fd, F_GETFL);
int newflag = oldflag | O_NONBLOCK;  
fcntl(fd, F_SETFL, newflag)
```

**If a function like `read()`, `recv()`, `write()`, etc. can’t complete immediately, it:**
- Returns `-1`
- Sets `errno` to:
    - `EWOULDBLOCK` or
    - `EAGAIN` (same meaning)

**Consequences of non-blocking I/O mode:**
- Very good utilization of channel bandwidth 
- Complication of I/O buffer management, because of partial reads/writes and a need for descriptor polling (e.g. with select()/pselect() function call)

### **`connect()` in Non-blocking Mode**

- **When using a non-blocking socket, calling `connect()` may return `-1` immediately.**
- **`errno` will be set to `EINPROGRESS`, which means:**
	- "The connection attempt has begun, but not completed yet.”
- **The actual connection is now handled asynchronously by the OS.**
- **You do not retry `connect()` yourself — instead, you:**
    1. Use `select()` or `poll()` to **wait until the socket is writable**.
    2. Then use `getsockopt(..., SO_ERROR)` to check:
        - If result is `0`: connection succeeded.
        - If non-zero: connection failed (e.g., `ECONNREFUSED`).

### **`accept()` in Non-blocking Mode**

Even if `select()` says a listening socket is ready for reading, `accept()` can still fail if:

- **A client starts a connection but closes it immediately before the server actually calls `accept()`.**
- **If the socket is in blocking mode, this can cause your server to block unexpectedly.**

So the solution is:

- **Put the listening socket into non-blocking mode, and**
- **Always check `errno` after `accept()`.**

```C
int add_new_client(int sfd) { /* sfd: non-blocking listening socket */
    int nfd;
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -1; // Try again later
        ERR("accept");
    }
    return nfd;
}
```

### ==Signal-Controlled I/O==

### **SIGIO and Asynchronous I/O Notifications**

- **`SIGIO` is a signal the kernel sends to a process when a file descriptor (like a socket) becomes ready for I/O (read or write).**

- **This allows asynchronous notification: your program doesn't need to poll or block — it gets interrupted when something happens.**
### **What is needed to use `SIGIO`?**

1. **Set the owner of the file descriptor (so the kernel knows where to send `SIGIO`)**
2. **Set the descriptor to asynchronous mode**

**First Technique (using `ioctl()`):**

```C
int flag = getpid();  
ioctl(fd, SIOCSPGRP, &flag);  // Set process group owner

flag = 1;
ioctl(fd, FIOASYNC, &flag);  // Enable async notification (SIGIO)
```

**Second Technique (using `fcntl()`):**

```C
fcntl(fd, F_SETOWN, getpid());   // Set owner (current process)
fcntl(fd, F_SETFL, O_ASYNC);     // Set file descriptor to async mode
```

### **SIGIO Signal-Based I/O – Key Points**

- **To use `SIGIO`, you must first set up a signal handler for `SIGIO` using `sigaction()`.**
- **TCP sockets are a poor fit for `SIGIO` because signals may be too frequent, causing overhead.**
- **Signals are not queued— if your handler is slow, events may be lost**.
- **Overall, `SIGIO` is unreliable for high-performance I/O — use `select()`, `poll()`, or `epoll()` instead.**

### ==Running Servers==

### **Daemon – a background process which cannot be controlled from any terminal."

- **TCP/IP server processes typically run as daemons.**
- **Most server applications (e.g., web servers, database servers) are background processes**.
- **They do not interact with the terminal and are always running, ready to accept client requests.**
- **In Linux `daemon()` function can be used to make a process a daemon**
	- The **`daemon()`** function (in `<unistd.h>`) is a helper that:
	    - **Detaches** the process from the terminal.
	    - **Forks** and creates a new session.
	    - Changes working directory and file descriptors.
	- This turns a regular process into a **well-behaved daemon**.
### **Traditional methods to start servers**

These are ways that daemons or server processes are started automatically:
#### 1. **Shell scripts during boot**

- Located in files like `/etc/rc`, `/etc/rc.local`
- These scripts launch daemons at system startup.
#### 2. **Super-server (e.g., `inetd` or `xinetd`)**

- Starts on boot and **launches server processes on demand**.
- Saves resources by not keeping every service running.
#### 3. **System daemon (e.g., `cron`)**

- Runs processes **on a schedule**.
- Controlled via `crontab`.
#### 4. **User terminal**

- For testing/debugging, a server can also be launched manually from the command line.

### **Systemd software**

- **`systemd`** is the modern init system (used in most Linux distributions today).
- It replaces older startup methods (`init`, `rc scripts`).
- You can define services in **unit files (`*.service`)**
- Benefits:
    - Easier to start, stop, restart daemons
    - Built-in logging and monitoring
    - More powerful service management