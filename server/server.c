#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>


#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_LINES 10

void setNonBlocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

typedef struct userNode {
    char *username;
    char *password;
    int isLoggedIn;
    struct userNode *next;
} userNode;

typedef struct {
    userNode *head;
    int size;
} userQueue;

typedef struct {
    userQueue *queue;
    int client_socket;
} clientArgs;

userQueue* createQueue(void) {
    userQueue *queue = (userQueue*)malloc(sizeof(userQueue));
    if(queue == NULL) {
        return NULL;
    }
    queue->head = NULL;
    queue->size = 0;
    return queue;
}

void destroyQueue(userQueue *queue) {
    if(queue == NULL) {
        return;
    }
    userNode *current = queue->head;
    while(current != NULL) {
        userNode *temp = current;
        current = current->next;
        free(temp->username);
        free(temp->password);
        
        free(temp);
    }
    free(queue);
}

void insertQueue(userQueue *queue, const char *usernameCombo) {
    userNode *newNode = (userNode*) malloc(sizeof(userNode));
    userNode *head = queue->head;
    if(newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    int usernameLen = 0;
    while(usernameCombo[usernameLen] != '|' && usernameCombo[usernameLen] != '\0') {
        usernameLen++;
    }

    int passwordLen = strlen(usernameCombo) - usernameLen - 2;

    char onlyUsername[usernameLen + 1];
    char onlyPassword[passwordLen + 1];

    strncpy(onlyUsername, usernameCombo, usernameLen);
    onlyUsername[usernameLen] = '\0';

    strncpy(onlyPassword, usernameCombo + usernameLen + 1, passwordLen);
    onlyPassword[passwordLen] = '\0';


    printf("username: '%s'\n", onlyUsername);
    printf("password: '%s'\n\n", onlyPassword);

    newNode->username = strdup(onlyUsername);
    newNode->password = strdup(onlyPassword);
    newNode->isLoggedIn = 0;
    newNode->next = NULL;

    if(queue->head == NULL) {
        queue->head = newNode;
    } else {
        while(head->next != NULL) {
            head = head->next;
        }
        head->next = newNode;
    }
    queue->size++;
}

bool isEmpty(userQueue *queue) {
    return(queue == NULL || queue->size == 0);
}

bool isInQueue(userQueue *queue, const char *username) {
    userNode *checkNode = queue->head;

    while (checkNode != NULL) {
        if (strcmp(username, checkNode->username) == 0) {
            return true;
        }
        checkNode = checkNode->next;
    }
    return false;
}

char *findPassword(userQueue *queue, const char *username) {
    userNode *checkNode = queue->head;

    while (checkNode != NULL) {
        if (strcmp(username, checkNode->username) == 0) {
            return checkNode->password;
        }
        checkNode = checkNode->next;
    }
    return NULL;
}

char* readLastLines(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Error opening file '%s'.\n", filepath);
        return NULL;
    }

    // Buffer to hold the last 10 lines
    char *lines[MAX_LINES];
    for (int i = 0; i < MAX_LINES; i++) {
        lines[i] = (char *)malloc(BUFFER_SIZE);
        lines[i][0] = '\0';  // Initialize to empty string
    }
    
    int lineCount = 0;
    char temp[BUFFER_SIZE];
    
    while (fgets(temp, sizeof(temp), file) != NULL) {
        strncpy(lines[lineCount % MAX_LINES], temp, BUFFER_SIZE - 1);
        lines[lineCount % MAX_LINES][BUFFER_SIZE - 1] = '\0';
        lineCount++;
    }
    
    fclose(file);

    int start = (lineCount > MAX_LINES) ? lineCount % MAX_LINES : 0;
    int totalLines = (lineCount < MAX_LINES) ? lineCount : MAX_LINES;

    int resultSize = totalLines * BUFFER_SIZE;
    char *result = (char *)malloc(resultSize);
    result[0] = '\0';

    for (int i = 0; i < totalLines; i++) {
        // note, strncat does exactly what it sounds like, adds the second string to the end of the first one
        strncat(result, lines[(start + i) % MAX_LINES], BUFFER_SIZE - 1);
    }

    for (int i = 0; i < MAX_LINES; i++) {
        free(lines[i]);
    }

    return result;
}

