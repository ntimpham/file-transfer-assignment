# File Transfer Assignment

## Building

```
$ make
```

For debugging:
```
$ make debug
```

## Running

### Client

Parameters:
- HOST: Hostname of the server to connect.
- PORT: Port of the server to connect.
- COMMAND:
  - G: Download a file through the server
  - P: Upload a file through the server
  - F: Signal the server to terminate
- KEY: 8-character ASCII key to identify download/upload
- FILENAME: Relative path to file to save/upload
- RECV_SIZE: Buffer size for receiving in bytes
- SEND_SIZE: Buffer size for sending in bytes
- WAIT_TIME: Time to wait inbetween subsequent requests

```
# Download
$ ./client {HOST} {PORT} G{KEY} {FILENAME} {RECV_SIZE}

# Upload
$ ./client {HOST} {PORT} P{KEY} {FILENAME} {SEND_SIZE} {WAIT_TIME}

# Terminate
$ ./client {HOST} {PORT} F
```

### Server

```
$ ./server
```

The port the server is running on can be found in the file `port`.

## Testing Environment

GNU Make 4.1

g++: gcc version 5.4.1 20160904
