/*
 * head-adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  // 숙제 문제 11.11 : HEAD 메서드를 사용하기 위해 포인터 변수 추가
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* 서버 프로세스가 만들어준 QUERY_STRING 환경변수를 getenv로 가져와 buf에 넣음 */
  if((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&'); // 포인터 p는 &가 있는 곳의 주소
    *p = '\0'; // &를 \0으로 변경
    strcpy(arg1, buf); // & 앞에 있었던 인자
    strcpy(arg2, p + 1); // & 뒤에 있었던 인자
    n1 = atoi(arg1); // 문자 스트링인 arg1을 int형 변환
    n2 = atoi(arg2); // 문자 스트링인 arg2을 int형 변환
  }

  // 숙제 문제 11.11 : 환경 변수로 넣어둔 요청 메서드를 확인하기 위해 추가
  method = getenv("REQUEST_METHOD");

  /* content라는 string에 응답 본문 포함 */
  sprintf(content, "QUERY_STRING = %s", buf); // ""안의 string이 content라는 string에 쓰임
  sprintf(content, "%sWelcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2); // 인자를 받아 처리
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* CGI 프로세스가 부모인 서버 프로세스 대신 채워 주어야 하는 응답 헤더 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");

  // 숙제 문제 11.11 : HEAD 메서드가 아닐 경우만 응답시 본문을 같이 출력(방법 1)
  if (strcasecmp(method, "HEAD") != 0)
    printf("%s", content);
  
  // 숙제 문제 11.11 : 메서드가 GET일 경우만 응답시 본문을 같이 출력(방법 2)
  // if (strcasecmp(method, "GET") == 0) {
  //   printf("%s", content);
  // }

  fflush(stdout);
  
  exit(0);
}
/* $end adder */
