/*
** aesdsocket.c
*/
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

#define PORT "9000"		// Port we're listening on

#define TEMP_OUTPUT_PATH "/var/tmp/aesdsocketdata"

typedef struct AESD_SERVER_ARGUMENT_TAG
{
  bool deamon_mode;
} AESD_SERVER_ARGUMENT;

static AESD_SERVER_ARGUMENT server_argument;

void
sigint_handler (int signum, siginfo_t * info, void *extra)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  printf ("Catch sig handler %d\n", signum);

  errno = saved_errno;
}

// Get sockaddr, IPv4 or IPv6:
void *
get_in_addr (struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    {
      return &(((struct sockaddr_in *) sa)->sin_addr);
    }

  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

// Return a listening socket
int
get_listener_socket (void)
{
  int listener;			// Listening socket descriptor
  int yes = 1;			// For setsockopt() SO_REUSEADDR, below
  int rv;

  struct addrinfo hints, *ai, *p;

  // Get us a socket and bind it
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((rv = getaddrinfo (NULL, PORT, &hints, &ai)) != 0)
    {
      fprintf (stderr, "selectserver: %s\n", gai_strerror (rv));
      exit (1);
    }

  for (p = ai; p != NULL; p = p->ai_next)
    {
      listener = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
      if (listener < 0)
	{
	  continue;
	}

      // Lose the pesky "address already in use" error message
      setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int));

      if (bind (listener, p->ai_addr, p->ai_addrlen) < 0)
	{
	  close (listener);
	  continue;
	}

      break;
    }

  freeaddrinfo (ai);		// All done with this

  // If we got here, it means we didn't get bound
  if (p == NULL)
    {
      return -1;
    }

  // Listen
  if (listen (listener, 10) == -1)
    {
      return -1;
    }

  return listener;
}

// Add a new file descriptor to the set
void
add_to_pfds (struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
  // If we don't have room, add more space in the pfds array
  if (*fd_count == *fd_size)
    {
      *fd_size *= 2;		// Double it

      *pfds = realloc (*pfds, sizeof (**pfds) * (*fd_size));
    }

  (*pfds)[*fd_count].fd = newfd;
  (*pfds)[*fd_count].events = POLLIN;	// Check ready-to-read

  (*fd_count)++;
}

// Remove an index from the set
void
del_from_pfds (struct pollfd pfds[], int i, int *fd_count)
{
  // Copy the one from the end over this one
  pfds[i] = pfds[*fd_count - 1];

  (*fd_count)--;
}

int
parse (int argc, char **argv)
{
  int c;
  int option_index = 0;
  memset ((void *) &server_argument, 0, sizeof server_argument);
  while (1)
    {
      static struct option long_option[] = {
	{"demon", no_argument, 0, 0},
	{0, 0, 0, 0}
      };
      c = getopt_long (argc, argv, "d", long_option, &option_index);
      if (c == -1)
	{
	  break;
	}

      switch (c)
	{
	case 'd':
	  server_argument.deamon_mode = true;
	  break;
	default:
	  syslog (LOG_DEBUG, "Invalid argument\n");
	  return -1;
	  break;
	}
    }
  return 0;
}

