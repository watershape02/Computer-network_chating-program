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
#include <ctype.h>
#include <fstream>
#include <string>

#define BUFSIZE 1024
#define MAX_CLIENT 64
#define NICKNAME_SIZE 20
//소켓할당==전화기를 할당한다.
int clnt_socks[MAX_CLIENT];
char clnt_nickname[MAX_CLIENT][NICKNAME_SIZE];
int clnt_id=0;
pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER;

void write_all(int sock, const char* msg, size_t len) {
    size_t sent = 0;
    while(sent < len) {
        ssize_t write_len = write(sock, msg + sent, len - sent);
        if(write_len <= 0) {
            break;
        }
        sent += write_len;
    }
}

void write_all(int sock, const std::string& msg) {
    write_all(sock, msg.c_str(), msg.size());
}

int find_client_index(int clnt_sock) {
    for(int i = 0; i < clnt_id; i++) {
        if(clnt_sock == clnt_socks[i]) {
            return i;
        }
    }
    return -1;
}

void broadcast_msg(const char* msg, int except_sock) {
    pthread_mutex_lock(&mutx);
    for(int i = 0; i < clnt_id; i++) {
        if(clnt_socks[i] != except_sock) {
            write_all(clnt_socks[i], msg, strlen(msg));
        }
    }
    pthread_mutex_unlock(&mutx);
}

void broadcast_msg(const std::string& msg, int except_sock) {
    pthread_mutex_lock(&mutx);
    for(int i = 0; i < clnt_id; i++) {
        if(clnt_socks[i] != except_sock) {
            write_all(clnt_socks[i], msg);
        }
    }
    pthread_mutex_unlock(&mutx);
}

int find_client_index_by_nickname(const char* nickname) {
    for(int i = 0; i < clnt_id; i++) {
        if(strcmp(clnt_nickname[i], nickname) == 0) {
            return i;
        }
    }
    return -1;
}

bool is_emoticon_header(const std::string& line) {
    size_t pos = 0;

    while(pos < line.size() && isdigit((unsigned char)line[pos])) {
        pos++;
    }

    if(pos == 0 || pos >= line.size() || line[pos] != '.') {
        return false;
    }

    return true;
}

std::string get_emoticon_name(const std::string& line) {
    size_t pos = line.find('.');

    if(pos == std::string::npos) {
        return "";
    }

    pos++;
    while(pos < line.size() && line[pos] == ' ') {
        pos++;
    }

    return line.substr(pos);
}

int get_emoticon_number(const std::string& line) {
    size_t pos = line.find('.');

    if(pos == std::string::npos) {
        return -1;
    }

    return atoi(line.substr(0, pos).c_str());
}

bool open_emoticon_file(std::ifstream& file) {
    file.open("quakcheol.txt");

    if(!file.is_open()) {
        file.open("socket_chating_program/quakcheol.txt");
    }

    return file.is_open();
}

bool read_emoticon_art(int number, std::string& name, std::string& art) {
    std::ifstream file;

    if(!open_emoticon_file(file)) {
        return false;
    }

    std::string line;
    bool collecting = false;

    name.clear();
    art.clear();
    while(std::getline(file, line)) {
        if(is_emoticon_header(line)) {
            if(collecting) {
                break;
            }

            if(get_emoticon_number(line) == number) {
                name = get_emoticon_name(line);
                collecting = true;
            }
            continue;
        }

        if(collecting) {
            art += line;
            art += "\n";
        }
    }

    return collecting && !art.empty();
}

std::string make_emoticon_list_msg() {
    std::ifstream file;

    if(!open_emoticon_file(file)) {
        return ">> 이모티콘 파일을 열 수 없습니다.\n";
    }

    std::string line;
    std::string list_msg = ">> 사용 가능한 이모티콘 목록\n";

    while(std::getline(file, line)) {
        if(is_emoticon_header(line)) {
            list_msg += line;
            list_msg += "\n";
        }
    }

    return list_msg;
}

void error_handling(const char* message){
    fputs(message, stderr); //표준 에러형식으로 메시지 출력
    fputc('\n',stderr);
    exit(-1);
}

