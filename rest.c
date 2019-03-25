#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define MAX_PACKET 512
#define UTC 0
#define LOCAL 1
#define UNIX 3
#define INTERNET 4

int check_flag(char *flag, char *check, int pos, int argc);
void print_response(char *resp, int len);

int main(int argc, char * argv[])
{
    int sock;
    struct sockaddr_in src_sock_addr;
    struct sockaddr_in dest_sock_addr;
    struct hostent *host;
    char *request;
    socklen_t addr_len = MAX_PACKET;
    int request_len;
    char post_body_len[4];

    const char *domain = "www.iu3.bmstu.ru";
    const int port = 8090;
    int time_t = LOCAL;
    int time_f = 0;
    char post_body[30] = "\r\n\r\n";
    char *server_response = NULL;
    int8_t is_get = 1;
    
    //Данной строки достаточно для хранения запроса под наши задачи
    request = (char*)malloc(512 * sizeof(char));
    request[0] = '\0';

    //Проверка флагов
    for (int i = 1; i < argc; i++){
        if (check_flag("-t", argv[i], i + 1, argc)){
            i++;
            if (strcmp(argv[i], "utc") == 0) time_t = UTC;
            else if (strcmp(argv[i], "local") == 0) time_t = LOCAL;
        }
        else if (check_flag("-f", argv[i], i + 1, argc))
        {
            i++;
            if (strcmp(argv[i], "unix") == 0) time_f = UNIX;
            else if (strcmp(argv[i], "internet") == 0) time_f = INTERNET;
        }
        else if (check_flag("-p", argv[i], i, argc)) is_get = 0;
    }

    /*
    Создаем низкоуровневый сокет
    AF_INET - сокет для работы с сетью
    SOCK_STREAM - протокол TCP
    */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) printf("Не удалось открыть соединение %d, %d\n", sock, errno);
    else
    {
        //Получаем адрес удаленного хоста по его имени
        host = gethostbyname(domain);
        //Получилось получить IP адрес
        if (host)
        {
            //Задаем параметры для установки соединения
            memset(&src_sock_addr,
                    0,
                    sizeof(src_sock_addr));
            src_sock_addr.sin_family = AF_INET;
            memcpy(&src_sock_addr.sin_addr.s_addr,
                    host->h_addr,
                    host->h_length);
            src_sock_addr.sin_port = htons(port);

            //Создаем соединение
            if (connect(sock,
                        (struct sockaddr *)&src_sock_addr,
                        sizeof(src_sock_addr)) < 0)
            {
                printf("Не удалось создать соединение\n");
            }

            //Составляем GET запрос
            if (is_get)
            {
                strcpy(request,
                        "GET /WebApi/time?");

                if (time_t == UTC) strcat(request, "type=utc");
                else if (time_t == LOCAL) strcat(request, "type=local");

                if (time_f == UNIX) strcat(request, "&format=unix");
                else if (time_f == INTERNET) strcat(request, "&format=internet");

                strcat(request,
                        " HTTP/1.0\r\n");
                strcat(request,
                        "Host: ");
                strcat(request,
                        domain);
                strcat(request,
                        "\r\n\r\n");
            }
            else //Составляем POST запрос
            {
                //Прописываем начало POST запроса
                strcpy(request, "POST /WebApi/time HTTP/1.0\r\n"
                                "Content-Type:application/x-www-form-urlencoded\r\n"
                                "Content-Length:");

                if (time_t == UTC) strcat(post_body, "type=utc");
                else if (time_t == LOCAL) strcat(post_body, "type=local");

                if (time_f == UNIX) strcat(post_body, "&format=unix");
                else if (time_f == INTERNET) strcat(post_body, "&format=internet");

                //Вычисляем длину строки с параметрами (за вычетом символов \r\n\r\n)
                sprintf(post_body_len, "%zu", strlen(post_body) - 4);
                strcat(request, post_body_len);
                strcat(request, post_body);
            }

            //Отправляем запрос
            request_len = (int)sendto(sock,
                                        request,
                                        (sizeof(char) * strlen(request)),
                                        0,
                                        (struct sockaddr *)&src_sock_addr,
                                        sizeof(src_sock_addr));

            //Проверим на ошибки
            if (errno) printf("При отправке произошла ошибка номер %d", errno);
            else
            {
                printf("Успешно отправлено %d байт\n", request_len);

                //Выделяем память для ответа, больше размер не потребуется
                server_response = (char *)malloc(MAX_PACKET);
                
                //Получаем ответ
                request_len = (int)recvfrom(sock,
                                            server_response,
                                            MAX_PACKET,
                                            0,
                                            (struct sockaddr *)&dest_sock_addr,
                                            &addr_len);
                
                //Если все в порядке выведем тело ответа
                if (request_len > 0) print_response(server_response, request_len);
                free(server_response);
            }
        }
        else printf("Невозможно разрешить DNS имя сервера.\n");
        if (close(sock) == -1) printf("Не удалось закрыть сокет, ошибка: %d", errno);
    }

    free(request);
    return 0;
}

//Функция вывода тела ответа от сервера
void print_response(char *response, int resp_len)
{
    int resp_cnt = 0;
    int pos_code_start;
    char status[23];
    
    //Пропустим символы до начала статуса ответа
    for (;
            response[resp_cnt] != ' ' && resp_cnt < resp_len;
            resp_cnt++);

    pos_code_start = resp_cnt;

    //Сохраним в отдельный массив статус
    for (;
            response[resp_cnt] != '\r' && resp_cnt < resp_len;
            resp_cnt++)
    {
        status[resp_cnt - pos_code_start] = response[resp_cnt];
    }
    status[resp_cnt - pos_code_start] = '\0';
    
    /*
     Если статус 200, то все в порядке, можно продолжить обработку
     Если нет, то напишем пользователю код HTTP ошибки
     */
    if (strcmp(status, " 200 OK") == 0)
    {
        //Ищем конец заголовков
        for (;
                !(response[resp_cnt] == '\r' &&
                    response[resp_cnt + 1] == '\n' &&
                    response[resp_cnt + 2] == '\r' &&
                    response[resp_cnt + 3] == '\n') &&
                    resp_cnt + 3 < resp_len;
                resp_cnt++);
        
        resp_cnt += 4;

        //Печатаем тело ответа
        printf("HTTP/1.0 200 OK\n");
        printf("\nЗапрошенное время:\n%s\n\n", response + resp_cnt);
    }
    else 
    {
        resp_cnt = 0;
        printf("%s\n", status);
        
        for (; resp_cnt < resp_len; resp_cnt++)
        {
            if (response[resp_cnt] == '\r') continue;
            else printf("%c", response[resp_cnt]);
        }
    }
}

//Функция для минимазации кода по проверки входных флагов
int check_flag(char *flag, char *check, int pos, int argc)
{
    return (strcmp(check, flag) == 0) && pos < argc;
}
