
/** 학습내용 
 * 1. Linux IPC 도구의 종류와 특징에 대해 알아보고, 소프트웨어 요구사항에 적합한 IPC 도구를 선택하여 설계/구현하는 방법
 * 2. file 다루기
 * => 리눅스의 추상화 개념 및 주요 기능 동작 방식에 대한 이해도를 높임 -> kernel programming / device driver 재작을 위한 기반을 다짐
 */

// 1. 어떤 자료구조로 상품정보를 저장해야 검색에 용이할까??
/// => struct shm_menuInfo(typedef shm_menuInfo_t) 로 상품 정보 저장 -> 주문받은 메뉴 이름으로 검색 -> 주문 처리

// 2. 어떤 IPC 기법을 사용? (어떤 목적과 근거로 이러한 IPC 기법을 사용?) 
// => data transfer 방식 + shared memory 방식

/// 2-1. data transfer 방식
//// => byte stream vs msg 두 가지의 data transfer 방식을 비교/분석

/// 2-2. shared memory 방식(parent에서 정의) 
//// 1. 상품 수량 정보 : struct 정의 -> shm 생성(shmget) -> attach(shmat) 
//// 2. 주문정보 : struct 정의 -> shm 생성(shmget) -> attach(shmat) 
////  -> shm에 parent에서 받은 주문 정보 입력 -> shm segment's kev value(seg id)를 전송
//// +) shm key -> shm id -> shm addr => shm id를 주고받음

/// msg queue 
//// msg queue로 주고받을 data protocol을 struct 로 정의

/// 아쉬운점
//// 1. memory leak 방지를 위해, shm segment / msg q 할당 해지를 위한 전역 struct를 사용했는데...꼭 global var을 쓰는게 좋은걸까?
//// 2. global var로 자식 process pid 저장하여 마지막에(shutDown thread) 자식 process 종료시킴
//// 3. struct garbage code의 재사용성이 떨어짐
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <mqueue.h>
#include <semaphore.h>

#include "./shm_types.h"
#include "./msgQueue_protocol.h" 

#define NUM_OF_MENU 5
#define NUM_OF_THREADS 3
#define NUM_OF_SHM_SEG 2

// 메모리 할당 해제 대상 저장
typedef struct garbage
{
    int shm_id_orderInfo_for_dealloc;
    int shm_id_menuInfo_for_dealloc;

    char *msg_queue_filename_for_dealloc;

    sem_t sems[NUM_OF_THREADS];    
} garbage;

// 
void menuPrint(const shm_menuInfo_t *menu_list);
void order_input(char *input_str);
int order_process1(const shm_menuInfo_t *menu_list, const char *ordered_menuName);

// 
void* thread_print_order_history(void *);    	//param : FILE *fp
void* thread_garbage_free(void *);          	//param : const garbage *garbage_collector
void* thread_shutDown(void *);

// thread_func array 
void * (*thread_funcs[NUM_OF_THREADS]) (void *) = {
    thread_print_order_history,
    thread_garbage_free,
    thread_shutDown,
};

// 
void SIGINT_handler(int);

// global var for shm, msg queue 할당해제
static garbage garbages_for_dealloc;

// global var for semaphore => thread간 통신(매모리 공유하는 multi-thread 환경) -> unnamed semaphore 
static sem_t sem_for_print_order_history_thread;
static sem_t sem_for_garbage_free_thread;
static sem_t sem_for_shutDown_thread;

// global var for terminating child proc
static u_int32_t child_pid;

