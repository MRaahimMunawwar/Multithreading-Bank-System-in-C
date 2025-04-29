#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>

#define FIFO_PATH "/tmp/bank_fifo"
#define MAX_MSG_SIZE 256

sem_t mutex;  // Semaphore to ensure exclusive access to user inputs

void* atm_user_session(void* arg) {
    int choice, account_index, amount, to_account, pin, entered_pin;
    char buffer[MAX_MSG_SIZE];
    
    printf("[*] ATM Started. Please enter your PIN to proceed.\n");
    
    // Ask for PIN
    printf("Enter your PIN: ");
    scanf("%d", &pin);
    snprintf(buffer, sizeof(buffer), "pin %d", pin);

    sem_wait(&mutex);
    // Send the PIN to the bank server to authenticate
    if (write(open(FIFO_PATH, O_WRONLY), buffer, strlen(buffer) + 1) == -1) {
        perror("write");
        sem_post(&mutex);
        return NULL;
    }
    sem_post(&mutex);

    printf("[*] ATM Menu:\n");
    printf("1. Deposit\n");
    printf("2. Withdraw\n");
    printf("3. View Balance\n");
    printf("4. Transfer\n");
    printf("5. Delete Account\n");
    printf("6. Exit\n");

    while (1) {
        printf("Select option: ");
        scanf("%d", &choice);

        if (choice == 6) {
            printf("[*] Exiting ATM...\n");
            break;
        }

        printf("Enter account index (0-4): ");
        scanf("%d", &account_index);
        if (account_index < 0 || account_index >= 5) {
            printf("[!] Invalid account index.\n");
            continue;
        }

        if (choice == 1) {  // Deposit
            printf("Enter deposit amount: ");
            scanf("%d", &amount);
            snprintf(buffer, sizeof(buffer), "%d deposit %d", account_index, amount);
        } else if (choice == 2) {  // Withdraw
            printf("Enter withdraw amount: ");
            scanf("%d", &amount);
            snprintf(buffer, sizeof(buffer), "%d withdraw %d", account_index, amount);
        } else if (choice == 3) {  // View Balance
            snprintf(buffer, sizeof(buffer), "%d view 0", account_index);
        } else if (choice == 4) {  // Transfer
            printf("Enter target account index: ");
            scanf("%d", &to_account);
            if (to_account < 0 || to_account >= 5 || to_account == account_index) {
                printf("[!] Invalid target account.\n");
                continue;
            }
            printf("Enter transfer amount: ");
            scanf("%d", &amount);
            snprintf(buffer, sizeof(buffer), "%d transfer %d %d", account_index, amount, to_account);
        } else if (choice == 5) {  // Delete Account
            snprintf(buffer, sizeof(buffer), "%d delete", account_index);
        } else {
            printf("[!] Invalid option.\n");
            continue;
        }

        sem_wait(&mutex);
        if (write(open(FIFO_PATH, O_WRONLY), buffer, strlen(buffer) + 1) == -1) {
            perror("write");
        }
        sem_post(&mutex);
    }

    return NULL;
}

int main() {
    pthread_t atm_thread;
    sem_init(&mutex, 0, 1);  // Initialize semaphore for mutual exclusion

    if (pthread_create(&atm_thread, NULL, atm_user_session, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }

    pthread_join(atm_thread, NULL);  // Wait for the ATM session to finish
    sem_destroy(&mutex);  // Destroy semaphore

    return 0;
}
