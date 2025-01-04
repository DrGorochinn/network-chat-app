#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

void setNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

void disableBuffering() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void enableBuffering() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

typedef struct ipNode {
    char *data;
    struct ipNode *next;
} ipNode;

typedef struct {
    ipNode *head;
    int size;
} knownHostsQueue;

knownHostsQueue* createQueue(void) {
    knownHostsQueue *queue = (knownHostsQueue*)malloc(sizeof(knownHostsQueue));
    if(queue == NULL) {
        return NULL;
    }
    queue->head = NULL;
    queue->size = 0;
    return queue;
}

void destroyQueue(knownHostsQueue *queue) {
    if(queue == NULL) {
        return;
    }
    ipNode *current = queue->head;
    while(current != NULL) {
        ipNode *temp = current;
        current = current->next;
        free(temp->data);
        free(temp);
    }
    free(queue);
}

void insertQueue(knownHostsQueue *known_hosts, const char *ip_address) {
    ipNode *newNode = (ipNode*) malloc(sizeof(ipNode));
    if(newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    newNode->data = strdup(ip_address);
    newNode->next = NULL;

    if(known_hosts->head == NULL) {
        known_hosts->head = newNode;
    } else {
        ipNode *selector = known_hosts->head;
        while (selector->next != NULL) {
            selector = selector->next;
        }
        selector->next = newNode;
    }
    known_hosts->size++;
}

bool isEmpty(knownHostsQueue *queue) {
    return(queue == NULL || queue->size == 0);
}

char* selectKnownHost(knownHostsQueue *queue) {
    system("clear");
    if(queue == NULL || queue->size == 0) {
        printf("No hosts available.\n\n");
    }
    disableBuffering();

    int selection = 1;
    char ch;

    while(1) {
        ipNode *selector = queue->head;
        int counter = 1;
        // Print host list with current selection
        printf("Please select the server to connect to:\n");
        for(counter; selector != NULL; counter++) {
            printf("%d. %s", counter, selector->data);
            if(selection == counter) {
                printf("    <---");  // Cool little arrow thing to navigate the menu haha
            }
            printf("\n");
            selector = selector->next;
        }

        printf("%d. Return to main menu", counter);
        if(selection == counter) {
            printf("    <---");
        }
        printf("\n");

        // Get user input
        ch = getchar();

        if(ch == 27) {  // Escape sequence for arrow keys
            getchar();  // Skip the '[' character
            ch = getchar();
            if(ch == 'A' && selection > 1) {  // Up arrow
                selection -= 1;
            } else if(ch == 'B' && selection <= queue->size) {  // Down arrow
                selection += 1;
            }
        } else if(ch == '\n') {  // Enter key
            // Return the selected host
            if(selection <= queue->size) {
                selector = queue->head;
                for(int counter = 1; counter < selection; counter++) {
                    selector = selector->next;
                }
                enableBuffering();
                return selector->data;
            } else {
                return "continue";
            }
        }

        system("clear");  // Clear the screen only after user navigates
    }
}

int selectServerOption() {
    int choice = 1;
    disableBuffering();

    char ch;
    while(1) {
        printf("Welcome to the chat application! Please select your desired option:\n\n");
        printf("1. Connect to known host");
        if(choice == 1) {
            printf("    <---\n");
        } else {
            printf("\n");
        }
        printf("2. Add new host");
        if(choice == 2) {
            printf("    <---\n");
        } else {
            printf("\n");
        }
        printf("3. Exit program");
        if(choice == 3) {
            printf("    <---\n");
        } else {
            printf("\n");
        }

        ch = getchar();

        if(ch == 27) {  // Escape sequence for arrow keys
            getchar();   // Skip the '[' character
            ch = getchar();
            if(ch == 'A' && choice > 1) {  // Up arrow
                choice -= 1;
            } else if(ch == 'B' && choice < 3) {  // Down arrow
                choice += 1;
            }
        } else if(ch == '\n') {  // Enter key
            break;
        }
        system("clear");
    }

    enableBuffering();
    return choice;
}

int main() {
    system("clear");
    while(1) {
        int sock = 0;
        struct sockaddr_in serv_addr;
        char message[BUFFER_SIZE];
        char username[BUFFER_SIZE];
        char password[BUFFER_SIZE];
        char buffer[BUFFER_SIZE] = {0};
        char *ipAddress = NULL;

        int selection = selectServerOption();

        if(selection == 1) {  // Wants to select from known hosts
            FILE *hostsFile = fopen("known_hosts.txt", "r");
            if(hostsFile == NULL) {
                printf("Could not open file\n");
                return 1;
            }

            knownHostsQueue *known_hosts = createQueue();
            if(known_hosts == NULL) {
                fprintf(stderr, "Failed to create queue\n");
                return 1;
            }

            char currentIp[16];  // Allocate enough space for IPv4 address + null terminator
            while(fscanf(hostsFile, "%15s", currentIp) == 1) {
                insertQueue(known_hosts, currentIp);
            }

            fclose(hostsFile);

            ipAddress = strdup(selectKnownHost(known_hosts));
            if(ipAddress[0] == 'c') { // idc what you say future george, this is a relic and it's staying forever now.
                system("clear");
                continue;
            }
            destroyQueue(known_hosts);

        } else if(selection == 2) {  // Wants to enter a new host
            char temp[20];

            printf("Please enter the IP address of the server (xxx.xxx.xxx.xxx): ");
            fgets(temp, sizeof(temp), stdin); 

            // Remove newline character added by fgets
            temp[strcspn(temp, "\n")] = 0;
            ipAddress = strdup(temp);

            FILE *known_hosts = fopen("known_hosts.txt", "a");
            if(known_hosts == NULL) {
                printf("Could not open file\n");
                return 1;
            }

            fprintf(known_hosts, "%s\n", ipAddress);
            fclose(known_hosts);
        } else {
            system("clear");
            printf("Exiting chat program...\n");
            break;
        }
        
        // Create socket
        if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            system("clear");
            printf("Socket creation error\n\n");
            continue;
        }

        // Set server address
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(8080);

        if(inet_pton(AF_INET, ipAddress, &serv_addr.sin_addr) <= 0) {
            system("clear");
            printf("Invalid address / Address not supported\n\n");
            continue;
        }

        // Connect to the server
        if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            system("clear");
            printf("Connection Failed\n\n");
            continue;
        }

        //Free allocated IP address
        if(selection == 2) {
            free(ipAddress);
        }

        system("clear");

        // Receive initial server prompt for username
        int valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));

        // Send username
        fgets(username, BUFFER_SIZE, stdin);
        username[strcspn(username, "\n")] = 0;
        send(sock, username, strlen(username), 0);

        // Confirm username reception
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));

        // Receive initial server prompt for password
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));

        // Send password
        fgets(password, BUFFER_SIZE, stdin);
        username[strcspn(username, "\n")] = 0;
        send(sock, password, strlen(password), 0);

        // Confirm password reception
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));

        system("clear");

        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));
        

        //sending client selection option
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0;
        send(sock, message, strlen(message), 0);
        
        system("clear");

        // reading the chat creation/access message 
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }
        memset(buffer, '\0', sizeof(buffer));

        // sending 'Y' confirmation message
        fgets(message, BUFFER_SIZE, stdin); 
        message[strcspn(message, "\n")] = 0;

        // Check if the user typed "Y"
        if (strcmp(message, "Y") != 0) {
            printf("Chat initiation cancelled, disconnecting...\n");
            close(sock);
            system("clear");
            break;
        }

        // This doesn't work for some reason... Can't figure it out for the life of me
        char* userEnter = "   <-- JUST JOINED THE CHAT";
        send(sock, userEnter, strlen(message), 0);

        system("clear");

        //new chat loop (actually works this time lol)
        char buffer1[BUFFER_SIZE];
        char message1[BUFFER_SIZE];
        fd_set readfds;
        struct timeval timeout;
        setNonBlocking(sock);

        while (1) {
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            FD_SET(STDIN_FILENO, &readfds);

            // timeout time is the duration between refreshes!
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);

            if (activity == -1) {
                perror("select");
                break;
            }

            // Check if there's data from the server
            if (FD_ISSET(sock, &readfds)) {
                int valread = read(sock, buffer1, BUFFER_SIZE - 1);
                if (valread > 0) {
                    buffer1[valread] = '\0'; 
                    system("clear");
                    printf("%s\n", buffer1);
                    
                    memset(buffer1, '\0', sizeof(buffer1));
                } else if (valread == 0) {
                    printf("Server disconnected.\n");
                    close(sock);
                    break;
                }
            }

            // Check if the user has input a message
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                // Move cursor up to prevent overwriting the input prompt <-- maybe good to remember this one for the future lol george
                printf("\033[A");

                // need to confirm if this actually does anything 
                printf("%s", message1);

                if (fgets(message1, BUFFER_SIZE, stdin) != NULL) {
                    // Remove newline character from input
                    message1[strcspn(message1, "\n")] = 0;

                    // Check if the user typed "/exit"
                    if (strcmp(message1, "/exit") == 0) {
                        system("clear");
                        printf("Disconnecting...\n");
                        close(sock);
                        break;
                    }

                    // Send message to the server
                    send(sock, message1, strlen(message1), 0);
                    memset(message1, '\0', sizeof(message1));
                }
            }
        }
    }

    return 0;
}
