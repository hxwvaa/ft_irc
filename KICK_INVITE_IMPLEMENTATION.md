# KICK and INVITE Commands Implementation

## Overview

I've implemented the KICK and INVITE commands for your IRC server with full support for invite-only channels.

## Changes Made

### 1. Server.hpp - Updated ChannelInfo Structure

Added an `invited` set to track users who have been invited to invite-only channels:

```cpp
struct ChannelInfo {
    std::set<int> members;          // Client file descriptors
    std::set<int> operators;        // Operator file descriptors
    std::set<int> invited;          // Invited client file descriptors (for +i mode)
    std::string key;                // Channel password
    size_t userLimit;               // 0 means no limit
    bool inviteOnly;                // +i mode
    bool topicRestricted;           // +t mode
    std::string topic;              // Channel topic
    ...
};
```

### 2. Server.hpp - Added New Command Handlers

```cpp
void handleKick(int fd, const std::vector<std::string>& params);
void handleInvite(int fd, const std::vector<std::string>& params);
```

### 3. Server.cpp - Updated handleJoin()

Modified the invite-only check to allow invited users:

```cpp
// Check invite-only mode
if (chanInfo.inviteOnly) {
    // Check if user is invited
    if (chanInfo.invited.find(fd) == chanInfo.invited.end()) {
        sendReply(fd, "473 " + client.nickname + " " + channel + " :Cannot join channel (+i)\r\n");
        return;
    }
    // User is invited, remove from invite list after successful join attempt
    chanInfo.invited.erase(fd);
}
```

### 4. Server.cpp - Implemented handleKick()

Full implementation of KICK command with:
- Parameter validation (requires channel and target nickname)
- Channel existence check
- Membership validation (kicker must be in channel)
- Operator permission check (only ops can kick)
- Target validation (target must exist and be in channel)
- Broadcast KICK message to all channel members
- Proper cleanup (remove from members, operators, and delete channel if empty)

### 5. Server.cpp - Implemented handleInvite()

Full implementation of INVITE command with:
- Parameter validation (requires target nickname and channel)
- Channel existence check
- Membership validation (inviter must be in channel)
- Permission check (for invite-only channels, only ops can invite)
- Target validation (target must exist and not already be in channel)
- Add target to invite list
- Send invite notification to target
- Send confirmation to inviter

### 6. Server.cpp - Updated processMessage()

Added command routing for KICK and INVITE:

```cpp
} else if (command == "KICK") {
    handleKick(fd, params);
} else if (command == "INVITE") {
    handleInvite(fd, params);
```

## Usage Examples

### KICK Command

**Syntax:** `KICK <channel> <nickname> [reason]`

**Examples:**

```irc
/KICK #mychannel baduser
/KICK #mychannel baduser Spamming
/KICK #mychannel alice You violated the rules
```

**What happens:**
1. Server validates that kicker is channel operator
2. Server validates that target user exists and is in the channel
3. KICK message is broadcast to all channel members (including the kicked user)
4. Target user is removed from the channel
5. If channel becomes empty, it's deleted

**Broadcast format:**
```
:kicker!user@host KICK #channel target :reason
```

### INVITE Command

**Syntax:** `INVITE <nickname> <channel>`

**Examples:**

```irc
/INVITE alice #mychannel
/INVITE bob #privatechat
```

**What happens:**
1. Server validates that inviter is in the channel
2. For invite-only channels (+i), validates that inviter is operator
3. Server validates that target user exists and is not already in channel
4. Target user is added to the invite list
5. Target receives invitation notification
6. Inviter receives confirmation

**Messages sent:**

To target:
```
:inviter!user@host INVITE target :#channel
```

To inviter (confirmation):
```
:server 341 inviter target #channel
```

## Workflow: Invite-Only Channel

### Setting up an invite-only channel:

```irc
# User creates and joins channel
/JOIN #private
# User becomes operator automatically (first to join)

# Set channel to invite-only
/MODE #private +i

# Now only invited users can join
/INVITE alice #private
/INVITE bob #private
```

### Alice trying to join:

```irc
# Without invite:
/JOIN #private
→ :server 473 alice #private :Cannot join channel (+i)

# After being invited:
# Alice receives: :operator!user@host INVITE alice :#private
/JOIN #private
→ Successfully joins! Invite is consumed and removed from list.
```

## Error Responses

### KICK Errors

- **451**: Not registered
- **461**: Not enough parameters (need channel and nickname)
- **403**: No such channel
- **442**: You're not on that channel
- **482**: You're not channel operator
- **401**: No such nick (target doesn't exist)
- **441**: They aren't on that channel

### INVITE Errors

- **451**: Not registered
- **461**: Not enough parameters (need nickname and channel)
- **403**: No such channel
- **442**: You're not on that channel
- **482**: You're not channel operator (for invite-only channels)
- **401**: No such nick (target doesn't exist)
- **443**: User is already on channel

## Implementation Details

### Invite List Management

- Invites are stored per-channel in `ChannelInfo::invited`
- Invites are automatically removed when user successfully joins
- Invites persist across sessions (until used or channel is deleted)
- When a channel is deleted (no members), all invites are lost

### Permission Model

#### KICK:
- Must be in the channel
- Must be channel operator
- Can kick any user (including other operators)

#### INVITE:
- Must be in the channel
- For normal channels: any member can invite
- For invite-only channels (+i): only operators can invite

### Integration with MODE

The INVITE command works seamlessly with the MODE +i (invite-only) flag:

```irc
# Create channel
/JOIN #test

# Make it invite-only
/MODE #test +i

# Now only invited users can join
/INVITE friend #test

# Friend can now join
```

## Testing

### Test KICK:

```bash
# Terminal 1 (operator)
./ircserv 6667 password
# Connect with irssi: /connect localhost 6667 password
/NICK op
/JOIN #test

# Terminal 2 (regular user)
# Connect with irssi: /connect localhost 6667 password
/NICK user
/JOIN #test

# Back to Terminal 1
/KICK #test user Goodbye
# User should be removed from channel
```

### Test INVITE:

```bash
# Terminal 1 (operator)
/JOIN #private
/MODE #private +i

# Terminal 2 (user trying to join)
/JOIN #private
# Should fail with "Cannot join channel (+i)"

# Back to Terminal 1
/INVITE user #private

# Terminal 2 should receive invitation
# Now can join:
/JOIN #private
# Should succeed!
```

## Notes

- KICK removes users from both the members set and operators set
- INVITE adds users to a temporary invite list that's consumed on JOIN
- Both commands broadcast appropriate messages to channel members
- Both commands have comprehensive error checking
- Invites are channel-specific and don't transfer between channels
