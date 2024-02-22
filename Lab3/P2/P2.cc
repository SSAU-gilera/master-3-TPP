#include <cstdlib>
#include <iostream>
#include <string.h>
#include <process.h>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>

#include <unistd.h>
#define GetCurrentDir getcwd

using std::cout;
using std::endl;
using std::FILE;

// Имя именованной памяти
#define NAMED_MEMORY "/9/namedMemory"
// Файл для записи трендов
#define TREND_FILE "/home/host/Lab3/Trend/trend.txt"

// ---- Структуры для именованной памяти ---- //
// Структура данных с информацией о течении времени приложения
struct Clock {
	long durTickT; 		// Длительность одного тика в наносекундах
	long durTickDt; 	// Длительность одного тика в наносекундах
	int countTickDt;	// Номер текущего тика часов ПРВ
	int countTickT;		// Номер текущего тика часов ПРВ
	long endTime;		// Длительность работы приложения в секундах
};
// Структура данных, хранящаяся в именованной памяти NAMED_MEMORY
struct NamedMemory {
	double p;							// Вычисляемый параметр

	int pidP0; 							// ID процесса P0
	int pidP2; 							// ID процесса P2
	int pidP1; 							// ID процесса P1
	int signChIdP2;						// ID канала процесса P1
	int tickSigusrP1;					// Номер сигнала наступления нового тика (уведомления)

	pthread_mutexattr_t mutexAttr;	 	// Атрибутная запись мутекса
	pthread_mutex_t mutex;			 	// Мутекс доступа к именованной памяти

	pthread_barrier_t startBarrier;		// Барьер старта таймеров

	Clock timeInfo;						// Информация о течении времени ПРВ
};
// ---- Структуры для именованной памяти ---- //

// Присоединение именованной памяти
struct NamedMemory *connectToNamedMemory(const char* name);
// Устанвока таймера завершения работы
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
// Обработка сигнала завершения работы
void deadHandler(int signo);
// Отправляет P0 сообщение о готовности к продолжению работы
void sendReadyMessageToP0(char *chIdP0Str, int pidP0);

FILE* trendFile;

int main(int argc, char *argv[]) {
	cout << "P2: Запущен" << endl;
	cout << "P2: Параметры: " << "argv[0]=  " << argv[0] <<  endl;

	struct NamedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);
	cout << "P2: Присоединился к именованной памяти" << endl;

	trendFile = fopen(TREND_FILE, "w");
	if(trendFile == NULL){
		cout << "P2: Ошибка открытия файла для записи тренда" << endl;
		exit(EXIT_FAILURE);
	}
	cout << "Р2: Открыт файл тренда trend.txt" << endl;

	//создание канала
	int signChIdP2 = ChannelCreate(_NTO_CHF_SENDER_LEN);
	namedMemoryPtr->signChIdP2 = signChIdP2;

	timer_t stopTimer;
	struct itimerspec stopPeriod;
	setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	sendReadyMessageToP0(argv[0], namedMemoryPtr->pidP0);

	cout << "P2: У барьера" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P2: Прошёл барьер" << endl;

	// запуск таймера завершения
	int res = timer_settime(stopTimer, 0, &stopPeriod, NULL);
	if(res == -1){
		cout << "P2: Ошибка запуска таймера" << strerror(res)<< endl;
	}

	// величина тика из нсек в сек 400000000 нсек -> 0,2 сек
	const double tickSecDuration = namedMemoryPtr->timeInfo.durTickDt / 1000000000.;
	//cout << "P2: tickSecDuration - " << tickSecDuration << endl;

	while(true){
		// Ожидаем импульс
		MsgReceivePulse(signChIdP2, NULL, 0, NULL);

		pthread_mutex_lock(&(namedMemoryPtr->mutex));
		double value = namedMemoryPtr->p;
		pthread_mutex_unlock(&(namedMemoryPtr->mutex));

		double time = namedMemoryPtr->timeInfo.countTickDt * tickSecDuration;

		fprintf(trendFile, "%f\t%f\n", value, time);
	}

	return EXIT_SUCCESS;
}

// Функция присоединения к процессу именованной памяти
struct NamedMemory* connectToNamedMemory(const char* name) {
	struct NamedMemory *namedMemoryPtr;

	//дескриптор именованной памяти
	int fd = shm_open(name, O_RDWR, 0777);
	if(fd == -1){
		cout << "P2: Ошибка открытия объекта именованной памяти - " << strerror(fd) <<  endl;
	    exit(EXIT_FAILURE);
	}

	namedMemoryPtr = (NamedMemory*) mmap(NULL, sizeof(struct NamedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(namedMemoryPtr == MAP_FAILED){
		cout << "P2: Ошибка сопоставления вир. адр. пространства" <<  endl;
	    exit(EXIT_FAILURE);
	}

	return namedMemoryPtr;
}

// Устанвока таймера завершения работы
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime) {
	struct sigevent event;
	SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	int res1 = timer_create(CLOCK_REALTIME, &event, stopTimer);
	if(res1 == -1){
		cout << "P2: Ошибка создания таймера остановки " << strerror(res1)<< endl;
	}
	stopPeriod->it_value.tv_sec = endTime;
	stopPeriod->it_value.tv_nsec = 0;
	stopPeriod->it_interval.tv_sec = 0;
	stopPeriod->it_interval.tv_nsec = 0;

   	struct sigaction act;
   	sigset_t set;
   	sigemptyset(&set);
   	sigaddset(&set, SIGUSR2);
   	act.sa_flags = 0;
   	act.sa_mask = set;
   	act.__sa_un._sa_handler = &deadHandler;
   	sigaction(SIGUSR2, &act, NULL);
}

// Обработка сигнала завершения работы
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		cout << "P2: пришёл сигнал завершения процесса" <<  endl;
		fclose(trendFile);
		exit(EXIT_SUCCESS);
	}
}

// Отправляет P0 сообщение о готовности к продолжению работы
void sendReadyMessageToP0(char *chIdP0Str, int pidP0){
	char rmsg[20];
	int chIdP0 = atoi(chIdP0Str);
	cout << "P2: установление соединения с каналом P0" <<  endl;
	int coidP0 = ConnectAttach(0, pidP0, chIdP0, _NTO_SIDE_CHANNEL, 0);
	if(coidP0 == -1){
		cout << "P2: Ошибка соединения с каналом P0 - " << strerror(coidP0) <<  endl;
	    exit(EXIT_FAILURE);
	}
	cout << "Р2: Посылаю сообщение Р0" <<  endl;
	char *smsg1 = (char *)"P1";
	int sendRes = MsgSend(coidP0, smsg1, strlen(smsg1) + 1, rmsg, sizeof(rmsg));
	if(sendRes == -1){
		cout << "P2: Ошибка MsgSend при отправки в P0 - " << strerror(sendRes) <<  endl;
		exit(EXIT_FAILURE);
	}
}
