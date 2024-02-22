#include <cstdlib>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pthread.h>

#include <semaphore.h>
#include <fcntl.h>

#include <time.h>

#include <unistd.h>
#define GetCurrentDir getcwd

using std::cout;
using std::endl;

// Именованные семафоры. Ждущие блокировки. Присоединение

#define SEMAPHORE_NAME "/lab2_semaphore_name"

// Запись одного символа с имитацией времени записи
void appendChar(char *text, char c);
// Функция потока T1
void* funcT1(void* args);
// Функция потока T2
void* funcT2(void* args);

// Семафор //
sem_t* sem;

// Ждущие блокировки //
// Флаг завершения записи нитью T1
bool isDoneT1 = false;

// Записываемый текст
char* mainText = (char*) "Text0, ";
char* t1Text = (char*) "Text1, ";
char* t2Text = (char*) "Text2.\n ";

int main(int argc, char *argv[]) {
	cout << "Main: Старт" << endl;

	// Буфер формируемого текста
	char textBuf[200] = {};

	// имя, управляющий флаг, уровень доступа, начальное значение
	sem = sem_open(SEMAPHORE_NAME, O_CREAT, 0777, 0);
	if (sem == SEM_FAILED ) {
		perror("Main: Ошибка создания именованного семаформа");
		return EXIT_FAILURE;
	 }

	// дескриптор нити, атрибутная запись, функция нити, аргументы - буфер текста
	pthread_t threadT1;
	int threadT1Res = pthread_create(&threadT1, NULL, funcT1, (void*) textBuf);
	if(threadT1Res != 0){
		cout << "Main: Ошибка старта T1 " << strerror(threadT1Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "Main: Поток T1 создан - " << threadT1 <<  endl;

	// дескриптор нити, атрибутная запись, функция нити, аргументы - буфер текста
	pthread_t threadT2;
	int threadT2Res = pthread_create(&threadT2, NULL, funcT2, (void*) textBuf);
	if(threadT2Res != 0){
		cout << "Main: Ошибка старта T2 " << strerror(threadT2Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "Main: Поток T2 создан - " << threadT2 <<  endl;

	for (unsigned int i = 0; i < strlen(mainText); i++) {
		appendChar(textBuf, mainText[i]);
	}

	cout << "Main: Увеличил счётчик семаформа" << endl;
	int semPost = sem_post(sem);

	cout << "Main: Присоединение к T2" <<  endl;
	pthread_join(threadT2, NULL);
	cout << "Main: T2 завершился" <<  endl;

	size_t p = strlen(textBuf);
	cout << "Main: Буфер сейчас: ";
	for(int i = 0; i < p; i++){
		cout << textBuf[i];
	}
	cout << endl;

	sem_close(sem);
	cout << "Main: Ok" <<  endl;
	return EXIT_SUCCESS;
}

// Функция потока T1
void* funcT1(void* args) {
	cout << "T1: Старт" << endl;

	char* textBuf = (char*) args;

	cout << "T1: Попытка уменьшить счётчик семаформа (блокирвока)" <<  endl;
	sem_wait(sem) ;

	pthread_sleepon_lock();
	cout << "T1: Приступаю к работе" <<  endl;

	for (unsigned int i = 0; i < strlen(t1Text); i++) {
		appendChar(textBuf, t1Text[i]);
	}

	cout << "T1: Закончил запись текста" <<  endl;
	isDoneT1 = true;

	pthread_sleepon_signal(&isDoneT1);

	pthread_sleepon_unlock();



	cout << "T1: Ok" <<  endl;
	return EXIT_SUCCESS;
}

// Функция потока T2
void* funcT2(void* args) {
	cout << "T2: Старт" << endl;

	char* textBuf = (char*) args;

	pthread_sleepon_lock();
	// Проверка надо ли ждать и ожидание смены флага
	while(isDoneT1 == false){
		cout << "T2: Ожидание T1" << endl;
		pthread_sleepon_wait(&isDoneT1);
	}

	pthread_sleepon_unlock();

	cout << "T2: Приступаю к работе" << endl;
	for (unsigned int i = 0; i < strlen(t2Text); i++) {
		appendChar(textBuf, t2Text[i]);
	}

	cout << "T2: Ok" <<  endl;
	return EXIT_SUCCESS;
}

// Запись одного символа с имитацией времени записи
void appendChar(char *text, char c) {
	size_t p = strlen(text);

	text[p] = c;
	text[p + 1] = '\0';

	for (int i = 0; i < 100000000; i++);
	sleep(1);
}
