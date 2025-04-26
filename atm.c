#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define QUEUE_NAME "/bank_queue"
#define MAX_MSG_SIZE 256

int main() {
    mqd_t mq;
    char buffer[MAX_MSG_SIZE];
    int choice, account_index, amount;

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // atm doesn't unlink, just connects
    mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == -1) {
        perror("mq_open (atm)");
        exit(EXIT_FAILURE);
    }

    printf("[*] ATM Connected to Bank.\n");

    while (1) {
        printf("\n=== ATM Menu ===\n");
        printf("1. Deposit\n");
        printf("2. Withdraw\n");
        printf("3. View Balance\n");
        printf("4. Exit\n");
        printf("Select option: ");
        scanf("%d", &choice);

        if (choice == 4) {
            strcpy(buffer, "exit");
            mq_send(mq, buffer, strlen(buffer) + 1, 0);
            printf("[*] ATM exiting...\n");
            break;
        }

        printf("Enter account index (0-4): ");
        scanf("%d", &account_index);

        if (account_index < 0 || account_index > 4) {
            printf("[!] Invalid account index.\n");
            continue;
        }

        if (choice == 1) {
            printf("Enter deposit amount: ");
            scanf("%d", &amount);
            snprintf(buffer, sizeof(buffer), "%d deposit %d", account_index, amount);
        } else if (choice == 2) {
            printf("Enter withdraw amount: ");
            scanf("%d", &amount);
            snprintf(buffer, sizeof(buffer), "%d withdraw %d", account_index, amount);
        } else if (choice == 3) {
            snprintf(buffer, sizeof(buffer), "%d view 0", account_index);
        } else {
            printf("[!] Invalid option.\n");
            continue;
        }

        if (mq_send(mq, buffer, strlen(buffer) + 1, 0) == -1) {
            perror("mq_send (atm)");
        }
    }

    mq_close(mq);

    return 0;
}
