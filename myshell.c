#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <LineParser.h>
#include <bits/waitflags.h>

typedef struct process
{
    cmdLine *cmd;         /* the parsed command line*/
    pid_t pid;            /* the process id that is running the command*/
    int status;           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next; /* next process in chain */
} process;

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 20

int debug = 0;
process *process_list = NULL;
int newest = 0;
int oldest = 0;
char *history[HISTLEN] = {NULL};

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
    process *pr = malloc(sizeof(process));
    pr->cmd = cmd;
    pr->pid = pid;
    pr->status = RUNNING;
    pr->next = *process_list; // set pr->next to point at the first element in process_list
    *process_list = pr;       // set the process_list pointer to point at pr so it will be the first element of the list.
}

void freeProcessList(process *process_list)
{
    process *nextProcess;
    while (process_list != NULL)
    {
        nextProcess = process_list->next;
        freeCmdLines(process_list->cmd);
        free(process_list);
        process_list = nextProcess;
    }
}

void updateProcessStatus(process *process_list, int pid, int status)
{
    process *curr = process_list;
    while (curr != NULL)
    {
        if (curr->pid == pid)
        {
            curr->status = status;
            break;
        }
        curr = curr->next;
    }
}

void updateProcessList(process **process_list)
{
    process *curr = *process_list;
    while (curr != NULL)
    {
        int status;
        pid_t pid = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if (pid == -1)
            curr->status = TERMINATED;

        else if (pid == 0)
            updateProcessStatus(*process_list, pid, RUNNING);

        else
        {
            if (WIFSTOPPED(status))
                curr->status = SUSPENDED;

            else if (WIFCONTINUED(status))
                curr->status = RUNNING;

            else if (WIFSIGNALED(status))
                curr->status = TERMINATED;
        }

        curr = curr->next;
    }
}

void printProcessList(process **process_list)
{
    if (*process_list == NULL)
    {
        perror("No active proccess\n");
        return;
    }

    updateProcessList(process_list);
    process *curr = *process_list;
    process *prev = NULL;
    int index = 0;
    printf("Index\t\tPID\t\tStatus\t\t\tCommand\n");

    while (curr != NULL)
    {
        printf("%d\t\t%d\t\t%s\t\t", index, curr->pid, curr->status == RUNNING ? "RUNNING" : curr->status == SUSPENDED ? "SUSPENDED"
                                                                                                                       : "TERMINATED");

        for (int i = 0; i < curr->cmd->argCount; i++)
        {
            printf("%s ", curr->cmd->arguments[i]);
        }

        printf("\n");

        if (curr->status == TERMINATED)
        {
            if (prev == NULL)
            {
                *process_list = curr->next;
                freeCmdLines(curr->cmd);
                free(curr);
                curr = *process_list;
            }

            else
            {
                prev->next = curr->next;
                freeCmdLines(curr->cmd);
                free(curr);
                curr = prev->next;
            }
        }
        else
        {
            prev = curr;
            curr = curr->next;
        }
        index++;
    }
}

void redirectIO(cmdLine *cmd)
{
    if (cmd->outputRedirect != NULL)
    {
        close(1);
        if (open(cmd->outputRedirect, O_WRONLY | O_CREAT) == -1)
        {
            if (debug == 1)
                fprintf(stderr, "Failed to open file %s\n", cmd->outputRedirect);
            freeCmdLines(cmd);
            exit(EXIT_FAILURE);
        }
    }

    if (cmd->inputRedirect != NULL)
    {
        close(0);
        if (open(cmd->inputRedirect, O_RDONLY) == -1)
        {
            if (debug == 1)
                fprintf(stderr, "Failed to open file %s\n", cmd->inputRedirect);
            freeCmdLines(cmd);
            exit(EXIT_FAILURE);
        }
    }
}

int execute(cmdLine *pCmdLine)
{
    int pid = fork();

    if (pid == 0) // child process
    {

        redirectIO(pCmdLine);

        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1)
        {
            if (debug == 1)
                perror("Failed to execute the command\n");
            perror("1");
            freeCmdLines(pCmdLine);
            exit(EXIT_FAILURE);
        }
        else
        {
            if (debug == 1)
                fprintf(stderr, "PID: %d\nExecuting command: %s\n", pid, pCmdLine->arguments[0]);
            return pid;
        }
    }
    else
    { // father process
        if (pCmdLine->blocking == 1)
            waitpid(pid, 0, 0);
        return pid;
    }
}

