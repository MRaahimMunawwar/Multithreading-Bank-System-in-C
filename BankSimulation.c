#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <unistd.h>

#define QUEUE_NAME "/bank_queue"
#define MAX_MSG_SIZE 256
#define MAX_ACCOUNTS 10
#define MAX_NAME_LEN 100

// Define constants for message types
#define DEPOSIT "deposit"
#define WITHDRAW "withdraw"

// Account structure
typedef struct {
    char name[MAX_NAME_LEN];
    char mother_name[MAX_NAME_LEN];
    char dob[20];
    int tax_filer; // 1 = filer, 0 = non-filer
    int balance;
    pthread_mutex_t lock; // Mutex for safe concurrent access
} Account;

// Global variables
Account accounts[MAX_ACCOUNTS];
int account_count = 0; // Number of accounts in the bank
sem_t counter_sem; // Semaphore for ATM counter

// Function to initialize an account
void init_account(Account *acc, const char *name, const char *mother_name, const char *dob, int tax_filer, int initial_balance) {
    strncpy(acc->name, name, MAX_NAME_LEN);
    strncpy(acc->mother_name, mother_name, MAX_NAME_LEN);
    strncpy(acc->dob, dob, 20);
    acc->tax_filer = tax_filer;
    acc->balance = initial_balance;
    pthread_mutex_init(&(acc->lock), NULL);
}

// Function to deposit money into the account
void deposit(Account *acc, int amount) {
    pthread_mutex_lock(&(acc->lock));
    acc->balance += amount;
    pthread_mutex_unlock(&(acc->lock));
}

// Function to withdraw money from the account
int withdraw(Account *acc, int amount) {
    int success = 0;
    pthread_mutex_lock(&(acc->lock));
    if (acc->balance >= amount) {
        acc->balance -= amount;
        success = 1;
    }
    pthread_mutex_unlock(&(acc->lock));
    return success;
}

// Function to print account details
void print_account(Account *acc) {
    printf("Name: %s\n", acc->name);
    printf("Mother's Name: %s\n", acc->mother_name);
    printf("DOB: %s\n", acc->dob);
    printf("Tax Filer: %s\n", acc->tax_filer ? "Yes" : "No");
    printf("Balance: $%d\n", acc->balance);
}

// Function to process transactions
void process_transaction(char *transaction, int account_index) {
    char action[50];
    int amount;
    
    sscanf(transaction, "%s %d", action, &amount);

    sem_wait(&counter_sem); // Wait if ATM counter is in use

    if (strcmp(action, DEPOSIT) == 0) {
        deposit(&accounts[account_index], amount);
        printf("Deposited $%d. New Balance: $%d\n", amount, accounts[account_index].balance);
    } else if (strcmp(action, WITHDRAW) == 0) {
        if (withdraw(&accounts[account_index], amount)) {
            printf("Withdrew $%d. New Balance: $%d\n", amount, accounts[account_index].balance);
        } else {
            printf("Withdrawal failed. Insufficient funds.\n");
        }
    }

    sem_post(&counter_sem); // Release the ATM counter

    print_account(&accounts[account_index]);
}

// Function to handle ATM messages and process transactions
void handle_atm_message(char *message) {
    int account_index;
    char transaction[256];

    sscanf(message, "%d %s", &account_index, transaction);
    if (account_index < 0 || account_index >= account_count) {
        printf("Invalid account index!\n");
        return;
    }

    process_transaction(transaction, account_index);
}

int main() {
    mqd_t mq;
    char buffer[MAX_MSG_SIZE];
    
    // Initialize semaphore for ATM counter
    sem_init(&counter_sem, 0, 1); // Only 1 ATM counter available for now

    // Initialize accounts with sample data
    init_account(&accounts[0], "John Doe", "Jane Doe", "01-01-1980", 1, 1000);
    init_account(&accounts[1], "Alice Smith", "Mary Smith", "02-03-1990", 0, 500);
    account_count = 2; // Currently we have 2 accounts

    // Open the message queue for receiving transactions from ATM
    mq = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, 0644, NULL);
    if (mq == -1) {
        perror("mq_open");
        exit(1);
    }

    while (1) {
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            printf("Received transaction: %s\n", buffer);
            handle_atm_message(buffer);
        } else {
            perror("mq_receive");
        }
    }

    // Cleanup
    mq_close(mq);
    return 0;
}
