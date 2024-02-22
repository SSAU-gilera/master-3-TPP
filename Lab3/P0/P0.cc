#include <cstdlib>
#include <iostream>
#include <string.h>
#include <process.h>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <unistd.h>
#define GetCurrentDir getcwd

using std::cout;
using std::endl;

// Длительность тика 1t (уведомление импульсом - 0,03 c) (наносекунды)
#define DUR_TICK_T 30000000
// Длительность тика dt (уведомление сигналом - 0,2 с) (наносекунды)
#define DUR_TICK_DT 200000000
// Время работы приложения (сек)
#define END_TIME 5 //83 5
// Номер сигнала наступления нового тика (уведомления)
#define TICK_SIGUSR_P1 SIGUSR1

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

// Создание именованной памяти
NamedMemory *createNamedMemory(const char* name);
// Устанвока переодического таймера для отправки импульсов
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int sigChId, long tick);
// Устанвока таймера завершения работы
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
// Обработка сигнала завершения работы
void deadHandler(int signo);

// Функция потока T1
void* funcT1(void* args);
// Функция потока T2
void* funcT2(void* args);

// Дескриптор T1
pthread_t threadT1;
// Дескриптор T2
pthread_t threadT2;

// Указатель именованной памяти
struct NamedMemory *namedMemoryPtr;

