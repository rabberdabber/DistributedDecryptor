#ifndef __COMMON_H_
#define __COMMON_H_

// this may not be the best way look at other files
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <fstream>


using namespace std;

#define HDR_LEN 8
#define BACKLOG 1024
#define BUFSIZE 51200

enum MSG_TYPE
{
    REQUEST = 0x00000010,
    DELIVERY = 0x00000011,
    DONE = 0x00000001,
    ERROR = 0x00000020
};

struct Hdr
{
    uint32_t tot_len;
    uint32_t msg_type;
} __attribute__((packed));

class LRUCache
{
private:
    struct mapping
    {
        string key;
        string val;
        mapping(string k, string v) : key(k), val(v) {}
    };

public:
    LRUCache(int max_size) : bytes_left(max_size) {}

    string get(string key)
    {
        if (cacheMap.find(key) == cacheMap.end())
            return "";

        cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
        cacheMap[key] = cacheList.begin();
        return cacheMap[key]->val;
    }

    void put(string key, string val)
    {
        int key_len = (int)key.size();
        int val_len = (int)val.size();

        bool space_avail = true;

        if (bytes_left < key_len + val_len)
        {
            space_avail = false;
        }

        if (cacheMap.find(key) == cacheMap.end())
        {
            while (!space_avail && cacheMap.size() > 0)
            {
                string removed_key = cacheList.back().key;
                string removed_val = cacheMap[removed_key]->val;

                cout << "removed_key" << removed_key << endl;
                cout << "removed_val" << removed_val << endl;

                int removed_key_len = (int)removed_key.size();
                int removed_val_len = (int)removed_val.size();

                cacheMap.erase(cacheList.back().key);
                cacheList.pop_back();

                bytes_left += removed_val_len + removed_key_len;

                if (bytes_left > key_len + val_len)
                {
                    space_avail = true;
                }
            }

            if (space_avail)
            {
                cacheList.push_front(mapping(key, val));
                cacheMap[key] = cacheList.begin();
                bytes_left -= key_len + val_len;
            }
        }

        else
        {
            cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
            cacheMap[key] = cacheList.begin();
        }
    }

private:
    int bytes_left;
    list<mapping> cacheList;

    unordered_map<string, list<mapping>::iterator> cacheMap;
};


int open_listenfd(int port)
{
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr = {0};

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(int)) < 0)
    {
        close(listenfd);
        perror("setsockopt");
        return -1;
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr *)&serveraddr,
             sizeof(serveraddr)) < 0)
    {
        close(listenfd);
        perror("bind");
        return -1;
    }

    if (listen(listenfd, BACKLOG) < 0)
    {
        close(listenfd);
        perror("listen");
        return -1;
    }

    return listenfd;
}

int open_clientfd(char *hostname, int port)
{
    int clientfd, result;
    struct hostent *hp;
    struct sockaddr_in serveraddr = {0};

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    /* the given String is a valid ip address */
    if ((result = inet_pton(AF_INET, hostname, &(serveraddr.sin_addr)) == 1))
    {
    }

    else
    {
        if ((hp = gethostbyname(hostname)) == NULL)
        {
            close(clientfd);
            perror("gethostbyname");
            return -1;
        }

        bzero((char *)&serveraddr, sizeof(serveraddr));

        serveraddr.sin_family = AF_INET;

        bcopy((char *)hp->h_addr_list[0],
              (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
        serveraddr.sin_port = htons(port);
    }

    if (connect(clientfd, (struct sockaddr *)&serveraddr,
                sizeof(serveraddr)) < 0)
    {
        close(clientfd);
        perror("connect");
        return -1;
    }

    return clientfd;
}


void get_ip(unsigned char *ip_addr,char * interface_name)
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET,SOCK_DGRAM,0);

    ifr.ifr_addr.sa_family = AF_INET;

    memcpy(ifr.ifr_name,interface_name,IFNAMSIZ-1);

    ioctl(fd,SIOCGIFADDR,&ifr);
    close(fd);

    strcpy((char *)ip_addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

}

struct Hdr *create_pckt(size_t len, uint32_t msg_type)
{

    struct Hdr *header = (struct Hdr *)calloc(1, sizeof(struct Hdr));

    if (!header)
        return NULL;

    header->tot_len = htonl(len + HDR_LEN);
    header->msg_type = htonl(msg_type);

    return header;
}

int recv_from_host(int socket, char *buf, size_t len)
{
    ssize_t rd = 0, tmp = 0;
    size_t rem = len;

    while (rem > 0)
    {
        tmp = read(socket, buf + rd, rem);
        rem -= tmp;
        rd += tmp;
        if (tmp < 0)
        {
            perror("read");
            return tmp;
        }

        if (tmp == 0)
            return rd;
    }

    return rd;
}

int send_to_host(int socket, const char *buf, size_t len)
{
    ssize_t sent = 0, tmp = 0;
    size_t rem = len;

    while (rem > 0)
    {
        tmp = write(socket, buf + sent, rem);
        rem -= tmp;
        sent += tmp;

        if (tmp < 0)
        {
            perror("write");
            return tmp;
        }
    }

    return sent;
}

#endif
