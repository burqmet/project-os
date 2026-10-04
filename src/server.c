#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include "common.h"

typedef struct {
    int id;
    SeatStatus seat_status;
    int client_id;
} Seat;

Seat seats[NUM_SEATS];

// ประกาศ mutex และตั้งค่าเริ่มต้น
pthread_mutex_t reservation_mutex = PTHREAD_MUTEX_INITIALIZER;

mqd_t request_queue;

int worker_ids[NUM_WORKERS];
pthread_t threads[NUM_WORKERS];

// เก็บจน worker ที่จะสร้างตามแต่ละ  experiment
int num_active_workers;
MutexStatus use_mutex;
// ใช้ควบคุมการทำงานของ Server
volatile sig_atomic_t server_running = 1;

// เตรียม shared resource ตั้งค่าเริ่มต้น
void initialize_seats() {
    for (int i = 0; i < NUM_SEATS; i++){
        seats[i].id = i + 1;
        seats[i].seat_status = AVAILABLE;
        seats[i].client_id = -1;
    }
}

// ตั้งค่า worker id ระบุ Worker แต่ละตัว
void initialize_worker_ids() {
    for (int i = 0; i < NUM_WORKERS; i++){
        worker_ids[i] = i + 1;
    }
}

// หน่วงเวลาแบบสุ่ม 50–500 ms ขยาย Race Window ให้เห็น Race Condition ง่ายขึ้น
void random_delay() {
    int delay_ms = 50 + rand() % 451;
    usleep(delay_ms * 1000);
}

// สร้างและแสดง log การทำงาน Worker
void log_message(int worker_id, int client_id, const char *cmd, int seat_id, const char *message) {
    struct timespec time;
    // เก็บเวลาปัจจุบันลง time
    clock_gettime(CLOCK_REALTIME, &time);

    struct tm *time_info = localtime(&time.tv_sec);

    printf("[%02d:%02d:%02d.%03ld] [Worker %d] [Client %d] | %s | Seat %d | %s\n",
        time_info->tm_hour,
        time_info->tm_min,
        time_info->tm_sec,
        time.tv_nsec / 1000000,
        worker_id,
        client_id,
        cmd,
        seat_id,
        message
    );
    // แสดง log ทันที
    fflush(stdout);
}

// ปิด server
void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        server_running = 0;
    }
}

void *worker(void *arg);

// สร้าง worker thread
void create_workers() {
    for (int i = 0; i < num_active_workers; i++) {
        pthread_create(
            &threads[i],
            NULL,
            worker,
            &worker_ids[i]
        );
    }
}

