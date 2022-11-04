#include <unistd.h>
#include <netinet/in.h>
#include <csignal>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "../header/server_utils.h"
#include "../header/Mail.h"

namespace twMailer
{
    using std::cerr;
    using std::cout;
    using std::endl;
    namespace fs = std::filesystem;
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
    if( !send_client(current_socket, message) )
    {
        return;
    }

    ////// main loop where client communication takes place
    do
    {

        if(!handle(current_socket, buffer, message))
        {
            break;
        }

    } while( !abortRequested );

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

bool send_client(const int* socket, string &message)
{
    if(send(*socket, message.c_str(), message.length(), 0) == -1)
    {
        cerr << "send answer failed" << endl;
        return false;
    }
    return true;
}

bool handle(const int *socket, char *buffer, string &message)
{
    if( !receive_client(socket, buffer, message))
    {
        return false;
    }

    string option = message.substr(0,message.find('\n')); //option is always first line
    message = message.substr(message.find('\n')+1,message.length());

    if(option == "SEND")
    {
        if(!send_protocol(message))
        {
            ERR(socket, message);
        }
        else
        {
            OK(socket, message);
        }
        return true;
    }
    else if(option == "LIST")
    {
        if(!list_protocol(message))
        {
            ERR(socket, message);
        }
        else
        {
            return send_client(socket, message);
        }
        return true;
    }
    else if(option == "READ")
    {
        if(!read_protocol(message))
        {
            ERR(socket, message);
        }
        else
        {
            return send_client(socket,message);
        }
        return true;
    }
    else if(option == "DEL")
    {
        if(delete_protocol(message))
        {
            OK(socket,message);
        }
        else
        {
            ERR(socket,message);
        }
        return true;
    }
    else if(option == "QUIT")
    {
        return false;
    }
    else
    {
        message = "invalid command";
        return send_client(socket, message);
    }
}

void OK(const int *socket, string &message)
{
    message = "OK";
    send_client(socket, message);
}

void ERR(const int *socket, string &message)
{
    message = "ERR";
    send_client(socket, message);
}

//////////////////////////////////////////////////////////////// PROTOCOLS

bool send_protocol(string &message)
{

    string sender = message.substr(0,message.find('\n'));
    message = message.substr(message.find('\n')+1, message.length());

    if(sender.empty() || sender.length() > 8)
        return false;

    string receiver = message.substr(0,message.find('\n'));
    message = message.substr(message.find('\n')+1, message.length());

    if(receiver.empty() || receiver.length() > 8)
        return false;

    string subject = message.substr(0,message.find('\n'));
    message = message.substr(message.find('\n')+1, message.length());

    if(subject.empty() || subject.length() > 80)
        return false;


    Mail mail(sender, receiver, subject, message);

    return mail.save(spool);
}

bool list_protocol(string &message)
{
    // message is username
    if(message.empty() || !fs::exists(spool + "/" + message) || !fs::is_directory(spool + "/" + message))
    {
        // user unknown
        return false;
    }

    string output;
    int count = 0;
    for(const auto& dirEntry: fs::directory_iterator(spool + "/" + message))
    {
        count++;
        string filename = dirEntry.path().filename();
        filename = filename.substr(0, filename.length()-4);
        output += filename + '\n';
    }
    output = "messages: " + std::to_string(count) + '\n' + output;
    message = output;
    return true;
}

bool read_protocol(string &message)
{
    if(message.empty())
    {
        return false;
    }
    else
    {
        string username = message.substr(0,message.find('\n'));
        string msgNumber = message.substr(message.find('\n')+1, message.length());

        if(username.empty() || msgNumber.empty() || !fs::exists(spool + "/" + username) || !fs::is_directory(spool + "/" + username))
        {
            // user unkwn | bad input
            return false;
        }
        else
        {
            for(const auto& dirEntry: fs::directory_iterator(spool + "/" + username))
            {
                string compare = dirEntry.path().filename();
                compare = compare.substr(0, compare.find('_'));
                if(msgNumber == compare)
                {
                    std::ifstream Message((string)dirEntry.path());
                    string collect;
                    message = "OK\n";
                    while(getline(Message, collect))
                    {
                        message += collect + '\n';
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool delete_protocol(string &message)
{
    if(message.empty())
    {
        return false;
    }
    else
    {
        string username = message.substr(0,message.find('\n'));
        string msgNumber = message.substr(message.find('\n')+1, message.length());

        if(username.empty() || msgNumber.empty() || !fs::exists(spool + "/" + username) || !fs::is_directory(spool + "/" + username))
        {
            // user unkwn | bad input
            return false;
        }
        else
        {
            for(const auto& dirEntry: fs::directory_iterator(spool + "/" + username))
            {
                string compare = dirEntry.path().filename();
                compare = compare.substr(0, compare.find('_'));
                if(msgNumber == compare)
                {
                    return fs::remove(dirEntry.path());
                }
            }
        }
    }
    return false;}
