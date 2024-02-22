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

// ��� ����������� ������
#define NAMED_MEMORY "/9/namedMemory"
// ���� ��� ������ �������
#define TREND_FILE "/home/host/Lab3/Trend/trend.txt"

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
// ��������� ������� ���������� ������
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
// ��������� ������� ���������� ������
void deadHandler(int signo);
// ���������� P0 ��������� � ���������� � ����������� ������
void sendReadyMessageToP0(char *chIdP0Str, int pidP0);

FILE* trendFile;

int main(int argc, char *argv[]) {
	cout << "P2: �������" << endl;
	cout << "P2: ���������: " << "argv[0]=  " << argv[0] <<  endl;

	struct NamedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);
	cout << "P2: ������������� � ����������� ������" << endl;

	trendFile = fopen(TREND_FILE, "w");
	if(trendFile == NULL){
		cout << "P2: ������ �������� ����� ��� ������ ������" << endl;
		exit(EXIT_FAILURE);
	}
	cout << "�2: ������ ���� ������ trend.txt" << endl;

	//�������� ������
	int signChIdP2 = ChannelCreate(_NTO_CHF_SENDER_LEN);
	namedMemoryPtr->signChIdP2 = signChIdP2;

	timer_t stopTimer;
	struct itimerspec stopPeriod;
	setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	sendReadyMessageToP0(argv[0], namedMemoryPtr->pidP0);

	cout << "P2: � �������" << endl;
	pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	cout << "P2: ������ ������" << endl;

	// ������ ������� ����������
	int res = timer_settime(stopTimer, 0, &stopPeriod, NULL);
	if(res == -1){
		cout << "P2: ������ ������� �������" << strerror(res)<< endl;
	}

	// �������� ���� �� ���� � ��� 400000000 ���� -> 0,2 ���
	const double tickSecDuration = namedMemoryPtr->timeInfo.durTickDt / 1000000000.;
	//cout << "P2: tickSecDuration - " << tickSecDuration << endl;

	while(true){
		// ������� �������
		MsgReceivePulse(signChIdP2, NULL, 0, NULL);

		pthread_mutex_lock(&(namedMemoryPtr->mutex));
		double value = namedMemoryPtr->p;
		pthread_mutex_unlock(&(namedMemoryPtr->mutex));

		double time = namedMemoryPtr->timeInfo.countTickDt * tickSecDuration;

		fprintf(trendFile, "%f\t%f\n", value, time);
	}

	return EXIT_SUCCESS;
}

// ������� ������������� � �������� ����������� ������
struct NamedMemory* connectToNamedMemory(const char* name) {
	struct NamedMemory *namedMemoryPtr;

	//���������� ����������� ������
	int fd = shm_open(name, O_RDWR, 0777);
	if(fd == -1){
		cout << "P2: ������ �������� ������� ����������� ������ - " << strerror(fd) <<  endl;
	    exit(EXIT_FAILURE);
	}

	namedMemoryPtr = (NamedMemory*) mmap(NULL, sizeof(struct NamedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(namedMemoryPtr == MAP_FAILED){
		cout << "P2: ������ ������������� ���. ���. ������������" <<  endl;
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
		cout << "P2: ������ �������� ������� ��������� " << strerror(res1)<< endl;
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
		cout << "P2: ������ ������ ���������� ��������" <<  endl;
		fclose(trendFile);
		exit(EXIT_SUCCESS);
	}
}

// ���������� P0 ��������� � ���������� � ����������� ������
void sendReadyMessageToP0(char *chIdP0Str, int pidP0){
	char rmsg[20];
	int chIdP0 = atoi(chIdP0Str);
	cout << "P2: ������������ ���������� � ������� P0" <<  endl;
	int coidP0 = ConnectAttach(0, pidP0, chIdP0, _NTO_SIDE_CHANNEL, 0);
	if(coidP0 == -1){
		cout << "P2: ������ ���������� � ������� P0 - " << strerror(coidP0) <<  endl;
	    exit(EXIT_FAILURE);
	}
	cout << "�2: ������� ��������� �0" <<  endl;
	char *smsg1 = (char *)"P1";
	int sendRes = MsgSend(coidP0, smsg1, strlen(smsg1) + 1, rmsg, sizeof(rmsg));
	if(sendRes == -1){
		cout << "P2: ������ MsgSend ��� �������� � P0 - " << strerror(sendRes) <<  endl;
		exit(EXIT_FAILURE);
	}
}
