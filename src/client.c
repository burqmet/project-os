#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include "common.h"

int main(int argc, char *argv[]) {

    // จน. argument ต้องเป็น 2 (ไฟล์ + id)
    if (argc != 2) {
        printf("Usage: ./client <client_id>\n");
        return 1;
    }

    int client_id = atoi(argv[1]);

    // เปิด request queue ที่ server สร้างไว้ส่งข้อมูล
    mqd_t request_queue = mq_open(REQUEST_QUEUE, O_WRONLY);
    if (request_queue == (mqd_t)-1) {
        perror("mq_open request queue");
        return 1;
    }

    // ตั้งชื่อ response queue ของแต่ละ client
    char response_queue_name[64];
    snprintf(
        response_queue_name,
        sizeof(response_queue_name),
        RESPONSE_QUEUE,
        client_id
    );

    // ลบ response queue เก่า
    mq_unlink(response_queue_name);

    struct mq_attr response_attr;
    // กำหนดคุณสมบัติของ response queue
    response_attr.mq_flags = 0;
    response_attr.mq_maxmsg = RESPONSE_MAXMSG;
    response_attr.mq_msgsize = sizeof(Response);
    response_attr.mq_curmsgs = 0;

    // เปิดหรือสร้าง (ถ้ายังไม่มี) response queue
    mqd_t response_queue = mq_open(
        response_queue_name,
        O_RDONLY | O_CREAT,
        0666,
        &response_attr
    );
    if (response_queue == (mqd_t)-1) {
        perror("mq_open response queue");
        mq_close(request_queue);
        return 1;
    }

    // เริ่มทำงานกับ user
    char input[64];
    while (1) {
        printf("\nClient %d > ", client_id);
        fflush(stdout);

        // รับ command จาก keyboard
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // ลบ '\n' ที่ fgets อ่านเข้ามา
        input[strcspn(input, "\n")] = '\0';

        // เตรียม Request
        Request request;
        request.client_id = client_id;
        request.seat_id = -1;       // ใช้ -1 สำหรับ command ที่ไม่ได้ระบุที่นั่ง เช่น LIST

        // คำสั่ง LIST: ไม่ต้องมี seat id
        if (strcmp(input, "LIST") == 0) {
            strcpy(request.cmd, "LIST");
        }
        // คำสั่ง QUIT: ไม่ต้องส่ง request ไป server
        else if (strcmp(input, "QUIT") == 0) {
            break;
        }
        // คำสั่ง STATUS
        else if (sscanf(input, "STATUS %d", &request.seat_id) == 1) {
            strcpy(request.cmd, "STATUS");
        }
        // คำสั่ง RESERVE
        else if (sscanf(input, "RESERVE %d", &request.seat_id) == 1) {
            strcpy(request.cmd, "RESERVE");
        }
        // คำสั่ง CANCEL
        else if (sscanf(input, "CANCEL %d", &request.seat_id) == 1) {
            strcpy(request.cmd, "CANCEL");
        }
        // คำสั่งอื่น ๆ
        else {
            printf("Invalid command.\n");
            printf("Available commands:\n");
            printf("  LIST\n");
            printf("  STATUS <seat_id>\n");
            printf("  RESERVE <seat_id>\n");
            printf("  CANCEL <seat_id>\n");
            printf("  QUIT\n");
            continue;
        }

        // ส่ง request เข้าคิวไปให้ worker ใน server
        if (mq_send(request_queue, (char *)&request, sizeof(request), 0) == -1) {
            perror("mq_send");
            continue;
        }
        printf("Sent: %s", input);    

        // รอรับ response จาก server
        Response response;
        if (mq_receive(response_queue, (char *)&response, sizeof(response), NULL) == -1) {
            perror("mq_receive");
            continue;
        }
        printf("\n%s\n", response.text);

    }
    // ปิด Queue
    mq_close(request_queue);
    mq_close(response_queue);
    mq_unlink(response_queue_name);

    printf("Client %d exited.\n", client_id);

    return 0;
}