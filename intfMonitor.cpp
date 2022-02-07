
#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <signal.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

using namespace std;

char socket_path[] = "/tmp/as1";
bool is_running, is_monitoring;
const int BUF_LEN = 4000;

const int MAXBUF = 64;

//signal handler
static void signalHandler(int signum)
{
    switch (signum)
    {
    case SIGINT:
        cout << "signalHandler(" << signum << "): SIGINT" << endl;
        is_running = false;
        break;
    default:
        cout << "signalHandler(" << signum << "): unknown" << endl;
    }
}

//set up function used to turn on interface, will use it below
int skfd = -1; /* AF_INET socket for ioctl() calls.*/
int set_if_flags(char *ifname, short flags)
{
    struct ifreq ifr;
    int res = 0;

    ifr.ifr_flags = flags;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("socket error %s\n", strerror(errno));
        res = 1;
    }

    res = ioctl(skfd, SIOCSIFFLAGS, &ifr);
    if (res < 0)
    {
        printf("Interface '%s': Error: SIOCSIFFLAGS failed: %s\n", ifname, strerror(errno));
    }
    else
    {
        printf("Interface '%s': flags set to %04X.\n", ifname, flags);
    }
    return res;
}
int set_if_up(char *ifname, short flags)
{
    return set_if_flags(ifname, flags | IFF_UP);
}

int main(int argc, char *argv[])
{
    char interface[MAXBUF];
    char statPath[2 * MAXBUF];
    int retVal = 0;

    strncpy(interface, argv[1], MAXBUF);

    //Set up socket communications
    struct sockaddr_un addr;
    char buf[BUF_LEN];
    int len, ret;
    int fd, rc;

    //Set up a signal handler to terminate the program gracefully
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    cout << "intfMonitor:main: interface:" << interface << ":  pid:" << getpid() << endl;

    memset(&addr, 0, sizeof(addr));
    //Create the socket
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        cout << "client(" << getpid() << "): " << strerror(errno) << endl;
        exit(-1);
    }

    addr.sun_family = AF_UNIX;
    //Set the socket path to a local socket file
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    //Connect to the local socket
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cout << "client(" << getpid() << "): " << strerror(errno) << endl;
        close(fd);
        exit(-1);
    }

    cout << "client(" << getpid() << "): ready to start monitoring" << endl;
    len = sprintf(buf, "Ready") + 1;
    ret = write(fd, buf, len); //send "Ready" message to server
    if (ret == -1)
    {
        cout << "client(" << getpid() << "): Write Error" << endl;
        cout << strerror(errno) << endl;
    }

    is_running = true;
    while (is_running)
    {
        //gather stats info
        char operstate[] = "unknown";
        int carrier_up_count = 0;
        int carrier_down_count = 0;
        int tx_bytes = 0;
        int rx_bytes = 0;
        int tx_packets = 0;
        int rx_packets = 0;
        int tx_dropped = 0;
        int rx_dropped = 0;
        int tx_errors = 0;
        int rx_errors = 0;

        ret = read(fd, buf, BUF_LEN); //Read something from the server
        if (ret < 0)
        {
            cout << "client(" << getpid() << "): error reading the socket" << endl;
            cout << strerror(errno) << endl;
        }

        if (strcmp(buf, "Monitor") == 0)
        { //Server requests the monitoring of the client

            len = sprintf(buf, "Monitoring") + 1;
            ret = write(fd, buf, len); //send "Monitoring" message to server
            if (ret == -1)
            {
                cout << "client(" << getpid() << "): Write Error" << endl;
                cout << strerror(errno) << endl;
            }
            is_monitoring = true;
        }
        else if (strcmp(buf, "Set Link Up") == 0)
        {                            //Server requests the client to set link up
            set_if_up(interface, 1); //set it back up using ioctl, reference is in the beginning function
            carrier_up_count++;
        }
        else if (strcmp(buf, "Shut Down") == 0)
        { //Server requests the client to terminate
            cout << "client(" << getpid() << "): received request to quit" << endl;
            is_running = false;
            is_monitoring = false;
            len = sprintf(buf, "Done") + 1;
            ret = write(fd, buf, len); //send "Done" after terminate
            if (ret == -1)
            {
                cout << "client(" << getpid() << "): Write Error" << endl;
                cout << strerror(errno) << endl;
            }
        }

        while (is_monitoring == true)
        {
            ifstream infile;
            sprintf(statPath, "/sys/class/net/%s/statistics/operstate", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> operstate;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/carrier_up_count", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> carrier_up_count;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/carrier_down_count", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> carrier_down_count;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/tx_bytes", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> tx_bytes;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/rx_bytes", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> rx_bytes;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/tx_packets", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> tx_packets;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/rx_packets", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> rx_packets;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/tx_dropped", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> tx_dropped;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/rx_dropped", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> rx_dropped;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/tx_errors", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> tx_errors;
                infile.close();
            }

            sprintf(statPath, "/sys/class/net/%s/statistics/rx_errors", interface);
            infile.open(statPath);
            if (infile.is_open())
            {
                infile >> rx_errors;
                infile.close();
            }

            //if state is down
            if (operstate == "down")
            {
                carrier_down_count++;
                len = sprintf(buf, "Link Down") + 1; //send "Link Down" instruction
                ret = write(fd, buf, len);
                if (ret == -1)
                {
                    cout << "client: Write Error" << endl;
                    cout << strerror(errno) << endl;
                }
            }

            //print out network state
            cout << "Interface: " << interface << " state: " << operstate << " up_count: " << carrier_up_count << " down_count: " << carrier_down_count << endl
                 << " rx_bytes: " << rx_bytes << " rx_dropped: " << rx_dropped << " rx_errors: " << rx_errors << " rx_packets: " << rx_packets << endl
                 << " tx_bytes: " << tx_bytes << " tx_dropped: " << tx_dropped << " tx_errors: " << tx_errors << " tx_packets: " << tx_packets << endl
                 << endl;

            //write the stats into network Monitor
            len = sprintf(buf, "Interface:%s state:%s up_count:%d down_count:%d \nrx_bytes:%d rx_dropped:%d rx_errors:%d rx_packets:%d \ntx_bytes:%d tx_dropped:%d tx_errors:%d tx_packets:%d\n\n", interface, operstate, carrier_up_count, carrier_down_count, rx_bytes, rx_dropped, rx_errors, rx_packets, tx_bytes, tx_dropped, tx_errors, tx_packets) + 1;
            ret = write(fd, buf, len); //The client writes the info to the server
            if (ret == -1)
            {
                cout << "client(" << getpid() << "): Write Error" << endl;
                cout << strerror(errno) << endl;
            }
            sleep(1); //delay 1 sec
        }
    }
    cout << "client(" << getpid() << "): stopping..." << endl;
    close(fd);
    return 0;
}

