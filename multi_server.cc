//표준 헤더
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//리눅스 헤더
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFSIZE 1024
#define MAX_CLIENT 64
//소켓할당==전화기를 할당한다.
int clnt_socks[MAX_CLIENT];
char clnt_nickname[MAX_CLIENT][20];
int clnt_id=0;

void error_handling(const char* message){
    fputs(message, stderr); //표준 에러형식으로 메시지 출력
    fputc('\n',stderr);
    exit(-1);
}

void* handle_clnt(void* arg){
    int clnt_sock = *((int*)arg);
    int str_len = 0;
    char msg[BUFSIZE+1]={0,};
    char send_msg[BUFSIZE + 30];
    int i, idx;
    for(idx=0; idx < clnt_id; idx++){
        if(clnt_sock == clnt_socks[idx]){
            break;
        }
    }

    while((str_len = read(clnt_sock, msg, sizeof(msg))) != 0){
        msg[str_len]='\0';
        snprintf(send_msg, sizeof(send_msg), "%s: %s", clnt_nickname[idx], msg);
        
        for(i=0; i < clnt_id; i++){
            if(clnt_sock != clnt_socks[i]){ //나에게 메시지를 보낸사람은 제외한다는 의미
                write(clnt_socks[i], send_msg, strlen(send_msg));
            }
        }
    }
    for(idx = 0; idx < clnt_id; idx++){
        if(clnt_sock == clnt_socks[idx]){
            while(idx++ < clnt_id-1){
                clnt_socks[idx] = clnt_socks[idx+1];//한칸씩 앞으로 땡겨서 지워진 자리를 채움
                break;
            }
        }
    }
    clnt_id--;
    close(clnt_sock);
    return NULL;
}

int main(int argc, char* argv[]){
    int serv_sock, clnt_sock;
    pthread_t t_id;
    //전화번호 부여하듯이 주소를 부여하는 과정
    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    //얼마만큼의 정보를 가지고 있는지?
    unsigned int clnt_addr_size;

    if(argc != 2){
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    //PF_INET: IPv4 인터넷 프로토콜, SOCK_STREAM: TCP, 0: 프로토콜 자동선택
    //SOCK_DGRAM: UDP, SOCK_RAW: RAW Socket
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP주소 자동, 원래는 127.0.0.1처럼 ip주소가 들어감.
    //loopback address: 자기 자신의 인터페이스를 확인하는 용도로 쓰임
    //INADDR_ANY: 내 ip주소를 자동으로 할당하여 넣으라는 의미와 같다.
    //htonl: host to network long, 4바이트를 꺼내서 바꾼다.

    serv_addr.sin_port = htons(atoi(argv[1])); //포트번호
    //htons: host to network short, 2바이트를 꺼내서 바꾼다.

    /*bind: 결합*/
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");
    /*listen: 대기, 5의 뜻: 5명까지만 처리가능*/
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    clnt_addr_size = sizeof(clnt_addr);
    while(1){   //서버는 항시 가동되어야 한다.
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1)
            error_handling("accept() error");

        char name[20];
        read(clnt_sock, name, sizeof(name));

        strcpy(clnt_nickname[clnt_id], name);
        clnt_socks[clnt_id++] = clnt_sock;
        
        printf("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
        //입장 메시지
        char msg[100];
        sprintf(msg, "%s님이 입장하셨습니다.\n", name);
        for(int i = 0; i < clnt_id; i++){
            write(clnt_socks[i], msg, strlen(msg));
        }
        //쓰레드 생성
        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock); //다중서버의 핵심이 되는코드
        pthread_detach(t_id); //쓰레드 종료시 자동으로 자원해제
        printf("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
        
    }
    return 0;

}
