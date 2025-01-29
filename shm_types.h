#ifndef _SHM_TYPES_H
#define _SHM_TYPES_H

#include <sys/stat.h>
#include <sys/shm.h>
#include "/usr/include/linux/limits.h"       
#include <semaphore.h>

// System V IPC - shared memory segment의 key 정의
enum def_shm_key                      
{
    SHM_KEY_BASE = 10,
    SHM_KEY_ITEM = SHM_KEY_BASE,
    SHM_KEY_ORDER
};

// Permission flag for shmget
#define SHMGET_FLAGS_CREAT (IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)                  // 644  
#define SHMGET_FLAGS_EXCL (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)        // 644    

//Flags for shmat
#define SHMAT_FLAGS_R (SHM_RDONLY)       
#define SHMAT_FLAGS_RW 0

// 주문 데이터 관련 SIZE, LEN
#define SHM_STR_MSG_BUF_SIZE 1024
#define MENU_NAME_LEN 30

// Defines structure of shared memory segment for menuInfo 
typedef struct shm_menuInfo
{
    //sem_t sem_for_shm_menuInfo;
    char menu_name[MENU_NAME_LEN];                  // 메뉴 이름
    u_int8_t menu_num;                              // 메뉴 idx     
    u_int8_t menu_count;                            // 메뉴 재고
    
    char order_history_fileName[MENU_NAME_LEN]; 
} shm_menuInfo_t;

// Defines structure of shared memory segment for Order Info 
typedef struct shm_orderInfo                                
{
    //sem_t sem_for_shm_orderInfo;            
    char input_menuName[MENU_NAME_LEN];            // 주문받은 메뉴 이름
} shm_orderInfo_t;

#endif // _SHM_TYPES_H 
