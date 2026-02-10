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
    int id;
    Gender gender;
    int usage_time;
} Student;

typedef struct {
    Gender current_gender; // состояние ванной (пол)
    int people_in_bathroom; // количество людей в ванной
} BathroomInfo;

sem_t semaph; // семафор
pthread_mutex_t mutex;  // мьютекс
pthread_cond_t cond; // condition variable

BathroomInfo* bathroom; // ванная 
int max_usage_time = 10, users_amt = 10, cabins = 5;

void parse_args(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "s:u:c:")) != -1) {
        switch (opt) {
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
                        "Usage: %s [-s students_amount] [-u max_bathroom_usage_time] [-c cabins_amount]\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (users_amt <= 0 || max_usage_time <= 0 || cabins <= 0) {
        fprintf(stderr, "All parameters must be positive\n");
        exit(EXIT_FAILURE);
    }
}


void* student(void* arg)
{
    Student* student_info = (Student *) arg; // пол студента
    printf("Student %d (%s) wants to go in\n", student_info->id, student_info->gender == MALE ? "male" : "female" );

    pthread_mutex_lock(&mutex); // захватили мьютекс
    
    while (bathroom->current_gender != student_info->gender && bathroom->people_in_bathroom > 0) 
    // ждём условие: либо ванная опустела, либо пол стал подходить
    {
        pthread_cond_wait(&cond, &mutex);
    }

    pthread_mutex_unlock(&mutex); // освободили мьютекс

    sem_wait(&semaph); // захватили семафор 

    pthread_mutex_lock(&mutex); // захватили мьютекс
    printf("Student %d (%s) is using the bathroom\n", student_info->id, student_info->gender == MALE ? "male" : "female" );

    bathroom->people_in_bathroom += 1; // увеличиваем количество людей в кабинке
    if (bathroom->people_in_bathroom == 1) // если это первый человек в ванной - устанавливаем пол
    {
        bathroom->current_gender = student_info->gender;
    }

    printf("\tPeople in bathroom currently: %d\n", bathroom->people_in_bathroom);

    pthread_mutex_unlock(&mutex); // освободили мьютекс
    
    sleep(student_info->usage_time); // используем ванную

    printf("Student %d (%s) has finished using the bathroom\n", student_info->id, student_info->gender == MALE ? "male" : "female" );

    pthread_mutex_lock(&mutex); // захватили мьютекс

    bathroom->people_in_bathroom -= 1; // освобождаем ванную - уменьшаем количество людей
    if (bathroom->people_in_bathroom == 0) // если мы были последним человеком - будим всех
    {
        bathroom->current_gender = NONE;
        pthread_cond_broadcast(&cond); 
    }
    
    pthread_mutex_unlock(&mutex); // освободили мьютекс
    sem_post(&semaph); // освободили семафор

    return NULL;
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);

    srand(time(NULL));
    setbuf(stdout, 0);

    bathroom = malloc(sizeof(BathroomInfo));

    bathroom->current_gender = NONE;
    bathroom->people_in_bathroom = 0;

    pthread_t students[users_amt];

    Student* students_info = malloc(users_amt * sizeof(Student));
    for (size_t i = 0; i < users_amt; i++)
    {
        Student* s = &students_info[i];
        s->id = i;
        s->gender = (rand() % 2) ? MALE : FEMALE;
        s->usage_time = rand() % max_usage_time + 1;
    }

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    sem_init(&semaph, 0, cabins);
    
    for (int i = 0; i < users_amt; i++) {
        pthread_create(&students[i], NULL, student, &students_info[i]);
    }

    for (int i = 0; i < users_amt; i++) {
        pthread_join(students[i], NULL);
    }

    sem_destroy(&semaph);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    free(students_info);
    free(bathroom);

    return 0;
}
