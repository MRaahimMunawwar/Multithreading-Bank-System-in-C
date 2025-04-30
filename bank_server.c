#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#define QUEUE_NAME "/bank_queue"
#define MAX_MSG_SIZE 256
#define ACCOUNT_COUNT 5
#define PIN_LENGTH 4
#define DOB_LENGTH 11 // Format: DD/MM/YYYY\0

typedef struct
{
    int account_num;
    int balance;
    char pin[PIN_LENGTH + 1];
    char dob[DOB_LENGTH];
    bool is_active;
    time_t last_access;
} Account;

Account accounts[ACCOUNT_COUNT];
sem_t account_sems[ACCOUNT_COUNT];
mqd_t mq;
volatile sig_atomic_t shutdown_flag = 0;

/* POSIX-compliant SIGINT handler */
void sigint_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\n[*] Received SIGINT, shutting down gracefully...\n");
        shutdown_flag = 1;
    }
}

void cleanup_resources()
{
    printf("[*] Cleaning up resources...\n");

    // Close and unlink message queue
    if (mq != -1)
    {
        mq_close(mq);
        mq_unlink(QUEUE_NAME);
    }

    // Destroy all semaphores
    for (int i = 0; i < ACCOUNT_COUNT; i++)
    {
        sem_destroy(&account_sems[i]);
    }

    printf("[*] Resources cleaned up. Goodbye!\n");
}

void initialize_accounts()
{
    srand(time(NULL));

    for (int i = 0; i < ACCOUNT_COUNT; i++)
    {
        accounts[i].account_num = 10000 + (rand() % 90000);
        accounts[i].balance = 1000 + (rand() % 9000);
        snprintf(accounts[i].pin, PIN_LENGTH + 1, "%04d", rand() % 10000);
        snprintf(accounts[i].dob, DOB_LENGTH, "%02d/%02d/%04d",
                 1 + rand() % 28, 1 + rand() % 12, 1950 + rand() % 50);
        accounts[i].is_active = true;
        accounts[i].last_access = time(NULL);

        sem_init(&account_sems[i], 0, 1);

        printf("[*] Account[%d] Number: %d, PIN: %s, DOB: %s, Balance: $%d\n",
               i, accounts[i].account_num, accounts[i].pin,
               accounts[i].dob, accounts[i].balance);
    }
}

bool validate_pin(int account_index, const char *pin)
{
    if (account_index < 0 || account_index >= ACCOUNT_COUNT)
    {
        printf("[!] Invalid account index\n");
        return false;
    }

    if (strlen(pin) != PIN_LENGTH || strspn(pin, "0123456789") != PIN_LENGTH)
    {
        printf("[!] PIN must be exactly 4 digits\n");
        return false;
    }

    if (strcmp(accounts[account_index].pin, pin) != 0)
    {
        printf("[!] Incorrect PIN\n");
        return false;
    }

    return true;
}

bool validate_dob(const char *dob)
{
    if (strlen(dob) != 10 || dob[2] != '/' || dob[5] != '/')
    {
        return false;
    }

    int day, month, year;
    if (sscanf(dob, "%2d/%2d/%4d", &day, &month, &year) != 3)
    {
        return false;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31)
    {
        return false;
    }

    return true;
}

void log_transaction(const char *operation, int account_index, int amount, bool success)
{
    time_t now = time(NULL);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("[%s] %s Account[%d] %s $%d - %s\n",
           timestamp, operation, account_index,
           amount >= 0 ? "Amount:" : "",
           amount >= 0 ? amount : 0,
           success ? "SUCCESS" : "FAILED");
}

void process_deposit(int account_index, int amount, const char *pin)
{
    bool success = false;

    sem_wait(&account_sems[account_index]);

    if (validate_pin(account_index, pin) && amount > 0)
    {
        accounts[account_index].balance += amount;
        accounts[account_index].last_access = time(NULL);
        printf("[+] Deposit successful. New balance: $%d\n", accounts[account_index].balance);
        success = true;
    }
    else
    {
        printf("[!] Deposit failed\n");
    }

    sem_post(&account_sems[account_index]);
    log_transaction("DEPOSIT", account_index, amount, success);
}

void process_withdrawal(int account_index, int amount, const char *pin)
{
    bool success = false;

    sem_wait(&account_sems[account_index]);

    if (validate_pin(account_index, pin) && amount > 0)
    {
        if (accounts[account_index].balance >= amount)
        {
            accounts[account_index].balance -= amount;
            accounts[account_index].last_access = time(NULL);
            printf("[+] Withdrawal successful. New balance: $%d\n", accounts[account_index].balance);
            success = true;
        }
        else
        {
            printf("[!] Insufficient funds\n");
        }
    }
    else
    {
        printf("[!] Withdrawal failed\n");
    }

    sem_post(&account_sems[account_index]);
    log_transaction("WITHDRAW", account_index, amount, success);
}

void process_transfer(int from_account, int to_account, int amount, const char *pin)
{
    bool success = false;

    if (from_account == to_account)
    {
        printf("[!] Cannot transfer to the same account\n");
        return;
    }

    // Lock accounts in order to prevent deadlock
    int first = from_account < to_account ? from_account : to_account;
    int second = from_account < to_account ? to_account : from_account;

    sem_wait(&account_sems[first]);
    sem_wait(&account_sems[second]);

    if (validate_pin(from_account, pin) && amount > 0)
    {
        if (accounts[from_account].balance >= amount)
        {
            accounts[from_account].balance -= amount;
            accounts[to_account].balance += amount;
            accounts[from_account].last_access = time(NULL);
            accounts[to_account].last_access = time(NULL);
            printf("[+] Transfer successful. From Account[%d] Balance: $%d, To Account[%d] Balance: $%d\n",
                   from_account, accounts[from_account].balance,
                   to_account, accounts[to_account].balance);
            success = true;
        }
        else
        {
            printf("[!] Insufficient funds for transfer\n");
        }
    }
    else
    {
        printf("[!] Transfer failed\n");
    }

    sem_post(&account_sems[second]);
    sem_post(&account_sems[first]);
    log_transaction("TRANSFER", from_account, amount, success);
}

