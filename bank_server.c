#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define REQUEST_FIFO "/tmp/bank_request_fifo"
#define RESPONSE_FIFO "/tmp/bank_response_fifo"
#define MAX_MSG_SIZE 256
#define MAX_RESPONSE_SIZE 256
#define MAX_ACCOUNTS 100  // Increased maximum number of accounts

// Account structure
typedef struct {
    int balance;
    int pin;
    int is_active;  // Flag to check if the account is active
    pthread_mutex_t account_mutex;  // Mutex for individual account access
} Account;

// Global variables
Account *accounts;  // Dynamic array of accounts
int account_count = 0;  // Current number of accounts
pthread_mutex_t accounts_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for account array access
sem_t request_sem;  // Semaphore to signal request availability

// Structure for worker thread data
typedef struct {
    int thread_id;
} WorkerData;

// Function to create a new account
int create_account(int pin) {
    pthread_mutex_lock(&accounts_mutex);
    
    if (account_count >= MAX_ACCOUNTS) {
        pthread_mutex_unlock(&accounts_mutex);
        return -1;  // Maximum account limit reached
    }
    
    // Initialize new account
    accounts[account_count].balance = 0;  // No initial balance
    accounts[account_count].pin = pin;
    accounts[account_count].is_active = 1;
    pthread_mutex_init(&accounts[account_count].account_mutex, NULL);
    
    int new_account_num = account_count;
    account_count++;
    
    pthread_mutex_unlock(&accounts_mutex);
    return new_account_num;
}

// Function to send response back to the client
void send_response(const char *response) {
    int fd_response;
    
    // Open response FIFO
    fd_response = open(RESPONSE_FIFO, O_WRONLY);
    if (fd_response == -1) {
        perror("Failed to open response FIFO");
        return;
    }
    
    // Send the response
    if (write(fd_response, response, strlen(response) + 1) == -1) {
        perror("write");
    }
    
    close(fd_response);
}

// Function to handle bank operations
void handle_request(char *request) {
    char response[MAX_RESPONSE_SIZE];
    char action[20];
    int account_num, amount, to_account, pin;
    
    // Parse create account request
    if (strncmp(request, "create", 6) == 0) {
        sscanf(request, "create %d", &pin);
        int new_account = create_account(pin);
        
        if (new_account >= 0) {
            snprintf(response, MAX_RESPONSE_SIZE, "Account %d created successfully with PIN %d", new_account, pin);
        } else {
            snprintf(response, MAX_RESPONSE_SIZE, "Failed to create account. Maximum limit reached.");
        }
        
        send_response(response);
        return;
    }
    
    // Parse other operations
    int params_read = sscanf(request, "%d %s", &account_num, action);
    
    if (params_read != 2) {
        snprintf(response, MAX_RESPONSE_SIZE, "Invalid request format");
        send_response(response);
        return;
    }
    
    // Validate account number
    pthread_mutex_lock(&accounts_mutex);
    if (account_num < 0 || account_num >= account_count) {
        pthread_mutex_unlock(&accounts_mutex);
        snprintf(response, MAX_RESPONSE_SIZE, "Invalid account number: %d", account_num);
        send_response(response);
        return;
    }
    pthread_mutex_unlock(&accounts_mutex);
    
    // Lock the specific account
    pthread_mutex_lock(&accounts[account_num].account_mutex);
    
    // Check if account is active
    if (!accounts[account_num].is_active) {
        pthread_mutex_unlock(&accounts[account_num].account_mutex);
        snprintf(response, MAX_RESPONSE_SIZE, "Account %d has been deleted", account_num);
        send_response(response);
        return;
    }
    
    // Handle different actions
    if (strcmp(action, "deposit") == 0) {
        sscanf(request, "%d deposit %d %d", &account_num, &amount, &pin);
        
        if (pin != accounts[account_num].pin) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid PIN for account %d", account_num);
        } else if (amount <= 0) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid deposit amount");
        } else {
            accounts[account_num].balance += amount;
            snprintf(response, MAX_RESPONSE_SIZE, "Deposited $%d into Account %d. New Balance: $%d", 
                    amount, account_num, accounts[account_num].balance);
        }
    } 
    else if (strcmp(action, "withdraw") == 0) {
        sscanf(request, "%d withdraw %d %d", &account_num, &amount, &pin);
        
        if (pin != accounts[account_num].pin) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid PIN for account %d", account_num);
        } else if (amount <= 0) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid withdrawal amount");
        } else if (accounts[account_num].balance < amount) {
            snprintf(response, MAX_RESPONSE_SIZE, "Insufficient funds for Account %d. Current Balance: $%d", 
                    account_num, accounts[account_num].balance);
        } else {
            accounts[account_num].balance -= amount;
            snprintf(response, MAX_RESPONSE_SIZE, "Withdrew $%d from Account %d. New Balance: $%d", 
                    amount, account_num, accounts[account_num].balance);
        }
    } 
    else if (strcmp(action, "view") == 0) {
        sscanf(request, "%d view %d", &account_num, &pin);
        
        if (pin != accounts[account_num].pin) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid PIN for account %d", account_num);
        } else {
            snprintf(response, MAX_RESPONSE_SIZE, "Account %d Balance: $%d", 
                    account_num, accounts[account_num].balance);
        }
    } 
    else if (strcmp(action, "transfer") == 0) {
        sscanf(request, "%d transfer %d %d %d", &account_num, &amount, &to_account, &pin);
        
        if (pin != accounts[account_num].pin) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid PIN for account %d", account_num);
        } else {
            pthread_mutex_unlock(&accounts[account_num].account_mutex);
            
            // Check target account
            pthread_mutex_lock(&accounts_mutex);
            if (to_account < 0 || to_account >= account_count || to_account == account_num) {
                pthread_mutex_unlock(&accounts_mutex);
                pthread_mutex_lock(&accounts[account_num].account_mutex);
                snprintf(response, MAX_RESPONSE_SIZE, "Invalid target account: %d", to_account);
            } else if (!accounts[to_account].is_active) {
                pthread_mutex_unlock(&accounts_mutex);
                pthread_mutex_lock(&accounts[account_num].account_mutex);
                snprintf(response, MAX_RESPONSE_SIZE, "Target account %d has been deleted", to_account);
            } else {
                pthread_mutex_unlock(&accounts_mutex);
                
                // Lock both accounts in order (to prevent deadlock)
                if (account_num < to_account) {
                    pthread_mutex_lock(&accounts[account_num].account_mutex);
                    pthread_mutex_lock(&accounts[to_account].account_mutex);
                } else {
                    pthread_mutex_lock(&accounts[to_account].account_mutex);
                    pthread_mutex_lock(&accounts[account_num].account_mutex);
                }
                
                if (amount <= 0) {
                    snprintf(response, MAX_RESPONSE_SIZE, "Invalid transfer amount");
                } else if (accounts[account_num].balance < amount) {
                    snprintf(response, MAX_RESPONSE_SIZE, "Insufficient funds for transfer from Account %d", account_num);
                } else {
                    accounts[account_num].balance -= amount;
                    accounts[to_account].balance += amount;
                    snprintf(response, MAX_RESPONSE_SIZE, "Transferred $%d from Account %d to Account %d. New Balance: $%d", 
                            amount, account_num, to_account, accounts[account_num].balance);
                }
                
                // Unlock the target account
                if (account_num != to_account) {
                    pthread_mutex_unlock(&accounts[to_account].account_mutex);
                }
                
                // Don't unlock source account here - will be unlocked at end of function
                return;
            }
        }
    } 
    else if (strcmp(action, "delete") == 0) {
        sscanf(request, "%d delete %d", &account_num, &pin);
        
        if (pin != accounts[account_num].pin) {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid PIN for account %d", account_num);
        } else {
            accounts[account_num].balance = 0;
            accounts[account_num].is_active = 0;
            snprintf(response, MAX_RESPONSE_SIZE, "Account %d has been deleted", account_num);
        }
    } 
    else {
        snprintf(response, MAX_RESPONSE_SIZE, "Unknown action: %s", action);
    }
    
    // Unlock the account
    pthread_mutex_unlock(&accounts[account_num].account_mutex);
    
    // Send response
    send_response(response);
}

