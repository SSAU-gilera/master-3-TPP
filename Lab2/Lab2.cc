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

// ����������� ��������. ������ ����������. �������������

#define SEMAPHORE_NAME "/lab2_semaphore_name"

// ������ ������ ������� � ��������� ������� ������
void appendChar(char *text, char c);
// ������� ������ T1
void* funcT1(void* args);
// ������� ������ T2
void* funcT2(void* args);

// ������� //
sem_t* sem;

// ������ ���������� //
// ���� ���������� ������ ����� T1
bool isDoneT1 = false;

// ������������ �����
char* mainText = (char*) "Text0, ";
char* t1Text = (char*) "Text1, ";
char* t2Text = (char*) "Text2.\n ";

int main(int argc, char *argv[]) {
	cout << "Main: �����" << endl;

	// ����� ������������ ������
	char textBuf[200] = {};

	// ���, ����������� ����, ������� �������, ��������� ��������
	sem = sem_open(SEMAPHORE_NAME, O_CREAT, 0777, 0);
	if (sem == SEM_FAILED ) {
		perror("Main: ������ �������� ������������ ���������");
		return EXIT_FAILURE;
	 }

	// ���������� ����, ���������� ������, ������� ����, ��������� - ����� ������
	pthread_t threadT1;
	int threadT1Res = pthread_create(&threadT1, NULL, funcT1, (void*) textBuf);
	if(threadT1Res != 0){
		cout << "Main: ������ ������ T1 " << strerror(threadT1Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "Main: ����� T1 ������ - " << threadT1 <<  endl;

	// ���������� ����, ���������� ������, ������� ����, ��������� - ����� ������
	pthread_t threadT2;
	int threadT2Res = pthread_create(&threadT2, NULL, funcT2, (void*) textBuf);
	if(threadT2Res != 0){
		cout << "Main: ������ ������ T2 " << strerror(threadT2Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "Main: ����� T2 ������ - " << threadT2 <<  endl;

	for (unsigned int i = 0; i < strlen(mainText); i++) {
		appendChar(textBuf, mainText[i]);
	}

	cout << "Main: �������� ������� ���������" << endl;
	int semPost = sem_post(sem);

	cout << "Main: ������������� � T2" <<  endl;
	pthread_join(threadT2, NULL);
	cout << "Main: T2 ����������" <<  endl;

	size_t p = strlen(textBuf);
	cout << "Main: ����� ������: ";
	for(int i = 0; i < p; i++){
		cout << textBuf[i];
	}
	cout << endl;

	sem_close(sem);
	cout << "Main: Ok" <<  endl;
	return EXIT_SUCCESS;
}

// ������� ������ T1
void* funcT1(void* args) {
	cout << "T1: �����" << endl;

	char* textBuf = (char*) args;

	cout << "T1: ������� ��������� ������� ��������� (����������)" <<  endl;
	sem_wait(sem) ;

	pthread_sleepon_lock();
	cout << "T1: ��������� � ������" <<  endl;

	for (unsigned int i = 0; i < strlen(t1Text); i++) {
		appendChar(textBuf, t1Text[i]);
	}

	cout << "T1: �������� ������ ������" <<  endl;
	isDoneT1 = true;

	pthread_sleepon_signal(&isDoneT1);

	pthread_sleepon_unlock();



	cout << "T1: Ok" <<  endl;
	return EXIT_SUCCESS;
}

// ������� ������ T2
void* funcT2(void* args) {
	cout << "T2: �����" << endl;

	char* textBuf = (char*) args;

	pthread_sleepon_lock();
	// �������� ���� �� ����� � �������� ����� �����
	while(isDoneT1 == false){
		cout << "T2: �������� T1" << endl;
		pthread_sleepon_wait(&isDoneT1);
	}

	pthread_sleepon_unlock();

	cout << "T2: ��������� � ������" << endl;
	for (unsigned int i = 0; i < strlen(t2Text); i++) {
		appendChar(textBuf, t2Text[i]);
	}

	cout << "T2: Ok" <<  endl;
	return EXIT_SUCCESS;
}

// ������ ������ ������� � ��������� ������� ������
void appendChar(char *text, char c) {
	size_t p = strlen(text);

	text[p] = c;
	text[p + 1] = '\0';

	for (int i = 0; i < 100000000; i++);
	sleep(1);
}
