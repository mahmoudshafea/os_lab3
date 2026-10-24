# os_lab3

# Mini Shell

## Description

A simple shell implemented in C++ for the Operating Systems lab.

It supports executing commands, pipes, input and output redirection, and background processes.

## Features

 Execute commands using `fork()` and `execvp()`.
 Support pipes (`|`).
 Input redirection (`<`).
 Output redirection (`>`).
 Append output (`>>`).
 Error redirection (`2>`).
 Redirect both output and errors (`>>&`).
 Run commands in the background (`&`).
 Built-in commands: `cd` and `exit`.
 Ignore Ctrl+C in the shell.
 Log terminated child processes to a file.

## Files

 `command.cc`: Shell implementation.
 `command.h`: Command and SimpleCommand definitions.
 `tokenizer.cc`: Splits input into tokens.
 `tokenizer.h`: Token definitions.
 `Makefile`: Compiles the project.

## Compilation

Run the following command:

```bash
make