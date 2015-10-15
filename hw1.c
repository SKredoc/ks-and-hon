/**
* Name: Wong Kam Shing
* SID: 1155047854
* Course: csci3150
**/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<limits.h>
#include<unistd.h>
#include<errno.h>
#include<sys/wait.h>
#include<sys/types.h>

char cwd[PATH_MAX+1];
char input[256];

typedef struct Node{
	char* data;
	struct Node* next;
}Node;

typedef struct Queue{
	Node* front;
	Node* rear;
	int size;
}Queue;

void init(){
	printf("[3150 shell:");
	if(getcwd(cwd, PATH_MAX+1) != NULL){
		printf("%s]$ ", cwd);
	}
	else{
		printf("Error Ocurred\n");
	}
}

/** createQueue
* @token (can be NULL)
* @return pointer of Queue
**/
Queue* createQueue(char* token){
	if(token == NULL){
		Queue* queue = (Queue*)malloc(sizeof(Queue));
		queue->front = NULL; 
		queue->rear = NULL;
		queue->size = 0;
		return queue;
	}

	Node* temp = (Node*)malloc(sizeof(Node));
	temp->data = token;
	temp->next = NULL;

	Queue* queue = (Queue*)malloc(sizeof(Queue));
	queue->front = temp;
	queue->rear = temp;
	queue->size = 1;

	return queue;
}

/** enqueue
* @queue -> the queue to be enqueued
* @token
**/
void enqueue(Queue* queue, char* token){
	Node* temp = (Node*)malloc(sizeof(Node));
	temp->data = token;
	temp->next = NULL;

	if(queue->size == 0){
		queue->front = temp;
		queue->rear = temp;
		queue->size++;
		return;
	}

	queue->rear->next = temp;
	queue->rear = temp;
	queue->size++;
}

/** dequeue
* @queue -> the queue to be dequeued
* @return the element just dequeued
**/
char* dequeue(Queue* queue){
	if(queue->size == 0){
		return NULL;
	}
	
	Node* temp = queue->front;
	char* tempData = queue->front->data;
	queue->front = queue->front->next;
	queue->size--;
	free(temp);
	return tempData;
}	

/** deleteQueue
* @queue -> the queue to be deleted
**/
void deleteQueue(Queue* queue){
	while(queue->size>0){
		Node* temp = queue->front;
		queue->front = queue->front->next;
		queue->size--;
		free(temp);
	}
}

char* top(Queue* queue){
	if(queue->size == 0)
		return NULL;
	return queue->front->data;
}

Queue* tokenizedInput(){
	Queue* queue = createQueue(NULL);
	char* token = strtok(input, " ");
	while(token != NULL){
		enqueue(queue, token);
		token = strtok(NULL, " ");
	}
	return queue;
}

void builtIn_command_cd(Queue* queue){
	dequeue(queue);
	if(queue->size != 1){
		printf("cd: wrong number of arguments\n");
		return;
	}

	char* token = dequeue(queue);
	if(chdir(token) == -1){
		printf("%s: cannot change directory\n", token);
	}
}

int builtIn_command_exit(Queue* queue){
	if(queue->size > 1){
		printf("exit: wrong number of arguments\n");
		return 0;
	}
	deleteQueue(queue);
	return 1;
}

void createProcess(Queue* queue){
	//convery queue to 2d array
	char *command[255];
	int i = 0;
	while(queue->size > 0)
		command[i++] = dequeue(queue);
	command[i] = NULL;

	pid_t child_pid;
	if(!(child_pid = fork())){
		setenv("PATH", "/bin:/usr/bin:.", 1);
		execvp(*command, command);

		if(errno == ENOENT)
			printf("%s: command not found\n", input);
		else
			printf("%s: unknown error\n", input);

		exit(0);
	}
	else{
		waitpid(child_pid, NULL, WUNTRACED);
	}
}

int main(){
	while(1){
		init();
		fgets(input, 256, stdin);
		input[strlen(input) - 1] = '\0';
		Queue* inputQueue = tokenizedInput();
		
		char* token = top(inputQueue);
		//empty string
		if(token == NULL);
		
		//built-in command: cd
		else if(strcmp(token, "cd") == 0)
			builtIn_command_cd(inputQueue);

		//built-in command: exit
		else if(strcmp(token, "exit") == 0){
			if(builtIn_command_exit(inputQueue))
				return 0;
		}
		
		//file name or path name
		else{
			createProcess(inputQueue);
		}

		deleteQueue(inputQueue);
		input[0] = '\0';
	}
}
