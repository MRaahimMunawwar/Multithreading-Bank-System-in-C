#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define REQUEST_FIFO "/tmp/bank_request_fifo"
#define RESPONSE_FIFO "/tmp/bank_response_fifo"
#define MAX_MSG_SIZE 256
#define MAX_RESPONSE_SIZE 256


typedef struct {
    int thread_id;
} ThreadData;

sem_t request_mutex;  // Mutex for request FIFO access
sem_t response_sem;   // Semaphore to signal response availability
pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for console I/O

// Function to read response from the response FIFO
void read_response() {
    int fd_response;
    char response[MAX_RESPONSE_SIZE];
    
    // Open response FIFO
    fd_response = open(RESPONSE_FIFO, O_RDONLY);
    if (fd_response == -1) {
        perror("Failed to open response FIFO");
        return;
    }
    
    // Read response
    ssize_t bytes_read = read(fd_response, response, MAX_RESPONSE_SIZE - 1);
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        pthread_mutex_lock(&io_mutex);
        printf("\n[SERVER RESPONSE] %s\n", response);
        pthread_mutex_unlock(&io_mutex);
    } else {
        pthread_mutex_lock(&io_mutex);
        printf("\n[ERROR] Failed to read response\n");
        pthread_mutex_unlock(&io_mutex);
    }
    
    close(fd_response);
}

// ATM client session thread function
void* atm_user_session(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int fd_request;
    int choice, account_num, amount, to_account, pin;
    char buffer[MAX_MSG_SIZE];
    
    pthread_mutex_lock(&io_mutex);
    printf("\n[*] ATM Client %d Started\n", data->thread_id);
    pthread_mutex_unlock(&io_mutex);
    
    while (1) {
        pthread_mutex_lock(&io_mutex);
        printf("\n===== ATM Menu (Client %d) =====\n", data->thread_id);
        printf("1. Create Account\n");
        printf("2. Deposit\n");
        printf("3. Withdraw\n");
        printf("4. View Balance\n");
        printf("5. Transfer\n");
        printf("6. Delete Account\n");
        printf("7. Exit\n");
        printf("Select option: ");
        scanf("%d", &choice);
        pthread_mutex_unlock(&io_mutex);
        
        if (choice == 7) {
            pthread_mutex_lock(&io_mutex);
            printf("[*] Exiting ATM Client %d...\n", data->thread_id);
            pthread_mutex_unlock(&io_mutex);
            break;
        }
        
        // Handle different operations
        switch (choice) {
            case 1: // Create Account
                pthread_mutex_lock(&io_mutex);
                printf("Enter PIN for new account: ");
                scanf("%d", &pin);
                snprintf(buffer, sizeof(buffer), "create %d", pin);
                pthread_mutex_unlock(&io_mutex);
                break;
                
            case 2: // Deposit
                pthread_mutex_lock(&io_mutex);
                printf("Enter account number: ");
                scanf("%d", &account_num);
                printf("Enter deposit amount: ");
                scanf("%d", &amount);
                printf("Enter PIN: ");
                scanf("%d", &pin);
                snprintf(buffer, sizeof(buffer), "%d deposit %d %d", account_num, amount, pin);
                pthread_mutex_unlock(&io_mutex);
                break;
                
            case 3: // Withdraw
                pthread_mutex_lock(&io_mutex);
                printf("Enter account number: ");
                scanf("%d", &account_num);
                printf("Enter withdraw amount: ");
                scanf("%d", &amount);
                printf("Enter PIN: ");
                scanf("%d", &pin);
                snprintf(buffer, sizeof(buffer), "%d withdraw %d %d", account_num, amount, pin);
                pthread_mutex_unlock(&io_mutex);
                break;
                
            case 4: // View Balance
                pthread_mutex_lock(&io_mutex);
                printf("Enter account number: ");
                scanf("%d", &account_num);
                printf("Enter PIN: ");
                scanf("%d", &pin);
                snprintf(buffer, sizeof(buffer), "%d view %d", account_num, pin);
                pthread_mutex_unlock(&io_mutex);
                break;
                
            case 5: // Transfer
                pthread_mutex_lock(&io_mutex);
                printf("Enter source account number: ");
                scanf("%d", &account_num);
                printf("Enter target account number: ");
                scanf("%d", &to_account);
                printf("Enter transfer amount: ");
                scanf("%d", &amount);
                printf("Enter PIN: ");
                scanf("%d", &pin);
                snprintf(buffer, sizeof(buffer), "%d transfer %d %d %d", account_num, amount, to_account, pin);
                pthread_mutex_unlock(&io_mutex);
                break;
                
            case 6: // Delete Account
                pthread_mutex_lock(&io_mutex);
                printf("Enter account number: ");
                scanf("%d", &account_num);
                printf("Enter PIN: ");
                scanf("%d", &pin);
                snprintf(buffer, sizeof(buffer), "%d delete %d", account_num, pin);
                pthread_mutex_unlock(&io_mutex);
                break;
                
            default:
                pthread_mutex_lock(&io_mutex);
                printf("[!] Invalid option.\n");
                pthread_mutex_unlock(&io_mutex);
                continue;
        }
        
        // Wait for mutex to ensure exclusive access to the request FIFO
        sem_wait(&request_mutex);
        
        // Open request FIFO
        fd_request = open(REQUEST_FIFO, O_WRONLY);
        if (fd_request == -1) {
            perror("Failed to open request FIFO");
            sem_post(&request_mutex);
            continue;
        }
        
        // Send the request
        if (write(fd_request, buffer, strlen(buffer) + 1) == -1) {
            perror("write");
            close(fd_request);
            sem_post(&request_mutex);
            continue;
        }
        
        close(fd_request);
        sem_post(&request_mutex);
        
        // Wait for a short time and then read the response
        usleep(100000); // 100ms delay to ensure server has time to process
        read_response();
    }
    
    free(data);
    return NULL;
}

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nCleaning up and exiting...\n");
        sem_destroy(&request_mutex);
        sem_destroy(&response_sem);
        pthread_mutex_destroy(&io_mutex);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
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
    
    // Initialize semaphores
    if (sem_init(&request_mutex, 0, 1) == -1) {
        perror("sem_init request_mutex");
        exit(EXIT_FAILURE);
    }
    
    if (sem_init(&response_sem, 0, 0) == -1) {
        perror("sem_init response_sem");
        exit(EXIT_FAILURE);
    }
    
    // Get the number of ATM clients to simulate
    int num_clients = 1;  // Default is 1 client
    
    if (argc > 1) {
        num_clients = atoi(argv[1]);
        if (num_clients <= 0) {
            num_clients = 1;
        }
    }
    
    pthread_t threads[num_clients];
    ThreadData* thread_data[num_clients];
    
    printf("[*] Starting %d ATM client(s)...\n", num_clients);
    
    // Create client threads
    for (int i = 0; i < num_clients; i++) {
        thread_data[i] = (ThreadData*)malloc(sizeof(ThreadData));
        thread_data[i]->thread_id = i + 1;
        
        if (pthread_create(&threads[i], NULL, atm_user_session, thread_data[i]) != 0) {
            perror("pthread_create");
            return -1;
        }
    }
    
    // Wait for all client threads to finish
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Clean up
    sem_destroy(&request_mutex);
    sem_destroy(&response_sem);
    pthread_mutex_destroy(&io_mutex);
    
    return 0;
}