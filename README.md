# sysProg-orderProgram(23-1 시스템프로그래밍 과제 리팩터링)

## WHAT?(명세) : 
<p align="center">
  <img src="https://github.com/user-attachments/assets/48997085-32f2-49bc-b64f-b0705f2c12ca">
</p>




## HOW?(code 구현 방식) :
### 1. 어떤 자료구조로 상품정보를 저장해야 검색에 용이할까??
#### hash table(unordered_map) 사용 :  '주문상품(string) : struct' 

### 2. 어떤 IPC 기법을 사용? (어떤 목적과 근거로 이러한 IPC 기법을 사용?) 
#### => data transfer 방식 + shared memory 방식

### 2-1. data transfer 방식
#### (1) Byte stream (2) Message 두 가지의 data transfer 방식을 비교/분석 후, 개별 메시지 단위의 처리 및 실시간성에 중요성을 두어 'Message' 방식 선택

### 2-2. shared memory 방식
#### 1. 상품 수량 정보 : struct 정의 -> shm 생성(shmget) -> attach(shmat) 
#### 2. 주문정보 : struct 정의 -> shm 생성(shmget) -> attach(shmat) 
##### => shm에 Parent process 에서 받은 주문 정보 입력 -> Child process에 shm segment's kev value(seg id)를 전송 -> Child process에서 주문 처리
##### +) shm key -> shm id -> shm addr => shm id를 주고받음
