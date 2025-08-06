#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define DHCP_SERVER_PORT 67
#define CIDR_NOTATION "192.17.0.1/32"
#define LEASE_TIME 20 // 5 seconds for testing purposes
#define DNS_SERVER "8.8.8.8"

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

typedef struct
{
    struct in_addr ip;
    time_t lease_start;
    time_t lease_expiration;
    uint8_t chaddr[16];
} IPLease;

IPLease ip_leases[256];
int lease_count = 0;

struct in_addr network_address;
struct in_addr subnet_mask;
struct in_addr broadcast_address;
struct in_addr default_gateway;
struct in_addr ip_range_start;
struct in_addr ip_range_end;

// Agregar un mutex global para proteger el acceso a recursos compartidos
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_network()
{
    char ip_str[16];
    int prefix_len;
    sscanf(CIDR_NOTATION, "%[^/]/%d", ip_str, &prefix_len);
    if (prefix_len > 32)
    {
        fprintf(stderr, "Error: CIDR prefix length cannot be greater than 32.\n");
        exit(1);
    }

    inet_pton(AF_INET, ip_str, &network_address);

    uint32_t mask = 0xffffffff << (32 - prefix_len);
    subnet_mask.s_addr = htonl(mask);

    broadcast_address.s_addr = network_address.s_addr | ~subnet_mask.s_addr;

    // Calculate default gateway (first usable IP in the network)
    default_gateway.s_addr = htonl(ntohl(network_address.s_addr) + 1);

    // Calculate IP range (10 IPs for testing)
    ip_range_start.s_addr = htonl(ntohl(network_address.s_addr) + 2);
    ip_range_end.s_addr = htonl(ntohl(ip_range_start.s_addr) + 9);

    printf("Network: %s\n", inet_ntoa(network_address));
    printf("Subnet Mask: %s\n", inet_ntoa(subnet_mask));
    printf("Broadcast: %s\n", inet_ntoa(broadcast_address));
    printf("Default Gateway: %s\n", inet_ntoa(default_gateway));
    printf("IP Range Start: %s\n", inet_ntoa(ip_range_start));
    printf("IP Range End: %s\n", inet_ntoa(ip_range_end));
}

int is_ip_in_range(struct in_addr ip)
{
    return (ntohl(ip.s_addr) >= ntohl(ip_range_start.s_addr) && ntohl(ip.s_addr) <= ntohl(ip_range_end.s_addr));
}

struct in_addr get_available_ip()
{
    struct in_addr ip = ip_range_start;
    for (uint32_t i = 0; i <= ntohl(ip_range_end.s_addr) - ntohl(ip_range_start.s_addr); i++)
    {
        ip.s_addr = htonl(ntohl(ip_range_start.s_addr) + i);
        int available = 1;
        for (int j = 0; j < lease_count; j++)
        {
            if (ip_leases[j].ip.s_addr == ip.s_addr)
            {
                available = 0;
                break;
            }
        }
        if (available)
            return ip;
    }
    ip.s_addr = INADDR_NONE;
    return ip;
}