void handelCommands(cmdLine *cmd)
{
    if (strcmp(cmd->arguments[0], "-d") == 0)
        debug = 1;

    else if (strcmp(cmd->arguments[0], "cd") == 0)
    {
        if (chdir(cmd->arguments[1]) == -1)
        {
            if (debug == 1)
                fprintf(stderr, "ERROR - cd failed: %s\n", cmd->arguments[1]);
        }
    }
    else if (strcmp(cmd->arguments[0], "suspend") == 0)
    {
        if (kill(atoi(cmd->arguments[1]), SIGTSTP) == -1)
        {
            if (debug == 1)
                fprintf(stderr, "ERROR - suspend failed\nproccess number: %d\n", atoi(cmd->arguments[1]));
        }
    }

    else if (strcmp(cmd->arguments[0], "wakeup") == 0)
    {
        if (kill(atoi(cmd->arguments[1]), SIGCONT) == -1)
        {
            if (debug == 1)
                fprintf(stderr, "ERROR - wakeup failed\nproccess number: %d\n", atoi(cmd->arguments[1]));
        }
    }
    else if (strcmp(cmd->arguments[0], "nuke") == 0)
    {
        if (kill(atoi(cmd->arguments[1]), SIGINT) == -1)
        {
            if (debug == 1)
                fprintf(stderr, "ERROR - nuke failed\nproccess number: %d\n", atoi(cmd->arguments[1]));
        }
    }
    else if (strcmp(cmd->arguments[0], "procs") == 0)
        printProcessList(&process_list);
    else
    {
        int pid = execute(cmd);
        addProcess(&process_list, cmd, pid);
    }
}

void handlePipe(cmdLine *cmd1, cmdLine *cmd2)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) // create pipe
    {
        if (debug == 1)
            perror("ERROR - creating pipe\n");
        exit(EXIT_FAILURE);
    }

    int pid1 = fork();

    if (pid1 == 0)
    { // in the first child

        if (debug == 1)
        {
            fprintf(stderr, "PID: %d\n", pid1);
            fprintf(stderr, "Executing command: %s\n", cmd1->arguments[0]);
        }

        close(STDOUT_FILENO); // close the standart output

        int dupwrite = dup(pipefd[1]); // duplicate write-end
        if (dupwrite == -1)
        {
            if (debug == 1)
                perror("Failed to duplicates the write-end.\n");
            exit(EXIT_FAILURE);
        }
        close(pipefd[1]); // close the write-end

        if (cmd1->outputRedirect != NULL)
        {
            if (debug == 1)
                perror("Can not change the output of the left-hand-side-process\n");
            exit(EXIT_FAILURE);
        }
        redirectIO(cmd1);

        if (execvp(cmd1->arguments[0], cmd1->arguments) == -1)
        {
            if (debug == 1)
                perror("Failed to execute the command\n");
            freeCmdLines(cmd1);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        close(pipefd[1]);  // close the write-end
        int pid2 = fork(); // fork a second child
        if (pid2 == 0)
        { // in the second child

            if (debug == 1)
                fprintf(stderr, "PID: %d\nExecuting command: %s\n", pid2, cmd2->arguments[0]);

            close(STDIN_FILENO); // close the standart output

            int dupread = dup(pipefd[0]); // duplicate read-end
            if (dupread == -1)
            {
                if (debug == 1)
                    perror("Failed to duplicates the write-end.\n");
                exit(EXIT_FAILURE);
            }
            close(pipefd[0]); // close the descriptor that was duplicates

            if (cmd1->inputRedirect != NULL)
            {
                if (debug == 1)
                    perror("Can not change the input of the right-hand-side-process\n");
                exit(EXIT_FAILURE);
            }
            redirectIO(cmd2);

            if (execvp(cmd2->arguments[0], cmd2->arguments) == -1)
            {
                if (debug == 1)
                    perror("Failed to execute the command\n");
                freeCmdLines(cmd2);
                exit(EXIT_FAILURE);
            }
        }
        else
        { // in the parent process
            close(pipefd[0]);
            waitpid(pid1, 0, 0);
            waitpid(pid2, 0, 0);
        }
        if (pid1 != 0 && pid2 != 0)
        {
            addProcess(&process_list, cmd1, pid1);
            addProcess(&process_list, cmd2, pid2);
        }
    }
}

