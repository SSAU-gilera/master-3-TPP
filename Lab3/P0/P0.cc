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

// ������������ ���� 1t (����������� ��������� - 0,03 c) (�����������)
#define DUR_TICK_T 30000000
// ������������ ���� dt (����������� �������� - 0,2 �) (�����������)
#define DUR_TICK_DT 200000000
// ����� ������ ���������� (���)
#define END_TIME 5 //83 5
// ����� ������� ����������� ������ ���� (�����������)
#define TICK_SIGUSR_P1 SIGUSR1

// ��� ����������� ������
#define NAMED_MEMORY "/9/namedMemory"

// ---- ��������� ��� ����������� ������ ---- //
// ��������� ������ � ����������� � ������� ������� ����������
struct Clock {
	long durTickT; 		// ������������ ������ ���� � ������������
	long durTickDt; 	// ������������ ������ ���� � ������������
	int countTickDt;	// ����� �������� ���� ����� ���
	int countTickT;		// ����� �������� ���� ����� ���
	long endTime;		// ������������ ������ ���������� � ��������
};
// ��������� ������, ���������� � ����������� ������ NAMED_MEMORY
struct NamedMemory {
	double p;							// ����������� ��������

	int pidP0; 							// ID �������� P0
	int pidP2; 							// ID �������� P2
	int pidP1; 							// ID �������� P1
	int signChIdP2;						// ID ������ �������� P1
	int tickSigusrP1;					// ����� ������� ����������� ������ ���� (�����������)

	pthread_mutexattr_t mutexAttr;	 	// ���������� ������ �������
	pthread_mutex_t mutex;			 	// ������ ������� � ����������� ������

	pthread_barrier_t startBarrier;		// ������ ������ ��������

	Clock timeInfo;						// ���������� � ������� ������� ���
};
// ---- ��������� ��� ����������� ������ ---- //

// �������� ����������� ������
NamedMemory *createNamedMemory(const char* name);
// ��������� �������������� ������� ��� �������� ���������
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int sigChId, long tick);
// ��������� ������� ���������� ������
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
// ��������� ������� ���������� ������
void deadHandler(int signo);

// ������� ������ T1
void* funcT1(void* args);
// ������� ������ T2
void* funcT2(void* args);

// ���������� T1
pthread_t threadT1;
// ���������� T2
pthread_t threadT2;

// ��������� ����������� ������
struct NamedMemory *namedMemoryPtr;

