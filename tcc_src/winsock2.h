/* Minimal winsock2.h for TCC */
#ifndef _WINSOCK2_H
#define _WINSOCK2_H

#include <stdint.h>

typedef uintptr_t SOCKET;
typedef struct { int len; char* buf; } WSABUF;
typedef struct { uintptr_t Internal; uintptr_t InternalHigh; union { struct { uint32_t Offset; uint32_t OffsetHigh; }; void* Pointer; }; void* hEvent; } OVERLAPPED;
typedef void (*LPWSAOVERLAPPED_COMPLETION_ROUTINE)(uint32_t, uint32_t, void*, uint32_t, uint32_t);

#define INVALID_SOCKET (~0ULL)
#define SOCKET_ERROR (-1)
#define SD_SEND 1

/* WSADATA */
typedef struct WSADATA {
    unsigned short wVersion;
    unsigned short wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
} WSADATA, *LPWSADATA;

/* from winsock.h */
#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define IPPROTO_TCP 6
#define SOL_SOCKET 0xFFFF
#define SO_REUSEADDR 4

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr {
        uint32_t s_addr;
    } sin_addr;
    char sin_zero[8];
};

#define INADDR_ANY 0
#define INADDR_LOOPBACK 0x7f000001
#define INADDR_BROADCAST 0xffffffff

#define SOMAXCONN 0x7fffffff
#define MAKEWORD(a,b) ((unsigned short)(((unsigned char)(a)) | ((unsigned short)((unsigned char)(b))) << 8))

/* Winsock functions */
unsigned short htons(unsigned short hostshort);
unsigned long htonl(unsigned long hostlong);
int WSAStartup(unsigned short wVersionRequested, WSADATA* lpWSAData);
int WSACleanup(void);
SOCKET socket(int af, int type, int protocol);
int bind(SOCKET s, const struct sockaddr* addr, int namelen);
int listen(SOCKET s, int backlog);
SOCKET accept(SOCKET s, struct sockaddr* addr, int* addrlen);
int closesocket(SOCKET s);
int send(SOCKET s, const char* buf, int len, int flags);
int recv(SOCKET s, char* buf, int len, int flags);
int setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen);
int ioctlsocket(SOCKET s, long cmd, uint32_t* argp);
int WSAGetLastError(void);

#endif
