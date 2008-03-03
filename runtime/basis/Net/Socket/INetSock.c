#include "platform.h"

void 
Socket_INetSock_toAddr (Vector(Word8_t) in_addr, Word16_t port, 
                        Array(Word8_t) addr, Ref(C_Socklen_t) addrlen) {
  struct sockaddr_in *sa = (struct sockaddr_in*)addr;

  sa->sin_family = AF_INET;
  sa->sin_port = (uint16_t)port;
  sa->sin_addr = *(const struct in_addr*)in_addr;
  *((socklen_t*)addrlen) = sizeof(struct sockaddr_in);
}

/* XXX global state */
static uint16_t fromAddr_port;
static struct in_addr fromAddr_in_addr;

void Socket_INetSock_fromAddr (Vector(Word8_t) addr) {
  const struct sockaddr_in *sa = (const struct sockaddr_in*)addr;

  assert(sa->sin_family == AF_INET);
  fromAddr_port = sa->sin_port;
  fromAddr_in_addr = sa->sin_addr;
}

Word16_t Socket_INetSock_getPort (void) {
  return (Word16_t)fromAddr_port;
}

void Socket_INetSock_getInAddr (Array(Word8_t) addr) {
  *(struct in_addr*)addr = fromAddr_in_addr;
}
