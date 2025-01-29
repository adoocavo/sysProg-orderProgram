#ifndef _MSGQUEUE_PROTOCOL_H_
#define _MSGQUEUE_PROTOCOL_H_

#include <unistd.h>


/** feature : msg queue 관련 설정
 * 
*/
#define MQ_NUM_MSG 1                 //각 posix msg queue에 저장될 수 있는 msg 개수
#define MQ_MSG_SIZE 2048             //각 msg의 size(default : 8KB)
#define NUM_OF_MQ 1                  //생성할 message queue file 개수
//#define MENU_NAME_LEN 30

// 플래그 값이나 권한 설정 등의 여러 옵션을 조합
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) 
#define FILE_FLAG (O_RDWR | O_CREAT | O_EXCL)


/** feature : parent <-> child 간의 data 통신 protocol  
 * pToC : parent 에서 attach 후, 주문 정보를 shm에게 저장 -> id 값을 child에게 전송
 * cToP : child에서는 id값을 받아서, 주문 처리 -> 재고처리를 shm에 저장 ->  주문 성공여부 및 누적 성공횟수를 parent에게 전송 
 */
typedef struct msgQ_protocol_pToC
{
    int shm_orderInfo_id;           // parent에서 주문 정보를 저장한 shm struct의 id 값 저장 
    int shm_menuInfo_id;            // parent에서 재고 정보를 저장한 shm struct의 id 값 저장 
} msgQ_protocol_pToC_t;


typedef struct msgQ_protocol_cToP
{   
    u_int8_t order_result;           // 주문 성공여부 : 0(실패), 1(성공)
    int sucess_cnt;                  // 주문 누적 성공횟수

} msgQ_protocol_cToP_t;



#endif 