void addToHistory(char *CL)
{
    if ((oldest <= newest) & (newest < HISTLEN))
    {
        if (history[newest] != NULL)
            free(history[newest]);
        history[newest] = CL;
        newest++;
        if (history[newest] != NULL) // if the array was full somewhen before
            oldest++;
    }

    else if (newest == HISTLEN)
    { // we filled the array
        if (history[oldest] != NULL)
            free(history[oldest]);
        newest = 0;
        oldest++;
        history[newest] = CL;
    }

    else if (oldest == HISTLEN - 1)
    {
        if (history[oldest] != NULL)
            free(history[oldest]);
        oldest = 0;
        newest++;
        history[newest] = CL;
    }

    else if (newest < oldest)
    { // the array is full and the newest pointer started from the beginning
        newest++;
        oldest++;
        if (history[newest] != NULL)
            free(history[newest]);
        history[newest] = CL;
    }
}

void runCommand(char *userIn)
{
    char userIncopy[256];
    strcpy(userIncopy, userIn);

    if (strstr(userIncopy, "|") != NULL)
    {
        char *firstcmdline = strtok(userIncopy, "|");
        char *seccmdline = strtok(NULL, "|");
        cmdLine *cmd1 = parseCmdLines(firstcmdline);
        cmdLine *cmd2 = parseCmdLines(seccmdline);
        if (cmd1 != NULL && cmd2 != NULL)
            handlePipe(cmd1, cmd2);
    }
    else
    {
        cmdLine *cmd = parseCmdLines(userIncopy);
        if (cmd != NULL)
            handelCommands(cmd);
    }
}

void freeHistory()
{
    for (int i = 0; i < HISTLEN; i++)
    {
        if (history[i] != NULL)
        {
            free(history[i]);
        }
    }
}

int main(int argc, char **argv)
{
    while (1)
    {
        char currPath[PATH_MAX];
        getcwd(currPath, PATH_MAX);
        printf("%s\n", currPath);

        char userIn[2048];
        fgets(userIn, 2048, stdin);
        userIn[strlen(userIn) - 1] = '\0'; // delete the '\n' last character

        if (strcmp(userIn, "history") == 0)
        {
            int index = 1;
            if (newest < oldest)
            {
                for (int i = oldest; i < HISTLEN; i++)
                {
                    printf("%d) %s\n", index, history[i]);
                    index++;
                }
                for (int i = 0; i <= newest; i++)
                {
                    printf("%d) %s\n", index, history[i]);
                    index++;
                }
            }
            else
            {
                for (int i = oldest; i < newest; i++)
                    if (history[i] != NULL)
                    {
                        printf("%d) %s\n", index, history[i]);
                        index++;
                    }
            }
        }
        else if (userIn[0] == '!')
        {
            if (userIn[1] == '!')
            {
                char *CL = malloc(strlen(history[newest - 1]) + 1);
                strcpy(CL, history[newest - 1]);
                addToHistory(CL);
                runCommand(CL);
            }
            else
            {
                int n = atoi(userIn + 1);
                int index = 1;
                int executed = 0;
                if (newest < oldest)
                {
                    for (int i = oldest; i < HISTLEN; i++)
                    {
                        if (index == n)
                        {
                            char *CL = malloc(strlen(history[i]) + 1);
                            strcpy(CL, history[i]);
                            addToHistory(CL);
                            runCommand(CL);
                            executed = 1;
                            break;
                        }
                        index++;
                    }
                    for (int i = 0; i <= newest; i++)
                    {
                        if (index == n)
                        {
                            char *CL = malloc(strlen(history[i]) + 1);
                            strcpy(CL, history[i]);
                            addToHistory(CL);
                            runCommand(CL);
                            executed = 1;
                            break;
                        }
                        index++;
                    }
                    if (executed == 0)
                        perror("History index out of range\n");
                }
                else
                {
                    for (int i = oldest; i <= newest; i++)
                    {
                        if (index == n)
                        {
                            char *CL = malloc(strlen(history[i]) + 1);
                            strcpy(CL, history[i]);
                            addToHistory(CL);
                            runCommand(CL);
                            executed = 1;
                            break;
                        }
                        index++;
                    }
                }
            }
        }

        else if (strcmp(userIn, "quit") == 0)
        {
            freeProcessList(process_list);
            freeHistory();
            return 0;
        }

        else
        {
            char *CL = malloc(strlen(userIn) + 1);
            strcpy(CL, userIn);
            addToHistory(CL);
            runCommand(CL);
        }
    }
    if (process_list != NULL)
    {
        freeProcessList(process_list);
    }

    freeHistory();
    return 0;
}
