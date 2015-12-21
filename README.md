(Taken from Stanley Eisentat's CPSC 323, Assignment 5)

CPSC 323   Homework #5   The Shell Game: Sister Sue Saw B-Shells ...

Bsh is a simple shell, a baby brother of the Bourne-again shell
bash, and offers a limited subset of bash's functionality (plus some extras):

- local variables

- simple command execution with zero or more arguments

- redirection of the standard input (<)

- redirection of the standard output (>, >>)

- pipelines (|) consisting of an arbitrary number of commands, each having zero
  or more arguments

- backgrounded commands;

- multiple commands per line, separated by ; or & or && or ||

- groups of commands (aka subcommands), enclosed in parentheses

- directory manipulation:

    cd directoryName
    cd                  (equivalent to "cd $HOME", where HOME
			 is an environment variable)
    dirs                (print to stdout the current working
			 directory as reported by getcwd())

- other built-in commands

    wait                Wait until all children of the shell process have died.
			The status is 0.  Note:  The bash wait command takes
			command-line arguments that specify which children to
			wait for (vs. all of them).

- reporting the status of the last simple command, pipeline, or subcommand
  executed in the foreground by setting the environment variable $? to its
  "printed" value (e.g., "0" if the value is zero).

Once the command line has been parsed, the exact semantics of Bsh are those
of bash, except for the status variable, and the notes and fine points listed
below.

The assignment is to write the process() function called from Hwk5/mainBsh.c;
thus you should use (i.e., link with)

* Hwk5/mainBsh.o as the main program (source is Hwk5/mainBsh.c)

* Hwk5/getLine.o to read command lines (interface in Hwk5/getLine.h; source
  is Hwk1/getLine.c)

* Hwk5/parse.o to tokenize each command line and parse it into a syntactically
  correct tree of CMD structures (interface in Hwk5/parse.h).

Fine Points
~~~~~~~~~~~
1. For a simple command, the status is that of the program executed (*), or
   the global variable errno if some system call failed while setting up to
   execute the program.

     (*) This status is normally the value WEXITSTATUS(status), where the
     variable status contains the value returned by the call to wait(&STATUS)
     that reported the death of that process.  However, for processes that are
     killed (i.e., for which WIFEXITED(status) is false), that value may be
     zero.  Thus you should use the expression

       (WIFEXITED(x) ? WEXITSTATUS(x) : 128+WTERMSIG(x))

     instead.

   For a backgrounded command, the status is 0.

   For a pipeline, the status is that of the latest (i.e., rightmost) stage to
   fail, or 0 if the status of every stage is true.  (This is the behavior of
   bash with the pipefail option enabled.)

   For a subcommand, the status is that of the last command/subcommand to be
   executed.

   For a built-in command, the status is 0 if successful, the value of errno if
   a system call failed, and 1 otherwise (e.g., when the number of arguments is
   incorrect).

   Note that this may differ from the status reported by bash.

2. In bash the status $? is an internal shell variable.  However, since Bsh
   does not expand these variables, it has no mechanism to check their value.
   Thus in Bsh the status is an environment variable, which can be checked
   using /usr/bin/printenv (i.e., printenv ?).

3. The command separators && and || have the same precedence, lower than |, but
   higher than ; or &.

   && causes the command following (a simple command, pipeline, or subcommand)
   to be skipped if the current command exits with a nonzero status (= FALSE,
   the opposite of C).  The status of the skipped command is that of the
   current command.

   || causes the command following to be skipped if the current command exits
   with a zero status (= TRUE, the opposite of C).  The status of the skipped
   command is that of the current command.

4. While executing a command, pipeline, or subcommand, Bsh waits until it
   terminates, unless it is followed by an &.  Bsh ignores SIGINT interrupts
   while waiting, but child processes (other than subshells) do not.  Hint:
   Do not implement signals until everything else seems to be working.

5. An EOF (^D in column 1) causes Bsh to exit since getLine() returns NULL.

6. Anything written to stdout by a built-in command is redirectable.

   When a built-in command fails, Bsh continues to execute commands.

   When a built-in command is invoked within a pipeline, is backgrounded, or
   appears in a subcommand, that command has no effect on the parent shell.
   For example, the commands

     (2)$ cd /c/cs323 | ls

   and

     (3)$ ls & cd .. & ls

   do not work as you might otherwise expect.

7. When a redirection fails, Bsh does not execute the command.  The status of
   the command (or pipeline stage) is the errno of the system call that failed.

8. When Bsh runs a command in the background, it writes the process id to
   stderr using the format "Backgrounded: %d\n".

9. Bsh reaps all zombies periodically (i.e., at least once during each call to
   process()) to avoid running out of processes.  When it does so, it writes
   the process id and status to stderr using the format "Completed: %d (%d)\n".

Appendix
~~~~~~~~
The syntax for a command is

  <stage>    = <simple> / (<command>)
  <pipeline> = <stage> / <pipeline> | <stage>
  <and-or>   = <pipeline> / <and-or> && <pipeline> / <and-or> || <pipeline>
  <sequence> = <and-or> / <sequence> ; <and-or> / <sequence> & <and-or>
  <command>  = <sequence> / <sequence> ; / <sequence> &

where a <simple> is a single command with arguments and I/O redirection, but no
|, &, ;, &&, ||, (, or ).

A command is represented by a tree of CMD structs corresponding to its simple
commands and the "operators" PIPE, && (SEP_AND), || (SEP_OR), ; (SEP_END), &
(SEP_BG), and SUBCMD.  The tree corresponds to how the command is parsed by a
bottom-up using the grammar above.

Note that I/O redirection is associated with a <stage> (i.e., a <simple> or
subcommand), but not with a <pipeline> (input/output redirection for the
first/last stage is associated with the stage, not the pipeline).

One way to write such a parser is to associate a function with each syntactic
type.  That function calls the function associated with its first alternative
(e.g., <stage> for <pipeline>), which consumes all tokens immediately following
that could be part of it.  If at that point the next token is one that could
lead to its second alternative (e.g., | in <pipeline> | <stage>), then that
token is consumed and the associated function called again.  If not, then the
tree is returned.

A CMD struct contains the following fields:

 typedef struct cmd {
   int type;             // Node type (SIMPLE, PIPE, SEP_AND, SEP_OR,
			 //   SEP_END, SEP_BG, SUBCMD, or NONE)

   int nLocal;           // Number of local variable assignments
   char **locVar;        // Array of local variable names and values to assign
   char **locVal;        //   to them when command executes or NULL (default)

   int argc;             // Number of command-line arguments
   char **argv;          // Null-terminated argument vector

   int fromType;         // Redirect stdin?
			 //  (NONE (default), RED_IN, RED_IN_HERE, RED_IN_CLS)
   char *fromFile;       // File to redirect stdin. contents of here
			 //   document, or NULL (default)

   int toType;           // Redirect stdout?
			 //  (NONE (default), RED_OUT, RED_OUT_APP, RED_OUT_CLS,
			 //   RED_OUT_ERR, RED_OUT_RED)
   char *toFile;         // File to redirect stdout or NULL (default)

   struct cmd *left;     // Left subtree or NULL (default)
   struct cmd *right;    // Right subtree or NULL (default)
 } CMD;

The tree for a <simple> is a single struct of type SIMPLE that specifies its
local variables (nLocal, locVar[], locVal[]) and arguments (argc, argv[]);
and whether and where to redirect its standard input (fromType, fromFile) and
its standard output (toType, toFile).  The left and right children are NULL.

The tree for a <stage> is either the tree for a <simple> or a struct
of type SUBCMD (which may have redirection) whose left child is the tree
representing the <command> and whose right child is NULL.

The tree for a <pipeline> is either the tree for a <stage> or a struct
of type PIPE whose left child is the tree representing the <pipeline> and
whose right child is the tree representing the <stage>.

The tree for an <and-or> is either the tree for a <pipeline> or a struct
of type && (= SEP_AND) or || (= SEP_OR) whose left child is the tree
representing the <and-or> and whose right child is the tree representing
the <pipeline>.

The tree for a <sequence> is either the tree for an <and-or> or a struct of
type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree representing
the <sequence> and whose right child is the tree representing the <and-or>.

The tree for a <command> is either the tree for a <sequence> or a struct of
type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree representing
the <sequence> and whose right child is NULL.

While the grammar above captures the syntax of bash commands, it does not
reflect the semantics of &, which specify that only the preceding <and-or>
should be executed in the background, not the entire preceding <sequence>.
