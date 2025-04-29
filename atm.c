#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_PATH "/tmp/bank_fifo"
#define MAX_MSG_SIZE 256

int main() {
    int fd;
    char buffer[MAX_MSG_SIZE];
    int choice, account_index, amount, to_account;

    
    printf("[*] ATM started. Connecting to Bank...\n");

    fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open FIFO");
        exit(1);
    }

    while (1) {
        printf("\n=== ATM Menu ===\n");
        printf("1. Open New Account\n");
        printf("2. Deposit\n");
        printf("3. Withdraw\n");
        printf("4. View Balance\n");
        printf("5. Transfer\n");
        printf("6. Exit\n");
        printf("Select option: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                snprintf(buffer, sizeof(buffer), "new_account");
                break;

            case 2:
                printf("Enter account index: ");
                scanf("%d", &account_index);
                printf("Enter deposit amount: ");
                scanf("%d", &amount);
                if (amount < 0) {
                    printf("[!] Amount cannot be negative.\n");
                    continue;
                }
                snprintf(buffer, sizeof(buffer), "%d deposit %d", account_index, amount);
                break;

            case 3:
                printf("Enter account index: ");
                scanf("%d", &account_index);
                printf("Enter withdraw amount: ");
                scanf("%d", &amount);
                if (amount < 0) {
                    printf("[!] Amount cannot be negative.\n");
                    continue;
                }
                snprintf(buffer, sizeof(buffer), "%d withdraw %d", account_index, amount);
                break;

            case 4:
                printf("Enter account index: ");
                scanf("%d", &account_index);
                snprintf(buffer, sizeof(buffer), "%d view", account_index);
                break;

            case 5:
                printf("Enter source account index: ");
                scanf("%d", &account_index);
                printf("Enter target account index: ");
                scanf("%d", &to_account);
                if (to_account == account_index) {
                    printf("[!] Cannot transfer to same account.\n");
                    continue;
                }
                printf("Enter transfer amount: ");
                scanf("%d", &amount);
                if (amount < 0) {
                    printf("[!] Amount cannot be negative.\n");
                    continue;
                }
                snprintf(buffer, sizeof(buffer), "%d transfer %d %d", account_index, amount, to_account);
                break;

            case 6:
                printf("[*] Exiting ATM...\n");
                close(fd);
                return 0;

            default:
                printf("[!] Invalid option.\n");
                continue;
        }

        // Write message to FIFO
        if (write(fd, buffer, strlen(buffer) + 1) == -1) {
            perror("write");
        } else {
            printf("[*] Request sent to bank: %s\n", buffer);
        }
    }

    close(fd);
    return 0;
}
