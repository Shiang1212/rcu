#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <urcu.h>
#include <stdatomic.h>
#include <urcu/rculfqueue.h>


// gcc -o rcu_queue rcu.c -lurcu-memb -lurcu-cds -lpthread

#ifndef MAX_PRODUCER
#define MAX_PRODUCER 100
#endif
#ifndef MAX_CONSUMER
#define MAX_CONSUMER 10
#endif

static int cnt_consumer = 0;
static int cnt_producer = 0;

static int count = 0;


struct node {
    int value;
    struct cds_lfq_node_rcu next;
};

struct cds_lfq_queue_rcu queue;

void *add_queue() {
    urcu_memb_register_thread();
    int tid = atomic_fetch_add(&cnt_producer, 1);
    long added;
    for (added = 0; added < 100; added++) {
        // 分配新節點
        struct node *new_node = malloc(sizeof(struct node));
        int num = atomic_fetch_add(&count, 1);  
        new_node->value = num;

        // 插入新節點到佇列
        urcu_memb_read_lock();
        cds_lfq_enqueue_rcu(&queue, &new_node->next);
        // printf("Writer %d: Enqueued %d\n", tid ,num);
        urcu_memb_read_unlock();

    }
    printf("Producer thread [%d] exited! Still %d running...\n",
           tid, atomic_load(&cnt_producer));
    urcu_memb_unregister_thread();
    return NULL;
}

void *remove_queue() {
    urcu_memb_register_thread();
    int tid = atomic_fetch_add(&cnt_consumer, 1);
    while (1) {
        urcu_memb_read_lock();
        
        struct cds_lfq_node_rcu *qnode = cds_lfq_dequeue_rcu(&queue);
        
        urcu_memb_read_unlock();

        if (qnode) {
            struct node *node = caa_container_of(qnode, struct node, next);
            if(node) {
                // printf("Reader %d: Dequeued %d\n", tid, node->value);
                free(node);
            }
        }
        else {
            printf("Consumer thread [%d] exited\n", tid);
            break;
        }


    }
    urcu_memb_unregister_thread();
    return NULL;
}

int main() {
    clock_t start, end;

    start = clock();

    pthread_t thread_cons[MAX_CONSUMER], thread_pros[MAX_PRODUCER];

    // 初始化 RCU 和佇列
    
    cds_lfq_init_rcu(&queue, urcu_memb_call_rcu);

    // 創建寫者和讀者線程
    for (int i = 0; i < MAX_CONSUMER; i++) {
        pthread_create(&thread_cons[i], NULL, remove_queue, NULL);
    }

    for (int i = 0; i < MAX_PRODUCER; i++) {
        // atomic_fetch_add(&cnt_producer, 1);
        pthread_create(&thread_pros[i], NULL, add_queue, NULL);
    }

    // 等待寫者線程結束
    for (int i = 0; i < MAX_PRODUCER; i++) 
        pthread_join(thread_pros[i], NULL);
    
    for (int i = 0; i < MAX_CONSUMER; i++)
        pthread_join(thread_cons[i], NULL);
    
    end = clock();

    printf("It's cost %f ms\n", (double) end - start);

    // int ret = cds_lfq_destroy_rcu(&queue);
	// if (ret) {
	// 	printf("Error destroying queue (non-empty)\n");
	// }

    return 0;
}