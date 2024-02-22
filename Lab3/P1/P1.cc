#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/neutrino.h>

#include <unistd.h>
#define GetCurrentDir getcwd

using std::cout;
using std::endl;
using std::cos;
using std::log;

// Имя именованной памяти
#define NAMED_MEMORY "/9/namedMemory"

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
// Выполнение расчёта функции в единицу времени
double func(double t);
// Устанвока таймера завершения работы
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
// Обработка сигнала завершения работы
void deadHandler(int signo);
// Отправляет P0 сообщение о готовности к продолжению работы
void sendReadyMessageToP0(char *chIdP0Str);

int main(int argc, char *argv[]) {
	cout << "P1: Запущен" << endl;
	cout << "P1: Параметры: " << "argv[0]=  " << argv[0] <<  endl;

	struct NamedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);
	cout << "P1: Присоединился к именованной памяти" << endl;

	//вызов дочернего процесса P2
	int pidP2 = spawnl( P_NOWAIT, "/home/host/Lab3/P2/x86/o/P2",(char*)argv[0], NULL);
	if (pidP2 < 0){
		cout << "P1: Ошибка запуска процесса P2 " << strerror(pidP2) << endl;
		exit(EXIT_FAILURE);
	}
	cout << "P1: pid процесса P2 - " << pidP2 <<  endl;

	namedMemoryPtr->pidP2 = pidP2;

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, namedMemoryPtr->tickSigusrP1);

	timer_t stopTimer;
	struct itimerspec stopPeriod;
	setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	sendReadyMessageToP0(argv[0]);

	cout << "P1: У барьера" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P1: Прошёл барьер" << endl;

	// запуск таймера завершения
	int res = timer_settime(stopTimer, 0, &stopPeriod, NULL);
	if(res == -1){
		cout << "P1: Ошибка запуска таймера" << strerror(res)<< endl;
	}

	// величина тика из нсек в сек 180000000 нсек -> 0,03 сек
	const double tickSecDuration = namedMemoryPtr->timeInfo.durTickT / 1000000000.;

	while(true){
		// Ожидание сигнала
		int sig = SignalWaitinfo(&set, NULL);
		if(sig == namedMemoryPtr->tickSigusrP1){

			double time = namedMemoryPtr->timeInfo.countTickT * tickSecDuration;
			double value = func(time);

			pthread_mutex_lock(&(namedMemoryPtr->mutex));
			namedMemoryPtr->p = value;
			pthread_mutex_unlock(&(namedMemoryPtr->mutex));
		}

	}
	return EXIT_SUCCESS;
}

// Выполнение расчёта функции в единицу времени
double func(double t) {
	const double a = 3.5;
	double sc1 = pow(M_E, -1 * t);
	double sc2 = a *sc1;
	double sc3 = 1 + sc2;
	double result = a * t * sc3;
	return result;
}

// Функция присоединения к процессу именованной памяти
struct NamedMemory* connectToNamedMemory(const char* name) {
	struct NamedMemory *namedMemoryPtr;

	//дескриптор именованной памяти
	int fd = shm_open(name, O_RDWR, 0777);
	if(fd == -1){
		cout << "P1: Ошибка открытия объекта именованной памяти - " << strerror(fd) <<  endl;
	    exit(EXIT_FAILURE);
	}

	namedMemoryPtr = (NamedMemory*) mmap(NULL, sizeof(struct NamedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(namedMemoryPtr == MAP_FAILED){
		cout << "P1: Ошибка сопоставления вир. адр. пространства" <<  endl;
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
		cout << "P1: Ошибка создания таймера остановки " << strerror(res1)<< endl;
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
		cout << "P1: пришёл сигнал завершения процесса" <<  endl;
		exit(EXIT_SUCCESS);
	}
}

// Отправляет P0 сообщение о готовности к продолжению работы
void sendReadyMessageToP0(char *chIdP0Str){
	char rmsg[20];
	int chIdP0 = atoi(chIdP0Str);
	cout << "P1: установление соединения с каналом P0" <<  endl;
	int coidP0 = ConnectAttach(0, getppid(), chIdP0, _NTO_SIDE_CHANNEL, 0);
	if(coidP0 == -1){
		cout << "P1: Ошибка соединения с каналом P0 - " << strerror(coidP0) <<  endl;
	    exit(EXIT_FAILURE);
	}
	cout << "Р1: Посылаю сообщение Р0" <<  endl;
	char *smsg1 = (char *)"P1";
	int sendRes = MsgSend(coidP0, smsg1, strlen(smsg1) + 1, rmsg, sizeof(rmsg));
	if(sendRes == -1){
		cout << "P1: Ошибка MsgSend при отправки в P0 - " << strerror(sendRes) <<  endl;
		exit(EXIT_FAILURE);
	}
}