int main()
{
    //0_1. SIGINT handler 등록
    signal(SIGINT, SIGINT_handler);

    //0_2. posix msg queue create (for IPC)
    char msg_queue_filename[] = "/order_mq";    //메세지 큐 파일이름
    mqd_t mqd;                                  //메세지 큐 파일디스크립터
    
    //추후 msg queue 할당 해제를 위함
    garbages_for_dealloc.msg_queue_filename_for_dealloc = msg_queue_filename;    

    int flags = FILE_FLAG;                      //메세지 큐 파일 flag : 
    mode_t perms = FILE_MODE;                   //메세지 큐 파일 접근권한 설정

    struct mq_attr attr; 
    attr.mq_maxmsg = MQ_NUM_MSG;                //메세지 큐 파일에 저장될 수 있는 msg 개수
    attr.mq_msgsize = MQ_MSG_SIZE;              //각 msg의 size(default : 8KB)

    mqd = mq_open(msg_queue_filename, flags, perms, &attr);
    assert(mqd != -1);
    
    //1.  
    int pid = fork();
    assert(pid != -1);
    child_pid = pid;
	
    /// parent proc  
    //if((child_pid = pid))
    if(pid > 0)
    {
        //1. shm mem 생성 + 초기화
        ///+) shm key -> shm id -> shm addr => shm id를 주고받음
        int shm_id_orderInfo;
        int shm_id_menuInfo;
        
        ///1-1. shmget() : shm 생성 후 id 추출(사용권을 받는다)
        shm_id_orderInfo = shmget(SHM_KEY_ORDER, sizeof(shm_orderInfo_t), SHMGET_FLAGS_CREAT);
        assert(shm_id_orderInfo != -1);
        shm_id_menuInfo = shmget(SHM_KEY_ITEM, sizeof(shm_menuInfo_t)*NUM_OF_MENU, SHMGET_FLAGS_CREAT);
        assert(shm_id_menuInfo != -1);

        //// shm 할당 해제 위함
        garbages_for_dealloc.shm_id_orderInfo_for_dealloc = shm_id_orderInfo;  
        garbages_for_dealloc.shm_id_menuInfo_for_dealloc = shm_id_menuInfo;

        ///1-2. shmat() : shm id를 사용해서, 생성된 shm을 attach(현재 process가 shm에 attach 한 주소를 받아온다)
        shm_menuInfo_t* shm_addr_menuInfo = (shm_menuInfo_t*)shmat(shm_id_menuInfo, NULL, SHMAT_FLAGS_RW);
        assert(shm_addr_menuInfo != (void*)-1);
        shm_orderInfo_t* shm_addr_orderInfo = (shm_orderInfo_t*)shmat(shm_id_orderInfo, NULL, SHMAT_FLAGS_RW);
        assert(shm_addr_orderInfo != (void*)-1); 

        ///1-3. 메뉴 정보 초기화	
        srand(time(NULL));          //for menu_count 		
        
        //// 메뉴 이름 초기화    
        strncpy(shm_addr_menuInfo->menu_name, "Cold Brew", strlen("Cold Brew"));
        strncpy((shm_addr_menuInfo+1)->menu_name, "Caffee Latte", strlen("Caffee Latte"));
        strncpy((shm_addr_menuInfo+2)->menu_name, "Iced Earl Grey tea", strlen("Iced Earl Grey tea"));
        strncpy((shm_addr_menuInfo+3)->menu_name, "Byul Byul Sandwich", strlen("Byul Byul Sandwich"));
        strncpy((shm_addr_menuInfo+4)->menu_name, "Ice Americano", strlen("Ice Americano"));

        //// 메뉴 idx, 수량 초기화
        for(int i = 0; i < NUM_OF_MENU; ++i)
        {
            (shm_addr_menuInfo+i)->menu_num = i;
            (shm_addr_menuInfo+i)->menu_count = rand()%3;
        }

        ///1-4. 주문 기록 저장할 file명 초기화
        strncpy(shm_addr_menuInfo->order_history_fileName, "Order_History.txt", strlen("Order_History.txt"));
        
        //2. 
        ///1. thread 초기 설정 - pthread_t, pthread_attr_t

        ///1_1. pthread var 선언
        pthread_t tids[NUM_OF_THREADS];               //thread id
        pthread_attr_t attrs[NUM_OF_THREADS];         //thread attributes object
    
        ///1_2. attr 설정
        for(int i = 0; i < NUM_OF_THREADS; ++i) 
        {
            //1. assign default values
            if(pthread_attr_init(&attrs[i])) perror("pthread_attr_init");

            //2. set detached 
            if(pthread_attr_setdetachstate(&attrs[i], PTHREAD_CREATE_DETACHED)) perror("pthread_attr_setdetachstate");
        }    

        ///1_3. thread 생성 - pthread_create()
        int pthread_retcode;
        pthread_retcode = pthread_create(&tids[0], &attrs[0], thread_funcs[0], (void *)shm_addr_menuInfo->order_history_fileName); 
        assert(pthread_retcode == 0);
        pthread_retcode = pthread_create(&tids[1], &attrs[1], thread_funcs[1], (void *)&garbages_for_dealloc); 
        assert(pthread_retcode == 0);
        pthread_retcode = pthread_create(&tids[2], &attrs[2], thread_funcs[2], (void *)NULL); 
        assert(pthread_retcode == 0);
        
        //3. 메뉴 출력 -> 주문받기 -> child proc에게 주문정보 송신 ->  child proc로부터 주문처리 결과 수신
        
        ///1. receive 관련 초기설정
        /** feature : receive data form "/order_mq" (message queue를 사용한 data IPC)
         * @note mq_getattr().mq_msgsize : receive 하여 저장할 buffer size 지정 위해 사용
         * @note uint32_t int prio_of_msg : receive 받은 message의 우선 순위 저장
         * @note ssize_t numRead : 몇 byte message 수신했는지 저장
         * @note msgQ_protocol_cToP *received_msg_buffer : mq_receive()로 받은 data를 저장할 buffer
         * @note bool recv_order_result : 주문 성공여부 
         * @note int recv_sucess_cnt : 주문 누적 성공횟수
        */
        msgQ_protocol_cToP_t *received_msg_buffer;
        struct mq_attr attr;
        u_int32_t prio_of_msg; 
        int recv_order_result;
        int recv_sucess_cnt;
        ssize_t numRead;

        assert(mq_getattr(mqd, &attr) != -1);
        received_msg_buffer = (msgQ_protocol_cToP_t *)malloc(attr.mq_msgsize);

        ///2. send 관련 초기설정
        msgQ_protocol_pToC_t *msg_to_child;

        char input_str[MENU_NAME_LEN];
        int mq_retcode;
        
	while(1)
        {
            //메뉴 출력_UI
            menuPrint(shm_addr_menuInfo);

            //주문받기_UI
            order_input(input_str);

            //주문받은 정보 shm에 저징 
            strncpy(shm_addr_orderInfo->input_menuName, input_str, strlen(input_str));
            
            //child에게 보넬 msg 데이터 저장
            msg_to_child.shm_orderInfo_id = shm_id_orderInfo;
            msg_to_child.shm_menuInfo_id = shm_id_menuInfo;
        
            //child에게 msg 송신
            mq_retcode = mq_send(mqd, (const char*)msg_to_child, sizeof(*msg_to_child), 1);
            assert(mq_retcode != -1);

            //child로부터 msg 수신
            numRead = mq_receive(mqd, (char*)received_msg_buffer, attr.mq_msgsize, &prio_of_msg);   //msg 받기 전까지 block 
            assert(numRead != -1);

            //수신 처리
            ///1. 주문 성공/실패여부 출력 -> 실패시 continue
            if(!received_msg_buffer->order_result)
            {
                printf("Failed...\n\n");
            }
            
            ///2. 성공시 누적 주문 성공횟수 확인 -> 3개 넘어갈시 자식 proc 종료
            else
            {
                printf("Successful!!\n\n");
                if((received_msg_buffer->sucess_cnt) >= 3) kill(getpid(), SIGINT);
            }
        }
    }

    /// child proc
    else 
    {   
        /*
        //0. 주문 내역 기록할 파일 초기설정
        FILE *order_history_fp = fopen("Order_History.txt", "a");
        assert(order_history_fp != NULL);
        */

        //1. receive 관련 초기설정
        /** feature : receive data form "/order_mq" (message queue를 사용한 data IPC)
         * @note mq_getattr().mq_msgsize : receive 하여 저장할 buffer size 지정 위해 사용
         * @note uint32_t int prio_of_msg : receive 받은 message의 우선 순위 저장
         * @note ssize_t numRead : 몇 byte message 수신했는지 저장
         * @note msgQ_protocol_pToC_t *received_msg_buffer : mq_receive()로 받은 data를 저장할 buffer
         * @note int recv_shm_orderInfo_id : for shmat(), shm id값
         * @note int recv_shm_menuInfo_id : for shmat(), shm id값
        */
        msgQ_protocol_pToC_t *received_msg_buffer;
        struct mq_attr attr;
        u_int32_t prio_of_msg; 
        int recv_shm_orderInfo_id;
        int recv_shm_menuInfo_id;
        ssize_t numRead;
        
        assert(mq_getattr(mqd, &attr) != -1);
        received_msg_buffer = (msgQ_protocol_pToC_t *)malloc(attr.mq_msgsize);

        //2. send 관련 초기설정
        msgQ_protocol_cToP_t *msg_to_parent;

        int sucess_cnt = 0;
        int mq_retcode;

        while(1)
        {
            ////1. receive
            numRead = mq_receive(mqd, (char*)received_msg_buffer, attr.mq_msgsize, &prio_of_msg);   //msg 받기 전까지 block 
            assert(numRead != -1);

            ////2. id 값 receive
            recv_shm_orderInfo_id = received_msg_buffer->shm_orderInfo_id;
            recv_shm_menuInfo_id  = received_msg_buffer->shm_menuInfo_id;

            ////3. 주문 처리 -> shm 수정         
            shm_menuInfo_t* shm_addr_menuInfo = (shm_menuInfo_t*)shmat(recv_shm_menuInfo_id, NULL, SHMAT_FLAGS_RW);
            assert(shm_addr_menuInfo != (void*)-1);
            shm_orderInfo_t* shm_addr_orderInfo = (shm_orderInfo_t*)shmat(recv_shm_orderInfo_id, NULL, SHMAT_FLAGS_RW);
            assert(shm_addr_orderInfo != (void*)-1); 

            ///// 입력받은 상품 명의 idx 구하기
            int ordered_menuName_idx = order_process1(shm_addr_menuInfo, shm_addr_orderInfo->input_menuName);
            
            ///// 저장할 file open
            FILE *order_history_fp = fopen(shm_addr_menuInfo->order_history_fileName, "a");
            assert(order_history_fp != NULL);

            ///// 주문 살패
            if(ordered_menuName_idx == -1 || (shm_addr_menuInfo+ordered_menuName_idx)->menu_count == 0)
            {
                //0. parent에 보낼 주문 처리 결과 정보 저장
                msg_to_parent.order_result = 0;
                msg_to_parent.sucess_cnt = sucess_cnt;

                //1. 주문 내역 file에 기록
                fprintf(order_history_fp, "Order 1 %s,   Failed...\n", 
                           (shm_addr_menuInfo+ordered_menuName_idx)->menu_name);
                
                //2. close file 
                fclose(order_history_fp);
            }

            ///// 주문 성공
            else
            {   
                //0. 주문내역 shm에 저장
                --((shm_addr_menuInfo+ordered_menuName_idx)->menu_count);
                ++sucess_cnt;

                //1. parent에 보낼 주문 처리 결과 정보 저장
                msg_to_parent.order_result = 1;
                msg_to_parent.sucess_cnt = sucess_cnt;
            
                //2. 주문 내역 file에 기록
                fprintf(order_history_fp, "Order 1 %s,   Successful!!\n", 
                           (shm_addr_menuInfo+ordered_menuName_idx)->menu_name);

                //3. close file 
                fclose(order_history_fp);
            }
    
            ////4. 처리 결과 send
            mq_retcode = mq_send(mqd, (const char*)msg_to_parent, sizeof(*msg_to_parent), 1);
            assert(mq_retcode != -1);
        }
    }
    return 0;
}

