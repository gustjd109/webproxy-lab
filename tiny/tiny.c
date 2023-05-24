/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* port 번호를 인자로 받아 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 함수를 호출 */
// argc: 인자 개수, argv: 인자 배열
// argv는 main 함수가 받은 각각의 인자들
// ./tiny 8000 => argv[0] = ./tiny, argv[1] => 8000
// 메인함수에 전달되는 정보의 개수가 2개여야함 (argv[0]: 실행경로, argv[1]: port num)
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen; // clientaddr 구조체의 크기 (byte)
  struct sockaddr_storage clientaddr; // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소

  /* Check command line args */
  if (argc != 2) { // 인자 2개 모두 받아야 함
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // f(파일)printf => 파일을 읽어주는 것, stderr(식별자 2) = standard error
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 해당 포트 번호에 해당하는 듣기 소켓 식별자 오픈
  while (1) { // 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 호출
    clientlen = sizeof(clientaddr);

    // 클라이언트에게서 받은 연결 요청 수락(connfd = 서버 연결 식별자)
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept

    // 연결이 성공했다는 메세지를 위해 Getnameinfo를 호출하면서 hostname과 port가 채워짐
    // 마지막 부분은 flag로 0 : 도메인 네임, 1 : IP address
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // doit 함수 실행
    doit(connfd);   // line:netp:tiny:doit

    // 서버 연결 식별자 닫음
    Close(connfd);  // line:netp:tiny:close
  }
}

/* 클라이언트의 요청 라인을 확인해 정적, 동적 컨텐츠인지를 구분하고 각각의 서버에 전송 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 클라이언트에게서 받은 요청(rio)으로 채움
  char filename[MAXLINE], cgiargs[MAXLINE]; // parse_uri를 통해 채움
  rio_t rio;
  
  /* 클라이언트가 rio로 보낸 request 라인과 헤더를 읽고 분석 */
  Rio_readinitb(&rio, fd); // rio 버퍼와 fd, 여기서는 서버의 connfd를 연결
  Rio_readlineb(&rio, buf, MAXLINE); // rio(=connfd)에 있는 string 한 줄(응답 라인)을 모두 buf로 이동
  printf("Request headers:\n");
  printf("%s", buf); // 요청 라인 buf = "GET /godzilla.gif HTTP/1.1\0"을 표준 출력만 수행
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 method, uri, version이라는 문자열에 저장

  // 요청 method가 GET이 아니면 종료하고, main으로 가서 연결 닫고 다음 요청 대기
  if(strcasecmp(method, "GET")) { // method 스트링이 GET이 아니면 0이 아닌 값이 나옴
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio); // 요청 라인을 뺀 나머지 요청 헤더들을 무시(그냥 프린트)

  /* Parse URI from GET request */
  /* parse_uri : 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분 */
  is_static = parse_uri(uri, filename, cgiargs); // is_static이 1이면 정적 컨텐츠, 0이면 동적 컨텐츠

  /* stat(file, *buffer) : file의 상태를 buffer에 넘김 */
  /* filename : 클라이언트가 요청한 서버의 컨텐츠 디렉토리 및 파일 이름 */
  if(stat(filename, &sbuf) < 0) { // 못 넘기면 fail(파일이 없음 = 404)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* 컨텐츠의 유형(정적, 동적)을 파악한 후 각각의 서버에 전송 */
  if(is_static) { /* Serve static content */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // !(일반 파일이다) or !(읽기 권한이 있다)
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // 정적 서버에 파일의 사이즈와 메서드를 같이 전송 -> Response header에 Content-length 위해
  }
  else { /* Serve dynamic content */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // !(일반 파일이다) or !(실행 권한이 있다)
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 서버에 인자와 메서드를 같이 전송
  }
}

