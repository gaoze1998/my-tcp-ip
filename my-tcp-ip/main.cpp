#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> // ETH_P_ALL
#include <net/if.h>       // IFNAMSIZ
#include <sys/ioctl.h>
#include <arpa/inet.h>

// ��ȡ�ӿڵ�������MAC��ַ
int get_interface_info(const char* ifname, int* ifindex, unsigned char* mac) {
    int fd;
    struct ifreq ifr;
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl SIOCGIFINDEX");
        close(fd);
        return -1;
    }
    *ifindex = ifr.ifr_ifindex;

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl SIOCGIFHWADDR");
        close(fd);
        return -1;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <interface> <dest_mac> <payload>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* ifname = argv[1];
    const char* dest_mac_str = argv[2];
    const char* payload = argv[3];
    unsigned char dest_mac[6];
    unsigned char src_mac[6];
    int ifindex;

    // ����Ŀ��MAC��ַ
    if (sscanf(dest_mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &dest_mac[0], &dest_mac[1], &dest_mac[2],
        &dest_mac[3], &dest_mac[4], &dest_mac[5]) != 6) {
        fprintf(stderr, "Invalid MAC address format\n");
        exit(EXIT_FAILURE);
    }

    // ��ȡ�ӿ���Ϣ
    if (get_interface_info(ifname, &ifindex, src_mac) != 0) {
        fprintf(stderr, "Failed to get interface info\n");
        exit(EXIT_FAILURE);
    }

    // ����ԭʼ�׽���
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // ������̫��֡
    unsigned char frame[1514];
    memset(frame, 0, sizeof(frame));

    // Ŀ��MAC��ַ
    memcpy(frame, dest_mac, 6);
    // ԴMAC��ַ
    memcpy(frame + 6, src_mac, 6);
    // ��̫���ͣ�ʾ��ʹ��0x0800��ʾIP��
    frame[12] = 0x08;
    frame[13] = 0x00;

    // �غ�
    size_t payload_len = strlen(payload);
    if (payload_len > (1514 - 14)) {
        fprintf(stderr, "Payload too large\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    memcpy(frame + 14, payload, payload_len);

    // ׼��Ŀ���ַ�ṹ
    struct sockaddr_ll socket_address;
    memset(&socket_address, 0, sizeof(struct sockaddr_ll));
    socket_address.sll_ifindex = ifindex;
    socket_address.sll_halen = ETH_ALEN;
    memcpy(socket_address.sll_addr, dest_mac, 6);

    // ����֡
    ssize_t sent = sendto(sockfd, frame, 14 + payload_len, 0,
        (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll));
    if (sent == -1) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Sent %zd bytes\n", sent);

    close(sockfd);
    return 0;
}