//메뉴판 출력 - UI
void menuPrint(const shm_menuInfo_t *menu_list)
{
    int i;
    
    printf("--- Menu List —--\n");
    for(i = 0; i < NUM_OF_MENU; ++i)
    {
        printf("%s: %u\n", menu_list[i].menu_name, menu_list[i].menu_count);
    }

    printf("\n");
}

//주문 받기 - UI
void order_input(char *input_str)
{
    FILE *inputFp = stdin;

    printf("Enter a menu : ");
    fgets(input_str, MENU_NAME_LEN-1, inputFp);
	  
    input_str[strlen(input_str)-1] = '\0'; 	  
}


//주문 처리1 - 입력받은 메뉴 이름과 매칭되는 메뉴 idx 찾아서 리턴
int order_process1(const shm_menuInfo_t *menu_list, const char *ordered_menuName)
{
    int targetIdx = -1;

    for(int i = 0; i < NUM_OF_MENU; ++i)
    {
        if(!strncmp((menu_list+i)->menu_name, ordered_menuName, MENU_NAME_LEN))
        {
            targetIdx = i;
            break;
        }
    }

    return targetIdx;
}


// signal handler를 처리하는 process?? => 해당 handler를 등록한 proecss, 그 signal을 받는 process
/// => 해당 code에서 ctrl + c, 주문 3번 성공시 -> 부모 process가 signal handler 호출하게 만든다. 

