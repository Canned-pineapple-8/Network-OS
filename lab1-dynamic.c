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

// -------------------------------
BathroomInfo* bathroom;
Queue queue_women, queue_men;

int users_amt = 10;        // начальное количество студентов
int max_usage_time = 10;   // максимальное время в ванной
int cabins = 5;            // количество кабинок

int waiting_male = 0;
int waiting_female = 0;

// -------------------------------

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

    if (s->gender == MALE) waiting_male++;
    else waiting_female++;

    // Ждем своей очереди и своего пола
    while (bathroom->board.batch_gender != NONE &&
          !(queue_peek(student_queue) == s->id && bathroom->board.batch_gender == s->gender &&
            bathroom->people_in_bathroom < cabins))
    {
        pthread_cond_wait(&cond, &mutex);
    }

    if (s->gender == MALE) waiting_male--;
    else waiting_female--;

    queue_pop(student_queue);

    if (bathroom->board.batch_gender == NONE) {
        bathroom->board.batch_gender = s->gender;
        printf("Board initialized: %s\n", s->gender == MALE ? "male" : "female");
    }

    pthread_mutex_unlock(&mutex);

    sem_wait(&cabins_sem);

    pthread_mutex_lock(&mutex);

    bathroom->people_in_bathroom++;
    pthread_cond_broadcast(&cond);

    printf("Student %d (%s) entered | in bathroom: %d\n",
           s->id, s->gender == MALE ? "male" : "female",
           bathroom->people_in_bathroom);

    pthread_mutex_unlock(&mutex);

    sleep(s->usage_time);

    printf("Student %d (%s) leaves\n",
           s->id, s->gender == MALE ? "male" : "female");

    pthread_mutex_lock(&mutex);

    bathroom->people_in_bathroom--;

    if (bathroom->people_in_bathroom == 0) {
        int waiting_opposite = (bathroom->board.batch_gender == MALE) ? waiting_female : waiting_male;

        if (waiting_opposite != 0) {
            bathroom->board.batch_gender =
                (bathroom->board.batch_gender == MALE) ? FEMALE : MALE;

            printf("Board switched to %s\n",
                   bathroom->board.batch_gender == MALE ? "male" : "female");
        } else {
            printf("Board left %s\n",
                   bathroom->board.batch_gender == MALE ? "male" : "female");
        }

        pthread_cond_broadcast(&cond);
    }

    pthread_mutex_unlock(&mutex);
    sem_post(&cabins_sem);

    free(s); // обязательно освобождаем память
    return NULL;
}

// -------------------------------
// Генератор студентов, добавляем студентов каждые 1-3 секунды
void* student_generator(void* arg)
{
    int next_id = users_amt;
    while (1) {
        int new_students = rand() % 3 + 1; // 1-3 студента за раз

        for (int i = 0; i < new_students; i++) {
            Student* s = malloc(sizeof(Student));
            s->id = next_id++;
            s->gender = rand() % 2 ? MALE : FEMALE;
            s->usage_time = rand() % max_usage_time + 1;

            pthread_t t;
            pthread_create(&t, NULL, student, s);
            pthread_detach(t);
        }

        sleep(rand() % 3 + 1); // пауза между партиями студентов
    }
    return NULL;
}

// -------------------------------
int main(int argc, char* argv[])
{
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

    // стартуем начальную партию студентов
    for (int i = 0; i < users_amt; i++) {
        Student* s = malloc(sizeof(Student));
        s->id = i;
        s->gender = rand() % 2 ? MALE : FEMALE;
        s->usage_time = rand() % max_usage_time + 1;

        pthread_t t;
        pthread_create(&t, NULL, student, s);
        pthread_detach(t);
    }

    // стартуем генератор студентов
    pthread_t gen_thread;
    pthread_create(&gen_thread, NULL, student_generator, NULL);

    // главный поток просто спит, программа будет работать динамически
    while (1) {
        sleep(1);
    }

    return 0;
}