// Worker thread function to process client requests
void* worker_thread(void* arg) {
    WorkerData* data = (WorkerData*)arg;
    int fd_request;
    char buffer[MAX_MSG_SIZE];
    
    printf("[*] Worker thread %d started\n", data->thread_id);
    
    while (1) {
        // Open the request FIFO for reading
        fd_request = open(REQUEST_FIFO, O_RDONLY);
        if (fd_request == -1) {
            perror("Failed to open request FIFO");
            sleep(1);
            continue;
        }
        
        // Read client request
        ssize_t bytes_read = read(fd_request, buffer, MAX_MSG_SIZE - 1);
        close(fd_request);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("[Thread %d] Processing request: %s\n", data->thread_id, buffer);
            handle_request(buffer);
        } else if (bytes_read == 0) {
            // FIFO closed, reopen it
            continue;
        } else {
            perror("read");
            sleep(1);
        }
    }
    
    free(data);
    return NULL;
}

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nShutting down bank server...\n");
        
        // Clean up accounts and their mutexes
        for (int i = 0; i < account_count; i++) {
            pthread_mutex_destroy(&accounts[i].account_mutex);
        }
        
        // Free dynamically allocated memory
        free(accounts);
        
        // Destroy mutexes and semaphores
        pthread_mutex_destroy(&accounts_mutex);
        sem_destroy(&request_sem);
        
        // Exit
        exit(0);
    }
}

int main() {
    // Allocate memory for accounts
    accounts = (Account*)malloc(MAX_ACCOUNTS * sizeof(Account));
    if (accounts == NULL) {
        perror("Failed to allocate memory for accounts");
        exit(EXIT_FAILURE);
    }
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    
    // Check if FIFOs exist, if not create them
    struct stat st;
    
    if (stat(REQUEST_FIFO, &st) == -1) {
        if (mkfifo(REQUEST_FIFO, 0666) == -1) {
            perror("mkfifo request");
            exit(EXIT_FAILURE);
        }
    }
    
    if (stat(RESPONSE_FIFO, &st) == -1) {
        if (mkfifo(RESPONSE_FIFO, 0666) == -1) {
            perror("mkfifo response");
            exit(EXIT_FAILURE);
        }
    }
    
    // Initialize semaphore
    if (sem_init(&request_sem, 0, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    
    printf("[*] Bank Server Started with capacity for %d accounts.\n", MAX_ACCOUNTS);
    
    // Create worker threads
    const int NUM_WORKERS = 5;  // Number of worker threads
    pthread_t workers[NUM_WORKERS];
    WorkerData* worker_data[NUM_WORKERS];
    
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_data[i] = (WorkerData*)malloc(sizeof(WorkerData));
        worker_data[i]->thread_id = i + 1;
        
        if (pthread_create(&workers[i], NULL, worker_thread, worker_data[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for worker threads to finish (they won't unless program is terminated)
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    
    // Clean up
    for (int i = 0; i < account_count; i++) {
        pthread_mutex_destroy(&accounts[i].account_mutex);
    }
    
    free(accounts);
    pthread_mutex_destroy(&accounts_mutex);
    sem_destroy(&request_sem);
    
    return 0;
}