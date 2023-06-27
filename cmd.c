// SPDX-License-Identifier: BSD-3-Clause
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1
#define ERR         2
#define VALGRIND    3

extern char **environ;

/**
 * The function that makes the input, output, error redirections for a simple external command
 * if/from the given files if necessary and sets the file descriptors to their new values
 */
static void redirect(simple_command_t *s, char *stdinFile, char *stdoutFile, char *stderrFile,
					 int *fdIn, int *fdOut, int *fdErr, int *fdOutErr)
{
	// If we have a input file, we set the file descriptor for it
	if (stdinFile != NULL) {
		*fdIn = open(stdinFile, O_RDONLY);

		if ((*fdIn) == -1 || dup2((*fdIn), READ) == -1) {
			close(*fdIn);

			free(stdinFile);
			free(stdoutFile);
			free(stderrFile);
			exit(-1);
		}
	}

	// If we have the same output and err file, we set the same file descriptor for them
	if (stdoutFile != NULL && stderrFile != NULL && strcmp(stdoutFile, stderrFile) == 0) {
		if (s->io_flags == IO_REGULAR) {
			*fdOutErr = open(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if ((*fdOutErr) == -1 || dup2((*fdOutErr), WRITE) == -1 || dup2((*fdOutErr), ERR) == -1) {
				close(*fdOutErr);

				free(stdinFile);
				free(stdoutFile);
				free(stderrFile);
				exit(-1);
			}
		}
	} else {
		// If we have a output file, we set the file descriptor for it depending on if we have
		// to open the file in the regular or append mode
		if (stdoutFile != NULL) {
			if (s->io_flags == IO_REGULAR) {
				*fdOut = open(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if ((*fdOut) == -1 || dup2((*fdOut), WRITE) == -1) {
					close(*fdOut);

					free(stdinFile);
					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			} else if (s->io_flags == IO_OUT_APPEND) {
				*fdOut = open(stdoutFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if ((*fdOut) == -1 || dup2((*fdOut), WRITE) == -1) {
					close(*fdOut);

					free(stdinFile);
					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			}
		}

		// If we have a err file, we set the file descriptor for it depending on if we have
		// to open the file in the regular or append mode
		if (stderrFile != NULL) {
			if (s->io_flags == IO_REGULAR) {
				*fdErr = open(stderrFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if ((*fdErr) == -1 || dup2((*fdErr), ERR) == -1) {
					close(*fdErr);

					free(stdinFile);
					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			} else if (s->io_flags == IO_ERR_APPEND) {
				*fdErr = open(stderrFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if ((*fdErr) == -1 || dup2((*fdErr), ERR) == -1) {
					close(*fdErr);

					free(stdinFile);
					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			}
		}
	}
}

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	// We get the path for the cd command
	char *path = get_word(dir);
	int retVal = -1;

	// If the path that we want to change the directory to is a null one, then we try to change
	// the current directory to the HOME env variable if it is set. If it is not, the directory
	// stays the same. If there is a path, we change the directory to the new path
	if (path == NULL) {
		char *homeEnv = getenv("HOME");

		if (homeEnv != NULL)
			retVal = chdir(homeEnv);
	} else {
		retVal = chdir(path);
	}

	free(path);
	return retVal;
}

/**
 * Internal change-directory wrapper. Here we make the redirections if needed for the cd internal
 * command before calling the shell_cd function. Also, here, we don't care about the input files
 * because cd internal command cannot have a input file
 */
static bool cd_wrapper(simple_command_t *s)
{
	int fdOut = -1, fdErr = -1, fdOutErr = -1;
	char *stdoutFile = get_word(s->out);
	char *stderrFile = get_word(s->err);

	// If there exists the same file for the standard output and err we create just one file
	if (stdoutFile != NULL && stderrFile != NULL && strcmp(stdoutFile, stderrFile) == 0) {
		if (s->io_flags == IO_REGULAR) {
			fdOutErr = open(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (fdOutErr == -1) {
				close(fdOutErr);

				free(stdoutFile);
				free(stderrFile);
				exit(-1);
			}
		}
	} else {
		// If we have a output file, we set the file descriptor for it depending on if we have
		// to open the file in the regular or append mode. Even if cd cannot have a standard
		// output, we still have to create the file if a redirect is made
		if (stdoutFile != NULL) {
			if (s->io_flags == IO_REGULAR) {
				fdOut = open(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fdOut == -1) {
					close(fdOut);

					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			} else if (s->io_flags == IO_OUT_APPEND) {
				fdOut = open(stdoutFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if (fdOut == -1) {
					close(fdOut);

					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			}
		}

		// If we have a err file, we set the file descriptor for it depending on if we have
		// to open the file in the regular or append mode.
		if (stderrFile != NULL) {
			if (s->io_flags == IO_REGULAR) {
				fdErr = open(stderrFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fdErr == -1) {
					close(fdErr);

					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			} else if (s->io_flags == IO_ERR_APPEND) {
				fdErr = open(stderrFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if (fdErr == -1) {
					close(fdErr);

					free(stdoutFile);
					free(stderrFile);
					exit(-1);
				}
			}
		}
	}

	// After redirections we call the function and wait for its return value
	int retVal = shell_cd(s->params);

	// If the result of changing the directory if a negative one we have to put some output in
	// the standard error file if there is a redirection or not. Otherwise no output exists so
	// we just close the redirections and return the success return value
	if (retVal == -1) {
		if (fdErr != -1) {
			FILE *errFile = fdopen(fdErr, "w");

			fprintf(errFile, "%s\n", "Error at changing directory");
			fclose(errFile);
		} else {
			fprintf(stderr, "%s\n", "Error at changing directory");
		}
	}

	free(stdoutFile);
	free(stderrFile);

	if (fdOut != -1)
		close(fdOut);

	if (fdErr != -1)
		close(fdErr);

	if (fdOutErr != -1)
		close(fdOutErr);

	return retVal;
}

/**
 * Internal exit/quit command. Here we just close the standard and valgrind file descriptors and
 * we return shell exit in order not to wait for any more commands
 */
static int shell_exit(void)
{
	close(READ);
	close(WRITE);
	close(ERR);
	close(VALGRIND);

	return SHELL_EXIT;
}

/**
 * The function that frees the argument values taken by get_argv already implemented function
 */
static void free_argv(char **argv, int numberOfArguments)
{
	for (int i = 0; i < numberOfArguments; i++)
		free(argv[i]);

	free(argv);
}

/**
 * The function that manages the specific function calls if we are sure that the simple command
 * is an internal one
 */
static int internal_command(simple_command_t *s, char *command)
{
	if (strcmp(command, "cd") == 0) {
		free(command);
		return cd_wrapper(s);
	}

	free(command);
	return shell_exit();
}

/**
 * The function that manages the environment assignments if we are sure that the simple command
 * is an env assignment
 */
static int environment_assignment(simple_command_t *s)
{
	// The name of the env variable is the first string of the command
	char *name = (char *) s->verb->string;

	// The value of the env variable is taken with get word of the last part of the command
	// because the value can also be an env variable so it has to be expanded
	char *value = get_word(s->verb->next_part->next_part);

	int retValue = setenv(name, value, 1);

	free(value);
	return retValue;
}

/**
 * The function that manages the specific function calls if we are sure that the simple command
 * is an external one so we don't have to manage it by ourselves
 */
static int external_command(simple_command_t *s, char *command)
{
	int status, numberOfArguments = 0;

	// We get the arguments of the command and we fork because we want to execute the command
	// in the child and not in the parrent
	char **argv = get_argv(s, &numberOfArguments);
	pid_t pid = fork();

	// If the fork failed we just end the execution
	if (pid < 0) {
		perror("Fork error\n");
		abort();
	} else {
		if (pid > 0) {
			// In the parent we just wait for the child to finish the execution of the command
			// we free the resoruces and if the child exited correctly we return it's exit status
			// Otherwise it means that the child did not exit so we should return 1 error code
			waitpid(pid, &status, 0);

			free_argv(argv, numberOfArguments);
			free(command);
			if (WIFEXITED(status))
				return WEXITSTATUS(status);

			return 1;
		}

		// In the child we firsly make the redirections of the command if necessary
		int retValue = 0;
		char **env = environ;

		char *stdinFile = get_word(s->in);
		char *stdoutFile = get_word(s->out);
		char *stderrFile = get_word(s->err);

		int fdIn = -1, fdOut = -1, fdErr = -1, fdOutErr = -1;

		redirect(s, stdinFile, stdoutFile, stderrFile,
					&fdIn, &fdOut, &fdErr, &fdOutErr);

		free(stdinFile);
		free(stdoutFile);
		free(stderrFile);

		// Then we pass the command to execvpe and if the execvpe returns a value (aka the
		// execution of the command failed) we just close the file descriptors and we
		// print an error and abort the execution
		retValue = execvpe(command, argv, env);
		if (fdIn != -1)
			close(fdIn);

		if (fdOut != -1)
			close(fdOut);

		if (fdErr != -1)
			close(fdErr);

		if (fdOutErr != -1)
			close(fdOutErr);

		if (retValue == -1) {
			fprintf(stderr, "%s '%s'\n", "Execution failed for", command);
			abort();
		}
	}

	return 0;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	if (s == NULL || level < 0)
		return SHELL_EXIT;

	// We take the name of the command and we check what type of a command it is and then call
	// the respective function
	char *command = get_word(s->verb);

	if (strcmp(command, "cd") == 0 || strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0)
		return internal_command(s, command);

	if (strchr(command, '=') != 0) {
		free(command);
		return environment_assignment(s);
	}

	return external_command(s, command);
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int status1;

	// Here we create the first child of the process
	pid_t pid1 = fork();

	if (pid1 < 0) {
		perror("Fork error\n");
		abort();
	} else {
		if (pid1 > 0) {
			// In the parent we create the second child
			int status2;
			pid_t pid2 = fork();

			if (pid2 < 0) {
				perror("Fork error\n");
				abort();
			} else {
				if (pid2 > 0) {
					// In the parent we wait for both children to finish their execution and
					// then we just return the bitwise or value between the return value of
					// the both child processes

					waitpid(pid1, &status1, 0);
					waitpid(pid2, &status2, 0);

					if (WIFEXITED(status1) && WIFEXITED(status2))
						return WEXITSTATUS(status1) | WEXITSTATUS(status2);

					return 1;
				}

				// In one of the children we execute the first command
				int retValue = parse_command(cmd1, level, father);

				exit(retValue);
			}
		} else {
			// In the other child we execute the second command
			int retValue = parse_command(cmd2, level, father);

			exit(retValue);
		}
	}

	return 0;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	// Here we create the pipe
	int currentPipe[2];
	int returnVal = pipe(currentPipe);

	// If the pipe failed we just abort the command
	if (returnVal < 0) {
		perror("Error at creating the pipe\n");
		abort();
	}

	int status1;

	// Otherwise, it means that the pipe was created correctly so we can create
	// the first child process
	pid_t pid1 = fork();

	if (pid1 < 0) {
		perror("Fork error\n");
		abort();
	} else {
		if (pid1 > 0) {
			// In the parent we create the second child process
			int status2;
			pid_t pid2 = fork();

			if (pid2 < 0) {
				perror("Fork error\n");
				abort();
			} else {
				if (pid2 > 0) {
					// In the parent we just close the both ends of the pipe because the parrent
					// does not use any of them and waits for both of the children to finish
					// their execution
					close(currentPipe[0]);
					close(currentPipe[1]);

					waitpid(pid1, &status1, 0);
					waitpid(pid2, &status2, 0);

					// We are interested on the exit status of the second command because that is
					// going to be the exit status of the entire pipe command
					if (WIFEXITED(status2))
						return WEXITSTATUS(status2);

					return 1;
				}

				// In the second child we run the second command so it means that we have to close
				// the write end of the pipe and duplicate the std read fd to read from the read
				// end of the pipe
				close(currentPipe[1]);

				if (dup2(currentPipe[0], READ) == -1) {
					close(currentPipe[0]);
					exit(-1);
				}

				// Afterwards we just run the second command, after it finished running we also close
				// the second end of the pipe and return the exit value
				int retValue = parse_command(cmd2, level, father);

				close(currentPipe[0]);
				exit(retValue);
			}
		} else {
			// In the first child, we run the first command so we have to close the input end of the
			// pipe and duplicate the output end of the pipe in order to write its output as an input
			// for the next process
			close(currentPipe[0]);

			if (dup2(currentPipe[1], WRITE) == -1) {
				close(currentPipe[1]);
				exit(-1);
			}

			// Afterwards we just run the first command, after it finished running we also close
			// the second end of the pipe and return the exit value
			int retValue = parse_command(cmd1, level, father);

			close(currentPipe[1]);
			exit(retValue);
		}
	}

	return 0;
}

/**
 * Parse and execute a command. Here we just see what type of command we have to execute and
 * depending on the type of command we call the specific function for it
 */
int parse_command(command_t *c, int level, command_t *father)
{
	if (c == NULL || level < 0)
		return SHELL_EXIT;

	if (c->op == OP_NONE)
		return parse_simple(c->scmd, level + 1, c);

	int retValue = 0;

	switch (c->op) {
	case OP_SEQUENTIAL:
		retValue = parse_command(c->cmd1, level + 1, c);
		retValue |= parse_command(c->cmd2, level + 1, c);

		break;

	case OP_PARALLEL:
		retValue = run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_NZERO:
		retValue = parse_command(c->cmd1, level + 1, c);
		if (retValue != 0)
			retValue = parse_command(c->cmd2, level + 1, c);

		break;

	case OP_CONDITIONAL_ZERO:
		retValue = parse_command(c->cmd1, level + 1, c);
		if (retValue == 0)
			retValue = parse_command(c->cmd2, level + 1, c);

		break;

	case OP_PIPE:
		retValue = run_on_pipe(c->cmd1, c->cmd2, level + 1, c);
		break;

	default:
		return SHELL_EXIT;
	}

	return retValue;
}
