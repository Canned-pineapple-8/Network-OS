#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

// перечисление для полов
typedef enum {
    NONE,
    MALE,
    FEMALE
} Gender;

// ванная комната - отображает количество занятых кабинок и пол, который сейчас обслуживается
typedef struct {
    int taken_cabins;
    Gender current_gender;
} BathroomInfo;

// студент - имеет идентификатор, пол и время, необходимое ему в ванной комнате
typedef struct {
    int id;
    Gender gender;
    int usage_time;
} Student;

// реализация очереди через узлы:
typedef struct Node {
    int data;           // в качестве данных - ID студента   
    struct Node* next;
} Node;

typedef struct {
    Node* head;            // первый элемент
    Node* tail;            // последний элемент
    int size;
} Queue;

// инициализация очереди
void queue_init(Queue* q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

// помещение элемента в очередь
void queue_push(Queue* q, int data)
{
    Node* n = malloc(sizeof(Node)); // выделяем память под новый объект
    n->data = data;
    n->next = NULL;

    if (q->tail == NULL) { // если очередь пустая - выставляем один элемент
        q->head = q->tail = n;
    } else {
        q->tail->next = n; // иначе прикрепляемся к концу
        q->tail = n;
    }

    q->size++;
}

// проверка первого в очереди элемента (без извлечения)
int queue_peek(Queue* q)
{
    if (q->head == NULL) // возвращаем -1 если очередь пустая
        return -1;

    return q->head->data; // иначе ID студента
}

// извлечение первого элемента из очереди
int queue_pop(Queue* q)
{
    if (q->head == NULL)
        return -1;

    Node* n = q->head;
    int data = n->data;

    q->head = n->next; // смещаем голову
    if (q->head == NULL)
        q->tail = NULL;

    free(n); // освобождаем предыдущую голову
    q->size--;

    return data; // возвращаем данные предыдущей головы
}

// инициализация базовых объектов
BathroomInfo* bathroom;
Queue queue_women, queue_men;

int users_amt = 10;        // -s (значение по умолчанию)
int max_usage_time = 10;   // -u (значение по умолчанию)
int cabins = 5;            // -c (значение по умолчанию)

// объекты для многопоточности
pthread_mutex_t mutex;
pthread_cond_t cond;
sem_t cabins_sem;

// чтение аргументов командной строки
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

    pthread_mutex_lock(&mutex);

    printf("Student %d (%s) wants to enter\n",
        s->id, s->gender == MALE ? "male" : "female");

    Queue* student_queue;
    
    if (s->gender == MALE) student_queue = &queue_men;
    else student_queue = &queue_women;

    // как только забрали мьютекс - заносим свой ID в очередь
    queue_push(student_queue, s->id);

    // в каких случаях мы можем войти?
    // если нас позвали, при этом табличка совпадает с нашим полом и подошла наша очередь
    // либо если табличка не проинициализирована (первый пришедший студент)
    while (bathroom->current_gender != NONE && !(queue_peek(student_queue) == s->id && bathroom->current_gender == s->gender )) 
    {
        pthread_cond_wait(&cond, &mutex);
    }

    // если дождались условия - достаём свой ID из очереди (он точно первый, потому что мы только что вошли)
    if (s->gender == MALE) queue_pop(&queue_men);
    else queue_pop(&queue_women);

    // если табличка не была проинициализирована - инициализируем табличку своим полом
    if (bathroom->current_gender == NONE) {
        bathroom->current_gender = s->gender;
        printf("\tBoard initialized: %s \n",
               s->gender == MALE ? "male" : "female");
    }

    pthread_mutex_unlock(&mutex);

    // захватываем кабинку
    sem_wait(&cabins_sem);

    // и захватываем мьютекс
    pthread_mutex_lock(&mutex);

    // увеличиваем число занятых кабинок
    bathroom->taken_cabins++;
    // если есть свободные кабинки - кого-нибудь зовём
    if (bathroom->taken_cabins < cabins) pthread_cond_broadcast(&cond);

    printf("Student %d (%s) entered | in bathroom: %d\n",
           s->id,
           s->gender == MALE ? "male" : "female",
           bathroom->taken_cabins);

    pthread_mutex_unlock(&mutex);

    // пользуемся ванной
    sleep(s->usage_time);

    printf("Student %d (%s) leaves\n",
           s->id, s->gender == MALE ? "male" : "female");

    pthread_mutex_lock(&mutex);

    // освобождаем кабинку
    bathroom->taken_cabins--;

    if (bathroom->taken_cabins == 0) {
        int waiting_opposite = (bathroom->current_gender == MALE) ? queue_women.size : queue_men.size;

        if (waiting_opposite != 0) // если очередь противоположного пола не пустая (есть ли смысл менять табличку?)
        {
            bathroom->current_gender =
                (bathroom->current_gender == MALE) ? FEMALE : MALE; // меняем табличку на противополжную

            printf("\tBoard switched to %s\n",
                bathroom->current_gender == MALE ? "male" : "female");

        }
        else
        {
            printf("\tBoard left %s\n",
                bathroom->current_gender == MALE ? "male" : "female");

        }
        pthread_cond_broadcast(&cond); // если ванная опустела - зовём следующую пачку

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
    bathroom->taken_cabins = 0;
    bathroom->current_gender = NONE;

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