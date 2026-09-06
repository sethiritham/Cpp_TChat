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

## Authentication

### Server Side

- Server verifies client credentials from the user_pass database

### Client Side

#### Joining

- Must enter their Username and The room password given to them by the server admin
- Room password verified by server side

#### Messaging

- Each Packet sent by the client contains a magic_protocol in its header which
is verified by the server and the packet is allowed

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

## Chat thread architecture

Non-blocking event-loop state machine, each client's event called asynced based
on readiness of the client-event

- In the main event handling function, there exist an array of events with its
client file-descriptor which are executed in the main loop

### Main loop

- Event handling
  - New Client connection
    - The client is authenticated and added to the client list

  - Client sending a packet
    - The packet is placed into the outgoing buffer
    of the current client's session and sent

  - Client reading a packet
    - The packet is read into the incoming buffer of the
    current client's session and displayed once the entire packet
    is received

#### Success

- There exist a non-zero number of events, the events are handled in a queue

#### Failures

- The number of events is ZERO, we break out of the main loop

```bash
Main-Loop
|
|______ determine num_events
                |
                |
        Loop through the events
                |
           _____|__________________________ ________________________
          |                                |                        |
      new client connection       incoming packet (read)   outgoing packet (write)
```

## Packet architecture

### Packet header structure

```bash
Frame
|__ u16 magic protocol
|__ u8 protocol version
|__ u8 Message Type ID
|__ u32 Payload Length
```

```bash
Packet = Header + Payload
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
  - Server Side & Other Client side

    ```
    ClientX has joined the chat
    ```

  - Client Side

  ```
    You have successfully joined the chat
  ```

### Failure packets

- Client kicked
  - Server Side

    ```
    You Kicked ClientX 
    ```

  - Other Client Side

    ```
    [SERVER] kicked clientX
    ```

  - Client Side

    ```
    You have been kicked from the chat
    ```

## Message

### Server Side & Client Side

- Server or Client sends a message, it is broadcasted to all other members
of the chat

- Incoming packets are stored into the read buffer of the client,
when the entire packet is received, the message is processed and displayed

- Outgoing packets are also stored into the write buffer of the client which
is both concurrently displayed in the input box and displayed all together in
the chat window

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