/* 에러 메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 전송 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body,longmsg, cause);
  sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // 에러메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  // 버퍼 rp의 마지막 끝을 만날 때까지("Content-length: %d\r\n\r\n에서 마지막 \r\n)
  // 계속 출력해줘서 없앰
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* uri를 받아 filename과 cgiarg를 채움 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  /* uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 리턴 */
  if(!strstr(uri, "cgi-bin")) { // Static content, uri안에 "cgi-bin"과 일치하는 문자열이 있는지 확인
    strcpy(cgiargs, ""); // 정적이니까 cgiargs는 아무것도 없음
    strcpy(filename, "."); // 현재경로에서부터 시작(./path ~~)
    strcat(filename, uri); // filename 스트링에 uri 스트링을 추가

    // 만약 uri뒤에 '/'이 있다면 그 뒤에 home.html을 추가
    // 내가 브라우저에 http://localhost:8000만 입력하면 바로 뒤에 '/'이 생기는데, '/' 뒤에 home.html을 붙여 해당 위치 해당 이름의 정적 컨텐츠가 출력
    if(uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1; // 정적 컨텐츠면 1 리턴
  }
  else { /* Dynamic content */
    ptr = index(uri, '?');
    if(ptr) { // '?'가 있으면 cgiargs를 '?' 뒤 인자들과 값으로 채워주고 ?를 NULL로 만듦
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else // '?'가 없으면 그냥 아무것도 안 넣어줌
      strcpy(cgiargs, "");
    strcpy(filename, "."); // 현재 디렉토리에서 시작
    strcat(filename, uri); // uri 추가
    return 0;
  }
}

/* 클라이언트가 원하는 정적 컨텐츠 디렉토리를 받아오고, 응답 라인과 헤더를 작성하고 서버에게 보낸다. */
/* 그 후 정적 컨텐츠 파일을 읽어 그 응답 본문을 클라이언트에 보낸다. */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE],buf[MAXBUF];

  /* 클라이언트에게 응답 헤더 전송 */
  get_filetype(filename, filetype);                           // 파일 타입 결정
  sprintf(buf, "HTTP/1.1 200 OK\r\n");                        // 상태 코드
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);        // 서버 이름
  sprintf(buf, "%sConnection : close\r\n", buf);              // 연결 방식
  sprintf(buf, "%sContent-length : %d\r\n", buf, filesize);   // 컨텐츠 길이
  sprintf(buf, "%sContent-type : %s\r\n\r\n", buf, filetype); // 컨텐츠 타입
  Rio_writen(fd, buf, strlen(buf));                           // buf에서 fd로 전송(헤더 정보 전송)
  printf("Response headers : \n");
  printf("%s", buf);

  /* 클라이언트에게 응답 본문 전송 */
  srcfd = Open(filename, O_RDONLY, 0);                        // 파일 열기
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 가상메모리에 매핑
  Close(srcfd);                                               // 파일 디스크립터 닫기
  Rio_writen(fd, srcp, filesize);                             // 파일 내용을 클라이언트에게 전송(응답 본문 전송)
  Munmap(srcp, filesize);                                     // 매핑된 가상메모리 해제
}

/* filename을 조사해 각각의 식별자에 맞는 MIME 타입을 filetype에 입력 */
void get_filetype(char *filename, char *filetype)
{
  if(strstr(filename, ".html")) // filename 스트링에 ".html" 
    strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpg");
  else
    strcpy(filetype, "text/plain");
}

/* 클라이언트가 원하는 동적 컨텐츠 디렉토리를 받아오고, 응답 라인과 헤더를 작성하고 서버에게 보낸다. */
/* CGI 자식 프로세스를 fork하고 그 프로세스의 표준 출력을 클라이언트 출력과 연결한다. */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0) { // 자식 프로세스 포크
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);   // QUERY_STRING 환경 변수를 URI에서 추출한 CGI 인수로 설정
    Dup2(fd, STDOUT_FILENO);              // 자식 프로세스의 표준 출력을 클라이언트 소켓에 연결된 파일 디스크립터로 변경
    Execve(filename, emptylist, environ); // 현재 프로세스의 이미지를 filename 프로그램으로 대체
  }
  Wait(NULL); /* Parent waits for and reaps child */
}