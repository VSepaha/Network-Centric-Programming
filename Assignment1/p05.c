//Warm-up Programming Project
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

//Iterate through all the arguments in the container to see how many times they appear
void findSubstring(int arguments, char* buffer, char** container){
	int i;
	for(i = 0; i < arguments; i++){
		char* temp = buffer;
		int count = 0;
			//make the arguments all lowercase for easy comparison
		int j;
		for(j = 0; j < sizeof(container[i]); j++){
			*container[i] = tolower(*container[i]);
		}
		//string compare
		while(temp = strstr(temp, container[i])) {
	   		count++;
	   		temp++;
		}
		printf("%d\n", count);
	}
}

int main(int argc, char* argv[]){

	//If there are not enough arguments, return
	if(argc < 3){
		printf("Invalid number of arguments!\n");
		return 0;
	}

	int offset;
	int arguments;

	//check for a system call
	int sysflag = 0;
	char* flag = "--systemcalls";
	if(!strcmp(argv[1],flag)){
		arguments = argc-3;
		offset = 3;
		sysflag = 1;
	} else {
		arguments = argc-2;
		offset = 2;
	}

	//create container to hold arguments
	char* container[arguments];

	//assign the arguments to the container
	int i;
	for(i = 0; i < arguments; i++){
		container[i] = argv[i+offset];
	}

	//create the file pointer
	FILE* file;

	//If there is no system call (sysflag = 0)
	if(sysflag == 0){
		//Get the file and check it is valid
		file = fopen(argv[offset-1], "r");
		if(file == NULL){
			perror("./p05");
			return 0;
		}

		//Get the size of the file
		fseek(file, 0, SEEK_END);
		long size = ftell(file);
		fseek(file, 0, SEEK_SET);

		//allocate memory for the file
		char* buffer = malloc(size);

		//read from the file
		int n = fread(buffer, 1, size-1, file);
		buffer[n] = 0;

		//Make buffer all lowercase
		for(i = 0; i < sizeof(buffer); i++){
			buffer[i] = tolower(buffer[i]);
		}

		//find the substrings
		findSubstring(arguments, buffer, container);

		free(buffer);
		fclose(file);

	//If there is a system call (sysflag = 1)
	} else {
		
		//Get the file descriptor
		int fd = open(argv[offset-1], O_RDONLY);
		if(fd < 0){
			perror("./p05");
			return 0;
		}

		//Get the size of the file
		lseek(fd, 0, SEEK_END);
		long size = lseek(fd, 0, SEEK_CUR);
		lseek(fd, 0, SEEK_SET);

		char* buffer = malloc(size); 

		int n = read(fd, buffer, size);
		buffer[n] = 0;

		//Make buffer all lowercase
		for(i = 0; i < sizeof(buffer); i++){
			buffer[i] = tolower(buffer[i]);
		}

		//Iterate through all the arguments in the container to see how many times they appear
		findSubstring(arguments, buffer, container);

		free(buffer);

	}
	return 0;
}