void writeMessageToFile(char *filename, char *message, char *user) {
    FILE *chat = fopen(filename, "a");
    if(chat == NULL) {
        printf("Could not open file\n");
        return;
    }

    char messageToAppend[2050];
    sprintf(messageToAppend, "%s%c%c%s%c", user, ':', ' ', message, '\0');
    fprintf(chat, "%s\n", messageToAppend);
    fclose(chat);
    return;
}

void *handle_client(void *args) {
    clientArgs *client_data = (clientArgs *)args;
    userQueue *queue = client_data->queue;
    int client_socket = client_data->client_socket;
    free(client_data);
    
    char buffer[BUFFER_SIZE] = {0};
    char username[BUFFER_SIZE] = {0};
    char password[BUFFER_SIZE] = {0};

    // Prompt for username
    const char *username_prompt = "Server: Enter your username please! ";
    send(client_socket, username_prompt, strlen(username_prompt), 0);

    int valread = read(client_socket, username, BUFFER_SIZE);
    if (valread > 0) {
        username[valread] = '\0'; // Remove newline character
        printf("User '%s' has connected\n\n", username);
    }

    const char *username_accept = "Server: I got your username, it's: ";
    char full_message[sizeof(username_accept) + sizeof(username) + 30]; // maybe adjust this 30 later on future george lol

    snprintf(full_message, sizeof(full_message), "%s%s", username_accept, username);
    send(client_socket, full_message, strlen(full_message), 0);

    // Prompt for password
    const char *password_prompt = "Server: Please now enter your password";
    send(client_socket, password_prompt, strlen(password_prompt), 0);

    valread = read(client_socket, password, BUFFER_SIZE);
    if (valread > 0) {
        password[valread - 1] = '\0'; // Remove newline character
        const char *password_accept = "Server: I got your password!";
        send(client_socket, password_accept, strlen(password_accept), 0);
    }
    
    printf("The username Client entered is '%s', and the password is '%s'\n\n", username, password);

    if(isInQueue(queue, username) == true) {
        //if the name is in the queue
        printf("the username Client entered is in the queue! Searching for password now...\n");
        char *correctPassword = findPassword(queue, username);
        printf("the associated password is: '%s'. The password user entered is '%s'\n", correctPassword, password);
        if(strcmp(correctPassword, password) == 0) { //if they ge tthe password right
            const char *you_got_denied_lmao = "Server: Welcome! Please select the user to message out of the list here :)\n\n";
            send(client_socket, you_got_denied_lmao, strlen(you_got_denied_lmao), 0);
            printf("%s login successful\n", username);
        } else {
            const char *you_got_denied_lmao = "Server: you got the password wrong nerd. Outta here!";
            send(client_socket, you_got_denied_lmao, strlen(you_got_denied_lmao), 0);
            close(client_socket);
        }
    } else {
        // if the name is not in the queue
        printf("No user going by the name %s found in current users... Creating new user\n", username);
        FILE *userList = fopen("./chat_data/user_and_passwords.txt", "a");
        if(userList == NULL) {
            printf("Could not open file\n");
            return NULL;
        }

        char username_and_password[2050];
        sprintf(username_and_password, "%s%c%s%c%c", username, '|', password, '\n', '\0');
        fprintf(userList, "%s", username_and_password);
        fclose(userList);

        insertQueue(queue, username_and_password);

        const char *you_made_user = "Server: New user created. Hope you remember the password you just entered lol\nPlease select the user to message out of the list here\n\n";
        send(client_socket, you_made_user, strlen(you_made_user), 0);
    }


    userNode *head = queue->head;
    char userList[1024 * queue->size];
    char userListBuffer[10];
    for(int i = 1; i <= queue->size; i++) {
        sprintf(buffer, "%d: ", i);
        strcat(userList, buffer);
        strcat(userList, head->username);
        strcat(userList, "\n");
        head = head->next;
    }

    send(client_socket, userList, strlen(userList), 0);
    memset(userList, '\0', sizeof(userList));


    head = queue->head;
    char selection[] = "1\n";
    valread = read(client_socket, selection, 3);
    selection[1] = '\0'; // Remove newline character


    int selectionInt = atoi(selection);

    for(int clientSelection = 1; clientSelection < selectionInt; clientSelection++) {
        head = head->next;
    }
    char *selectedUser = head->username;
    char selectedUserMessage[1100];
    snprintf(selectedUserMessage, sizeof(selectedUserMessage), "Server: You selected user %s. ", selectedUser);

    send(client_socket, selectedUserMessage, strlen(selectedUserMessage), 0);

    // create/access text file
    char filename[2048];
    
    // Compare the strings and order them alphabetically
    if (strcmp(username, selectedUser) < 0) {
        snprintf(filename, sizeof(filename), "./chat_data/chats/%s+%s.txt", username, selectedUser);
    } else {
        snprintf(filename, sizeof(filename), "./chat_data/chats/%s+%s.txt", selectedUser, username);
    }

    // Check if the file already exists
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        printf("File '%s' already exists.\n", filename);
        fclose(file);
        const char *dmExists = "Chat already exists. Now attempting to open chat history... (send 'Y' to proceed)\nNote: while you're typing stuff, your input may disappear. It's all still there, just not visible.";
        send(client_socket, dmExists, strlen(dmExists), 0);
    } else {
        // Create the file
        file = fopen(filename, "w");
        if (file != NULL) {
            printf("File '%s' created successfully.\n", filename);
            fclose(file);
            const char *dmCreated = "New chat created! Now attempting to open new chat... (send 'Y' to proceed)\nNote: while you're typing stuff, your input may disappear. It's all still there, just not visible.";
            send(client_socket, dmCreated, strlen(dmCreated), 0);
        } else {
            printf("Error creating file '%s'.\n", filename);
        }
    }

    // note, this is still a blocking statement 
    valread = read(client_socket, buffer, BUFFER_SIZE);

    if (valread == 0) {
        // Client disconnected
        printf("User '%s' disconnected\n", username);
    } else if (valread > 0) {
        printf("%s: %s\n", username, buffer);
        writeMessageToFile(filename, buffer, username);
        const char *lastLines = readLastLines(filename);
        send(client_socket, lastLines, strlen(lastLines), 0);
    }

    char buffer1[BUFFER_SIZE];
    ssize_t valread1;
    time_t lastSendTime = 0;
    setNonBlocking(client_socket);

    while (1) {
        memset(buffer1, 0, BUFFER_SIZE);
        printf("Waiting for client data...\n");

        // Read from client (non-blocking)
        valread1 = read(client_socket, buffer1, BUFFER_SIZE);

        if (valread1 == -1 && errno != EWOULDBLOCK) {
            perror("Read error");
            break;
        } else if (valread1 > 0) {
            printf("%s: %s\n", username, buffer1);
            writeMessageToFile(filename, buffer1, username);
        } else if (valread1 == 0) {
            // Client disconnected
            printf("User '%s' disconnected\n", username);
            break;
        }

        // note to future george, surely there's a better way to measure time elapsed lol
        // maybe even just strategically placed 'sleeps'

        // Get the current time
        time_t currentTime = time(NULL);

        // Check if 1 second has passed since the last send
        if (difftime(currentTime, lastSendTime) >= 1.0) {
            // Fetch and send the last 10 lines to the client
            const char *lastLines = readLastLines(filename);
            printf("I just sent the latest data~\n");
            send(client_socket, lastLines, strlen(lastLines), 0);
            lastSendTime = currentTime;
        }
        usleep(500000);
    }

    close(client_socket);
    return NULL;
}

int main() {
    userQueue *username_and_password = createQueue();

    FILE *file = fopen("./chat_data/user_and_passwords.txt", "r");
    if(file == NULL) {
        printf("Could not open file\n");
        return 1;
    }

    char line[BUFFER_SIZE];
    while(fgets(line, sizeof(line), file)) {
        insertQueue(username_and_password, line);
    }

    fclose(file);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // note to self: SO_REUSEPORT is probably coming up as squiggly cause you havent added the appropriate header file to vscode's include path!!!!!! (nice one lol)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("New client connected\n");

        clientArgs *client_data = malloc(sizeof(clientArgs));
        client_data->queue = username_and_password;
        client_data->client_socket = new_socket;

        pthread_t client_thread;

        if (pthread_create(&client_thread, NULL, handle_client, (void *)client_data) != 0) { 
            perror("Thread creation failed");
            free(client_data);
            continue;
        }

        pthread_detach(client_thread);
    }

    destroyQueue(username_and_password);
    return 0;
}

