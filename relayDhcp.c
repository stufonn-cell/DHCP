#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define DHCP_SERVER_PORT 69
#define DHCP_RELAY_PORT 67

typedef struct
{
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint8_t options[312];
} DHCPMessage;

void relay_dhcp_message(int from_socket, int to_socket, struct sockaddr_in *from_addr, struct sockaddr_in *to_addr)
{
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(struct sockaddr_in);

    ssize_t recv_len = recvfrom(from_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)from_addr, &addr_len);
    if (recv_len < 0)
    {
        perror("Error receiving data");
        return;
    }

    DHCPMessage *dhcp_msg = (DHCPMessage *)buffer;

    // Increment the hops field
    dhcp_msg->hops++;

    // If this is a request from a client, set the giaddr field
    if (dhcp_msg->op == 1 && dhcp_msg->giaddr == 0)
    {
        struct in_addr relay_ip;
        inet_pton(AF_INET, "192.168.182.129", &relay_ip); // Replace with your relay's IP
        dhcp_msg->giaddr = relay_ip.s_addr;
    }

    // Print information about the received message
    printf("Received DHCP message from %s:%d\n",
           inet_ntoa(from_addr->sin_addr), ntohs(from_addr->sin_port));

    // Forward the message
    ssize_t sent_len = sendto(to_socket, buffer, recv_len, 0, (struct sockaddr *)to_addr, sizeof(*to_addr));
    if (sent_len < 0)
    {
        perror("Error sending data");
        return;
    }

    printf("Relayed DHCP message to %s:%d\n",
           inet_ntoa(to_addr->sin_addr), ntohs(to_addr->sin_port));
}

int main(int argc, char *argv[])
{
    int client_socket, server_socket;
    struct sockaddr_in relay_addr, server_addr, client_addr;

    // Create sockets
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0 || server_socket < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    // Configure relay address
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_addr.s_addr = INADDR_ANY;
    relay_addr.sin_port = htons(DHCP_RELAY_PORT);

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // Bind client socket
    if (bind(client_socket, (struct sockaddr *)&relay_addr, sizeof(relay_addr)) < 0)
    {
        perror("Error binding client socket");
        exit(1);
    }

    printf("DHCP Relay is running...\n");

    fd_set readfds;
    int max_fd = (client_socket > server_socket) ? client_socket : server_socket;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(client_socket, &readfds);
        FD_SET(server_socket, &readfds);

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
        {
            perror("select error");
            exit(1);
        }

        if (FD_ISSET(client_socket, &readfds))
        {
            relay_dhcp_message(client_socket, server_socket, &client_addr, &server_addr);
        }

        if (FD_ISSET(server_socket, &readfds))
        {
            relay_dhcp_message(server_socket, client_socket, &server_addr, &client_addr);
        }
    }

    close(client_socket);
    close(server_socket);
    return 0;
}
