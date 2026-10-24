#include "command.h"
#include "tokenizer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>

using namespace std;

volatile sig_atomic_t childFinished = 0;

void childSignal(int)
{
    childFinished = 1;
}

void logChild(pid_t pid)
{
    ofstream file("child.log", ios::app);
    file << "Child process " << pid << " terminated\n";
}

void checkChildren()
{
    int status;
    pid_t pid;

    childFinished = 0;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        logChild(pid);
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(
        _numberOfAvailableArguments * sizeof(char *)
    );

    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfArguments + 1 >= _numberOfAvailableArguments)
    {
        _numberOfAvailableArguments *= 2;

        _arguments = (char **)realloc(
            _arguments,
            _numberOfAvailableArguments * sizeof(char *)
        );
    }

    _arguments[_numberOfArguments++] = argument;
    _arguments[_numberOfArguments] = NULL;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _numberOfSimpleCommands = 0;

    _simpleCommands = (SimpleCommand **)malloc(
        _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *)
    );

    _outFile = NULL;
    _inputFile = NULL;
    _errFile = NULL;

    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfSimpleCommands == _numberOfAvailableSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;

        _simpleCommands = (SimpleCommand **)realloc(
            _simpleCommands,
            _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *)
        );
    }

    _simpleCommands[_numberOfSimpleCommands++] = simpleCommand;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0;
             j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    free(_outFile);
    free(_inputFile);
    free(_errFile);

    _numberOfSimpleCommands = 0;

    _outFile = NULL;
    _inputFile = NULL;
    _errFile = NULL;

    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::print()
{
   for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0;
             j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            cout << _simpleCommands[i]->_arguments[j] << " ";
        }

        if (i < _numberOfSimpleCommands - 1)
            cout << "| ";

    }

    cout << endl;

    if (_inputFile)
        cout << "Input: " << _inputFile << endl;

    if (_outFile)
        cout << "Output: " << _outFile << endl;

    if (_errFile)
        cout << "Error: " << _errFile << endl;

    if (_background)
        cout << "Background: YES" << endl;
}

void parse(vector<Token> &tokens)
{
    Command currentCommand;
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    currentCommand.insertSimpleCommand(currentSimpleCommand);

    bool bad = false;

    for (int i = 0; tokens[i].type != TOKEN_EOF; i++)
    {
        Token token = tokens[i];

        if (token.type == TOKEN_PIPE)
        {
            if (currentSimpleCommand->_numberOfArguments == 0)
            {
                cerr << "Missing command before pipe\n";
                bad = true;
                break;
            }

            currentSimpleCommand = new SimpleCommand();
            currentCommand.insertSimpleCommand(currentSimpleCommand);
        }
        else if (token.type == TOKEN_BACKGROUND)
        {
            if (tokens[i + 1].type != TOKEN_EOF)
            {
                cerr << "& must be at the end\n";
                bad = true;
                break;
            }

            currentCommand._background = 1;
        }
        else if (token.type == TOKEN_REDIRECT ||
                 token.type == TOKEN_APPEND ||
                 token.type == TOKEN_INPUT ||
                 token.type == TOKEN_ERROR ||
                 token.type == TOKEN_REDIRECT_AND_ERROR)
        {
            TokenType nextType = tokens[i + 1].type;

            if (nextType == TOKEN_EOF ||
                nextType == TOKEN_PIPE ||
                nextType == TOKEN_BACKGROUND ||
                nextType == TOKEN_REDIRECT ||
                nextType == TOKEN_APPEND ||
                nextType == TOKEN_INPUT ||
                nextType == TOKEN_ERROR ||
                nextType == TOKEN_REDIRECT_AND_ERROR)
            {
                cerr << "Missing filename after redirection\n";
                bad = true;
                break;
            }

            char *file = strdup(tokens[++i].value.c_str());

            if (token.type == TOKEN_INPUT)
            {
                free(currentCommand._inputFile);
                currentCommand._inputFile = file;
            }
            else if (token.type == TOKEN_ERROR)
            {
                free(currentCommand._errFile);
                currentCommand._errFile = file;
            }
            else
            {
                free(currentCommand._outFile);
                currentCommand._outFile = file;

                currentCommand._append =
                    token.type == TOKEN_APPEND ||
                    token.type == TOKEN_REDIRECT_AND_ERROR;

                currentCommand._out_error =
                    token.type == TOKEN_REDIRECT_AND_ERROR;
            }
        }
        else
        {
            currentSimpleCommand->insertArgument(
                strdup(token.value.c_str())
            );
        }
    }

    if (currentSimpleCommand->_numberOfArguments == 0)
    {
        cerr << "Missing command\n";
        bad = true;
    }

    if (bad)
    {
        currentCommand.clear();
        return;
    }

    currentCommand.execute();
}

