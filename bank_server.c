#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIFO_NAME "bank_fifo"
#define MAX_MSG_SIZE 256
#define MAX_ACCOUNTS 100

typedef struct {
    int id;
    int balance;
} Account;

Account accounts[MAX_ACCOUNTS];
int account_count = 0;
sem_t bank_sem;

void create_account() {
    if (account_count >= MAX_ACCOUNTS) {
        printf("[!] Cannot create more accounts.\n");
        return;
    }
    sem_wait(&bank_sem);
    accounts[account_count].id = account_count;
    accounts[account_count].balance = 0;
    printf("[✓] Account[%d] created with balance $0\n", account_count);
    account_count++;
    sem_post(&bank_sem);
}

int get_account_index(int id) {
    if (id < 0 || id >= account_count) return -1;
    return id;
}

void deposit(int id, int amount) {
    if (amount < 0) {
        printf("[!] Deposit failed: Negative amount.\n");
        return;
    }
    int idx = get_account_index(id);
    if (idx == -1) {
        printf("[!] Invalid account ID.\n");
        return;
    }

    sem_wait(&bank_sem);
    accounts[idx].balance += amount;
    printf("[✓] Deposited $%d to Account[%d]. New Balance: $%d\n", amount, id, accounts[idx].balance);
    sem_post(&bank_sem);
}

void withdraw(int id, int amount) {
    if (amount < 0) {
        printf("[!] Withdrawal failed: Negative amount.\n");
        return;
    }
    int idx = get_account_index(id);
    if (idx == -1) {
        printf("[!] Invalid account ID.\n");
        return;
    }

    sem_wait(&bank_sem);
    if (accounts[idx].balance >= amount) {
        accounts[idx].balance -= amount;
        printf("[✓] Withdrew $%d from Account[%d]. New Balance: $%d\n", amount, id, accounts[idx].balance);
    } else {
        printf("[!] Withdrawal failed: Insufficient funds in Account[%d].\n", id);
    }
    sem_post(&bank_sem);
}

void view_balance(int id) {
    int idx = get_account_index(id);
    if (idx == -1) {
        printf("[!] Invalid account ID.\n");
        return;
    }

    sem_wait(&bank_sem);
    printf("[*] Account[%d] Balance: $%d\n", id, accounts[idx].balance);
    sem_post(&bank_sem);
}

void transfer(int from, int to, int amount) {
    if (amount < 0) {
        printf("[!] Transfer failed: Negative amount.\n");
        return;
    }
    int idx1 = get_account_index(from);
    int idx2 = get_account_index(to);
    if (idx1 == -1 || idx2 == -1 || from == to) {
        printf("[!] Invalid transfer accounts.\n");
        return;
    }

    sem_wait(&bank_sem);
    if (accounts[idx1].balance >= amount) {
        accounts[idx1].balance -= amount;
        accounts[idx2].balance += amount;
        printf("[✓] Transferred $%d from Account[%d] to Account[%d]\n", amount, from, to);
    } else {
        printf("[!] Transfer failed: Insufficient funds in Account[%d].\n", from);
    }
    sem_post(&bank_sem);
}

void handle_command(char* msg) {
    char cmd[20];
    int id1, id2, amount;

    if (strncmp(msg, "create", 6) == 0) {
        create_account();
    } else if (sscanf(msg, "%d deposit %d", &id1, &amount) == 2) {
        deposit(id1, amount);
    } else if (sscanf(msg, "%d withdraw %d", &id1, &amount) == 2) {
        withdraw(id1, amount);
    } else if (sscanf(msg, "%d view", &id1) == 1) {
        view_balance(id1);
    } else if (sscanf(msg, "%d transfer %d %d", &id1, &amount, &id2) == 3) {
        transfer(id1, id2, amount);
    } else {
        printf("[!] Invalid command received: %s\n", msg);
    }
}

int main() {
    char buffer[MAX_MSG_SIZE];

    mkfifo(FIFO_NAME, 0666);
    sem_init(&bank_sem, 0, 1);

    printf("[*] Bank Server Started. Waiting for ATM commands...\n");

    int fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        perror("open FIFO");
        return 1;
    }

    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            handle_command(buffer);
        } else {
            sleep(1);
        }
    }

    close(fd);
    unlink(FIFO_NAME);
    sem_destroy(&bank_sem);
    return 0;
}
