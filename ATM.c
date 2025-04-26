#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>

#define QUEUE_NAME "/bank_queue"

void print_menu() {
    printf("\nATM Menu:\n");
    printf("1. Deposit\n");
    printf("2. Withdraw\n");
    printf("3. View Account\n");
    printf("4. Exit\n");
}

int main() {
    mqd_t mq;
    char transaction[256];
    int account_index, choice, amount;

    // Open the message queue for writing
    mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == -1) {
        perror("mq_open");
        exit(1);
    }

    while (1) {
        print_menu();
        printf("Enter your choice: ");
        scanf("%d", &choice);

        if (choice == 4) {
            break;
        }

        printf("Enter account index (0, 1, etc.): ");
        scanf("%d", &account_index);

        if (choice == 1) {
            printf("Enter deposit amount: ");
            scanf("%d", &amount);
            sprintf(transaction, "%d deposit %d", account_index, amount);
        } else if (choice == 2) {
            printf("Enter withdrawal amount: ");
            scanf("%d", &amount);
            sprintf(transaction, "%d withdraw %d", account_index, amount);
        } else if (choice == 3) {
            sprintf(transaction, "%d view", account_index);
        } else {
            printf("Invalid choice!\n");
            continue;
        }

        if (mq_send(mq, transaction, strlen(transaction), 0) == -1) {
            perror("mq_send");
        } else {
            printf("Transaction sent: %s\n", transaction);
        }
    }

    // Cleanup
    mq_close(mq);
    return 0;
}