void Command::execute()
{
    if (_numberOfSimpleCommands == 0 ||
        _simpleCommands[0]->_numberOfArguments == 0)
    {
        clear();
        return;
    }

    char *name = _simpleCommands[0]->_arguments[0];

    if (_numberOfSimpleCommands == 1 &&
        strcmp(name, "exit") == 0)
    {
        clear();
        cout << "Good bye!!" << endl;
        exit(0);
    }

    if (_numberOfSimpleCommands == 1 &&
        strcmp(name, "cd") == 0)
    {
        char *dir = _simpleCommands[0]->_arguments[1];

        if (dir == NULL)
            dir = getenv("HOME");

        if (dir == NULL || chdir(dir) == -1)
            perror("cd");

        clear();
        return;
    }

    vector<pid_t> children;
    int input = -1;

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        int fd[2] = {-1, -1};

        if (i < _numberOfSimpleCommands - 1)
        {
            if (pipe(fd) == -1)
            {
                perror("pipe");

                if (input != -1)
                    close(input);

                clear();
                return;
            }
        }

        pid_t pid = fork();

        if (pid == -1)
        {
            perror("fork");

            if (input != -1)
                close(input);

            if (fd[0] != -1)
                close(fd[0]);

            if (fd[1] != -1)
                close(fd[1]);

            clear();
            return;
        }

        if (pid == 0)
        {
            signal(SIGINT, SIG_DFL);

            if (input != -1)
            {
                if (dup2(input, STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    _exit(1);
                }
            }

            if (fd[1] != -1)
            {
                if (dup2(fd[1], STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    _exit(1);
                }
            }

            if (input != -1)
                close(input);

            if (fd[0] != -1)
                close(fd[0]);

            if (fd[1] != -1)
                close(fd[1]);

            if (i == 0 && _inputFile != NULL)
            {
                int file = open(_inputFile, O_RDONLY);

                if (file == -1)
                {
                    perror(_inputFile);
                    _exit(1);
                }

                if (dup2(file, STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    close(file);
                    _exit(1);
                }

                close(file);
            }

            if (i == _numberOfSimpleCommands - 1 &&
                _outFile != NULL)
            {
                int flags = O_WRONLY | O_CREAT;

                if (_append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;

                int file = open(_outFile, flags, 0666);

                if (file == -1)
                {
                    perror(_outFile);
                    _exit(1);
                }

                if (dup2(file, STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    close(file);
                    _exit(1);
                }

                if (_out_error)
                {
                    if (dup2(file, STDERR_FILENO) == -1)
                    {
                        perror("dup2");
                        close(file);
                        _exit(1);
                    }
                }

                close(file);
            }

            if (_errFile != NULL)
            {
                int file = open(
                    _errFile,
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0666
                );

                if (file == -1)
                {
                    perror(_errFile);
                    _exit(1);
                }

                if (dup2(file, STDERR_FILENO) == -1)
                {
                    perror("dup2");
                    close(file);
                    _exit(1);
                }

                close(file);
            }

            execvp(
                _simpleCommands[i]->_arguments[0],
                _simpleCommands[i]->_arguments
            );

            perror(_simpleCommands[i]->_arguments[0]);
            _exit(127);
        }

        children.push_back(pid);

        if (input != -1)
            close(input);

        if (fd[1] != -1)
            close(fd[1]);

        input = fd[0];
    }

    if (input != -1)
        close(input);

    if (!_background)
    {
        for (pid_t pid : children)
        {
            int status;
            pid_t result;

            do
            {
                result = waitpid(pid, &status, 0);
            }
            while (result == -1 && errno == EINTR);

            if (result == pid)
                logChild(pid);
        }
    }
    else
    {
        for (pid_t pid : children)
            cout << "Started background process " << pid << endl;
    }

    clear();
}

void Command::prompt()
{
    signal(SIGINT, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = childSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sigaction(SIGCHLD, &sa, NULL);

    string line;

    while (true)
    {
        if (childFinished)
            checkChildren();

        cout << "myshell> " << flush;

        if (!getline(cin, line))
            break;

        vector<Token> tokens = tokenize(line);

        parse(tokens);

        if (childFinished)
            checkChildren();
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand = NULL;

int main()
{
    Command::_currentCommand.prompt();
    return 0;
}