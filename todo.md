# TODO: IRC Server Implementation (C++98)

## Project Setup
- [x] Create Makefile with rules: $(NAME), all, clean, fclean, re
- [x] Set up project structure with proper header and source files
- [x] Configure compilation with flags: -Wall -Wextra -Werror -std=c++98

## Core Server Infrastructure
- [x] Implement TCP/IP socket creation (IPv4/IPv6)
- [x] Set up non-blocking socket operations
- [x] Implement poll() (or equivalent) for handling multiple clients
- [x] Create server initialization with port and password parameters
- [x] Implement proper error handling for all system calls
- [x] Set up signal handling for graceful shutdown

## Client Management
- [x] Implement client connection acceptance
- [x] Create client data structure to track:
  - [x] Socket file descriptor
  - [ ] Authentication status
  - [ ] Nickname
  - [ ] Username
  - [ ] Real name
  - [ ] Joined channels
  - [ ] Operator status
- [x] Implement client disconnection handling
- [ ] Create message buffering system for partial commands

## Authentication System
- [x] Implement password authentication
- [x] Create NICK command handler
- [x] Create USER command handler
- [x] Implement registration sequence (PASS → NICK → USER)

## Message Handling
- [ ] Implement PRIVMSG command for private messages
- [ ] Implement channel message broadcasting
- [x] Create message parsing system
- [ ] Handle message formatting according to IRC standards
- [x] Implement error message generation

## Operator Commands
- [ ] Implement KICK command
- [ ] Implement INVITE command
- [ ] Implement TOPIC command
- [ ] Implement MODE command with support for:
  - [ ] +i/-i (Invite-only channel)
  - [ ] +t/-t (Topic restriction)
  - [ ] +k/-k (Channel key)
  - [ ] +o/-o (Operator privilege)
  - [ ] +l/-l (User limit)

## Utility Functions
- [ ] Create functions for parsing IRC messages
- [ ] Implement string manipulation utilities
- [ ] Create error handling utilities
- [ ] Implement logging system for debugging

## Testing Infrastructure
- [ ] Create test cases for all commands
- [ ] Implement partial message handling test
- [ ] Create stress test for multiple clients
- [ ] Test error conditions and edge cases
- [ ] Verify compatibility with reference IRC client

## Documentation
- [ ] Document code with comments
- [ ] Create README with build and run instructions
- [ ] Document protocol implementation details

## Bonus Features (Optional - Only if mandatory is perfect)
- [ ] File transfer implementation (DCC)
- [ ] Bot system implementation

## Important Constraints
- [ ] No forking allowed
- [ ] All I/O operations must be non-blocking
- [ ] Only one poll() (or equivalent) for all operations
- [ ] Must handle multiple clients simultaneously
- [ ] Must not crash under any circumstances
- [ ] Must use C++98 standard only
- [ ] No external libraries except allowed system calls

## Allowed System Calls
- socket, close, setsockopt, getsockname, getprotobyname
- gethostbyname, getaddrinfo, freeaddrinfo, bind, connect
- listen, accept, htons, htonl, ntohs, ntohl, inet_addr
- inet_ntoa, send, recv, signal, sigaction, lseek, fstat
- fcntl (only for F_SETFL, O_NONBLOCK), poll (or equivalent)

This TODO list covers all mandatory requirements from the subject. The server should be implemented in phases, starting with the core infrastructure, then adding authentication, channel system, commands, and finally testing.