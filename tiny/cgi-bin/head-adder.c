/*
 * head-adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  // char *buf, *p;
  // 숙제 문제 11.11 : HEAD 메서드를 사용하기 위해 포인터 변수 추가
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p + 1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  // 숙제 문제 11.11 : 환경 변수로 넣어둔 요청 메서드를 확인하기 위해 추가
  method = getenv("REQUEST_METHOD");

  /* Make the response body */
  sprintf(content, "QUERY_STRING = %s", buf);
  sprintf(content, "%sWelcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  // printf("%s", content);

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