// การทำงานของ worker thread
void *worker(void *arg) {
    int worker_id = *(int *)arg;
    Request request;

    while (server_running) {
        // รับ request จาก request queue
        ssize_t bytes_received = mq_receive(
            request_queue,
            (char *)&request,
            sizeof(request),
            NULL
        );
        if (bytes_received == -1) {
            if (!server_running) {
                break;
            }
            // ไม่มี message ใน queue
            if (errno == EAGAIN) {
                usleep(10000);
                continue;
            }
            perror("mq_receive");
            continue;
        }
        // เตรียม response กำหนดค่า
        Response response;
        response.client_id = request.client_id;
        response.seat_id = request.seat_id;
        strcpy(response.cmd, request.cmd);
        response.response_status = FAILED;
        response.text[0] = '\0';
        
        // LIST: ขอดูข้อมูลทุกที่นั่ง
        if (strcmp(request.cmd, "LIST") == 0) {
            response.seat_id = -1;
            response.response_status = SUCCESS;

            if (use_mutex == USE_MUTEX) {
                pthread_mutex_lock(&reservation_mutex);
                log_message(
                    worker_id,
                    request.client_id,
                    request.cmd,
                    response.seat_id,
                    "ENTER CRITICAL SECTION"
                );
            } else {
                log_message(
                    worker_id,
                    request.client_id,
                    request.cmd,
                    response.seat_id,
                    "CHECK START"
                );
            }

            strcpy(response.text, "Seat Status: \n");

            for (int i = 0; i < NUM_SEATS; i++) {
                char seat_info[64];

                if (seats[i].seat_status == AVAILABLE) {
                    snprintf(
                        seat_info,
                        sizeof(seat_info),
                        "Seat %d: AVAILABLE\n", seats[i].id
                    );
                } else {
                    snprintf(
                        seat_info,
                        sizeof(seat_info),
                        "Seat %d: RESERVED by Client %d\n", seats[i].id, seats[i].client_id
                    );
                }
                strcat(response.text, seat_info);
            }
            if (use_mutex == USE_MUTEX) {
                log_message(
                    worker_id,
                    request.client_id,
                    request.cmd,
                    response.seat_id,
                    "EXIT CRITICAL SECTION"
                );
                pthread_mutex_unlock(&reservation_mutex);
            }  else {
                log_message(
                    worker_id,
                    request.client_id,
                    request.cmd,
                    response.seat_id,
                    "CHECK END"
                );
            }
        }

        // STATUS: ขอดูสถานะที่นั่งนั้น ๆ
        else if (strcmp(request.cmd, "STATUS") == 0) {
            int seat_index = request.seat_id - 1;

            if (request.seat_id < 1 || request.seat_id > NUM_SEATS) {
                strcpy(response.text, "Invalid seat ID.");
            } else {
                if (use_mutex == USE_MUTEX) {
                    pthread_mutex_lock(&reservation_mutex);
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "ENTER CRITICAL SECTION"
                    );
                } else {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "CHECK START"
                    );
                }
                if (seats[seat_index].seat_status == AVAILABLE) {
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d: AVAILABLE", request.seat_id
                    );
                } else {
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d: RESERVED by Client %d", request.seat_id, seats[seat_index].client_id
                    );
                }
                response.response_status = SUCCESS;
                if (use_mutex == USE_MUTEX) {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "EXIT CRITICAL SECTION"
                    );
                    pthread_mutex_unlock(&reservation_mutex);
                } else {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "CHECK END"
                    );
                }
            }
        }

        // RESERVE: ขอจองที่นั่ง
        else if (strcmp(request.cmd, "RESERVE") == 0) {
            int seat_index = request.seat_id - 1;

            if (request.seat_id < 1 || request.seat_id > NUM_SEATS) {
                strcpy(response.text, "Invalid seat ID.");
            } else {
                if (use_mutex == USE_MUTEX) {
                    pthread_mutex_lock(&reservation_mutex);
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "ENTER CRITICAL SECTION"
                    );
                } else {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "CHECK START"
                    );
                }
                if (seats[seat_index].seat_status == AVAILABLE) {
                    random_delay();
                    seats[seat_index].seat_status = RESERVED;
                    seats[seat_index].client_id = request.client_id;
                    response.response_status = SUCCESS;
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d reserved successfully.", request.seat_id
                    );
                } else {
                    response.response_status = FAILED;
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d is already reserved by Client %d.",
                        request.seat_id,
                        seats[seat_index].client_id
                    );
                }
                if (use_mutex == USE_MUTEX) {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "EXIT CRITICAL SECTION"
                    );
                    pthread_mutex_unlock(&reservation_mutex);
                } else {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "CHECK/UPDATE END"
                    );
                }
            }
        }

        // CANCEL: ยกเลิกการจอง
        else if (strcmp(request.cmd, "CANCEL") == 0) {
            int seat_index = request.seat_id - 1;

            if (request.seat_id < 1 || request.seat_id > NUM_SEATS) {
                strcpy(response.text, "Invalid seat ID.");
            } else {
                if (use_mutex == USE_MUTEX) {
                    pthread_mutex_lock(&reservation_mutex);
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "ENTER CRITICAL SECTION"
                    );
                } else {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "CHECK START"
                    );
                }
                if (seats[seat_index].seat_status == AVAILABLE) {
                    response.response_status = FAILED;
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d is not reserved.", request.seat_id
                    );
                } else if (seats[seat_index].client_id != request.client_id) {
                    response.response_status = FAILED;
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d is reserved by another client.", request.seat_id
                    );
                } else {
                    seats[seat_index].seat_status = AVAILABLE;
                    seats[seat_index].client_id = -1;
                    response.response_status = SUCCESS;
                    snprintf(
                        response.text,
                        sizeof(response.text),
                        "Seat %d cancelled successfully.", request.seat_id
                    );
                }
                if (use_mutex == USE_MUTEX) {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "EXIT CRITICAL SECTION"
                    );

                    pthread_mutex_unlock(&reservation_mutex);
                } else {
                    log_message(
                        worker_id,
                        request.client_id,
                        request.cmd,
                        request.seat_id,
                        "CHECK END"
                    );
                }
            }
        }

        char response_queue_name[64];
        snprintf(
            response_queue_name,
            sizeof(response_queue_name),
            RESPONSE_QUEUE,
            response.client_id
        );
        // เปิด response queue
        mqd_t response_queue = mq_open(
            response_queue_name,
            O_WRONLY
        );
        if (response_queue == (mqd_t)-1) {
            perror("mq_open response queue");
            continue;
        }
        // ส่ง response ให้ client
        if (mq_send(response_queue, (char *)&response, sizeof(response), 0) == -1) {
            perror("mq_send");
        }
        // ปิด queue หลังส่งเสร็จ
        mq_close(response_queue);
    }    
    return NULL;
}

