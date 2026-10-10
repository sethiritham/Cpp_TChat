
# ARCHITECTURE Specification

## Participants

### Server

- Hosts the port (8080 by default), Invites connections via LAN  
- Creates the room
- Accepts Client Connections
- Functions are Non-blocking

### Client

- Joins a given room hosted on a given IP (local host by default)
- Reads and writes messages
- Functions are Non-blocking
- Unique Session for every client

#### Client Session

- The structure of the client session looks as follows

```
--> username
|
--> token
|
--> file_descriptor
|
--> SSL pointer
|
--> A tracker to check weather the client is connected
|
--> A tracker to track the amount of time passed since last activity by the client
|
--> A tracker to track the amount of time passed since the client disconnected

```

## Encryption

- Server and Client sockets are encrypted via TLS

## Authentication

### Client-Side

- Client creates a credential packet of the format ```username|password```
and writes it to the server.
- Packet is of type LGN
- Client waits for acknowledgement packet from the server

#### Sucessfull Login

- Client creates a new session and connection.

#### Failed to Login

- Client closes the socket and ends the program.
