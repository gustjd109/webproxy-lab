#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_MAGIC_NUMBER 9999
#define CACHE_OBJS_COUNT 10

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Hst: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void *thread(void *vargp);
void doit(int commfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port, char *http_header);

void cache_init();
int cache_find(char *url);
int cache_eviction();
void cache_LRU(int index);
void cache_uri(char *uri, char *buf);
void readerPre(int i);
void readerAfter(int i);

typedef struct {
  char cache_obj[MAX_OBJECT_SIZE];
  char cache_url[MAXLINE];
  int LRU;
  int isEmpty;

  int readCnt;
  sem_t wmutex;
  sem_t rdcntmutex;

  int writeCnt;
  sem_t wtcntMutex;
  sem_t queue;
} cache_block;

typedef struct {
  cache_block cacheobjs[CACHE_OBJS_COUNT];
} Cache;

Cache cache;

int main(int argc, char **argv) {
  int listenfd, connfd;
  pthread_t tid;
  socklen_t clientlen;
  char hostname[MAXLINE], port[MAXLINE];
  struct sockaddr_storage clientaddr;

  cache_init();

  if(argc != 2) {
    fprintf(stderr, "using : %s <port> \n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);
  listenfd = Open_listenfd(argv[1]);
  while(1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s).\n", hostname, port);

    Pthread_create(&tid, NULL, thread, (void *)connfd);
  }
  return 0;
}

void *thread(void *vargp) {
  int connfd = (int)vargp;
  Pthread_detach(pthread_self());
  doit(connfd);
  Close(connfd);
}

void doit(int connfd) {
  int end_serverfd;

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char endserver_http_header [MAXLINE];

  char hostname[MAXLINE], path[MAXLINE];
  int port;

  rio_t rio, server_rio;

  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);

  char url_store[100];
  strcpy(url_store, uri);
  if(strcasecmp(method, "GET")) {
    printf("Proxy does not implement the method");
    return;
  }

  int cache_index;
  if((cache_index = cache_find(url_store)) != -1) {
    readerPre(cache_index);
    Rio_writen(connfd, cache.cacheobjs[cache_index].cache_obj, strlen(cache.cacheobjs[cache_index].cache_obj));
    readerAfter(cache_index);
    cache_LRU(cache_index);
    return;
  }

  parse_uri(uri, hostname, path, &port);

  build_http_header(endserver_http_header, hostname, path, port, &rio);

  end_serverfd = connect_endServer(hostname, port, endserver_http_header);
  if(end_serverfd < 0) {
    printf("connection failed\n");
    return;
  }

  Rio_readinitb(&server_rio, end_serverfd);
  Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

  char cachebuf[MAX_OBJECT_SIZE];
  int sizebuf = 0;
  size_t n ;
  while((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
    sizebuf += n;
    if(sizebuf < MAX_OBJECT_SIZE) strcat(cachebuf, buf);
    Rio_writen(connfd, buf, n);
  }
  Close(end_serverfd);

  if(sizebuf < MAX_OBJECT_SIZE) {
    cache_uri(url_store, cachebuf);
  }
}

void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio)
{
  char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
  sprintf(request_hdr, requestlint_hdr_format, path);
  while(rio_readlineb(client_rio, buf, MAXLINE > 0)) {
    if(strcmp(buf, endof_hdr) == 0) break;

    if(!strncasecmp(buf, host_key, strlen(host_key))) {
      strcpy(host_hdr, buf);
      continue;
    }

    if(!strncasecmp(buf, connection_key, strlen(connection_key)) && !strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
      strcat(other_hdr, buf);
    }
  }
  if(strlen(host_hdr) == 0) {
    sprintf(host_hdr, host_hdr_format, hostname);
  }
  sprintf(http_header, "%s%s%s%s%s%s%s",
          request_hdr,
          host_hdr,
          conn_hdr,
          prox_hdr,
          user_agent_hdr,
          other_hdr,
          endof_hdr);
  return;
}

inline int connect_endServer(char *hostname, int port, char *http_header) {
  char portStr[100];
  sprintf(portStr, "%d", port);
  return Open_clientfd(hostname, portStr);
}