// SIGINT handler에서 sem post -> 파일출력 thread wake up  -> 메모리 회수 thread wake up 
//// 파일출력 thread : 파일출력 -> 메모리 회수 thread wake up
//// 메모리 회수 thread : 메모리 회수 -> 자식 종료 & 부모 종료시키는 thread wake up  

//SIGINT handler
void SIGINT_handler(int signal)
{
    printf("SIGINT_handler success !!\n");
    sem_post(&sem_for_print_order_history_thread);
}

//"Order_History.txt" file 출력
void* thread_print_order_history(void* fileName) //order_history_fp_read)
{
    //0. semaphore 초기화 후 signal handler로부터 post 대기
    int sem_retcode;
    sem_retcode = sem_init(&sem_for_print_order_history_thread, 0, 0);
    assert(sem_retcode != -1);

    sem_retcode = sem_wait(&sem_for_print_order_history_thread);
    assert(sem_retcode != -1);
    
    FILE *stdout_fp = stdout;
    FILE *order_history_fp_read = fopen((const char*)fileName, "r");

    //1. "Order_History.txt"의 전체 file size 계산 -> for szBuffer
    /// => "Order_History.txt" file data 읽어올 buffer size 결정
    fseek(order_history_fp_read, 0, SEEK_END);
    long file_size = ftell(order_history_fp_read);
    fseek(order_history_fp_read, 0, SEEK_SET);
    int read_bytes;
    
    //2. '1.'에서 구한 buffer size + 1(for '\0') 만큼 동적할당
    char *szBuffer = (char *)malloc(file_size + 1);

    //3. file 끝까지 모두 read
    read_bytes = fread(szBuffer, sizeof(*szBuffer), 
                        file_size, order_history_fp_read);
    *(szBuffer + file_size) = '\0';        

    //4. 출력
    printf("\n---Your Order History---\n");
    //fputc('\n', stdout_fp);
    fputs(szBuffer, stdout_fp);

    //5. free
    free(szBuffer);
    szBuffer = NULL;

    //6.sem 원복 -> thread_garbage_free post
    sem_retcode = sem_init(&sem_for_print_order_history_thread, 0, 0);
    assert(sem_retcode != -1);
    sem_retcode = sem_post(&sem_for_garbage_free_thread);
    assert(sem_retcode != -1);

    return NULL;
}



