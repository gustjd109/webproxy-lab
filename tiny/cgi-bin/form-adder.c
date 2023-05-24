/*
 * form-adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* 서버 프로세스가 만들어준 QUERY_STRING 환경변수를 getenv로 가져와 buf에 넣음 */
  if((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&'); // 포인터 p는 &가 있는 곳의 주소
    *p = '\0'; // &를 \0으로 변경
    sscanf(buf, "first = %d", &n1); // buf에서 %d를 읽어서 n1에 저장
    sscanf(p + 1, "second = %d", &n2); // buf에서 %d를 읽어서 n2에 저장
  }

  /* content라는 string에 응답 본문 포함 */
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2); // 인자를 받아 처리
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* CGI 프로세스가 부모인 서버 프로세스 대신 채워 주어야 하는 응답 헤더 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content); // 응답 본문 출력
  fflush(stdout);
  
  exit(0);
}
/* $end adder */