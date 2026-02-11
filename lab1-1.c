#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef enum {
    NONE,
    MALE,
    FEMALE
} Gender;

typedef struct {
    Gender batch_gender;   // какой пол сейчас обслуживается
} Board;

typedef struct {
    int people_in_bathroom;
    Board board;
} BathroomInfo;

typedef struct {
    int id;
    Gender gender;
    int usage_time;
} Student;

pthread_mutex_t mutex;
pthread_cond_t cond;
sem_t cabins_sem;

typedef struct Node {
    int data;              // храним id студентов
    struct Node* next;
} Node;

typedef struct {
    Node* head;            // первый элемент
    Node* tail;            // последний элемент
    int size;
} Queue;

void queue_init(Queue* q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

void queue_push(Queue* q, int data)
{
    Node* n = malloc(sizeof(Node));
    n->data = data;
    n->next = NULL;

    if (q->tail == NULL) {
        q->head = q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }

    q->size++;
}

int queue_peek(Queue* q)
{
    if (q->head == NULL)
        return -1;

    return q->head->data;
}

int queue_pop(Queue* q)
{
    if (q->head == NULL)
        return -1;

    Node* n = q->head;
    int data = n->data;

    q->head = n->next;
    if (q->head == NULL)
        q->tail = NULL;

    free(n);
    q->size--;

    return data;
}

int queue_empty(Queue* q)
{
    return q->size == 0;
}


BathroomInfo* bathroom;
Queue queue_women, queue_men;

int users_amt = 10;        // -s
int max_usage_time = 10;   // -u
int cabins = 5;            // -c

int waiting_male = 0;
int waiting_female = 0;

void parse_args(int argc, char* argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "s:u:c:b:")) != -1) {
        switch(opt) {
            case 's':
                users_amt = atoi(optarg);
                break;
            case 'u':
                max_usage_time = atoi(optarg);
                break;
            case 'c':
                cabins = atoi(optarg);
                break;
            default:
                fprintf(stderr,
                        "Usage: %s [-s students_amount] [-u max_usage_time] [-c cabins]\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (users_amt <= 0 || max_usage_time <= 0 || cabins <= 0) {
        fprintf(stderr, "All parameters must be positive numbers\n");
        exit(EXIT_FAILURE);
    }
}

void* student(void* arg)
{
    Student* s = (Student*)arg;

    printf("Student %d (%s) wants to enter\n",
           s->id, s->gender == MALE ? "male" : "female");

    pthread_mutex_lock(&mutex);

    Queue* student_queue;

    if (s->gender == MALE) student_queue = &queue_men;
    else student_queue = &queue_women;

    queue_push(student_queue, s->id);

    if (s->gender == MALE) waiting_male++; // ?
    else waiting_female++;

    // в каких случаях мы можем войти?
    // если табло совпадает с нашим полом и пачка не пустая
    // либо если табло не проинициализировано (первый пришедший студент, условно)
    while (bathroom->board.batch_gender != NONE && !(queue_peek(student_queue) == s->id && bathroom->board.batch_gender == s->gender && 
         bathroom->people_in_bathroom < cabins) ) 
    {
        pthread_cond_wait(&cond, &mutex);
    }

    if (s->gender == MALE) waiting_male--; // ?
    else waiting_female--;

    if (s->gender == MALE) queue_pop(&queue_men);
    else queue_pop(&queue_women);

    // если табло не было проинициализировано - инициализируем табло
    if (bathroom->board.batch_gender == NONE) {

        bathroom->board.batch_gender = s->gender;
        printf("Board initialized: %s \n",
               s->gender == MALE ? "male" : "female");
    }

    pthread_mutex_unlock(&mutex);

    sem_wait(&cabins_sem);

    pthread_mutex_lock(&mutex);

    bathroom->people_in_bathroom++;
    pthread_cond_broadcast(&cond);

    printf("Student %d (%s) entered | in bathroom: %d\n",
           s->id,
           s->gender == MALE ? "male" : "female",
           bathroom->people_in_bathroom);

    pthread_mutex_unlock(&mutex);

    sleep(s->usage_time);

    printf("Student %d (%s) leaves\n",
           s->id, s->gender == MALE ? "male" : "female");

    pthread_mutex_lock(&mutex);

    bathroom->people_in_bathroom--;

    if (bathroom->people_in_bathroom == 0) {
        int waiting_opposite = (bathroom->board.batch_gender == MALE) ? waiting_female : waiting_male;

  
        if (waiting_opposite != 0)
        {
            bathroom->board.batch_gender =
                (bathroom->board.batch_gender == MALE) ? FEMALE : MALE;

            printf("Board switched to %s\n",
                bathroom->board.batch_gender == MALE ? "male" : "female");

        }
        else
        {
            printf("Board left %s\n",
                bathroom->board.batch_gender == MALE ? "male" : "female");

        }
        pthread_cond_broadcast(&cond);

    }

    pthread_mutex_unlock(&mutex);
    sem_post(&cabins_sem);

    return NULL;
}

int main(int argc, char* argv[])
{
    parse_args(argc, argv); 

    srand(time(NULL));
    setbuf(stdout, NULL);

    queue_init(&queue_women);
    queue_init(&queue_men);

    bathroom = malloc(sizeof(BathroomInfo));
    bathroom->people_in_bathroom = 0;
    bathroom->board.batch_gender = NONE;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    sem_init(&cabins_sem, 0, cabins);

    pthread_t threads[users_amt];
    Student* students_info = malloc(users_amt * sizeof(Student));

    for (int i = 0; i < users_amt; i++) {
        students_info[i].id = i;
        students_info[i].gender = rand() % 2 ? MALE : FEMALE;
        students_info[i].usage_time = rand() % max_usage_time + 1;
        pthread_create(&threads[i], NULL, student, &students_info[i]);
    }

    for (int i = 0; i < users_amt; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&cabins_sem);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    free(students_info);
    free(bathroom);

    return 0;
}