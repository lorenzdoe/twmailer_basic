#include <unistd.h>
#include <netinet/in.h>
#include <csignal>
#include <iostream>

#include "../header/server_utils.h"
#include "../header/Mail.h"

namespace twMailer
{
    using std::cerr;
    using std::cout;
    using std::endl;
}

using namespace twMailer;

// handles signals and closes connection controlled
void signalHandler(int sig)
{
    if( sig == SIGINT )
    {
        cout << "abort Requested...";
        abortRequested = true;

        // With shutdown() one can initiate normal TCP close sequence ignoring
        // the reference count.
        if(new_socket != -1)
        {
            if(shutdown(new_socket, SHUT_RDWR) == -1)
            {
                cerr << "shutdown new_socket" << endl;
            }
            if(close(new_socket) == -1)
            {
                cerr << "close new_socket" << endl;
            }
            new_socket = -1;
        }

        if(create_socket != -1)
        {
            if(shutdown(create_socket, SHUT_RDWR) == -1)
            {
                cerr << "shutdown create_socket" << endl;
            }
            if(close(create_socket) == -1)
            {
                cerr << "close create_socket" << endl;
            }
            create_socket = -1;
        }
    }
    else
    {
        exit(sig);
    }
}

void clientCommunication(void* data)
{
    char buffer[BUF];
    string message;
    int *current_socket = (int*) data;

    ////////////////////////////////////////////////////////////////////////////
    // SEND welcome message
    message = "Welcome to myserver!\r\nPlease enter your commands...\r\n";
    if(! send_client(current_socket, message))
    {
        return;
    }

    do
    {
        if( !handle(current_socket, buffer, message))
        {
            break;
        }


    } while(message != "QUIT" && !abortRequested);

    // closes/frees the descriptor if not already
    if (*current_socket != -1)
    {
        if (shutdown(*current_socket, SHUT_RDWR) == -1)
        {
            perror("shutdown new_socket");
        }
        if (close(*current_socket) == -1)
        {
            perror("close new_socket");
        }
        *current_socket = -1;
    }

}

bool receive_client(const int *socket, char *buffer, string &message)
{
    int size;
    /////////////////////////////////////////////////////////////////////////
    // RECEIVE
    size = recv(*socket, buffer, BUF - 1 , 0);
    if( size == -1 /* just to be sure */)
    {
        if(abortRequested)
        {
            cerr << "recv error after aborted " << endl;
        }
        else
        {
            cerr << "recv error" << endl;
        }
        return false;
    }

    if( size == 0 )
    {
        cout << "Client closed remote socket" << endl;
        return false;
    }

    // remove ugly debug message, because of the sent newline of client
    if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
    {
        size -= 2;
    }
    else if (buffer[size - 1] == '\n')
    {
        --size;
    }
    buffer[size] = '\0';

    message = buffer;   // handles buffer in string message

    return true;
}

void print_usage(char* program_name)
{
    cout << "Usage: " << program_name << " <port> <Mail-spool-directoryname>" << endl;
}

bool send_client(const int* socket, string &buffer)
{
    if(send(*socket, buffer.c_str(), buffer.length(), 0) == -1)
    {
        cerr << "send answer failed" << endl;
        return false;
    }
    return true;
}

bool handle(int *socket, char *buffer, string &message)
{
    if( !receive_client(socket, buffer, message))
    {
        return false;
    }

    cout << "Handle message: " << message << endl;

    if(message == "SEND")
    {
        if(!send_protocol(socket, buffer, message))
        {
            ERR(socket, message);
        }
        else
        {
            OK(socket, message);
        }
        return true;
    }
    else if(message == "LIST")
    {
        return list_protocol(socket, buffer, message);
    }
    else if(message == "READ")
    {
        return read_protocol(socket, buffer, message);
    }
    else if(message == "DELETE")
    {
        return delete_protocol(socket, buffer, message);
    }
    else if(message == "QUIT")
    {
        return false;
    }
    else
    {
        message = "invalid command";
        return send_client(socket, message);
    }
}

void OK(int *socket, string &message)
{
    message = "OK";
    send_client(socket, message);
}

void ERR(int *socket, string &message)
{
    message = "ERR";
    send_client(socket, message);
}

//////////////////////////////////////////////////////////////// PROTOCOLS

bool send_protocol(int *socket, char *buffer, string &message)
{
    // get Sender
    string sender;
    if(receive_client(socket,buffer,message))
    {
        sender = message;
        if(sender.empty() || sender.length() > 8)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    cout << "sender: "<< sender << endl;

    // get Receiver
    string receiver;
    if(receive_client(socket,buffer,message))
    {
        receiver = message;
        if(receiver.empty() || receiver.length() > 8)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    cout << "receiver: " << receiver << endl;

    // get Subject
    string subject;
    if(receive_client(socket,buffer,message))
    {
        subject = message;
        if(subject.empty() || subject.length() > 80)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    cout << "subject: " << subject << endl;

    // get Message
    message = " ";      //to make sure message is != "."
    string mail_message;
    while(message != ".")
    {
        if(receive_client(socket,buffer,message))
        {
            mail_message += message + "\n";
        }
        else
        {
            return false;
        }

        cout << "message: " << message << endl;
    }

    Mail mail(sender, receiver, subject, mail_message);
    mail.save(spool);

    //TODO: check if save worked

    return true;
}

bool list_protocol(int *socket, char *buffer, string &message)
{
    return true;
}

bool read_protocol(int *socket, char *buffer, string &message)
{
    return true;
}

bool delete_protocol(int *socket, char *buffer, string &message)
{
    return true;
}
