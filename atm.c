#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#define QUEUE_NAME "/bank_queue"
#define MAX_MSG_SIZE 256
#define PIN_LENGTH 4
#define DOB_LENGTH 11 // Format: DD/MM/YYYY\0

void clear_input_buffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

bool validate_pin(const char *pin)
{
    if (strlen(pin) != PIN_LENGTH)
        return false;
    for (int i = 0; i < PIN_LENGTH; i++)
    {
        if (!isdigit(pin[i]))
            return false;
    }
    return true;
}

bool validate_dob(const char *dob)
{
    if (strlen(dob) != 10 || dob[2] != '/' || dob[5] != '/')
        return false;

    for (int i = 0; i < 10; i++)
    {
        if (i == 2 || i == 5)
            continue;
        if (!isdigit(dob[i]))
            return false;
    }

    int day = (dob[0] - '0') * 10 + (dob[1] - '0');
    int month = (dob[3] - '0') * 10 + (dob[4] - '0');
    int year = (dob[6] - '0') * 1000 + (dob[7] - '0') * 100 +
               (dob[8] - '0') * 10 + (dob[9] - '0');

    if (month < 1 || month > 12 || day < 1 || day > 31)
        return false;
    if (year < 1900 || year > 2100)
        return false;

    return true;
}

void print_menu(int user_id)
{
    printf("\n=============================\n");
    printf("=== ATM Menu (User %d) ===\n", user_id);
    printf("=============================\n");
    printf("1. Deposit\n");
    printf("2. Withdraw\n");
    printf("3. View Account\n");
    printf("4. Transfer\n");
    printf("5. Change PIN\n");
    printf("6. Change DOB\n");
    printf("7. Exit\n");
    printf("-----------------------------\n");
    printf("Select option: ");
}

void send_request(mqd_t mq, const char *request, const char *req)
{
    printf("[*] Sending: %s\n", req);
    if (mq_send(mq, request, strlen(request) + 1, 0) == -1)
    {
        perror("[ERROR] mq_send failed");
    }
    else
    {
        printf("[*] Request sent successfully\n");
    }
}

void atm_session(int user_id)
{
    mqd_t mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == -1)
    {
        perror("[ERROR] mq_open failed");
        return;
    }

    printf("\n[*] User %d: Connected to Bank\n", user_id);

    while (1)
    {
        print_menu(user_id);

        int choice;
        if (scanf("%d", &choice) != 1)
        {
            printf("[!] Invalid input\n");
            clear_input_buffer();
            continue;
        }
        clear_input_buffer();

        if (choice == 7)
        {
            printf("[*] Ending session...\n");
            break;
        }
        else if (choice > 7 || choice < 1)
        {
            printf("[!] Invalid option\n");
            system("clear");
            continue;
        }

        int account_index;
        printf("Enter account index (0-4): ");
        if (scanf("%d", &account_index) != 1 || account_index < 0 || account_index > 4)

        {
            printf("[!] Invalid account index\n");
            clear_input_buffer();
            continue;
        }
        clear_input_buffer();

        char pin[PIN_LENGTH + 1];
        printf("Enter PIN: ");
        if (scanf("%4s", pin) != 1 || !validate_pin(pin))
        {
            printf("[!] Invalid PIN\n");
            clear_input_buffer();
            continue;
        }
        clear_input_buffer();

        char request[MAX_MSG_SIZE];
        char req[MAX_MSG_SIZE];
        switch (choice)
        {
        case 1:
        { // Deposit
            int amount;
            printf("Enter amount: ");
            if (scanf("%d", &amount) != 1 || amount <= 0)
            {
                printf("[!] Invalid amount\n");
                clear_input_buffer();
                continue;
            }
            clear_input_buffer();

            snprintf(request, sizeof(request), "%d deposit %d %s",
                     account_index, amount, pin);
            snprintf(req, sizeof(request), "Deposit: Account Number %d ", account_index);
            break;
        }
        case 2:
        { // Withdraw
            int amount;
            printf("Enter amount: ");
            if (scanf("%d", &amount) != 1 || amount <= 0)
            {
                printf("[!] Invalid amount\n");
                clear_input_buffer();
                continue;
            }
            clear_input_buffer();

            snprintf(request, sizeof(request), "%d withdraw %d %s",
                     account_index, amount, pin);
            snprintf(req, sizeof(request), "Withdraw: %d ", account_index);
            break;
        }
        case 3:
        { // View
            snprintf(request, sizeof(request), "%d view %s", account_index, pin);
            snprintf(req, sizeof(request), "View: Account Number:%d ", account_index);
            break;
        }
        case 4:
        { // Transfer
            int to_account, amount;
            printf("Enter target account (0-4): ");
            if (scanf("%d", &to_account) != 1 || to_account < 0 || to_account > 4)
            {
                printf("[!] Invalid target account\n");
                clear_input_buffer();
                continue;
            }
            clear_input_buffer();

            printf("Enter amount: ");
            if (scanf("%d", &amount) != 1 || amount <= 0)
            {
                printf("[!] Invalid amount\n");
                clear_input_buffer();
                continue;
            }
            clear_input_buffer();

            snprintf(request, sizeof(request), "%d transfer %d %d %s",
                     account_index, amount, to_account, pin);
            snprintf(req, sizeof(request), "Transfer \nAccount Number: %d ", account_index);
            break;
        }
        case 5:
        { // Change PIN
            char new_pin[PIN_LENGTH + 1];
            printf("Enter new PIN: ");
            if (scanf("%4s", new_pin) != 1 || !validate_pin(new_pin))
            {
                printf("[!] Invalid new PIN\n");
                clear_input_buffer();
                continue;
            }
            clear_input_buffer();

            snprintf(request, sizeof(request), "%d changepin %s %s",
                     account_index, pin, new_pin);
            snprintf(req, sizeof(request), "Change pin done\n Account Number: %d ",
                     account_index);
            break;
        }
        case 6:
        { // Change DOB
            char new_dob[DOB_LENGTH];
            printf("Enter new DOB (DD/MM/YYYY): ");
            if (scanf("%10s", new_dob) != 1 || !validate_dob(new_dob))
            {
                printf("[!] Invalid DOB\n");
                clear_input_buffer();
                continue;
            }
            clear_input_buffer();

            snprintf(request, sizeof(request), "%d changedob %s %s",
                     account_index, pin, new_dob);
            snprintf(req, sizeof(request), "Change DOB \n Account Number: %d ", account_index);
            break;
        }
        default:
            printf("[!] Invalid option\n");
            continue;
        }

        send_request(mq, request, req);
        sleep(1); // Give time for bank to process
    }

    mq_close(mq);
}

int main()
{
    printf("\n==============================\n");
    printf("=== ATM CLIENT APPLICATION ===\n");
    printf("==============================\n");

    int user_id = getpid() % 1000;
    atm_session(user_id);

    printf("\n[*] Thank you for using our ATM!\n\n");
    return 0;
}
