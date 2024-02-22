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
	std::cout << "P1: Запущен" << std::endl;

	int chid = ChannelCreate(_NTO_CHF_SENDER_LEN);
	std::cout << "P1: Канал P1 создан: chid = " << chid << std::endl;

	char buffer[20];
	const char *chidStr = itoa(chid, buffer, 10);

	int pidP2 = spawnl(P_NOWAIT, "/home/host/Lab1/M2/x86/o/M2", chidStr, NULL);
	if (pidP2 < 0){
		std::cout << "P1: Ошибка запуска процесса P2" << pidP2 << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "P1: Процесс P2 запущен: pid = " << pidP2 <<  std::endl;

	int pidP3 = {};
	int chidP3 = {};

	std::cout << "P1: Старт получения сообщений" << std::endl;
	int countMsg = 0;
	while(countMsg < 3){

		char msg[200] = {};
	    _msg_info info;
	    int rcvid;

	    rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
	    if(rcvid == -1){
	    	std::cout << "P1: Ошибка MsgReceive" << std::endl;
	    }

	    std::cout << "P1: Получено сообщение от " << info.pid << " - " << msg << std::endl;
	    if (countMsg == 0){
	    	MsgReply(rcvid, NULL, msg, sizeof(msg));
	    }
	    if (countMsg == 1) {
		    pidP3 = atoi(msg);
	    	MsgReply(rcvid, NULL, msg, sizeof(msg));
	    }
	    if (countMsg == 2) {
	    	chidP3 = atoi(msg);
	    	MsgReply(rcvid, NULL, msg, sizeof(msg));
	    }

	    countMsg++;
	}

	char rmsg[200] = {};

	int coidP2 = ConnectAttach(0, pidP2, chid, _NTO_SIDE_CHANNEL, 0);
	if(coidP2 == -1){
		std::cout << "P1: Ошибка соединения с каналом P2" <<  std::endl;
		exit(EXIT_FAILURE);
	}

	char *msg1 = (char *)"P1 send message to P2";

	if(MsgSend(coidP2, msg1, strlen(msg1) + 1, rmsg, sizeof(rmsg)) == -1){
		std::cout << "P1: Ошибка MsgSend" <<  std::endl;
		exit(EXIT_FAILURE);
	}
	if(strlen(rmsg) > 0){
		std::cout << "P1: Сервер P2 ответил - " << rmsg << std::endl;
	}

	int coidP3 = ConnectAttach(0, pidP3, chidP3, _NTO_SIDE_CHANNEL, 0);
	if(coidP3 == -1){
		std::cout << "P1: Ошибка соединения с каналом P3" <<  std::endl;
		    exit(EXIT_FAILURE);
	}

	char *msg2 = (char *)"P1 send message to P3";

	if(MsgSend(coidP3, msg2, strlen(msg2) + 1, rmsg, sizeof(rmsg)) == -1){
		std::cout << "P1: Ошибка MsgSend при отправки в P3" <<  std::endl;
		exit(EXIT_FAILURE);
	}
	if(strlen(rmsg) > 0){
		std::cout << "P1: Сервер P3 ответил - " << rmsg << std::endl;
	}

	std::cout << "P1: Р1 ОК" << std::endl;
	return EXIT_SUCCESS;
}
