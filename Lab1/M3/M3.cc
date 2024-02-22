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

	std::cout << "P3: Запущен" << std::endl;

	std::cout << "P3: Параметры: " << "argv[0]=  " << argv[0] <<  std::endl;

	int chid = ChannelCreate(_NTO_CHF_SENDER_LEN);
	std::cout << "P3: Канал P3 создан: chid = " << chid << std::endl;

	int pChid = atoi(argv[0]);

	int pidP1;
	int pChidP1;

	std::cout << "P3: Установка соединения с каналом P2" <<  std::endl;
	int coidP2 = ConnectAttach(0, getppid(), pChid, _NTO_SIDE_CHANNEL, 0);
	if(coidP2 == -1){
		std::cout << "P3: Ошибка соединения с каналом P2" <<  std::endl;
	    exit(EXIT_FAILURE);
	}

	char rmsg[200] = {};

	std::cout << "Р3: Отправка сообщение Р2" << rmsg <<  std::endl;
	char *smsg1 = (char *)"Запрашиваю pid процесса Р1";
	if(MsgSend(coidP2, smsg1, strlen(smsg1) + 1, rmsg, sizeof(rmsg)) == -1){
		std::cout << "P3: Ошибка MsgSend" <<  std::endl;
		exit(EXIT_FAILURE);
	}

	if(strlen(rmsg) > 0){
		std::cout << "P3: Сервер P2 ответил " << std::endl;
		pidP1 = atoi(rmsg);
		std::cout << "P3: pid процесса Р1: " << pidP1 << std::endl;
	}

	std::cout << "Р3: Отправка сообщение Р2" <<  std::endl;
	char *smsg2 = (char *)"Запрашиваю chid канала Р1";
	if(MsgSend(coidP2, smsg2, strlen(smsg2) + 1, rmsg, sizeof(rmsg)) == -1){
		std::cout << "P3: Ошибка MsgSend" <<  std::endl;
		exit(EXIT_FAILURE);
	}

	if(strlen(rmsg) > 0){
		std::cout << "P3: Сервер P2 ответил " << rmsg << std::endl;
		pChidP1 = atoi(rmsg);
		std::cout << "P3: chid канала Р1: " << pChidP1 << std::endl;
	}

	std::cout << "P3: Установка соединения с каналом P1" <<  std::endl;
	int coidP1 = ConnectAttach(0, pidP1, pChidP1, _NTO_SIDE_CHANNEL, 0);
	if(coidP1 == -1){
		std::cout << "P3: Ошибка соединения с каналом P1" <<  std::endl;
	    exit(EXIT_FAILURE);
	}

	char rmsg2[200] = {};

	std::cout << "Р3: Отправка сообщения Р1" <<  std::endl;

	int pid = getpid();
	char pidBuffer[50];
	char const *pidStr = itoa(pid, pidBuffer, 10);

	char buffer[20];
	const char *chidStr = itoa(chid, buffer, 10);

	if(MsgSend(coidP1, pidStr, strlen(pidStr) + 1, rmsg2, sizeof(rmsg2)) == -1){
		std::cout << "P3: Ошибка MsgSend" <<  std::endl;
		exit(EXIT_FAILURE);
	}
	if(strlen(rmsg2) > 0){
		std::cout << "P3: Сервер P1 ответил - " << rmsg2 << std::endl;
	}

	if(MsgSend(coidP1, chidStr, strlen(chidStr) + 1, rmsg2, sizeof(rmsg2)) == -1){
		std::cout << "P3: Ошибка MsgSend" <<  std::endl;
			exit(EXIT_FAILURE);
	}
	if(strlen(rmsg2) > 0){
		std::cout << "P3: Сервер P1 ответил - " << rmsg2 << std::endl;
	}

	std::cout << "P3: Старт получения сообщений" << std::endl;
	int countMsg = 0;
	while(countMsg < 1){

		char msg[200] = {};
		_msg_info info;
		int rcvid;

		rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
		if(rcvid == -1){
			std::cout << "P3: Ошибка MsgReceive" << std::endl;
		}

		char *msgEx = (char *)"P3 Executed";
		std::cout << "P3: Получено сообщение от " << info.pid << " - " << msg << std::endl;
		MsgReply(rcvid, NULL, msgEx, strlen(msgEx) + 1);

		countMsg++;
	}

	std::cout << "P3: Р3 ОК" << std::endl;
	return EXIT_SUCCESS;
}
