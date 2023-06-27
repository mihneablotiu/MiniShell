# Tema 4 SO - Minishell - 16.05.2023
# Blotiu Mihnea-Andrei - 333CA (FIRST BONUS IMPLEMENTED)

* Introduction
    - The main idea of the homework was to implement a minishell similar to the one existent in
    the Linux terminal that would support 3 types of commands: internal commands (the change 
    directory command, exit/quit), external commands (executed by execvpe) and environment variables
    assignments.
    - This was done in a true while loop waiting for commands until a quit/exit command comes and
    parsing them. That being said, the most important function that travells the commands tree
    is the parse_command function and its implementation is going to be described below.

* Parse-command:
    - In this function we just get the current command as an input and we check what type of a
    command it is. 
    - If it is a simple command we just call the parse simple function. Otherwise, if it is a 
    composed command we have to recursivelly call the parse command function down into the
    tree and return the first, second or a combination of the return values depending on the
    operator used between the commands.
    - If it is a pipe or a parallel command we call specific functions for those because we
    have to do more actions than just calling recursivelly.

* Parse-simple:
    - In this function we take care about commands that cannot be decomposed so we have to execute
    them.
    - There can be three types of commands to be executed. We verify what type of command we have
    to execute and we call the specific function for those:
        - An internal command: cd, exit or quit;
        - An environment variable assignment;
        - An external command.

    * Internal-command:
        - If we have to execute an internal command we just divide the execution depending on what
        internal command we have to execute:
        - For executing exit/quit, we just close the standard and valgrind file descriptors and we
        return shell exit which will stop the true while loop and will no longer wait for commands
        - For executing cd, we call cd_wrapper which will make the redirections for cd command.
        Here, the ideea is that even if the cd command can have only an error output, we still have
        to create the out files if the redirections are made. After creating the files we call shell_cd
        which changes the actual directory to the given path or, for the BONUS, if there is no path
        given, we change it to $HOME if the variable is defined. This function returns if the change
        directory command was effective and if not, we print an error to the error file descriptor.

    * Environment-assignment:
        - If we have an environment assignment we just extract the name and the value from the command
        and we change the value of the env variable by using setenv function

    * External-command:
        - For an external command, we firstly have to fork the process because we want to run the
        command in the child process.
        - In the parent we just wait for the child to finish the execution of the command and if
        it ran successfully we return its output status.
        - In the child, we perform the redirections of the input/output/error files (if necessary)
        This is done by creating the files (if needed) and by using dup2 we duplicate the stdout,
        stdin and stderr to point to the new files. After doing the redirections we just execute
        the command using execvpe and we stop the execution in case the exec could not run the
        given command.

* Run-in-parallel:
    - In order to run in parallel a command, we have to acutally run two separate commands;
    - To do that we just have to make to different children processes, and each of those is going
    to execute one of the commands, recursivelly.
    - The parent is going to wait for both of the children to finish their execution and the final
    result is going to be the combination of the results of the children.

* Run-on-pipe:
    - In order to run two commands on pipe we actually have to do the same thing as in running
    two commands in parallel with the mention that the output of the first command have to be
    the input of the second command.
    - To do that we just create an anonymous pipe and:
        - In the parent we have to close both ends of the pipe because the parent does not use
        the pipe. After that, we have to wait for both of the childrent to finish their exec
        and the final result is going to be the result of the second command (because that
        makes sense to do when running a command on pipe).
        - In the child that executes the first command, we close the read end of the pipe
        and with dup2 we change the standard output to be the other end of the pipe that 
        is going to be the input of the other child
        - In the child that executes the second command, we close the output end of the pipe
        and with dup2 we change the standard input to be the read end of the pipe where the
        first childe wrote its output.