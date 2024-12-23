#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>

#define FILENAME "commands.txt"
#define PARSED_COMMANDS_FILE "parse.txt"

using namespace std;
pthread_mutex_t locksmith = PTHREAD_MUTEX_INITIALIZER;

void * printer(void * arg){
    int* fdPtr = (int*)arg; // Cast the argument back to an int pointer
    int fdRead = *fdPtr; // Dereference to get the actual file descriptor
    delete fdPtr;
    FILE * pipeline;

    pthread_mutex_lock(&locksmith);
    pipeline = fdopen(fdRead, "r"); // Open the pipe in read mode
    if (!pipeline) { // Check if the pipe file was successfully opened
        cerr << "An error emerged while opening pipe file for reading." << endl;
        return NULL;
    }
    printf("---- %ld\n", pthread_self());
    char buff[100]; // Buffer to hold the message
    while (fgets(buff, sizeof(buff), pipeline) != NULL) { // Read until EOF
        printf("%s", buff); // Print the results
        fsync(STDOUT_FILENO);
    }
    printf("---- %ld\n", pthread_self());
    fsync(STDOUT_FILENO);
    pthread_mutex_unlock(&locksmith);

    fclose(pipeline); // Close the pipe file
    return NULL;
}


int main(int argc, char * argv[]) {
    ifstream file;
    file.open(FILENAME);
    ofstream commandReader;
    commandReader.open(PARSED_COMMANDS_FILE);

    string line, word;
    vector<int> pIDs; //will contain newly opened process IDs
    vector<pthread_t> tIDs; //will contain newly created tIDs

    while(getline(file, line)){ //gets the commands from the lines of commands.txt
        /*-----PARSING BEGINS-----*/
        istringstream lineReader(line); //to read the line word by word
        lineReader >> word; //now the word IS the theCommand to be executed

        char redirectionType = '-';
        string redirectedFile, input, option;

        char * theCommand[4] = {NULL, NULL, NULL, NULL}; //theCommand consists of 4 elements: theCommand itself, input, option and the null for execvp
        theCommand[0] = strdup(word.c_str()); //the theCommand to be executed
        commandReader << "----------" << endl;
        commandReader << "Command: " << theCommand[0] << endl;
        int idx = 1;

        while(lineReader >> word){
            if (word == ">"){
                redirectionType = '>';
                lineReader >> word;
                redirectedFile = word;
            }
            else if (word == "<"){
                redirectionType = '<';
                lineReader >> word;
                redirectedFile = word;
            }
            else if (word != "&"){ // background job indicator is the last word,
                // so we may use the "word" variable as background job indicator after the loop

                // if the word is not a redirection or background job indicator, then it is a part of the theCommand
                if (word.find("-") == 0){ //is it an option?
                    option = word;
                }
                else{
                    input = word;
                }
                theCommand[idx] = strdup(word.c_str());
                idx++;
            }
        }

        commandReader << "Input: " << input << endl;
        commandReader << "Option: " << option << endl;
        commandReader << "Redirection: " << redirectionType << endl;
        commandReader << "Background: ";
        if (word == "&") commandReader << "y" << endl;
        else commandReader << "n" << endl;
        commandReader << "----------" << endl;
        commandReader.flush(); // C++ version of fflush() for streams
        /*-----PARSING ENDS-----*/

        if(word != "wait"){
            if(redirectionType == '>'){ // redirecting the output to a file
                int pid_fileprinter = fork();

                if (pid_fileprinter < 0){
                    cout << "An error emerged while forking."<< endl;
                }
                else if (pid_fileprinter == 0){
                    int out = open(redirectedFile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
                    dup2(out, STDOUT_FILENO);
                    close(out);
                    execvp(theCommand[0], theCommand);
                }
                else{
                    if (word != "&"){ // when theCommand is not a background job
                        waitpid(pid_fileprinter, NULL, 0);
                    }
                    else{
                        pIDs.push_back(pid_fileprinter);
                    }
                }
            }
            else{ // the output with synchronization will be on the console
                int fd[2];
                if (pipe(fd) == -1) { // Error handling for pipe
                    cerr << "An error emerged while creating the pipe." << endl;
                    exit(EXIT_FAILURE);
                }
                pthread_t thread1;

                int pid_consoleprinter = fork();

                if(pid_consoleprinter < 0){
                    cout << "An error emerged while forking." << endl;
                }
                else if (pid_consoleprinter == 0){
                    if (redirectionType == '<'){
                        int in = open(redirectedFile.c_str(), O_RDONLY);
                        if (in == -1 || dup2(in, STDIN_FILENO) == -1) { // Error handling for open and dup2
                            cerr << "An error emerged while opening the file or redirecting the input." << endl;
                            exit(1);
                        }
                        close(in);

                    }
                    close(fd[0]); //close the read end, not used
                    if (dup2(fd[1], STDOUT_FILENO) == -1) { // Error handling for dup2
                        cerr << "An error emerged while redirecting the output to pipe." << endl;
                        exit(1);
                    }
                    close(fd[1]);
                    execvp(theCommand[0], theCommand); //run the theCommand
                }
                else{
                    close(fd[1]); //close the write end
                    int* fdPtr = new int(fd[0]); // Dynamically allocate memory to store the file descriptor
                    if (pthread_create(&thread1, NULL, printer, fdPtr) != 0) { // Error handling for pthread_create
                        cerr << "An error emerged while creating the thread." << endl;
                        exit(EXIT_FAILURE);
                    }

                    if (word != "&"){ //if it is not a background job
                        waitpid(pid_consoleprinter, NULL, 0); // waiting this process to finish before continuing executing new commands
                        pthread_join(thread1, NULL); //joining this thread to wait for it to finish
                    }
                    else{ //push them to keep track
                        pIDs.push_back(pid_consoleprinter);
                        tIDs.push_back(thread1);
                    }
                }
            }
        }
        else{ // waiting for the background jobs to get finished
            for (int pID : pIDs){
                waitpid(pID, NULL, 0);
            }
            pIDs.clear(); //clearing the memory

            for(int i=0; i < tIDs.size(); i ++){
                pthread_join(tIDs[i], NULL);
            }
            tIDs.clear();
        }
    }
    commandReader.close(); //close the files, work is done.
    file.close();

    //waiting, joining every thread to make sure everything is printed.
    for (int pID : pIDs){
        waitpid(pID, NULL, 0);
    }
    pIDs.clear();

    for(auto & tID : tIDs){
        pthread_join(tID, NULL);
    }
    tIDs.clear();
    return 0;
}