// สร้าง request queue รับคำสั่งจาก Client
void create_request_queue() {
    struct mq_attr request_attr;
    // กำหนดคุณสมบัติ message queue
    request_attr.mq_flags = 0;
    request_attr.mq_maxmsg = REQUEST_MAXMSG;
    request_attr.mq_msgsize = sizeof(Request);
    request_attr.mq_curmsgs = 0;

    // เปิด Request Queue
    request_queue = mq_open(
        REQUEST_QUEUE,
        O_RDONLY | O_CREAT | O_NONBLOCK,
        0666,
        &request_attr
    );
    if (request_queue == (mqd_t)-1) {
        perror("mq=open");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // check experiment
    if (argc != 2) {
        printf("Usage: ./server <exp>\n");
        printf("Exp 1: 1 Worker, No Mutex\n");
        printf("Exp 2: 3 Workers, No Mutex\n");
        printf("Exp 3: 3 Workers, Use Mutex\n");
        return 1;
    }

    int exp = atoi(argv[1]);

    // กำหนดจำนวน Worker และการใช้ Mutex ตาม exp
    if (exp == 1) {
        num_active_workers = 1;
        use_mutex = NO_MUTEX;
    }
    else if (exp == 2) {
        num_active_workers = 3;
        use_mutex = NO_MUTEX;
    }
    else if (exp == 3) {
        num_active_workers = 3;
        use_mutex = USE_MUTEX;
    }
    else {
        printf("Invalid exp.\n");
        printf("Please use exp 1, 2, or 3.\n");
        return 1;
    }

    // กำหนดให้ server หยุด worker
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting server...\n");
    printf("Experiment: %d\n", exp);
    printf("Workers: %d\n", num_active_workers);
    printf("Mutex: %s\n", use_mutex == USE_MUTEX ? "ON" : "OFF");

    srand(time(NULL));

    initialize_seats();
    initialize_worker_ids();
    create_request_queue();
    create_workers();

    // รอ worker ทำงาน
    for (int i = 0; i < num_active_workers; i++) {
        pthread_join(threads[i], NULL);
    }

    // ปิด queue
    mq_close(request_queue);
    mq_unlink(REQUEST_QUEUE);
    pthread_mutex_destroy(&reservation_mutex);

    printf("Server stopped.\n");

    return 0;
}