void handle_dhcp_discover(int sockfd, DHCPMessage *msg, struct sockaddr_in *client_addr)
{
    struct in_addr available_ip = get_available_ip();
    if (available_ip.s_addr == INADDR_NONE)
    {
        printf("No available IP addresses\n");
        return;
    }

    DHCPMessage offer_msg;
    memset(&offer_msg, 0, sizeof(offer_msg));
    offer_msg.op = 2; // BOOTREPLY
    offer_msg.htype = msg->htype;
    offer_msg.hlen = msg->hlen;
    offer_msg.xid = msg->xid;
    memcpy(offer_msg.chaddr, msg->chaddr, 16);
    offer_msg.yiaddr = available_ip.s_addr;
    offer_msg.flags = htons(0x8000); // Broadcast flag

    // Set DHCP options
    uint8_t *options = offer_msg.options;
    options[0] = 0x63; // Magic cookie
    options[1] = 0x82;
    options[2] = 0x53;
    options[3] = 0x63;

    options[4] = 53; // DHCP Message Type
    options[5] = 1;  // Length
    options[6] = 2;  // DHCPOFFER

    options[7] = 51; // IP Address Lease Time
    options[8] = 4;  // Length
    uint32_t lease_time = htonl(LEASE_TIME);
    memcpy(&options[9], &lease_time, 4);

    options[13] = 1; // Subnet Mask
    options[14] = 4; // Length
    memcpy(&options[15], &subnet_mask, 4);

    options[19] = 6; // DNS Server
    options[20] = 4; // Length
    struct in_addr dns_server;
    inet_aton(DNS_SERVER, &dns_server);
    memcpy(&options[21], &dns_server, 4);

    options[25] = 3; // Router (Default Gateway)
    options[26] = 4; // Length
    memcpy(&options[27], &default_gateway, 4);

    options[31] = 255; // End option

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = client_addr->sin_port;
    dest_addr.sin_addr = client_addr->sin_addr;

    ssize_t sent_len = sendto(sockfd, &offer_msg, sizeof(offer_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (sent_len < 0)
    {
        perror("Error sending DHCP OFFER");
    }
    else
    {
        printf("Sent DHCP OFFER to %s\n", inet_ntoa(dest_addr.sin_addr));
    }
}

void handle_dhcp_request(int sockfd, DHCPMessage *msg, struct sockaddr_in *client_addr)
{
    struct in_addr requested_ip;
    requested_ip.s_addr = msg->yiaddr;

    if (!is_ip_in_range(requested_ip))
    {
        printf("Requested IP out of range %s\n", inet_ntoa(requested_ip));
        return;
    }

    for (int i = 0; i < lease_count; i++)
    {
        if (ip_leases[i].ip.s_addr == requested_ip.s_addr)
        {
            printf("IP already leased\n");
            return;
        }
    }

    ip_leases[lease_count].ip = requested_ip;
    ip_leases[lease_count].lease_start = time(NULL);
    ip_leases[lease_count].lease_expiration = time(NULL) + LEASE_TIME;
    memcpy(ip_leases[lease_count].chaddr, msg->chaddr, 16);
    lease_count++;

    DHCPMessage ack_msg;
    memset(&ack_msg, 0, sizeof(ack_msg));
    ack_msg.op = 2; // BOOTREPLY
    ack_msg.htype = msg->htype;
    ack_msg.hlen = msg->hlen;
    ack_msg.xid = msg->xid;
    memcpy(ack_msg.chaddr, msg->chaddr, 16);
    ack_msg.yiaddr = requested_ip.s_addr;

    // Set DHCP options
    uint8_t *options = ack_msg.options;
    bzero(options, sizeof(ack_msg.options));
    options[0] = 0x63; // Magic cookie
    options[1] = 0x82;
    options[2] = 0x53;
    options[3] = 0x63;

    options[4] = 53; // DHCP Message Type
    options[5] = 1;  // Length
    options[6] = 5;  // DHCPACK

    options[7] = 51; // IP Address Lease Time
    options[8] = 4;  // Length
    uint32_t lease_time = htonl(LEASE_TIME);
    memcpy(&options[9], &lease_time, 4);

    options[13] = 1; // Subnet Mask
    options[14] = 4; // Length
    memcpy(&options[15], &subnet_mask, 4);

    options[19] = 6; // DNS Server
    options[20] = 4; // Length
    struct in_addr dns_server;
    inet_aton(DNS_SERVER, &dns_server);
    memcpy(&options[21], &dns_server, 4);

    options[25] = 3; // Router (Default Gateway)
    options[26] = 4; // Length
    memcpy(&options[27], &default_gateway, 4);

    options[31] = 255; // End option

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = client_addr->sin_port;
    dest_addr.sin_addr = client_addr->sin_addr;

    sendto(sockfd, &ack_msg, sizeof(ack_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    printf("Sent DHCP ACK to %s\n", inet_ntoa(dest_addr.sin_addr));
}

void handle_dhcp_release(DHCPMessage *msg)
{
    struct in_addr released_ip;
    released_ip.s_addr = msg->yiaddr;

    printf("Releasing IP: %s\n", inet_ntoa(released_ip));

    for (int i = 0; i < lease_count; i++)
    {
        if (ip_leases[i].ip.s_addr == released_ip.s_addr && memcmp(ip_leases[i].chaddr, msg->chaddr, 16) == 0)
        {
            printf("Releasing IP: %s\n", inet_ntoa(released_ip));
            // Shift remaining leases down
            for (int j = i; j < lease_count - 1; j++)
            {
                ip_leases[j] = ip_leases[j + 1];
            }
            lease_count--;
            pthread_mutex_unlock(&mutex);
            return;
        }
    }
    pthread_mutex_unlock(&mutex);
    printf("IP not found for release: %s\n", inet_ntoa(released_ip));
}

void handle_dhcp_renew(int sockfd, DHCPMessage *msg, struct sockaddr_in *client_addr)
{
    struct in_addr client_ip;
    client_ip.s_addr = msg->ciaddr; // Cambiado de msg->yiaddr a msg->ciaddr

    for (int i = 0; i < lease_count; i++)
    {
        if (ip_leases[i].ip.s_addr == client_ip.s_addr && memcmp(ip_leases[i].chaddr, msg->chaddr, 16) == 0)
        {
            // Renew the lease
            ip_leases[i].lease_expiration = time(NULL) + LEASE_TIME;

            // Send DHCPACK
            DHCPMessage ack_msg;
            memset(&ack_msg, 0, sizeof(ack_msg));
            ack_msg.op = 2; // BOOTREPLY
            ack_msg.htype = msg->htype;
            ack_msg.hlen = msg->hlen;
            ack_msg.xid = msg->xid;
            memcpy(ack_msg.chaddr, msg->chaddr, 16);
            ack_msg.yiaddr = client_ip.s_addr;

            // Set DHCP options
            uint8_t *options = ack_msg.options;
            options[0] = 0x63; // Magic cookie
            options[1] = 0x82;
            options[2] = 0x53;
            options[3] = 0x63;

            options[4] = 53; // DHCP Message Type
            options[5] = 1;  // Length
            options[6] = 5;  // DHCPACK

            options[7] = 51; // IP Address Lease Time
            options[8] = 4;  // Length
            uint32_t lease_time = htonl(LEASE_TIME);
            memcpy(&options[9], &lease_time, 4);

            options[13] = 1; // Subnet Mask
            options[14] = 4; // Length
            memcpy(&options[15], &subnet_mask, 4);

            options[19] = 6; // DNS Server
            options[20] = 4; // Length
            struct in_addr dns_server;
            inet_aton(DNS_SERVER, &dns_server);
            memcpy(&options[21], &dns_server, 4);

            options[25] = 3; // Router (Default Gateway)
            options[26] = 4; // Length
            memcpy(&options[27], &default_gateway, 4);

            options[31] = 255; // End option

            sendto(sockfd, &ack_msg, sizeof(ack_msg), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
            printf("Renewed lease for IP: %s\n", inet_ntoa(client_ip));
            return;
        }
    }
    printf("Renewal failed for IP: %s\n", inet_ntoa(client_ip));
}

void print_active_leases()
{
    pthread_mutex_lock(&mutex);
    printf("\n--- Active IP Leases ---\n");
    for (int i = 0; i < lease_count; i++)
    {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 ip_leases[i].chaddr[0], ip_leases[i].chaddr[1], ip_leases[i].chaddr[2],
                 ip_leases[i].chaddr[3], ip_leases[i].chaddr[4], ip_leases[i].chaddr[5]);

        time_t remaining = ip_leases[i].lease_expiration - time(NULL);

        printf("IP: %s, Expires in: %ld seconds\n",
               inet_ntoa(ip_leases[i].ip), remaining);
    }
    printf("------------------------\n\n");
    pthread_mutex_unlock(&mutex);
}

void *handle_client(void *arg)
{
    int sockfd = *(int *)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    DHCPMessage *dhcp_msg;

    while (1)
    {
        // Receive DHCP message
        print_active_leases();
        ssize_t recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (recv_len < 0)
        {
            perror("Error receiving data");
            continue;
        }

        dhcp_msg = (DHCPMessage *)buffer;

        // Process DHCP message
        pthread_mutex_lock(&mutex);
        switch (dhcp_msg->options[6])
        {
        case 1: // DHCP DISCOVER
            handle_dhcp_discover(sockfd, dhcp_msg, &client_addr);
            break;
        case 7: // DHCP RELEASE
            handle_dhcp_release(dhcp_msg);
            break;
        case 3: // DHCP REQUEST (could be new request or renewal)
            if (dhcp_msg->ciaddr != 0)
            {
                handle_dhcp_renew(sockfd, dhcp_msg, &client_addr);
            }
            else
            {
                handle_dhcp_request(sockfd, dhcp_msg, &client_addr);
            }
            break;
        default:
            printf("Unknown DHCP message type\n");
            break;
        }
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

void *lease_manager(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex);
        time_t current_time = time(NULL);

        for (int i = 0; i < lease_count; i++)
        {
            if (current_time > ip_leases[i].lease_expiration)
            {
                printf("Lease expired for IP: %s\n", inet_ntoa(ip_leases[i].ip));

                // Remove the expired lease
                for (int j = i; j < lease_count - 1; j++)
                {
                    ip_leases[j] = ip_leases[j + 1];
                }
                lease_count--;
                i--; // Adjust index after removal
            }
        }

        pthread_mutex_unlock(&mutex);
        sleep(1); // Check every second
    }
    return NULL;
}

int main()
{
    int sockfd;
    struct sockaddr_in server_addr;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);

    // Bind socket to address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        close(sockfd);
        exit(1);
    }

    initialize_network();

    printf("DHCP server is running...\n");

    pthread_t lease_manager_tid, lease_display_tid;
    if (pthread_create(&lease_manager_tid, NULL, lease_manager, NULL) != 0)
    {
        perror("Failed to create lease manager thread");
        exit(1);
    }

    // Create threads to handle clients
    pthread_t tid;
    for (int i = 0; i < 3; i++)
    { // Create 5 threads to handle clients
        if (pthread_create(&tid, NULL, handle_client, (void *)&sockfd) != 0)
        {
            perror("Failed to create thread");
            exit(1);
        }
    }

    // Wait for threads to finish (which they never will in this case)
    pthread_exit(NULL);

    close(sockfd);
    return 0;
}
