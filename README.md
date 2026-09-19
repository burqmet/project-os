# Cinema Seat Reservation System (CSS223 - OS Project)
 
ระบบจองที่นั่งโรงภาพยนตร์

## สถานการณ์ที่เลือก
**จองที่นั่งโรงภาพยนตร์** — มีที่นั่งทั้งหมด 20 ที่นั่ง (หมายเลข 1–20)
 
## เทคโนโลยีที่ใช้
- ภาษา C
- POSIX Message Queue
- pthread
- pthread_mutex

---
 
## 1. วิธี Build Docker Image
```bash
docker build -t cinema-seat-reserve .
```

## 2. วิธี Run Container
```bash
docker run -dit --name cinema-reservation cinema-seat-reserve
```
 
## 3. วิธีเปิด Server
```bash
docker exec -it cinema-reservation bash
cd /app
./server
```
 
## 4. วิธีเปิด Client หลายตัว
เปิด terminal ใหม่ต่อ client แต่ละคน (อย่างน้อย 5 terminal):
```bash
# Terminal 2
docker exec -it cinema-reservation bash
./client 1
 
# Terminal 3
docker exec -it cinema-reservation bash
./client 2
 
# ... ทำซ้ำจนถึง client 5
```
 
## 5. รูปแบบ Message Queue ที่ใช้
- **Request Queue**: 
- **Response Queue**: 
**Request message :**
```c


```
 
**Response message :**
```c


```
 
 
## 6. คำสั่งที่ Client รองรับ
| คำสั่ง | ตัวอย่าง | ความหมาย |
|---|---|---|
 
## 7. วิธีทดลอง Race Condition
1. 

## 8. วิธีเปิด/ปิด Synchronization

```c


```