# ARCHITECTURE Specification

## Participants

### Server

- Hosts the port (8080 by default), Invites connections via LAN  
- Creates the room
- Accepts Client Connections
- All functions are thread locked

### Client

- Joins a given room hosted on a given IP (local host by default)
- Reads and writes messages via the Send() and Recieve() functions

## Packet architecture

```bash
Frame
|__ u8 Type
|__ u32 Payload Length
|__ Payload
```

### Types

- FILE_META - 0
- FILE_CHUNK - 1
- MSG - 2
- CMND - 3
- ACK - 4
- PRESENCE - 5
- POLL - 6
- PING - 7

### Success packets

- Client authenticated
  - Server Side

## Authentication

### Server Side

- Server verifies client credentials from the user_pass database

### Client Side

- Must enter their Username and The room password given to them by the server admin
- Room password verified by server side

#### Invariants

- Client must enter all of:
  - Username
  - Password
  - Server IP
To join a particular connection

#### Success

- Client connects:
  - Once the client connects, a new chat session will be opened
  - Server sends an acknowledgement packet to the client
  - Server sends a broadcast message to all current clients announcing:

```
clientX has joined the chat
```

#### Failure

- Client fails to connect:
  - Client enters the wrong server IP
    - Program ends after displaying an error message stating *Invalid IP*
  - Client does not exist in the database
    - Server sends an ack stating the absence of the client
  - Client enters the wrong password
    - Server sends an ack exclaiming the incorrect password

Client is not authenticated:
Server will send a failure packet to the client (invalid credentials)

## Connections

- Client connects to the room via LAN

### Success

- Client receives acknowledgement and joins the room

### Failures

- In case client disconnects from a running session, Server recieves a PING, Client has an option to reconnect

## Message

### Server Side & Client Side

- Server or Client sends a message, it is broadcasted to all other members of the chat

### Commands

- Each command gets broadcasted to all clients via the ADMIN(server host)
- Commands are:
  - /kick (Admin only) - ability to kick clients via their username
  - /quit (All participants) - quit the room, Admin quitting ends the room

## File Transfer

Current transferable file formats:

- PNG
- Text

Flags

- FILE_META, FILE_CHUNK
