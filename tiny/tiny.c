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
// void echo(int connfd){
//     size_t n;
//     char buf[MAXLINE];
//     rio_t rio;

//     Rio_readinitb(&rio, connfd);
//     while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
//     {
//       // 찾았다 준규야
//       printf("%s\n", buf);
//       Rio_writen(connfd, buf, n);
//     }
// }

// int main(int argc, char **argv)
// {
//     int listenfd, connfd;
//     socklen_t clientlen;
//     struct sockaddr_storage clientaddr;
//     char client_hostname[MAXLINE], client_port[MAXLINE];

//     if (argc != 2)
//     {
//         fprintf(stderr, "포트써라~ %s <port>\n", argv[1]);
//         exit(0);
//     }

//     listenfd = Open_listenfd(argv[1]);
//     while(1)
//     {
//         clientlen = sizeof(struct sockaddr_storage);
//         connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
//         Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
//         printf("연결했당! to (%s, %s)\n", client_hostname, client_port);
//         echo(connfd);
//         Close(connfd);
//     }
//     exit(0);
// }


int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  // argc == 메인함수에 전달되는 정보의 개수 (신기하게 전달 정보가 없어도 argv[0]으로 인해 개수가 1)
  // 따라서 아래 2가 아니라는건 port번호가 없이 실행될 때 꺼버린다는 뜻!
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  // argv = 메인함수에 실질적인 매개변수로, 문자열의 배열을 의미 첫번째 문자열 argv[0]은 프로그램의 실행경로로 고정
  // 포트넘버로 클라이언트에서 요청한 
  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 요청과 헤더를 읽는다.
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  //URI 파싱
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static)
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    //과제
    if (strcasecmp(method, "GET"))
    {
      char filetype[MAXLINE];
      get_filetype(filename, filetype);
      sprintf(buf, "HTTP/1.0 200 OK\r\n");
      sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
      sprintf(buf, "%sConnection: close\r\n", buf);
      sprintf(buf, "%sContent-length: %d\r\n", buf, sbuf.st_size);
      sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
      Rio_writen(fd, buf, strlen(buf));
      printf("Response headers:\n");
      printf("%s", buf);
    }
    else
    {
      serve_static(fd, filename, sbuf.st_size);
    }
    
  }
  else
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    if (strcasecmp(method, "GET"))
    {
      sprintf(buf, "HTTP/1.0 200 OK\r\n");
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Server: Tiny Web Server\r\n");
      Rio_writen(fd, buf, strlen(buf));
    }
    else
    {
      serve_dynamic(fd, filename, cgiargs);
    }
    
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXLINE];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n",body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
    {
      strcat(filename, "jg.html");
    }
    else if (uri[strlen(uri) - 1] == '/jg')
    {
      strcat(filename, "jg.html");
    }
    return 1;
  }
  else
  {
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
    {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXLINE];

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  //과제
  FILE *fp = fopen(filename, "r");
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  srcp = malloc(size+1);
  memset(srcp, 0, size+1);
  fseek(fp, 0, SEEK_SET);
  fread(srcp, filesize, 1, fp);
  fclose(fp);
  Rio_writen(fd, srcp, size);
  free(srcp);

  // 요청과 헤더를 읽는다.
  // srcfd = Open(filename, O_RDONLY, 0);
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  // Close(srcfd);
  // Rio_writen(fd, srcp, filesize);
  // Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "image/mp4");
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "image/mpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  {
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}