void process_view(int account_index, const char *pin)
{
    sem_wait(&account_sems[account_index]);

    if (validate_pin(account_index, pin))
    {
        printf("\n=== Account[%d] Details ===\n", account_index);
        printf("Account Number: %d\n", accounts[account_index].account_num);
        printf("Balance: $%d\n", accounts[account_index].balance);
        printf("Date of Birth: %s\n", accounts[account_index].dob);
        printf("Last Access: %s", ctime(&accounts[account_index].last_access));
        accounts[account_index].last_access = time(NULL);
    }
    else
    {
        printf("[!] Failed to view account details\n");
    }

    sem_post(&account_sems[account_index]);
}

void process_pin_change(int account_index, const char *old_pin, const char *new_pin)
{
    sem_wait(&account_sems[account_index]);

    if (validate_pin(account_index, old_pin))
    {
        if (strlen(new_pin) == PIN_LENGTH && strspn(new_pin, "0123456789") == PIN_LENGTH)
        {
            strncpy(accounts[account_index].pin, new_pin, PIN_LENGTH + 1);
            accounts[account_index].last_access = time(NULL);
            printf("[+] PIN changed successfully\n");
        }
        else
        {
            printf("[!] New PIN must be 4 digits\n");
        }
    }
    else
    {
        printf("[!] PIN change failed\n");
    }

    sem_post(&account_sems[account_index]);
}

void process_dob_change(int account_index, const char *pin, const char *new_dob)
{
    sem_wait(&account_sems[account_index]);

    if (validate_pin(account_index, pin))
    {
        if (validate_dob(new_dob))
        {
            strncpy(accounts[account_index].dob, new_dob, DOB_LENGTH);
            accounts[account_index].last_access = time(NULL);
            printf("[+] DOB changed successfully\n");
        }
        else
        {
            printf("[!] Invalid DOB format (DD/MM/YYYY)\n");
        }
    }
    else
    {
        printf("[!] DOB change failed\n");
    }

    sem_post(&account_sems[account_index]);
}

void handle_message(char *message)
{
    char action[20];
    int account_index, amount, to_account;
    char pin[PIN_LENGTH + 1], new_pin[PIN_LENGTH + 1], dob[DOB_LENGTH];

    if (sscanf(message, "%d %19s", &account_index, action) < 2)
    {
        printf("[!] Invalid message format\n");
        return;
    }

    if (account_index < 0 || account_index >= ACCOUNT_COUNT)
    {
        printf("[!] Invalid account index\n");
        return;
    }

    if (strcmp(action, "deposit") == 0)
    {
        if (sscanf(message, "%d deposit %d %4s", &account_index, &amount, pin) == 3)
        {
            process_deposit(account_index, amount, pin);
        }
    }
    else if (strcmp(action, "withdraw") == 0)
    {
        if (sscanf(message, "%d withdraw %d %4s", &account_index, &amount, pin) == 3)
        {
            process_withdrawal(account_index, amount, pin);
        }
    }
    else if (strcmp(action, "view") == 0)
    {
        if (sscanf(message, "%d view %4s", &account_index, pin) == 2)
        {
            process_view(account_index, pin);
        }
    }
    else if (strcmp(action, "transfer") == 0)
    {
        if (sscanf(message, "%d transfer %d %d %4s", &account_index, &amount, &to_account, pin) == 4)
        {
            process_transfer(account_index, to_account, amount, pin);
        }
    }
    else if (strcmp(action, "changepin") == 0)
    {
        if (sscanf(message, "%d changepin %4s %4s", &account_index, pin, new_pin) == 3)
        {
            process_pin_change(account_index, pin, new_pin);
        }
    }
    else if (strcmp(action, "changedob") == 0)
    {
        if (sscanf(message, "%d changedob %4s %10s", &account_index, pin, dob) == 3)
        {
            process_dob_change(account_index, pin, dob);
        }
    }
    else if (strcmp(action, "exit") == 0)
    {
        printf("[*] Received exit command\n");
        shutdown_flag = 1;
    }
    else
    {
        printf("[!] Unknown action: %s\n", action);
    }
}

int main()
{
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = MAX_MSG_SIZE,
        .mq_curmsgs = 0};

    // Initialize message queue
    mq_unlink(QUEUE_NAME);
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if (mq == -1)
    {
        perror("[ERROR] mq_open failed");
        exit(EXIT_FAILURE);
    }

    // Initialize accounts
    initialize_accounts();

    printf("\n===================================\n");
    printf("   BANK SERVER - READY FOR REQUESTS\n");
    printf("===================================\n\n");

    // Main processing loop
    char buffer[MAX_MSG_SIZE];
    while (!shutdown_flag)
    {
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read >= 0)
        {
            buffer[bytes_read] = '\0';
            printf("[*] Processing: %s\n", buffer);
            handle_message(buffer);
        }
        else
        {
            perror("[ERROR] mq_receive");
            sleep(1);
        }
    }

    cleanup_resources();
    return 0;
}
