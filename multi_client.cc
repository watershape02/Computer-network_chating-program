/*
 * [설계 및 구현 내용 주석]
 * 1. 목적: 멀티스레드 기반 TCP 채팅 클라이언트 구현
 * 2. 구조:
 * - 메인 스레드: 사용자의 키보드 입력을 대기하고, 입력된 문자열을 서버로 전송 (Send)
 * - 수신 스레드(recv_thread): 서버로부터 실시간으로 데이터가 오는지 감시하고 화면에 출력 (Receive)
 * 3. 확장 기능 계획: (여기에 본인이 추가할 기능을 주석으로 기록하세요)
 * - 예: 닉네임 기능 추가 예정
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFSIZE 1024
#define NICKNAME_SIZE 32
void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(const char* message);
int process_command(char* msg);

char msg[BUFSIZE];
char nickname[NICKNAME_SIZE];
char color_code[10] = "\033[0m";

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void* thread_return;

    if (argc != 4) {
        printf("Usage: %s <IP> <PORT> <NICKNAME>\n", argv[0]);
        exit(1);
    }
    // 닉네임 저장
    strcpy(nickname, argv[3]); 
    // 1. 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // 2. 연결할 서버 주소 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // 매개변수로 받은 IP 설정
    serv_addr.sin_port = htons(atoi(argv[2]));      // 매개변수로 받은 Port 설정

    // 3. 서버에 연결 요청 (connect)
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    // 연결되면 닉네임을 먼저 전송
    write(sock, nickname, strlen(nickname) + 1);
    
    printf(">> 서버 연결 성공! 채팅을 시작하세요. (종료하려면 q 입력)\n");

    // 4. 송신/수신을 위한 독립적인 스레드 각각 생성
    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);

    // 5. 스레드가 종료될 때까지 메인 함수가 끝나지 않고 대기
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    close(sock);
    return 0;
}

// [송신 전담 스레드 함수]
void* send_msg(void* arg) {
    int sock = *((int*)arg);
    while (1) {
        fgets(msg, BUFSIZE, stdin);
        
        // 종료 조건 (q나 Q 입력 시 탈출)
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
            close(sock);
            exit(0);
        }
        // /로 시작하는 명령어처리
        //구현 기능1: /color r, /color g, /color b, /color --> 색상 변경 명령어 처리
        if(msg[0] == '/'){
            process_command(msg);
            continue; // 명령어 처리 후 메시지 전송을 건너뜀
        }

        if(msg[0] != '/'){
            printf("\033[1A"); // 커서를 한 줄 위로
            printf("\033[2K"); // 그 줄 삭제
            printf("\r");
            printf("%s", color_code);
            printf("나: %s", msg); // 화면에 내가 보낸 메시지 출력
            printf("\033[0m");
        }
        // 서버로 메시지 전송
        write(sock, msg, strlen(msg));
    }
    return NULL;
}

// [수신 전담 스레드 함수]
void* recv_msg(void* arg) {
    int sock = *((int*)arg);
    char name_msg[BUFSIZE];
    while (1) {
        int str_len = read(sock, name_msg, BUFSIZE - 1);
        if (str_len == -1) {
            return (void*)-1;
        } else if (str_len == 0) {
            // 서버가 연결을 끊은 경우
            printf("\n>> 서버와 연결이 끊어졌습니다.\n");
            exit(0);
        }
        name_msg[str_len] = '\0';
        printf("%s", color_code); // 현재 색상 적용
        fputs(name_msg, stdout);    // 화면에 수신한 메시지 출력
        printf("\033[0m"); 
    }
    return NULL;
}

void error_handling(const char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int process_command(char* msg) {
    // 명령어 처리 로직 구현
    // 예: /color r, /color g, /color b, /color - 등
    if(strcmp(msg, "/color r\n") == 0) {
        strcpy(color_code, "\033[31m");
        printf(">> 글자색을 빨간색으로 변경했습니다.\n");
    } else if(strcmp(msg, "/color g\n") == 0) {
        strcpy(color_code, "\033[32m");
        printf(">> 글자색을 초록색으로 변경했습니다.\n");
    } else if(strcmp(msg, "/color b\n") == 0) {
        strcpy(color_code, "\033[34m");
        printf(">> 글자색을 파란색으로 변경했습니다.\n");
    } else if(strcmp(msg, "/color -\n") == 0) {
        strcpy(color_code, "\033[0m");
        printf(">> 기본색으로 변경했습니다.\n");
    } else if(strncmp(msg, "/color ", 7) == 0) {
        printf(">> 알 수 없는 색상입니다. 사용 가능한 색상: r, g, b, -\n");
    } else if(strcmp(msg, "/help\n") == 0) {
        printf(">> 사용 가능한 명령어:\n");
        printf("   /color r - 글자색을 빨간색으로 변경\n");
        printf("   /color g - 글자색을 초록색으로 변경\n");
        printf("   /color b - 글자색을 파란색으로 변경\n");
        printf("   /color - - 글자색을 기본색으로 변경\n");
        printf("   /help - 명령어 도움말 출력\n");
    }
    else {
        printf(">> 알 수 없는 명령어입니다.\n");
        printf("명령어 도움말은 /help 를 입력하세요.\n");
    }
    return 0; // 명령어 처리 후 성공 여부 반환
}