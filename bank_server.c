#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_PATH "/tmp/bank_fifo"
#define MAX_MSG_SIZE 256

typedef struct {
    int balance;
    int pin;  // Added pin for account
    int is_deleted;  // Flag to check if the account is deleted
} Account;

Account accounts[5];  // Supporting 5 accounts
sem_t mutex;  // Semaphore to ensure mutual exclusion during account access

void initialize_accounts() {
    for (int i = 0; i < 5; i++) {
        accounts[i].balance = 1000;
        accounts[i].pin = 1234;  // Default PIN for simplicity
        accounts[i].is_deleted = 0;  // Initially, accounts are not deleted
    }
}

void handle_request(char* request) {
    int account_index, amount, pin, entered_pin;
    char action[20];
    
    sscanf(request, "%d %s %d", &account_index, action, &amount);

    if (account_index < 0 || account_index >= 5 || accounts[account_index].is_deleted) {
        printf("[!] Invalid account or account is deleted.\n");
        return;
    }

    if (strcmp(action, "pin") == 0) {  // Authenticate PIN
        sscanf(request, "pin %d", &entered_pin);
        if (entered_pin == accounts[account_index].pin) {
            printf("[*] Authentication successful for Account[%d].\n", account_index);
        } else {
            printf("[!] Invalid PIN.\n");
        }
    }

    if (strcmp(action, "deposit") == 0) {
        accounts[account_index].balance += amount;
        printf("[+] Deposited $%d into Account[%d]. New Balance: $%d\n", amount, account_index, accounts[account_index].balance);
    } else if (strcmp(action, "withdraw") == 0) {
        if (accounts[account_index].balance >= amount) {
            accounts[account_index].balance -= amount;
            printf("[+] Withdrew $%d from Account[%d]. New Balance: $%d\n", amount, account_index, accounts[account_index].balance);
        } else {
            printf("[!] Insufficient funds for Account[%d]\n", account_index);
        }
    } else if (strcmp(action, "view") == 0) {
        printf("[*] Account[%d] Balance: $%d\n", account_index, accounts[account_index].balance);
    } else if (strcmp(action, "transfer") == 0) {
        int to_account;
        sscanf(request, "%d %*s %d %d", &account_index, &amount, &to_account);
        if (to_account < 0 || to_account >= 5 || to_account == account_index) {
            printf("[!] Invalid target account: %d\n", to_account);
            return;
        }

        if (accounts[account_index].balance >= amount) {
            accounts[account_index].balance -= amount;
            accounts[to_account].balance += amount;
            printf("[*] Transferred $%d from Account[%d] to Account[%d]\n", amount, account_index, to_account);
        } else {
            printf("[!] Insufficient funds for transfer from Account[%d]\n", account_index);
        }
    } else if (strcmp(action, "delete") == 0) {
        accounts[account_index].balance = 0;
        accounts[account_index].is_deleted = 1;
        printf("[*] Account[%d] has been deleted.\n", account_index);
    }
}

int main() {
    char buffer[MAX_MSG_SIZE];
    int fd;

    fd = open(FIFO_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open FIFO");
        exit(1);
    }

    sem_init(&mutex, 0, 1);
    initialize_accounts();

    printf("[*] Bank Server Started.\n");

    while (1) {
        ssize_t bytes_read = read(fd, buffer, MAX_MSG_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            sem_wait(&mutex);
            handle_request(buffer);
            sem_post(&mutex);
        }
    }

    close(fd);
    sem_destroy(&mutex);
    return 0;
}