void parse_uri(char *uri, char *hostname, char *path, int *port)
{
  *port = 80;
  char* pos = strstr(uri, "//");

  pos = pos != NULL ? pos + 2 : uri;

  char* pos2 = strstr(pos, ":");
  if(pos2 != NULL) {
    *pos2 = '\0';
    sscanf(pos, "%s", hostname);
    sscanf(pos2 + 1, "%d%s", port, path);
  }
  else {
    pos2 = strstr(pos, "/");
    if(pos2 != NULL) {
      *pos2 = '\0';
      sscanf(pos, "%s", hostname);
      *pos2 = '/';
      sscanf(pos2, "%s", path);
    }
    else {
      sscanf(pos, "%s", hostname);
    }
  }
  return;
}

void cache_init()
{
  int i;
  for(i = 0; i < CACHE_OBJS_COUNT; i++) {
    cache.cacheobjs[i].LRU = 0;
    cache.cacheobjs[i].isEmpty = 1;
    Sem_init(&cache.cacheobjs[i].wmutex, 0, 1);
    Sem_init(&cache.cacheobjs[i].rdcntmutex, 0, 1);
    cache.cacheobjs[i].readCnt = 0;
    
    cache.cacheobjs[i].writeCnt = 0;
    Sem_init(&cache.cacheobjs[i].wtcntMutex, 0, 1);
    Sem_init(&cache.cacheobjs[i].queue, 0, 1);
  }
}

void readerPre(int i)
{
  P(&cache.cacheobjs[i].queue);
  P(&cache.cacheobjs[i].rdcntmutex);
  cache.cacheobjs[i].readCnt++;
  if(cache.cacheobjs[i].readCnt == 1) P(&cache.cacheobjs[i].wmutex);
  V(&cache.cacheobjs[i].rdcntmutex);
  V(&cache.cacheobjs[i].queue);
}

void readerAfter(int i)
{
  P(&cache.cacheobjs[i].rdcntmutex);
  cache.cacheobjs[i].readCnt--;
  if(cache.cacheobjs[i].readCnt == 0) V(&cache.cacheobjs[i].wmutex);
  V(&cache.cacheobjs[i].rdcntmutex);
}

void writePre(int i)
{
  P(&cache.cacheobjs[i].wtcntMutex);
  cache.cacheobjs[i].writeCnt++;
  if(cache.cacheobjs[i].writeCnt == 1) P(&cache.cacheobjs[i].queue);
  V(&cache.cacheobjs[i].wtcntMutex);
  P(&cache.cacheobjs[i].wmutex);
}

void writeAfter(int i)
{
  V(&cache.cacheobjs[i].wmutex);
  P(&cache.cacheobjs[i].wtcntMutex);
  cache.cacheobjs[i].writeCnt--;
  if(cache.cacheobjs[i].writeCnt == 0) V(&cache.cacheobjs[i].queue);
  V(&cache.cacheobjs[i].wtcntMutex);
}

int cache_find(char *url)
{
  int i;
  for(i = 0; i <CACHE_OBJS_COUNT; i++) {
    readerPre(i);
    if((cache.cacheobjs[i].isEmpty == 0) && (strcmp(url, cache.cacheobjs[i].cache_url) == 0)) {
      readerAfter(i);
      break;
    }
    readerAfter(i);
  }
  if(i >= CACHE_OBJS_COUNT) return -1;
  return i;
}

int cache_eviction()
{
  int min = LRU_MAGIC_NUMBER;
  int minindex = 0;
  int i;
  for(i = 0; i < CACHE_OBJS_COUNT; i++) {
    readerPre(i);
    if(cache.cacheobjs[i].isEmpty == 1) {
      minindex = i;
      readerAfter(i);
      break;
    }
    if(cache.cacheobjs[i].LRU < min) {
      minindex = i;
      readerAfter(i);
      continue;
    }
    readerAfter(i);
  }
  return minindex;
}

void cache_LRU(int index)
{
  writePre(index);
  cache.cacheobjs[index].LRU = LRU_MAGIC_NUMBER;
  writeAfter(index);

  int i;
  for(i = 0; i < CACHE_OBJS_COUNT; i++) {
    if(i == index)
    continue;
    writePre(i);
    if(cache.cacheobjs[i].isEmpty == 0) {
      cache.cacheobjs[i].LRU--;
    }
    writeAfter(i);
  }
}

void cache_uri(char *uri, char *buf)
{
  int i = cache_eviction();

  writePre(i);

  strcpy(cache.cacheobjs[i].cache_obj, buf);
  strcpy(cache.cacheobjs[i].cache_url, uri);
  cache.cacheobjs[i].isEmpty = 0;

  writeAfter(i);

  cache_LRU(i);
}