// 메모리, 메세지 큐 할당 해제
void* thread_garbage_free(void *garbage_collector_ptr)
{  
    //0_1. semaphore 초기화 후 thread_print_order_history 로부터 post 대기
    int sem_retcode;
    sem_retcode = sem_init(&sem_for_garbage_free_thread, 0, 0);
    assert(sem_retcode != -1);

    sem_retcode = sem_wait(&sem_for_garbage_free_thread);
    assert(sem_retcode != -1);

    //0_2. 
    const garbage *garbage_collector = (const garbage *)garbage_collector_ptr;

    //0_3. shm 세그먼트 해제
    int shm_retcode;
    shm_retcode = shmctl(garbage_collector->shm_id_menuInfo_for_dealloc, IPC_RMID, NULL);
    assert(shm_retcode != -1);
    shm_retcode = shmctl(garbage_collector->shm_id_orderInfo_for_dealloc, IPC_RMID, NULL);
    assert(shm_retcode != -1);

    //1. message queue delete
    int mq_retcode;
    mq_retcode = mq_unlink(garbage_collector->msg_queue_filename_for_dealloc);
    assert(mq_retcode != -1);
      
    printf("garbage_free success !!\n");

    //2. sem 원복 -> shutDown_thread post
    sem_retcode = sem_init(&sem_for_garbage_free_thread, 0, 0);
    assert(sem_retcode != -1);
    sem_retcode = sem_post(&sem_for_shutDown_thread);
    assert(sem_retcode != -1);
   
    return NULL;
}

// 부모, 자식 process 정상 종료
void* thread_shutDown(void *arg)
{
    //0. semaphore 초기화 후 thread_print_order_history 로부터 post 대기
    int sem_retcode;
    sem_retcode = sem_init(&sem_for_shutDown_thread, 0, 0);
    assert(sem_retcode != -1);

    sem_retcode = sem_wait(&sem_for_shutDown_thread);
    assert(sem_retcode != -1);

    //1. child process 종료
    kill(child_pid, SIGKILL);

    //2. child process 상태 회수
    int child_status;
    waitpid(-1, &child_status, WNOHANG);
    
    if (WIFSIGNALED(child_status) == 1) 
    {
        printf("\nChild process terminated by signal %d\n", WTERMSIG(child_status));
    } 
    else
    {
        printf("\nChild process terminated with exit status %d\n", WEXITSTATUS(child_status));
    }
		    
    //3. parent process 종료
    printf("thread_shutDown success !!\n");

    sem_retcode = sem_init(&sem_for_shutDown_thread, 0, 0);
    assert(sem_retcode != -1);

    exit(0);
}