int main(int argc, char *argv[]) {
	cout << "P0: �������" << endl;

	//�������� ������
	int chId = ChannelCreate(_NTO_CHF_SENDER_LEN);
	char buffer[20];
	const char *chIdStr = itoa(chId, buffer, 10);
	cout << "P0: ����� ������: chId = " << chId << endl;

	// ������������� ����������� ������
	namedMemoryPtr = createNamedMemory(NAMED_MEMORY);
	cout << "P0: ����������� ������ �������" << endl;

	// ��������� ���������� ������� ����������
	namedMemoryPtr->timeInfo.durTickT = DUR_TICK_T;
	namedMemoryPtr->timeInfo.durTickDt = DUR_TICK_DT;
	namedMemoryPtr->timeInfo.endTime = END_TIME;
	// ��������� ����� ��� � �����, -1 - ���� �� ��������
	namedMemoryPtr->timeInfo.countTickT = -1;
	namedMemoryPtr->timeInfo.countTickDt = -1;

	namedMemoryPtr->tickSigusrP1 = TICK_SIGUSR_P1;

	// ������ ��� ������������� ������ �������� � ���������
	pthread_barrierattr_t startAttr;
	pthread_barrierattr_init(&startAttr);
	pthread_barrierattr_setpshared(&startAttr, PTHREAD_PROCESS_SHARED);
	pthread_barrier_init(&(namedMemoryPtr->startBarrier), &startAttr, 5);

	// ������������� ���������� ������ ������������ �������
	int r1 = pthread_mutexattr_init(&namedMemoryPtr->mutexAttr);
	if(r1 != EOK){
		cout << "�0: ������ pthread_mutexattr_init: " << strerror(errno) << endl;
		return EXIT_FAILURE;
	}
	 // ���������� � ���������� ������ ������� �������� "�����������"
	int r2 = pthread_mutexattr_setpshared(&namedMemoryPtr->mutexAttr, PTHREAD_PROCESS_SHARED);
	if(r2 != EOK){
		cout << "�0: ������ pthread_mutexattr_setpshared: " << strerror(errno) << endl;
		return EXIT_FAILURE;
	}
	// ������������� ������������ �������
	int r3 = pthread_mutex_init(&namedMemoryPtr->mutex, &namedMemoryPtr->mutexAttr);
	if(r3 != EOK){
		cout << "�0: ������ pthread_mutex_init: " << strerror(errno) << endl;
		return EXIT_FAILURE;
	}

	//����� ��������� �������� P1
	int pidP1 = spawnl( P_NOWAIT, "/home/host/Lab3/P1/x86/o/P1", chIdStr, NULL);
	if (pidP1 < 0){
		cout << "P0: ������ ������� �������� P1 " << strerror(pidP1) << endl;
		exit(EXIT_FAILURE);
	}
	cout << "P0: pid �������� P1 - " << pidP1 <<  endl;

	namedMemoryPtr->pidP1 = pidP1;

	namedMemoryPtr->pidP0 = getpid();

	int count = 0;
	while(count < 2){
		char msg[20];
		_msg_info info;
	    int rcvid = MsgReceive(chId, msg, sizeof(msg), &info);
	    if(rcvid == -1){
	    	 cout << "P0: ������ MsgReceive - " << strerror(rcvid) << endl;
	    }

	    cout << "P0: �������� ��������� �� " << info.pid <<  endl;
	    MsgReply(rcvid, NULL, msg, sizeof(msg));
	    count++;
	}

	int threadT1Res = pthread_create(&threadT1, NULL, funcT1, NULL);
	if(threadT1Res != 0){
		cout << "P0: ������ ������ T1 " << strerror(threadT1Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "P0: ����� T1 ������ - " << threadT1 <<  endl;

	int threadT2Res = pthread_create(&threadT2, NULL, funcT2, NULL);
	if(threadT2Res != 0){
		cout << "P0: ������ ������ T2 " << strerror(threadT2Res) <<  endl;
		return EXIT_FAILURE;
	}
	cout << "P0: ����� T2 ������ - " << threadT2 <<  endl;

	timer_t stopTimer;
	struct itimerspec stopPeriod;
	setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	cout << "P0: � �������" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P0: ������ ������" << endl;

	// ������ ������� ����������
	int res = timer_settime(stopTimer, 0, &stopPeriod, NULL);
	if(res == -1){
		cout << "P0: ������ ������� �������" << strerror(res)<< endl;
	}

	while(true){ }

	return EXIT_SUCCESS;
}

// ������� ������ T1
void* funcT1(void* args) {
	cout << "P0-T1: �����" << endl;

    int sigChId = ChannelCreate(_NTO_CHF_SENDER_LEN);
    cout << "P0-T1: ����� ������: sigChId = " << sigChId << endl;

    timer_t periodicTimer;
    struct itimerspec periodicTick;
    setPeriodicTimer(&periodicTimer, &periodicTick, sigChId, namedMemoryPtr->timeInfo.durTickT);

	cout << "P0-T1: � �������" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P0-T1: ������ ������" << endl;

	int res = timer_settime(periodicTimer, 0, &periodicTick, NULL);
	if(res == -1){
		cout << "P0-T1: ������ ������� �������������� ������� - " << strerror(res)<< endl;
	}

	while(true){
		MsgReceivePulse(sigChId, NULL, 0, NULL);
		namedMemoryPtr->timeInfo.countTickT++;

		// ���������� ������ P1
		kill(namedMemoryPtr->pidP1, namedMemoryPtr->tickSigusrP1);
	}
}

// ������� ������ T2
void* funcT2(void* args) {
	cout << "P0-T2: �����" << endl;

    int sigChId = ChannelCreate(_NTO_CHF_SENDER_LEN);
    cout << "P0-T2: ����� ������: sigChId = " << sigChId << endl;

	int p1TickCoid = ConnectAttach(0, namedMemoryPtr->pidP2, namedMemoryPtr->signChIdP2, _NTO_SIDE_CHANNEL, 0);
	if(p1TickCoid < 0){
		cout << "P0-T2: ������ ��������� ���������� c P2 - " << strerror(p1TickCoid) <<  endl;
	    exit(EXIT_FAILURE);
	}

    timer_t periodicTimer;
    struct itimerspec periodicTick;
    setPeriodicTimer(&periodicTimer, &periodicTick, sigChId, namedMemoryPtr->timeInfo.durTickDt);

	cout << "P0-T2: � �������" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P0-T2: ������ ������" << endl;

	int res = timer_settime(periodicTimer, 0, &periodicTick, NULL);
	if(res == -1){
		cout << "P0-T2: ������ ������� �������������� ������� - " << strerror(res)<< endl;
	}

	while(true){
		MsgReceivePulse(sigChId, NULL, 0, NULL);
		namedMemoryPtr->timeInfo.countTickDt++;;

	    // ���������� ������� ���� �������� P1: ��������� - 10, ��� - 10, �������� - 10
	    MsgSendPulse(p1TickCoid, 10, 10, 10);
	}
}

// ��������� �������������� ������� ��� �������� ���������
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int sigChId, long tick){
	// ���������� ��� ��������� �����������
	int coid = ConnectAttach(0, 0, sigChId, 0, _NTO_COF_CLOEXEC);
	if(coid ==-1){
		cout << "P0: ������ ��������� ���������� ������ � �������� - " << strerror(coid) <<  endl;
	    exit(EXIT_FAILURE);
	}

	// ��������
	struct sigevent event;
	SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);

	timer_create(CLOCK_REALTIME, &event,  periodicTimer);

	// ���������� �������� ������������ �������������� ������� ���� � ��������� �������
	periodicTimerStruct->it_value.tv_sec = 0;
	periodicTimerStruct->it_value.tv_nsec = tick;
	periodicTimerStruct->it_interval.tv_sec = 0;
	periodicTimerStruct->it_interval.tv_nsec =  tick;
}

