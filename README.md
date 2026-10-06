# Cinema Seat Reservation System (CSS223 - OS Project)

ระบบจองที่นั่งโรงภาพยนตร์

## สถานการณ์ที่เลือก

**จองที่นั่งโรงภาพยนตร์** — มีที่นั่งทั้งหมด 20 ที่นั่ง (หมายเลข 1–20)

## เทคโนโลยีที่ใช้

- ภาษา C
- POSIX Message Queue
- pthread
- pthread_mutex
- Docker
- Debian

---

## 1. วิธี Build Docker Image

```bash
docker build -t os-server .
```

## 2. วิธี Run Container
สร้างและเปิด Container:
```bash
docker run -d --name cinema-demo --ipc=shareable os-server bash
```
ตรวจสอบว่า Container กำลังทำงาน:
```bash
docker ps
```
เข้าสู่ Container:
```bash
docker exec -it cinema-demo bash
```
หมายเหตุ: Container cinema-demo ใช้สำหรับแชร์ IPC namespace ให้กับ Server และ Client เพื่อให้สามารถสื่อสารผ่าน POSIX Message Queue ได้
 
## 3. วิธีเปิด Server
เข้าไปใน Container:
```bash
docker exec -it cinema-demo bash
```
จากนั้นเปิด Server โดยเลือก Mode ที่ต้องการ

**Mode 1: 1 Worker, ไม่ใช้ Mutex:**
```bash
./server 1
```
**Mode 2: 3 Workers, ไม่ใช้ Mutex:**
```bash
./server 2
```
**Mode 3: 3 Workers, ใช้ Mutex:**
```bash
./server 3
```
แต่ละ Mode ใช้สำหรับการทดลองที่แตกต่างกัน
 
## 4. วิธีเปิด Client หลายตัว
เปิด Terminal ใหม่สำหรับ Client แต่ละตัว โดย Client ต้องใช้ IPC namespace เดียวกับ Server

**Client 1**
```bash
docker run --rm -it --ipc=container:cinema-demo os-server ./client 1
```
**Client 2**
```bash
docker run --rm -it --ipc=container:cinema-demo os-server ./client 2
```
**Client 3**
```bash
docker run --rm -it --ipc=container:cinema-demo os-server ./client 3
```
**Client 4**
```bash
docker run --rm -it --ipc=container:cinema-demo os-server ./client 4
```
**Client 5**
```bash
docker run --rm -it --ipc=container:cinema-demo os-server ./client 5
```
Client แต่ละตัวจะมี Response Queue เป็นของตัวเอง เพื่อรับผลตอบกลับจาก Server
 
## 5. รูปแบบ Message Queue ที่ใช้
- **Request Queue**: `/reserve_seat_request` Client ทุกตัวส่ง Request เข้ามาที่ Queue เดียวกัน และ Worker จะรับ Request จาก Queue นี้
- **Response Queue**: `/reserve_seat_response_<client_id>` Server ส่ง Response กลับไปยัง Client แต่ละตัวผ่าน Queue ที่แยกตาม Client ID
- ข้อความส่งเป็น **binary struct** ผ่าน `mq_send`/`mq_receive` โดยตรง
- POSIX Message Queue ไม่มี `mtype` แบบ System V จึงไม่ต้องมี field `long` นำหน้า struct

**Request structure:**
```c
typedef struct {
    int client_id;
    int seat_id;
    char cmd[MAX_COMMAND];
} Request;
```
**Response structure:**
```c
typedef struct {
    int client_id;
    int seat_id;
    char cmd[MAX_COMMAND];
    ResponseStatus response_status;
    char text[MAX_RESPONSE];
} Response;
```
รายละเอียดเต็มอยู่ใน [`src/common.h`](src/common.h)
 
## 6. คำสั่งที่ Client รองรับ
| คำสั่ง | ตัวอย่าง | ความหมาย |
|---|---|---|
| `LIST` | `LIST` | แสดงสถานะที่นั่งทั้ง 20 ที่ |
| `STATUS <id>` | `STATUS 10` | เช็คสถานะที่นั่งหมายเลข 10 |
| `RESERVE <id>` | `RESERVE 10` | จองที่นั่งหมายเลข 10 |
| `CANCEL <id>` | `CANCEL 10` | ยกเลิกการจองที่นั่งหมายเลข 10 |
| `QUIT` | `QUIT` | ออกจากโปรแกรม client |
 