int main(int argc, char *argv[]) {
	cout << "P0: Запущен" << endl;

	//создание канала
	int chId = ChannelCreate(_NTO_CHF_SENDER_LEN);
	char buffer[20];
	const char *chIdStr = itoa(chId, buffer, 10);
	cout << "P0: Канал создан: chId = " << chId << endl;

	// Присоединение именованной памяти
	namedMemoryPtr = createNamedMemory(NAMED_MEMORY);
	cout << "P0: Именованная память создана" << endl;

	// Установка параметров времени приложения
	namedMemoryPtr->timeInfo.durTickT = DUR_TICK_T;
	namedMemoryPtr->timeInfo.durTickDt = DUR_TICK_DT;
	namedMemoryPtr->timeInfo.endTime = END_TIME;
	// показание часов ПРВ в тиках, -1 - часы не запущены
	namedMemoryPtr->timeInfo.countTickT = -1;
	namedMemoryPtr->timeInfo.countTickDt = -1;

	namedMemoryPtr->tickSigusrP1 = TICK_SIGUSR_P1;

	// Барьер для синхронизации старта таймеров в процессах
	pthread_barrierattr_t startAttr;
	pthread_barrierattr_init(&startAttr);
	pthread_barrierattr_setpshared(&startAttr, PTHREAD_PROCESS_SHARED);
	pthread_barrier_init(&(namedMemoryPtr->startBarrier), &startAttr, 5);

	// Инициализация атрибутной записи разделяемого мутекса
	int r1 = pthread_mutexattr_init(&namedMemoryPtr->mutexAttr);
	if(r1 != EOK){
		cout << "Р0: Ошибка pthread_mutexattr_init: " << strerror(errno) << endl;
		return EXIT_FAILURE;
	}
	 // Установить в атрибутной записи мутекса свойство "разделяемый"
	int r2 = pthread_mutexattr_setpshared(&namedMemoryPtr->mutexAttr, PTHREAD_PROCESS_SHARED);
	if(r2 != EOK){
		cout << "Р0: Ошибка pthread_mutexattr_setpshared: " << strerror(errno) << endl;
		return EXIT_FAILURE;
	}
	// Инициализация разделяемого мутекса
	int r3 = pthread_mutex_init(&namedMemoryPtr->mutex, &namedMemoryPtr->mutexAttr);
	if(r3 != EOK){
		cout << "Р0: Ошибка pthread_mutex_init: " << strerror(errno) << endl;
		return EXIT_FAILURE;
	}

	//вызов дочернего процесса P1
	int pidP1 = spawnl( P_NOWAIT, "/home/host/Lab3/P1/x86/o/P1", chIdStr, NULL);
	if (pidP1 < 0){
		cout << "P0: Ошибка запуска процесса P1 " << strerror(pidP1) << endl;
		exit(EXIT_FAILURE);
	}
	cout << "P0: pid процесса P1 - " << pidP1 <<  endl;

	namedMemoryPtr->pidP1 = pidP1;

	namedMemoryPtr->pidP0 = getpid();

	int count = 0;
	while(count < 2){
		char msg[20];
		_msg_info info;
	    int rcvid = MsgReceive(chId, msg, sizeof(msg), &info);
	    if(rcvid == -1){
	    	 cout << "P0: Ошибка MsgReceive - " << strerror(rcvid) << endl;
	    }

	    cout << "P0: Получено сообщение от " << info.pid <<  endl;
	    MsgReply(rcvid, NULL, msg, sizeof(msg));
	    count++;
	}

	int threadT1Res = pthread_create(&threadT1, NULL, funcT1, NULL);
	if(threadT1Res != 0){
		cout << "P0: Ошибка старта T1 " << strerror(threadT1Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "P0: Поток T1 создан - " << threadT1 <<  endl;

	int threadT2Res = pthread_create(&threadT2, NULL, funcT2, NULL);
	if(threadT2Res != 0){
		cout << "P0: Ошибка старта T2 " << strerror(threadT2Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "P0: Поток T2 создан - " << threadT2 <<  endl;

	timer_t stopTimer;
	struct itimerspec stopPeriod;
	setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	cout << "P0: У барьера" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P0: Прошёл барьер" << endl;

	// запуск таймера завершения
	int res = timer_settime(stopTimer, 0, &stopPeriod, NULL);
	if(res == -1){
		cout << "P0: Ошибка запуска таймера" << strerror(res)<< endl;
	}

	while(true){ }

	return EXIT_SUCCESS;
}

// Функция потока T1
void* funcT1(void* args) {
	cout << "P0-T1: Старт" << endl;

    int sigChId = ChannelCreate(_NTO_CHF_SENDER_LEN);
    cout << "P0-T1: Канал создан: sigChId = " << sigChId << endl;

    timer_t periodicTimer;
    struct itimerspec periodicTick;
    setPeriodicTimer(&periodicTimer, &periodicTick, sigChId, namedMemoryPtr->timeInfo.durTickT);

	cout << "P0-T1: У барьера" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P0-T1: Прошёл барьер" << endl;

	int res = timer_settime(periodicTimer, 0, &periodicTick, NULL);
	if(res == -1){
		cout << "P0-T1: Ошибка запуска переодического таймера - " << strerror(res)<< endl;
	}

	while(true){
		MsgReceivePulse(sigChId, NULL, 0, NULL);
		namedMemoryPtr->timeInfo.countTickT++;

		// Отправляем сишнал P1
		kill(namedMemoryPtr->pidP1, namedMemoryPtr->tickSigusrP1);
	}
}

// Функция потока T2
void* funcT2(void* args) {
	cout << "P0-T2: Старт" << endl;

    int sigChId = ChannelCreate(_NTO_CHF_SENDER_LEN);
    cout << "P0-T2: Канал создан: sigChId = " << sigChId << endl;

	int p1TickCoid = ConnectAttach(0, namedMemoryPtr->pidP2, namedMemoryPtr->signChIdP2, _NTO_SIDE_CHANNEL, 0);
	if(p1TickCoid < 0){
		cout << "P0-T2: Ошибка устанвоки соединения c P2 - " << strerror(p1TickCoid) <<  endl;
	    exit(EXIT_FAILURE);
	}

    timer_t periodicTimer;
    struct itimerspec periodicTick;
    setPeriodicTimer(&periodicTimer, &periodicTick, sigChId, namedMemoryPtr->timeInfo.durTickDt);

	cout << "P0-T2: У барьера" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P0-T2: Прошёл барьер" << endl;

	int res = timer_settime(periodicTimer, 0, &periodicTick, NULL);
	if(res == -1){
		cout << "P0-T2: Ошибка запуска переодического таймера - " << strerror(res)<< endl;
	}

	while(true){
		MsgReceivePulse(sigChId, NULL, 0, NULL);
		namedMemoryPtr->timeInfo.countTickDt++;;

	    // Отправляем импульс тика процессу P1: приоритет - 10, код - 10, значение - 10
	    MsgSendPulse(p1TickCoid, 10, 10, 10);
	}
}

// Устанвока переодического таймера для отправки импульсов
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int sigChId, long tick){
	// соединение для импульсов уведомления
	int coid = ConnectAttach(0, 0, sigChId, 0, _NTO_COF_CLOEXEC);
	if(coid ==-1){
		cout << "P0: Ошибка устанвоки соединения канала и процесса - " << strerror(coid) <<  endl;
	    exit(EXIT_FAILURE);
	}

	// импульсы
	struct sigevent event;
	SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);

	timer_create(CLOCK_REALTIME, &event,  periodicTimer);

	// установить интервал срабатывания периодического таймера тика в системном времени
	periodicTimerStruct->it_value.tv_sec = 0;
	periodicTimerStruct->it_value.tv_nsec = tick;
	periodicTimerStruct->it_interval.tv_sec = 0;
	periodicTimerStruct->it_interval.tv_nsec =  tick;
}

// Создание именованной памяти
NamedMemory *createNamedMemory(const char* name){
	struct NamedMemory *namedMemoryPtr;

	//дескриптор именованной памяти
	int fd = shm_open(name, O_RDWR | O_CREAT, 0777);
	if(fd == -1){
		cout << "P0: Ошибка создания/открытия объекта именованной памяти - " << strerror(fd) <<  endl;
	    exit(EXIT_FAILURE);
	}

	int tr1 = ftruncate(fd, 0);
	int tr2 = ftruncate(fd, sizeof(struct NamedMemory));
	if(tr1 == -1 || tr2 == -1){
		cout << "P0: Ошибка ftruncate" <<  endl;
		exit(EXIT_FAILURE);
	}

	namedMemoryPtr = (NamedMemory*) mmap(NULL, sizeof(struct NamedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(namedMemoryPtr == MAP_FAILED){
		cout << "P0: Ошибка сопоставления вир. адр. пространства" <<  endl;
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
		cout << "P0: Ошибка создания таймера остановки " << strerror(res1)<< endl;
	}
	stopPeriod->it_value.tv_sec = endTime;
	stopPeriod->it_value.tv_nsec = 0;
	stopPeriod->it_interval.tv_sec = 0;
	stopPeriod->it_interval.tv_nsec = 0;

   	struct sigaction act;
   	sigset_t set;
   	sigemptyset(&set);
   	sigaddset(&set, SIGUSR2);
   	act.sa_flags = 0; //учитывать последнюю инициацию сигнала
   	act.sa_mask = set;
   	act.__sa_un._sa_handler = &deadHandler; //Используется старый тип обработчика
   	sigaction(SIGUSR2, &act, NULL);
}

// Обработка сигнала завершения работы
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		cout << "P0: пришёл сигнал завершения процесса" <<  endl;
		pthread_barrier_destroy(&(namedMemoryPtr->startBarrier));
		pthread_abort(threadT1);
		pthread_abort(threadT2);
		exit(EXIT_SUCCESS);
	}
}

