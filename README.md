# Bank and ATM System

This project simulates a **Bank** and an **ATM** system, where multiple accounts can be managed concurrently. The system uses **semaphores**, **mutexes**, and **message queues (IPC)** to facilitate safe and concurrent transactions. The ATM system interacts with the bank to perform **deposit**, **withdrawal**, and **view account details** operations.

## Features

- **Multiple Accounts**: The bank can handle multiple customer accounts.
- **Concurrency**: Each account is protected by a mutex to allow concurrent transactions safely.
- **ATM and Bank Interaction**: The ATM and Bank interact via **Message Passing** (using **message queues** for IPC).
- **Semaphore for ATM Counter**: A semaphore is used to ensure that only one ATM can access a bank account at a time.
- **User-friendly Menu**: The ATM offers a menu-based interface to interact with the user.

## Requirements

- **Linux-based OS** (e.g., Ubuntu, CentOS)
- **GCC Compiler**
- **pthread library** (`-lpthread`)
- **Real-time library** for message queues (`-lrt`)

## Files

- **bank_server.c**: Implements the Bank's logic for managing accounts, deposits, withdrawals, and handling ATM transactions.
- **atm_client.c**: Implements the ATM's logic for interacting with the Bank, allowing users to deposit, withdraw, and view their account balance.
- **README.md**: Project description and instructions.

## Installation

1. **Clone the repository** (if applicable):
   ```bash
   git clone <repository_url>
