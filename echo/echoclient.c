#include "csapp.h"

/* 0번째 인자로 실행 파일, 1번째로 서버의 호스트 네임, 2번째로 서버의 포트 넘버를 전달 받음 */
int main(int argc, char **argv)
{
    int clientfd;
    char*host, *port, buf[MAXLINE];
    rio_t rio;

    if(argc != 3) { // 인자 3개를 모두 받아야 함
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port); // 서버와의 연결에 성공해서 클라이언트 소켓 식별자 리턴
    Rio_readinitb(&rio, clientfd); // 1. 클라이언트 소켓 파일 식별자와 읽기 버퍼 rio를 연결

    // 표준 입력에서 텍스트 줄을 반복적으로 읽음
    // 2. 표준 입력 stdin에서 MAXLINE만큼 바이트를 가져와 buf에 저장
    while(Fgets(buf, MAXLINE, stdin) != NULL) { // 6. EOF 표준 입력을 만나면 종료
        Rio_writen(clientfd, buf, strlen(buf)); // 3. buf 메모리 안의 strlen(buf) 바이트 만큼의(사실상 모두)를 clientfd로 전송
        Rio_readlineb(&rio, buf, MAXLINE); // 4. 서버가 buf에 echo줄을 쓰면 그 buf를 읽어서 읽기 버퍼 rio에 쓰기
        Fputs(buf, stdout); // 5. buf에 받아온 값을 표준 출력으로 인쇄
    }
    Close(clientfd); // 루프가 종료되면 클라이언트 식별자를 닫고, 서버에 EOF 통지를 전송
    exit(0); // 클라이언트 종료
}