void* handle_clnt(void* arg){
    int clnt_sock = *((int*)arg);
    free(arg);
    int str_len = 0;
    char msg[BUFSIZE+1]={0,};
    char send_msg[BUFSIZE + 30];
    char current_name[NICKNAME_SIZE];
    int idx;
    bool announced_exit = false;

    while((str_len = read(clnt_sock, msg, BUFSIZE)) > 0){
        msg[str_len]='\0';

        pthread_mutex_lock(&mutx);
        idx = find_client_index(clnt_sock);
        if(idx == -1) {
            pthread_mutex_unlock(&mutx);
            break;
        }
        strcpy(current_name, clnt_nickname[idx]);
        pthread_mutex_unlock(&mutx);

        if(strcmp(msg, "/quit\n") == 0) {
            printf("%s님이 퇴장하셨습니다.\n", current_name);
            snprintf(send_msg, sizeof(send_msg), "%s님이 퇴장하셨습니다.\n", current_name);
            broadcast_msg(send_msg, clnt_sock);
            announced_exit = true;
            break; // 클라이언트 연결 종료
        }
        if(strcmp(msg, "/who\n") == 0) {
            std::string who_msg;

            pthread_mutex_lock(&mutx);
            who_msg = ">> 현재 채팅방 인원: " + std::to_string(clnt_id) + "/" + std::to_string(MAX_CLIENT) + "\n";
            who_msg += ">> 접속자 목록\n";
            for(int i = 0; i < clnt_id; i++) {
                who_msg += "   - ";
                who_msg += clnt_nickname[i];
                who_msg += "\n";
            }
            pthread_mutex_unlock(&mutx);

            write_all(clnt_sock, who_msg);
            continue;
        }
        if(strcmp(msg, "/emoticonlist\n") == 0) {
            write_all(clnt_sock, make_emoticon_list_msg());
            continue;
        }
        if(strncmp(msg, "/rename ", 8) == 0){
            char old_name[NICKNAME_SIZE];
            char new_name[NICKNAME_SIZE];

            strncpy(new_name, msg + 8, NICKNAME_SIZE - 1);
            new_name[NICKNAME_SIZE - 1] = '\0';
            new_name[strcspn(new_name, "\n")] = '\0';

            if(strlen(new_name) == 0) {
                continue;
            }

            pthread_mutex_lock(&mutx);
            idx = find_client_index(clnt_sock);
            if(idx == -1) {
                pthread_mutex_unlock(&mutx);
                break;
            }
            strcpy(old_name, clnt_nickname[idx]);
            strcpy(clnt_nickname[idx], new_name);
            pthread_mutex_unlock(&mutx);

            snprintf(send_msg, sizeof(send_msg), "%s님이 %s(으)로 닉네임을 변경했습니다.\n",
                    old_name,
                    new_name);

            printf("%s", send_msg);
            broadcast_msg(send_msg, -1);
            continue;
        }
        if(strncmp(msg, "/whisper ", 9) == 0) {
            char target_name[NICKNAME_SIZE];
            char whisper_body[BUFSIZE];
            char* body_start;
            int target_idx;
            int target_sock = -1;
            char target_msg[BUFSIZE + NICKNAME_SIZE + 40];
            char sender_msg[BUFSIZE + NICKNAME_SIZE + 40];

            body_start = strchr(msg + 9, ' ');
            if(body_start == NULL) {
                snprintf(send_msg, sizeof(send_msg), ">> 사용법: /whisper <닉네임> <메시지>\n");
                write(clnt_sock, send_msg, strlen(send_msg));
                continue;
            }

            *body_start = '\0';
            strncpy(target_name, msg + 9, NICKNAME_SIZE - 1);
            target_name[NICKNAME_SIZE - 1] = '\0';

            strncpy(whisper_body, body_start + 1, BUFSIZE - 1);
            whisper_body[BUFSIZE - 1] = '\0';
            whisper_body[strcspn(whisper_body, "\n")] = '\0';

            if(strlen(target_name) == 0 || strlen(whisper_body) == 0) {
                snprintf(send_msg, sizeof(send_msg), ">> 귓속말 대상과 내용을 모두 입력하세요.\n");
                write(clnt_sock, send_msg, strlen(send_msg));
                continue;
            }

            pthread_mutex_lock(&mutx);
            target_idx = find_client_index_by_nickname(target_name);
            if(target_idx != -1) {
                target_sock = clnt_socks[target_idx];
            }
            pthread_mutex_unlock(&mutx);

            if(target_sock == -1) {
                snprintf(send_msg, sizeof(send_msg), ">> %s님을 찾을 수 없습니다.\n", target_name);
                write(clnt_sock, send_msg, strlen(send_msg));
                continue;
            }

            snprintf(target_msg, sizeof(target_msg), "[귓속말] %s님: %s\n", current_name, whisper_body);
            snprintf(sender_msg, sizeof(sender_msg), "[귓속말 전송] %s님에게: %s\n", target_name, whisper_body);

            write(target_sock, target_msg, strlen(target_msg));
            write(clnt_sock, sender_msg, strlen(sender_msg));
            continue;
        }
        if(strncmp(msg, "/emoticon ", 10) == 0) {
            char emoticon_number_str[20];
            int emoticon_number;
            bool valid_number = true;
            std::string emoticon_name;
            std::string art;
            std::string emoticon_msg;

            strncpy(emoticon_number_str, msg + 10, sizeof(emoticon_number_str) - 1);
            emoticon_number_str[sizeof(emoticon_number_str) - 1] = '\0';
            emoticon_number_str[strcspn(emoticon_number_str, " \n")] = '\0';

            if(strlen(emoticon_number_str) == 0) {
                snprintf(send_msg, sizeof(send_msg), ">> 이모티콘 번호를 입력하세요.\n");
                write_all(clnt_sock, send_msg, strlen(send_msg));
                continue;
            }

            for(size_t i = 0; i < strlen(emoticon_number_str); i++) {
                if(!isdigit((unsigned char)emoticon_number_str[i])) {
                    valid_number = false;
                    break;
                }
            }

            if(!valid_number) {
                snprintf(send_msg, sizeof(send_msg), ">> 이모티콘 번호는 숫자로 입력하세요.\n");
                write_all(clnt_sock, send_msg, strlen(send_msg));
                continue;
            }

            emoticon_number = atoi(emoticon_number_str);
            if(!read_emoticon_art(emoticon_number, emoticon_name, art)) {
                snprintf(send_msg, sizeof(send_msg), ">> %d번 이모티콘이 없습니다.\n", emoticon_number);
                write_all(clnt_sock, send_msg, strlen(send_msg));
                continue;
            }

            emoticon_msg = current_name;
            emoticon_msg += "님이 ";
            emoticon_msg += std::to_string(emoticon_number);
            emoticon_msg += "번 이모티콘 '";
            emoticon_msg += emoticon_name;
            emoticon_msg += "'을 보냈습니다.\n\n";
            emoticon_msg += art;

            broadcast_msg(emoticon_msg, -1);
            continue;
        }
        snprintf(send_msg, sizeof(send_msg), "%s: %s", current_name, msg);
        
        broadcast_msg(send_msg, clnt_sock);
    }

    pthread_mutex_lock(&mutx);
    idx = find_client_index(clnt_sock);
    if(idx != -1) {
        strcpy(current_name, clnt_nickname[idx]);
        for(int i = idx; i < clnt_id - 1; i++) {
            clnt_socks[i] = clnt_socks[i + 1];//한칸씩 앞으로 땡겨서 지워진 자리를 채움
            strcpy(clnt_nickname[i], clnt_nickname[i + 1]);
        }
        clnt_id--;
    }
    pthread_mutex_unlock(&mutx);

    if(!announced_exit && idx != -1) {
        printf("%s님이 퇴장하셨습니다.\n", current_name);
        snprintf(send_msg, sizeof(send_msg), "%s님이 퇴장하셨습니다.\n", current_name);
        broadcast_msg(send_msg, clnt_sock);
    }

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

        char name[NICKNAME_SIZE];
        read(clnt_sock, name, sizeof(name));
        name[NICKNAME_SIZE - 1] = '\0';

        pthread_mutex_lock(&mutx);
        strcpy(clnt_nickname[clnt_id], name);
        clnt_socks[clnt_id++] = clnt_sock;
        pthread_mutex_unlock(&mutx);
        
        printf("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
        //입장 메시지
        char msg[100];
        snprintf(msg, sizeof(msg), "%s님이 입장하셨습니다.\n", name);
        broadcast_msg(msg, -1);
        //쓰레드 생성
        int* clnt_sock_ptr = (int*)malloc(sizeof(int));
        *clnt_sock_ptr = clnt_sock;
        pthread_create(&t_id, NULL, handle_clnt, (void*)clnt_sock_ptr); //다중서버의 핵심이 되는코드
        pthread_detach(t_id); //쓰레드 종료시 자동으로 자원해제
        printf("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
        
    }
    return 0;

}
