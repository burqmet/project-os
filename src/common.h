#ifndef COMMON_H
#define COMMON_H

/* request queue ที่ client ทุกตัวใช้ส่งคำสั่งให้ Server
response queue ของแต่ละ client */
#define REQUEST_QUEUE "/reserve_seat_request"
#define RESPONSE_QUEUE "/reserve_seat_response_%d"

// จำนวนข้อความสูงสุดที่สามารถเก็บได้
#define REQUEST_MAXMSG 10
#define RESPONSE_MAXMSG 4

#define NUM_CLIENT 5
#define NUM_WORKERS 3
#define NUM_SEATS 20

#define MAX_COMMAND 16
#define MAX_RESPONSE 1024

typedef enum {
    AVAILABLE,
    RESERVED
} SeatStatus;

typedef enum {
    SUCCESS,
    FAILED
} ResponseStatus;

typedef enum {
    NO_MUTEX = 0,
    USE_MUTEX = 1
} MutexStatus;

typedef struct {
    int  client_id;
    int  seat_id;
    char cmd[MAX_COMMAND];
} Request;

typedef struct {
    int  client_id;
    int  seat_id;
    char cmd[MAX_COMMAND];
    ResponseStatus response_status;
    char text[MAX_RESPONSE];
} Response;

#endif