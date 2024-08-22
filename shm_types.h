#ifndef _SHM_TYPES_H
#define _SHM_TYPES_H

#include <sys/stat.h>
#include <sys/shm.h>
#include "/usr/include/linux/limits.h"        /* ??? : for NAME_MAX?*/
#include <semaphore.h>
//#include <bits.h>
//#include <unordered_map>
//#include <string>
//#include <vector>
//using namespace std;

/* Hard-coded keys for IPC objects */
enum def_shm_key                        /* Key for shared memory segment */
{
    SHM_KEY_BASE = 10,
    SHM_KEY_ITEM = SHM_KEY_BASE,
    SHM_KEY_ORDER
};

/*
// Permission flag for shmget.  
#define SHM_R		0400		// or S_IRUGO from <linux/stat.h> 
#define SHM_W		0200		// or S_IWUGO from <linux/stat.h> 
*/
/* set of options and permission bits */
#define SHMGET_FLAGS_CREAT (IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)              /* 644*/  
#define SHMGET_FLAGS_EXCL (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)   /* 644*/    

/*
//Flags for `shmat'.  
#define SHM_RDONLY	010000		// attach read-only else read-write 
#define SHM_RND		020000		// round attach address to SHMLBA()
#define SHM_REMAP	040000		// take-over region on attach 
#define SHM_EXEC	0100000		// execution access 
*/
#define SHMAT_FLAGS_R (SHM_RDONLY)       
#define SHMAT_FLAGS_RW 0
#define SHM_STR_MSG_BUF_SIZE 1024
#define MENU_NAME_LEN 30

/* Defines structure of shared memory segment for Item inventory*/
// hash : "상품명" -> idx
// vector : idx -> 상품 개수
/*
typedef struct shm_itemIventory                                
{
    unordered_map<string, int> strToIdx;
    vector<int> idxToInvetory;
} shm_itemIventory_t;
*/

/* Defines structure of shared memory segment for menuInfo*/
typedef struct shm_menuInfo
{
    sem_t sem_for_shm_menuInfo;
    char menu_name[MENU_NAME_LEN];          // 메뉴 이름
    u_int8_t menu_num;                      // 메뉴 idx     
    u_int8_t menu_count;                    // 메뉴 재고
    
    char order_history_fileName[MENU_NAME_LEN]; 
} shm_menuInfo_t;


/* Defines structure of shared memory segment for Order Info */
typedef struct shm_orderInfo                                
{
    sem_t sem_for_shm_orderInfo;
    char input_menuName[MENU_NAME_LEN];
    //int orderItem_idx;              // 주문한 제품의 idx
    //int orderItem_cnt;              // 주문한 제품의 수량
} shm_orderInfo_t;



#endif /* _SHM_TYPES_H */