// Main
int
main (int argc, char *argv[])
{
  int listener;			// Listening socket descriptor

  int newfd;			// Newly accept()ed socket descriptor
  struct sockaddr_storage remoteaddr;	// Client address
  socklen_t addrlen;

  char buf[4096];		// Buffer for client data

  char remoteIP[INET6_ADDRSTRLEN];

  // Start off with room for 5 connections
  // (We'll realloc as necessary)
  int fd_count = 0;
  int fd_size = 5;
  int fd_log;
  int count = 0;
  unsigned int completePacket = 0;
  struct sigaction sa;
  struct pollfd *pfds = NULL;

  if (-1 == parse (argc, argv))
    {
      fprintf (stderr, "error getting command argument\n");
      exit (-1);
    }

  // Set up and get a listening socket
  listener = get_listener_socket ();

  if (listener == -1)
    {
      fprintf (stderr, "error getting listening socket\n");
      exit (-1);
    }

  if (server_argument.deamon_mode)
    {
      pid_t pid;
      pid = fork ();
      if (pid < 0)
	{
	  close (listener);
	  fprintf (stderr, "error while trying to enter deamon mode\n");
	  exit (-1);
	}
      if (pid > 0)
	{
	  close (listener);
	  syslog (LOG_DEBUG, "starting deamon at PID=%d\n", pid);
	  printf ("starting deamon at PID=%d\n", pid);
	  exit (0);
	}

      if (setsid () < 0)
	{
	  // FAIL
	  close (listener);
	  fprintf (stderr, "error while trying to set session id\n");
	  exit (-1);
	}
      //Child process in deamon mode redirect all std io to null
      close (0);		//stdin
      close (1);		//stdout
      close (2);		//stderr
      open ("/dev/null", O_RDWR);
      dup (0);			//stdout
      dup (0);			//stderr
      // Create a SID for child
    }
  pfds = (struct pollfd *) malloc (sizeof (struct pollfd) * fd_size);
  if (NULL == pfds)
    {
      fprintf (stderr, "error allocation memory\n");
      exit (-1);
    }
  memset ((void *) pfds, 0, sizeof *pfds * fd_size);

  // Add the listener to set
  pfds[0].fd = listener;
  pfds[0].events = POLLIN;	// Report ready to read on incoming connection

  fd_count = 1;			// For the listener

  //sa.sa_handler = sigint_handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_sigaction = sigint_handler;
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction (SIGINT, &sa, NULL) == -1)
    {
      free (pfds);
      close (listener);
      perror ("sigaction");
      exit (1);
    }

  if (sigaction (SIGTERM, &sa, NULL) == -1)
    {
      free (pfds);
      close (listener);
      perror ("sigaction");
      exit (1);
    }

  openlog (NULL, LOG_PID, LOG_USER);
  fd_log =
    open (TEMP_OUTPUT_PATH,
	  O_RDWR | O_TRUNC | O_CLOEXEC | O_CREAT | O_DSYNC,
	  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd_log == -1)
    {
      free (pfds);
      close (listener);
      fprintf (stderr, "%s on %s\n", strerror (errno), TEMP_OUTPUT_PATH);
      exit (1);
    }
  // Main loop
  for (;;)
    {
      int poll_count = poll (pfds, fd_count, -1);

      if (poll_count == -1)
	{
	  perror ("poll");
	  break;
	}

      // Run through the existing connections looking for data to read
      for (int i = 0; i < fd_count; i++)
	{

	  // Check if someone's ready to read
	  if (pfds[i].revents & POLLIN)
	    {			// We got one!!

	      if (pfds[i].fd == listener)
		{
		  // If listener is ready to read, handle new connection

		  addrlen = sizeof remoteaddr;
		  newfd = accept (listener,
				  (struct sockaddr *) &remoteaddr, &addrlen);

		  if (newfd == -1)
		    {
		      perror ("accept");
		    }
		  else
		    {
		      add_to_pfds (&pfds, newfd, &fd_count, &fd_size);

		      syslog (LOG_DEBUG, "Accepted connection from %s",
			      inet_ntop (remoteaddr.ss_family,
					 get_in_addr ((struct sockaddr *)
						      &remoteaddr), remoteIP,
					 INET6_ADDRSTRLEN));
		      printf ("pollserver: new connection from %s on "
			      "socket %d\n", inet_ntop (remoteaddr.ss_family,
							get_in_addr ((struct
								      sockaddr
								      *)
								     &remoteaddr),
							remoteIP,
							INET6_ADDRSTRLEN),
			      newfd);
		    }
		}
	      else
		{
                  memset(buf, 0, sizeof buf);
		  // If not the listener, we're just a regular client
		  int nbytes = recv (pfds[i].fd, buf, sizeof buf, 0);

		  int sender_fd = pfds[i].fd;

		  if (nbytes <= 0)
		    {
		      // Got error or connection closed by client
		      if (nbytes == 0)
			{
			  // Connection closed
			  syslog (LOG_DEBUG, "Closed connection from %s",
				  inet_ntop (remoteaddr.ss_family,
					     get_in_addr ((struct sockaddr *)
							  &remoteaddr),
					     remoteIP, INET6_ADDRSTRLEN));
			  printf ("pollserver: socket %d hung up\n",
				  sender_fd);
			}
		      else
			{
			  perror ("recv");
			}

		      close (pfds[i].fd);	// Bye!

		      del_from_pfds (pfds, i, &fd_count);

		    }
		  else
		    {
                      char delimiter[]={"\n"};
		      char *token = NULL;
		      int rbyte;
		      int wbyte = write (fd_log, buf, nbytes);
		      if (wbyte == -1)
			{
			  perror ("write");
			}
		      else if (wbyte != nbytes)
			{
			  printf
			    ("pollserver: write less the requested maybe try again\n");
			}

		      token = strstr(buf, delimiter);
		      printf ("%s token\n",
			      (token == NULL) ? ("Not found") : ("Found"));
		      if (token != NULL)
			{
                          count = 0;
			  completePacket++;
			  if (lseek (fd_log, 0, SEEK_SET) == -1)
			    {
			      perror ("lseek");
			    }
			  rbyte = read (fd_log, buf, sizeof buf);
			  while (rbyte > 0 && count < completePacket)
			    {
			      if (send (pfds[i].fd, buf, rbyte, 0) == -1)
				{
				  perror ("send");
				}
			      rbyte = read (fd_log, buf, sizeof buf);
			      count++;
			    }

			  if (lseek (fd_log, 0, SEEK_END) == -1)
			    {
			      perror ("lseek");
			    }
			}
		    }
		}		// END handle data from client
	    }			// END got ready-to-read from poll()
	}			// END looping through file descriptors
    }				// END for(;;)--and you thought it would never end!

  printf ("pollserver: cleanup...\n");
  closelog ();
  for (int i = 0; i < fd_count; i++)
    {
      if (pfds[i].fd != listener)
	close (pfds[i].fd);
    }
  close (listener);
  close (fd_log);
  free (pfds);
  return 0;
}