// �������� ����������� ������
NamedMemory *createNamedMemory(const char* name){
	struct NamedMemory *namedMemoryPtr;

	//���������� ����������� ������
	int fd = shm_open(name, O_RDWR | O_CREAT, 0777);
	if(fd == -1){
		cout << "P0: ������ ��������/�������� ������� ����������� ������ - " << strerror(fd) <<  endl;
	    exit(EXIT_FAILURE);
	}

	int tr1 = ftruncate(fd, 0);
	int tr2 = ftruncate(fd, sizeof(struct NamedMemory));
	if(tr1 == -1 || tr2 == -1){
		cout << "P0: ������ ftruncate" <<  endl;
		exit(EXIT_FAILURE);
	}

	namedMemoryPtr = (NamedMemory*) mmap(NULL, sizeof(struct NamedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(namedMemoryPtr == MAP_FAILED){
		cout << "P0: ������ ������������� ���. ���. ������������" <<  endl;
	    exit(EXIT_FAILURE);
	}

	return namedMemoryPtr;
}

// ��������� ������� ���������� ������
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime) {
	struct sigevent event;
	SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	int res1 = timer_create(CLOCK_REALTIME, &event, stopTimer);
	if(res1 == -1){
		cout << "P0: ������ �������� ������� ��������� " << strerror(res1)<< endl;
	}
	stopPeriod->it_value.tv_sec = endTime;
	stopPeriod->it_value.tv_nsec = 0;
	stopPeriod->it_interval.tv_sec = 0;
	stopPeriod->it_interval.tv_nsec = 0;

   	struct sigaction act;
   	sigset_t set;
   	sigemptyset(&set);
   	sigaddset(&set, SIGUSR2);
   	act.sa_flags = 0; //��������� ��������� ��������� �������
   	act.sa_mask = set;
   	act.__sa_un._sa_handler = &deadHandler; //������������ ������ ��� �����������
   	sigaction(SIGUSR2, &act, NULL);
}

// ��������� ������� ���������� ������
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		cout << "P0: ������ ������ ���������� ��������" <<  endl;
		pthread_barrier_destroy(&(namedMemoryPtr->startBarrier));
		pthread_abort(threadT1);
		pthread_abort(threadT2);
		exit(EXIT_SUCCESS);
	}
}

