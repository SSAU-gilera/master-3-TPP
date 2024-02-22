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

// ������������� ����������� ������
struct NamedMemory *connectToNamedMemory(const char* name);
// ���������� ������� ������� � ������� �������
double func(double t);
// ��������� ������� ���������� ������
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
// ��������� ������� ���������� ������
void deadHandler(int signo);
// ���������� P0 ��������� � ���������� � ����������� ������
void sendReadyMessageToP0(char *chIdP0Str);

int main(int argc, char *argv[]) {
	cout << "P1: �������" << endl;
	cout << "P1: ���������: " << "argv[0]=  " << argv[0] <<  endl;

	struct NamedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);
	cout << "P1: ������������� � ����������� ������" << endl;

	//����� ��������� �������� P2
	int pidP2 = spawnl( P_NOWAIT, "/home/host/Lab3/P2/x86/o/P2",(char*)argv[0], NULL);
	if (pidP2 < 0){
		cout << "P1: ������ ������� �������� P2 " << strerror(pidP2) << endl;
		exit(EXIT_FAILURE);
	}
	cout << "P1: pid �������� P2 - " << pidP2 <<  endl;

	namedMemoryPtr->pidP2 = pidP2;

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, namedMemoryPtr->tickSigusrP1);

	timer_t stopTimer;
	struct itimerspec stopPeriod;
	setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	sendReadyMessageToP0(argv[0]);

	cout << "P1: � �������" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P1: ������ ������" << endl;

	// ������ ������� ����������
	int res = timer_settime(stopTimer, 0, &stopPeriod, NULL);
	if(res == -1){
		cout << "P1: ������ ������� �������" << strerror(res)<< endl;
	}

	// �������� ���� �� ���� � ��� 180000000 ���� -> 0,03 ���
	const double tickSecDuration = namedMemoryPtr->timeInfo.durTickT / 1000000000.;

	while(true){
		// �������� �������
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

// ���������� ������� ������� � ������� �������
double func(double t) {
	const double a = 3.5;
	double sc1 = pow(M_E, -1 * t);
	double sc2 = a *sc1;
	double sc3 = 1 + sc2;
	double result = a * t * sc3;
	return result;
}

// ������� ������������� � �������� ����������� ������
struct NamedMemory* connectToNamedMemory(const char* name) {
	struct NamedMemory *namedMemoryPtr;

	//���������� ����������� ������
	int fd = shm_open(name, O_RDWR, 0777);
	if(fd == -1){
		cout << "P1: ������ �������� ������� ����������� ������ - " << strerror(fd) <<  endl;
	    exit(EXIT_FAILURE);
	}

	namedMemoryPtr = (NamedMemory*) mmap(NULL, sizeof(struct NamedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(namedMemoryPtr == MAP_FAILED){
		cout << "P1: ������ ������������� ���. ���. ������������" <<  endl;
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
		cout << "P1: ������ �������� ������� ��������� " << strerror(res1)<< endl;
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

// ��������� ������� ���������� ������
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		cout << "P1: ������ ������ ���������� ��������" <<  endl;
		exit(EXIT_SUCCESS);
	}
}

// ���������� P0 ��������� � ���������� � ����������� ������
void sendReadyMessageToP0(char *chIdP0Str){
	char rmsg[20];
	int chIdP0 = atoi(chIdP0Str);
	cout << "P1: ������������ ���������� � ������� P0" <<  endl;
	int coidP0 = ConnectAttach(0, getppid(), chIdP0, _NTO_SIDE_CHANNEL, 0);
	if(coidP0 == -1){
		cout << "P1: ������ ���������� � ������� P0 - " << strerror(coidP0) <<  endl;
	    exit(EXIT_FAILURE);
	}
	cout << "�1: ������� ��������� �0" <<  endl;
	char *smsg1 = (char *)"P1";
	int sendRes = MsgSend(coidP0, smsg1, strlen(smsg1) + 1, rmsg, sizeof(rmsg));
	if(sendRes == -1){
		cout << "P1: ������ MsgSend ��� �������� � P0 - " << strerror(sendRes) <<  endl;
		exit(EXIT_FAILURE);
	}
}
