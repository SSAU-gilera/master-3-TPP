#include <cstdlib>
#include <iostream>

#include <stdlib.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <process.h>
#include <string.h>

#include <unistd.h>
#define GetCurrentDir getcwd

int main(int argc, char *argv[]) {

	std::cout << "P2: Запущен" << std::endl;

	std::cout << "P2: Параметры: " << "argv[0] = " << argv[0] <<  std::endl;

	int pChid = atoi(argv[0]);

	int chid = ChannelCreate(_NTO_CHF_SENDER_LEN);
	std::cout << "P2: Канал P2 создан: chid = " << chid << std::endl;

	char buffer[20];
	const char *chidStr = itoa(chid, buffer, 10);

	int pidP3 = spawnl(P_NOWAIT, "/home/host/Lab1/M3/x86/o/M3", chidStr, NULL);
	if (pidP3 < 0){
		std::cout << "P2: Ошибка запуска процесса P3" << pidP3 << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "P2: Процесс P3 запущен: pid = " << pidP3 <<  std::endl;

	std::cout << "P2: Установка соединения с каналом P1" <<  std::endl;
	int coidP1 = ConnectAttach(0, getppid(), pChid, _NTO_SIDE_CHANNEL, 0);
	if(coidP1 == -1){
		std::cout << "P2: Ошибка соединения с каналом P1" <<  std::endl;
		exit(EXIT_FAILURE);
	}

	char rmsg1[200] = {};
	if(MsgSend(coidP1, chidStr, strlen(chidStr) + 1, rmsg1, sizeof(rmsg1)) == -1){
		std::cout << "P2: Ошибка MsgSend" <<  std::endl;
		exit(EXIT_FAILURE);
	}

	std::cout << "P2: Старт получения сообщений" << std::endl;
	int countMsg = 0;
	while(countMsg < 3){

		char msg[200] = {};
	    _msg_info info;
	    int rcvid;

	    rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
	    if(rcvid == -1){
	    	std::cout << "P2: Ошибка MsgReceive" << std::endl;
	    }
	    std::cout << "P2: Получено сообщение от " << info.pid << " - " << msg << std::endl;

	    if(countMsg == 0){
	    	int pidP1 = getppid();
	    	char pidP1Buffer[50];
	    	char const *pidP3Str = itoa(pidP1, pidP1Buffer, 10);
	    	MsgReply(rcvid, NULL, pidP3Str, strlen(pidP3Str));
	    }

	    if(countMsg == 1){
	    	char *pChidStr = argv[0];
	    	MsgReply(rcvid, NULL, pChidStr, sizeof(pChidStr));
	    }

	    if(countMsg == 2){
	    	char *msgEx = (char *)"P2 Executed";
	    	MsgReply(rcvid, NULL, msgEx, strlen(msgEx) + 1);
	    }

	    countMsg ++;
	}

	std::cout << "P2: Р2 ОК" << std::endl;
	return EXIT_SUCCESS;

}