## 7. วิธีทดลอง Race Condition
 
### วิธีอัตโนมัติ — ใช้ `experiment.sh`
สคริปต์นี้จะ Compile โปรแกรม, เปิด Server, เปิด Client 5 ตัวเพื่อส่ง RESERVE ที่นั่งเดียวกันพร้อมกัน, สรุปผล และปิด Server ให้อัตโนมัติ

**หมายเหตุ:** ต้องรันใน Container ที่ไม่มี Server ตัวอื่นกำลังทำงานอยู่ เพราะ `experiment.sh` จะเปิดและปิด Server ให้โดยอัตโนมัติ
สร้าง Container สำหรับการทดลอง:
```bash
docker run --rm -it os-server
```
จากนั้นรันการทดลองตาม Mode ที่ต้องการ:

**Experiment 1: 1 Worker, ไม่ใช้ Mutex — Sequential Baseline**
```bash
./experiment.sh 1
```
**Experiment 2: 3 Workers, ไม่ใช้ Mutex — Concurrent และเกิด Race Condition**
```bash
./experiment.sh 2
```
**Experiment 3: 3 Workers, ใช้ Mutex — Concurrent และป้องกัน Race Condition**
```bash
./experiment.sh 3
```
สามารถปรับจำนวน Client หรือหมายเลขที่นั่งที่ใช้ทดสอบได้ด้วย Environment Variable:
```bash
CLIENTS=8 SEAT=15 ./experiment.sh 2
```
ผลลัพธ์ของ Server และ Client แต่ละตัวจะถูกเก็บไว้ในโฟลเดอร์ results/
 
### วิธีมือ/ทดสอบเอง
1. เปิด Server โดยเลือก Mode ที่ต้องการ เช่น
```bash
./server 2
```
2. เปิด client อย่างน้อย 5 ตัว
3. ให้ทุกตัวสั่ง:
```bash
RESERVE 10
```
4. ใน Mode 2 ซึ่งมี 3 Workers และไม่มี Mutex จะมี random delay 50–500 ms ระหว่างการตรวจสอบและการเปลี่ยนแปลงสถานะที่นั่ง ทำให้ Worker หลายตัวสามารถตรวจพบว่าที่นั่งยังว่างพร้อมกัน และอาจมีมากกว่า 1 Client จองที่นั่งเดียวกันสำเร็จ
5. ตรวจสอบ Server log เพื่อดูการทำงานของ Worker และลำดับการตรวจสอบ/เปลี่ยนแปลงสถานะของที่นั่ง
6. เปรียบเทียบผลกับ Mode 3 ซึ่งใช้ Mutex โดยจะมีเพียง Client เดียวที่สามารถจองที่นั่งเดียวกันได้สำเร็จ
   
## 8. วิธีเปิด/ปิด Synchronization
กำหนดผ่าน Mode ตอนเริ่ม Server โดยไม่ต้อง Compile ใหม่:

**Mode 1: 1 Worker, ไม่ใช้ Mutex**
```bash
./server 1
```
**Mode 2: 3 Workers, ไม่ใช้ Mutex**
```bash
./server 2
```
**Mode 3: 3 Workers, ใช้ Mutex**
```bash
./server 3
```
Mutex จะถูกใช้ใน Critical Section ที่เกี่ยวข้องกับการตรวจสอบและเปลี่ยนแปลงสถานะของที่นั่ง เพื่อป้องกัน Race Condition
 
---

## สมาชิกกลุ่ม
| ชื่อ | รหัสนักศึกษา |
|---|---|
| นางสาวชัญญานุช ธนูศร | 68090500404 |
| นางสาวณัฐชญา อัตโยโค | 68090500410 |
| นางสาวณิชกานต์ นวลแก้ว | 68090500434 |
| นายรัชพล จันดาบุตร | 68090500443 |
| นางสาวสุภัสสร นันทานนท์ | 68090500447 |