/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "socket_peer_is_trusted.h"

#if !defined(_WIN32)
#include <pwd.h>
#include <sys/socket.h>
#endif

#include <android-base/logging.h>

namespace art {

// Returns true if the user on the other end of the socket is root or shell.
#ifdef ART_TARGET_ANDROID
bool SocketPeerIsTrusted(int fd) {
  ucred cr;
  socklen_t cr_length = sizeof(cr);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &cr_length) != 0) {
    PLOG(ERROR) << "couldn't get socket credentials";
    return false;
  }

  passwd* shell = getpwnam("shell");
  if (cr.uid != 0 && cr.uid != shell->pw_uid) {
    LOG(ERROR) << "untrusted uid " << cr.uid << " on other end of socket";
    return false;
  }

  return true;
}
#else
bool SocketPeerIsTrusted(int /* fd */) {
  return true;
}
#endif

}  // namespace art
