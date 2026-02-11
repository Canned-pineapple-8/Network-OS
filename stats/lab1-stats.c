#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

typedef enum {
    NONE,
    MALE,
    FEMALE
} Gender;

typedef struct {
    int taken_cabins;
    Gender current_gender;
} BathroomInfo;

typedef struct {
    int id;
    Gender gender;
    int usage_time;
    double wait_time; // новое поле: время ожидания в очереди
} Student;

pthread_mutex_t mutex;
pthread_cond_t cond;
sem_t cabins_sem;

typedef struct Node {
    int data;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int size;
} Queue;

void queue_init(Queue* q) { q->head = q->tail = NULL; q->size = 0; }
void queue_push(Queue* q, int data) { Node* n = malloc(sizeof(Node)); n->data = data; n->next = NULL; if (!q->tail) q->head=q->tail=n; else {q->tail->next=n;q->tail=n;} q->size++; }
int queue_peek(Queue* q) { return q->head ? q->head->data : -1; }
int queue_pop(Queue* q) { if(!q->head) return -1; Node* n=q->head; int d=n->data; q->head=n->next; if(!q->head) q->tail=NULL; free(n); q->size--; return d; }

BathroomInfo* bathroom;
Queue queue_women, queue_men;
int users_amt=30, max_usage_time=5, cabins=5;

void* student(void* arg) {
    Student* s=(Student*)arg;

    struct timeval start_wait, end_wait;
    gettimeofday(&start_wait, NULL);

    pthread_mutex_lock(&mutex);

    Queue* student_queue = (s->gender==MALE)?&queue_men:&queue_women;
    queue_push(student_queue, s->id);

    while(bathroom->current_gender!=NONE && !(queue_peek(student_queue)==s->id && bathroom->current_gender==s->gender && bathroom->taken_cabins<cabins))
        pthread_cond_wait(&cond,&mutex);

    queue_pop(student_queue);
    if(bathroom->current_gender==NONE) bathroom->current_gender=s->gender;

    pthread_mutex_unlock(&mutex);

    gettimeofday(&end_wait,NULL);
    s->wait_time = (end_wait.tv_sec - start_wait.tv_sec) + (end_wait.tv_usec - start_wait.tv_usec)/1e6;

    sem_wait(&cabins_sem);
    pthread_mutex_lock(&mutex);
    bathroom->taken_cabins++;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    sleep(s->usage_time);

    pthread_mutex_lock(&mutex);
    bathroom->taken_cabins--;
    if(bathroom->taken_cabins==0) {
        int waiting_opposite = (bathroom->current_gender==MALE)?queue_women.size:queue_men.size;
        if(waiting_opposite!=0) bathroom->current_gender=(bathroom->current_gender==MALE)?FEMALE:MALE;
        pthread_cond_broadcast(&cond);
    }
    pthread_mutex_unlock(&mutex);
    sem_post(&cabins_sem);

    return NULL;
}

int main() {
    FILE* f = fopen("bathroom_stats.txt","w");

    for(cabins=2; cabins<=20; cabins++) {
        pthread_mutex_init(&mutex,NULL);
        pthread_cond_init(&cond,NULL);
        sem_init(&cabins_sem,0,cabins);

        queue_init(&queue_women);
        queue_init(&queue_men);

        bathroom = malloc(sizeof(BathroomInfo));
        bathroom->taken_cabins=0; bathroom->current_gender=NONE;

        pthread_t threads[users_amt];
        Student* students = malloc(sizeof(Student)*users_amt);
        srand(time(NULL));

        for(int i=0;i<users_amt;i++) {
            students[i].id=i;
            students[i].gender=rand()%2?MALE:FEMALE;
            students[i].usage_time=rand()%max_usage_time+1;
            students[i].wait_time=0;
            pthread_create(&threads[i],NULL,student,&students[i]);
        }

        double total_wait=0;
        for(int i=0;i<users_amt;i++) { pthread_join(threads[i],NULL); total_wait+=students[i].wait_time; }

        double avg_wait = total_wait/users_amt;
        fprintf(f,"%d %.6f\n",cabins,avg_wait);
        printf("Cabins %d: avg wait %.6f sec\n",cabins,avg_wait);

        free(students);
        free(bathroom);
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
        sem_destroy(&cabins_sem);
    }

    fclose(f);
    return 0;
}
