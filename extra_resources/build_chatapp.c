#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	int serverOrClient = 0;
	int runOrDocker = 0;
	while(1) {
		system("clear");
		printf("Welcome to the chat application setup helper!\n\n- '1' to setup client\n- '2' to setup server\n\nYou: ");
		scanf("%d", &serverOrClient);
		if(serverOrClient != 1 && serverOrClient != 2) {
			printf("\n\nInvalid input!");
			continue;
		}
		system("clear");
		printf("Now please enter how you would like to run the program.\n\n- '1' to run the executable file (runs on Lab machines)\n- '2' to run the application in a Docker container (Theoretically works, but needs permissions)\n\nYou: ");
		scanf("%d", &runOrDocker);
		if(runOrDocker != 1 && runOrDocker != 2) {
			printf("\n\nInvalid input!");
			continue;
		}
		break;
	}
	
	if(runOrDocker == 2) {
		if(serverOrClient == 1) {
			if (chdir("client") != 0) {
				perror("chdir() failed");
				return 1;
			}
			system("clear");
			system("docker build -t chat_client .");
			system("docker run -it chat_client");
		} else {
			if (chdir("server") != 0) {
				perror("chdir() failed");
				return 1;
			}
			system("clear");
			system("docker build -t chat_server .");
			system("docker run -p 8080:8080 -it chat_server");
		}
	} else {
		if(serverOrClient == 1) {
			if (chdir("client") != 0) {
				perror("chdir() failed");
				return 1;
			}
			system("clear");
			system("./client");
		} else {
			if (chdir("server") != 0) {
				perror("chdir() failed");
				return 1;
			}
			system("clear");
			system("./server");
		}
	}
	return 0;
}
