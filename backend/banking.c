#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SOCKET_CLOSE closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET_CLOSE close
#endif

#define MAX_USERS 50
#define MAX_TRANSACTIONS 20
#define DATA_FILE "bank_data.dat"
#define DEFAULT_SERVER_PORT 8080
#define RESPONSE_BUFFER_SIZE 8192

typedef struct {
    int accountNumber;
    char name[60];
    int pin;
    double balance;
    char transactions[MAX_TRANSACTIONS][120];
    int transactionCount;
} Account;

static Account accounts[MAX_USERS];
static int accountCount = 0;

static void readLine(const char *prompt, char *value, size_t size) {
    printf("%s", prompt);
    if (fgets(value, (int)size, stdin) == NULL) {
        value[0] = '\0';
        return;
    }
    value[strcspn(value, "\n")] = '\0';
}

static int readInt(const char *prompt) {
    char input[64];
    char *end;
    long value;
    while (1) {
        readLine(prompt, input, sizeof(input));
        value = strtol(input, &end, 10);
        if (end != input && *end == '\0') return (int)value;
        printf("Invalid number. Please try again.\n");
    }
}

static double readAmount(const char *prompt) {
    char input[64];
    char *end;
    double value;
    while (1) {
        readLine(prompt, input, sizeof(input));
        value = strtod(input, &end);
        if (end != input && *end == '\0' && value > 0) return value;
        printf("Enter a valid amount greater than zero.\n");
    }
}

static void addTransaction(Account *account, const char *message) {
    if (account->transactionCount < MAX_TRANSACTIONS) {
        snprintf(account->transactions[account->transactionCount], sizeof(account->transactions[0]), "%s", message);
        account->transactionCount++;
        return;
    }

    memmove(account->transactions, account->transactions[1], sizeof(account->transactions[0]) * (MAX_TRANSACTIONS - 1));
    snprintf(account->transactions[MAX_TRANSACTIONS - 1], sizeof(account->transactions[0]), "%s", message);
}

static void saveData(void) {
    FILE *file = fopen(DATA_FILE, "wb");
    if (file == NULL) {
        printf("Warning: Could not save banking data.\n");
        return;
    }
    fwrite(&accountCount, sizeof(accountCount), 1, file);
    fwrite(accounts, sizeof(Account), accountCount, file);
    fclose(file);
}

static void loadData(void) {
    FILE *file = fopen(DATA_FILE, "rb");
    if (file != NULL) {
        fread(&accountCount, sizeof(accountCount), 1, file);
        if (accountCount < 0 || accountCount > MAX_USERS ||
            fread(accounts, sizeof(Account), accountCount, file) != (size_t)accountCount) {
            accountCount = 0;
        }
        fclose(file);
    }

    if (accountCount == 0) {
        Account initialAccounts[] = {
            {1001, "Rahul", 1234, 5000.0, {{0}}, 0},
            {1002, "Amit", 5678, 8000.0, {{0}}, 0},
            {1003, "Neha", 9876, 10000.0, {{0}}, 0}
        };
        accountCount = 3;
        memcpy(accounts, initialAccounts, sizeof(initialAccounts));
        addTransaction(&accounts[0], "Opening balance: Rs. 5000.00");
        addTransaction(&accounts[1], "Opening balance: Rs. 8000.00");
        addTransaction(&accounts[2], "Opening balance: Rs. 10000.00");
        saveData();
    }
}

static int findAccount(int accountNumber) {
    int i;
    for (i = 0; i < accountCount; i++) {
        if (accounts[i].accountNumber == accountNumber) return i;
    }
    return -1;
}

static void createAccount(void) {
    Account *account;
    if (accountCount >= MAX_USERS) {
        printf("Account limit reached.\n");
        return;
    }

    account = &accounts[accountCount];
    account->accountNumber = 1001 + accountCount;
    while (findAccount(account->accountNumber) != -1) account->accountNumber++;

    do {
        readLine("Enter your full name: ", account->name, sizeof(account->name));
    } while (account->name[0] == '\0');

    do {
        account->pin = readInt("Set a 4-digit PIN: ");
        if (account->pin < 1000 || account->pin > 9999) {
            printf("PIN must contain exactly 4 digits.\n");
        }
    } while (account->pin < 1000 || account->pin > 9999);

    account->balance = 0.0;
    account->transactionCount = 0;
    addTransaction(account, "Account opened: Rs. 0.00");
    accountCount++;
    saveData();
    printf("Account created. Your account number is %d.\n", account->accountNumber);
}

static void showStatement(const Account *account) {
    int i;
    printf("\n--- Mini Statement for %s (%d) ---\n", account->name, account->accountNumber);
    if (account->transactionCount == 0) {
        printf("No transactions yet.\n");
        return;
    }
    for (i = 0; i < account->transactionCount; i++) {
        printf("%d. %s\n", i + 1, account->transactions[i]);
    }
}

static void deposit(Account *account) {
    double amount = readAmount("Enter deposit amount: Rs. ");
    char message[120];
    account->balance += amount;
    snprintf(message, sizeof(message), "Deposited Rs. %.2f | Balance: Rs. %.2f", amount, account->balance);
    addTransaction(account, message);
    saveData();
    printf("Deposit successful. New balance: Rs. %.2f\n", account->balance);
}

static void withdraw(Account *account) {
    double amount = readAmount("Enter withdrawal amount: Rs. ");
    char message[120];
    if (amount > account->balance) {
        printf("Insufficient balance. Available: Rs. %.2f\n", account->balance);
        return;
    }

    account->balance -= amount;
    snprintf(message, sizeof(message), "Withdrawn Rs. %.2f | Balance: Rs. %.2f", amount, account->balance);
    addTransaction(account, message);
    saveData();
    printf("Withdrawal successful. New balance: Rs. %.2f\n", account->balance);
}

static void transfer(Account *sender) {
    int receiverNumber = readInt("Enter receiver account number: ");
    int receiverIndex = findAccount(receiverNumber);
    double amount;
    char senderMessage[120];
    char receiverMessage[120];

    if (receiverIndex == -1 || receiverNumber == sender->accountNumber) {
        printf("Receiver account not found.\n");
        return;
    }

    amount = readAmount("Enter transfer amount: Rs. ");
    if (amount > sender->balance) {
        printf("Insufficient balance. Available: Rs. %.2f\n", sender->balance);
        return;
    }

    sender->balance -= amount;
    accounts[receiverIndex].balance += amount;
    snprintf(senderMessage, sizeof(senderMessage), "Transferred Rs. %.2f to A/C %d | Balance: Rs. %.2f",
             amount, receiverNumber, sender->balance);
    snprintf(receiverMessage, sizeof(receiverMessage), "Received Rs. %.2f from A/C %d | Balance: Rs. %.2f",
             amount, sender->accountNumber, accounts[receiverIndex].balance);
    addTransaction(sender, senderMessage);
    addTransaction(&accounts[receiverIndex], receiverMessage);
    saveData();
    printf("Transfer successful to %s.\n", accounts[receiverIndex].name);
}

static void changePin(Account *account) {
    int newPin = readInt("Enter new 4-digit PIN: ");
    if (newPin < 1000 || newPin > 9999) {
        printf("PIN must contain exactly 4 digits.\n");
        return;
    }

    account->pin = newPin;
    addTransaction(account, "Security alert: PIN changed");
    saveData();
    printf("PIN changed successfully.\n");
}

static void accountMenu(int index) {
    int choice;
    Account *account = &accounts[index];
    do {
        printf("\n--- Smart Banking Dashboard ---\n");
        printf("Welcome, %s | A/C: %d\n", account->name, account->accountNumber);
        printf("1. Check balance\n2. Deposit money\n3. Withdraw money\n");
        printf("4. Transfer money\n5. Mini statement\n6. Change PIN\n7. Logout\n");
        choice = readInt("Choose an option: ");

        switch (choice) {
            case 1: printf("Available balance: Rs. %.2f\n", account->balance); break;
            case 2: deposit(account); break;
            case 3: withdraw(account); break;
            case 4: transfer(account); break;
            case 5: showStatement(account); break;
            case 6: changePin(account); break;
            case 7: printf("Logged out securely.\n"); break;
            default: printf("Invalid option.\n");
        }
    } while (choice != 7);
}

static void login(void) {
    int accountNumber = readInt("Enter account number: ");
    int index = findAccount(accountNumber);
    int attempts;
    int pin;

    if (index == -1) {
        printf("Account not found.\n");
        return;
    }

    for (attempts = 1; attempts <= 3; attempts++) {
        pin = readInt("Enter 4-digit PIN: ");
        if (pin == accounts[index].pin) {
            accountMenu(index);
            return;
        }
        printf("Incorrect PIN. Attempts left: %d\n", 3 - attempts);
    }

    printf("Account temporarily locked for this session.\n");
}

static void makeJsonString(const char *input, char *output, size_t outputSize) {
    size_t i = 0, j = 0;
    output[0] = '\0';
    if (outputSize == 0) return;
    output[j++] = '"';

    while (input[i] != '\0' && j + 2 < outputSize) {
        char c = input[i++];
        switch (c) {
            case '\\':
                output[j++] = '\\';
                output[j++] = '\\';
                break;
            case '"':
                output[j++] = '\\';
                output[j++] = '"';
                break;
            case '\n':
                output[j++] = '\\';
                output[j++] = 'n';
                break;
            case '\r':
                output[j++] = '\\';
                output[j++] = 'r';
                break;
            case '\t':
                output[j++] = '\\';
                output[j++] = 't';
                break;
            default:
                output[j++] = c;
                break;
        }
    }

    output[j++] = '"';
    output[j] = '\0';
}

static void buildTransactionsJson(const Account *account, char *output, size_t outputSize) {
    size_t i = 0;
    size_t offset = 0;
    output[0] = '\0';

    if (outputSize == 0) return;
    output[offset++] = '[';

    for (i = 0; i < (size_t)account->transactionCount; i++) {
        char text[160];
        char quoted[180];
        if (offset + 4 >= outputSize) break;
        if (i > 0) {
            output[offset++] = ',';
        }
        makeJsonString(account->transactions[i], quoted, sizeof(quoted));
        snprintf(text, sizeof(text), "%s", quoted);
        if (offset + strlen(text) + 1 >= outputSize) break;
        memcpy(output + offset, text, strlen(text));
        offset += strlen(text);
    }

    output[offset++] = ']';
    output[offset] = '\0';
}

static void buildAccountJson(const Account *account, char *output, size_t outputSize) {
    char nameJson[128];
    char transactionJson[800];
    makeJsonString(account->name, nameJson, sizeof(nameJson));
    buildTransactionsJson(account, transactionJson, sizeof(transactionJson));

    snprintf(output, outputSize,
             "{\"accountNumber\":%d,\"name\":%s,\"balance\":%.2f,\"transactions\":%s}",
             account->accountNumber,
             nameJson,
             account->balance,
             transactionJson);
}

static char *findJsonKeyValue(const char *json, const char *key) {
    char pattern[64];
    char *match;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    match = strstr(json, pattern);
    if (match == NULL) return NULL;
    match = strchr(match + strlen(pattern), ':');
    if (match == NULL) return NULL;
    return match + 1;
}

static double extractDoubleField(const char *json, const char *key) {
    char *value = findJsonKeyValue(json, key);
    if (value == NULL) return 0.0;
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    return strtod(value, NULL);
}

static int extractIntField(const char *json, const char *key) {
    char *value = findJsonKeyValue(json, key);
    if (value == NULL) return 0;
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    return (int)strtol(value, NULL, 10);
}

static void extractStringField(const char *json, const char *key, char *output, size_t outputSize) {
    char *value = findJsonKeyValue(json, key);
    char *end;
    size_t index = 0;

    output[0] = '\0';
    if (value == NULL || outputSize == 0) return;

    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    if (*value != '"') {
        snprintf(output, outputSize, "%s", value);
        return;
    }

    value++;
    end = strchr(value, '"');
    while (value != end && index + 1 < outputSize) {
        output[index++] = *value++;
    }
    output[index] = '\0';
}

static void splitPath(const char *path, char *endpoint, size_t endpointSize, char *query, size_t querySize) {
    const char *question = strchr(path, '?');
    size_t length;

    endpoint[0] = '\0';
    query[0] = '\0';

    if (question == NULL) {
        snprintf(endpoint, endpointSize, "%s", path);
        return;
    }

    length = (size_t)(question - path);
    if (length >= endpointSize) length = endpointSize - 1;
    memcpy(endpoint, path, length);
    endpoint[length] = '\0';

    snprintf(query, querySize, "%s", question + 1);
}

static int getQueryValue(const char *query, const char *name, char *output, size_t outputSize) {
    char prefix[64];
    char *start;
    char *end;
    size_t len;

    snprintf(prefix, sizeof(prefix), "%s=", name);
    start = strstr(query, prefix);
    if (start == NULL) return 0;

    start += strlen(prefix);
    end = strchr(start, '&');
    if (end == NULL) {
        len = strlen(start);
    } else {
        len = (size_t)(end - start);
    }

    if (len >= outputSize) len = outputSize - 1;
    memcpy(output, start, len);
    output[len] = '\0';
    return 1;
}

static void jsonResponse(int statusCode, const char *statusText, const char *body, char *response, size_t responseSize) {
    size_t bodyLength = strlen(body);
    snprintf(response, responseSize,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: application/json\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             "Access-Control-Allow-Headers: Content-Type\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             statusCode,
             statusText,
             bodyLength,
             body);
}

static int routeRequest(const char *method, const char *path, const char *body, char *response, size_t responseSize) {
    char endpoint[128];
    char query[256];
    char accountJson[500];
    char bodyJson[512];
    int statusCode = 200;
    const char *statusText = "OK";
    int accountIndex;

    splitPath(path, endpoint, sizeof(endpoint), query, sizeof(query));

    if (strcmp(method, "OPTIONS") == 0) {
        snprintf(response, responseSize, "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        return 1;
    }

    if (strcmp(method, "GET") == 0 && strcmp(endpoint, "/api/health") == 0) {
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"Banking API is alive\"}");
        jsonResponse(statusCode, statusText, bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "GET") == 0 && strcmp(endpoint, "/api/account") == 0) {
        char accountNumberText[32];
        int accountNumber = 0;
        if (!getQueryValue(query, "accountNumber", accountNumberText, sizeof(accountNumberText))) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"accountNumber is required\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        accountNumber = atoi(accountNumberText);
        accountIndex = findAccount(accountNumber);
        if (accountIndex == -1) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account not found\"}");
            jsonResponse(404, "Not Found", bodyJson, response, responseSize);
            return 1;
        }

        buildAccountJson(&accounts[accountIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "POST") == 0 && strcmp(endpoint, "/api/create-account") == 0) {
        char name[60];
        int pin = extractIntField(body, "pin");
        extractStringField(body, "name", name, sizeof(name));

        if (name[0] == '\0') {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Name is required\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        if (pin < 1000 || pin > 9999) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"PIN must be 4 digits\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        if (accountCount >= MAX_USERS) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account limit reached\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        accountIndex = accountCount;
        accounts[accountIndex].accountNumber = 1001 + accountIndex;
        while (findAccount(accounts[accountIndex].accountNumber) != -1) {
            accounts[accountIndex].accountNumber++;
        }
        snprintf(accounts[accountIndex].name, sizeof(accounts[accountIndex].name), "%s", name);
        accounts[accountIndex].pin = pin;
        accounts[accountIndex].balance = 0.0;
        accounts[accountIndex].transactionCount = 0;
        addTransaction(&accounts[accountIndex], "Account opened: Rs. 0.00");
        accountCount++;
        saveData();

        buildAccountJson(&accounts[accountIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"Account created successfully\",\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "POST") == 0 && strcmp(endpoint, "/api/login") == 0) {
        int accountNumber = extractIntField(body, "accountNumber");
        int pin = extractIntField(body, "pin");
        accountIndex = findAccount(accountNumber);

        if (accountNumber < 1000 || accountNumber > 9999 || pin < 1000 || pin > 9999) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account number and PIN must be 4 digits\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        if (accountIndex == -1 || accounts[accountIndex].pin != pin) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account number or PIN is incorrect\"}");
            jsonResponse(401, "Unauthorized", bodyJson, response, responseSize);
            return 1;
        }

        buildAccountJson(&accounts[accountIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"Login successful\",\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "POST") == 0 && strcmp(endpoint, "/api/change-pin") == 0) {
        int accountNumber = extractIntField(body, "accountNumber");
        int currentPin = extractIntField(body, "currentPin");
        int newPin = extractIntField(body, "newPin");
        accountIndex = findAccount(accountNumber);

        if (accountIndex == -1) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account not found\"}");
            jsonResponse(404, "Not Found", bodyJson, response, responseSize);
            return 1;
        }

        if (accounts[accountIndex].pin != currentPin) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Current PIN is incorrect\"}");
            jsonResponse(401, "Unauthorized", bodyJson, response, responseSize);
            return 1;
        }

        if (newPin < 1000 || newPin > 9999) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"PIN must be 4 digits\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        accounts[accountIndex].pin = newPin;
        addTransaction(&accounts[accountIndex], "Security alert: PIN changed");
        saveData();
        buildAccountJson(&accounts[accountIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"PIN changed successfully\",\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "POST") == 0 && strcmp(endpoint, "/api/deposit") == 0) {
        int accountNumber = extractIntField(body, "accountNumber");
        double amount = extractDoubleField(body, "amount");
        char message[120];
        accountIndex = findAccount(accountNumber);

        if (accountIndex == -1) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account not found\"}");
            jsonResponse(404, "Not Found", bodyJson, response, responseSize);
            return 1;
        }

        if (amount <= 0) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Amount must be greater than zero\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        accounts[accountIndex].balance += amount;
        snprintf(message, sizeof(message), "Cash deposit: Rs. %.2f | Balance: Rs. %.2f", amount, accounts[accountIndex].balance);
        addTransaction(&accounts[accountIndex], message);
        saveData();
        buildAccountJson(&accounts[accountIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"Deposit successful\",\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "POST") == 0 && strcmp(endpoint, "/api/withdraw") == 0) {
        int accountNumber = extractIntField(body, "accountNumber");
        double amount = extractDoubleField(body, "amount");
        char message[120];
        accountIndex = findAccount(accountNumber);

        if (accountIndex == -1) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Account not found\"}");
            jsonResponse(404, "Not Found", bodyJson, response, responseSize);
            return 1;
        }

        if (amount <= 0) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Amount must be greater than zero\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        if (amount > accounts[accountIndex].balance) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Insufficient balance\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        accounts[accountIndex].balance -= amount;
        snprintf(message, sizeof(message), "Cash withdrawal: Rs. %.2f | Balance: Rs. %.2f", amount, accounts[accountIndex].balance);
        addTransaction(&accounts[accountIndex], message);
        saveData();
        buildAccountJson(&accounts[accountIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"Withdrawal successful\",\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    if (strcmp(method, "POST") == 0 && strcmp(endpoint, "/api/transfer") == 0) {
        int senderAccountNumber = extractIntField(body, "accountNumber");
        int receiverAccountNumber = extractIntField(body, "receiverAccountNumber");
        double amount = extractDoubleField(body, "amount");
        char senderMessage[120];
        char receiverMessage[120];
        int senderIndex = findAccount(senderAccountNumber);
        int receiverIndex = findAccount(receiverAccountNumber);

        if (senderIndex == -1 || receiverIndex == -1 || senderAccountNumber == receiverAccountNumber) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Receiver account not found\"}");
            jsonResponse(404, "Not Found", bodyJson, response, responseSize);
            return 1;
        }

        if (amount <= 0) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Amount must be greater than zero\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        if (amount > accounts[senderIndex].balance) {
            snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Insufficient balance\"}");
            jsonResponse(400, "Bad Request", bodyJson, response, responseSize);
            return 1;
        }

        accounts[senderIndex].balance -= amount;
        accounts[receiverIndex].balance += amount;

        snprintf(senderMessage, sizeof(senderMessage), "Transferred Rs. %.2f to A/C %d | Balance: Rs. %.2f",
                 amount, receiverAccountNumber, accounts[senderIndex].balance);
        snprintf(receiverMessage, sizeof(receiverMessage), "Received Rs. %.2f from A/C %d | Balance: Rs. %.2f",
                 amount, senderAccountNumber, accounts[receiverIndex].balance);

        addTransaction(&accounts[senderIndex], senderMessage);
        addTransaction(&accounts[receiverIndex], receiverMessage);
        saveData();

        buildAccountJson(&accounts[senderIndex], accountJson, sizeof(accountJson));
        snprintf(bodyJson, sizeof(bodyJson), "{\"success\":true,\"message\":\"Transfer successful\",\"account\":%s}", accountJson);
        jsonResponse(200, "OK", bodyJson, response, responseSize);
        return 1;
    }

    snprintf(bodyJson, sizeof(bodyJson), "{\"success\":false,\"message\":\"Endpoint not found\"}");
    jsonResponse(404, "Not Found", bodyJson, response, responseSize);
    return 1;
}

static void handleClient(int clientSocket) {
    char request[4096];
    char response[RESPONSE_BUFFER_SIZE];
    char method[16];
    char path[256];
    char *headerEnd;
    char *bodyStart = "";
    int bytesRead;
    int totalBytes = 0;
    int contentLength = 0;

    memset(request, 0, sizeof(request));
    memset(response, 0, sizeof(response));

    bytesRead = recv(clientSocket, request, sizeof(request) - 1, 0);
    if (bytesRead <= 0) {
        SOCKET_CLOSE(clientSocket);
        return;
    }

    totalBytes = bytesRead;
    request[totalBytes] = '\0';
    headerEnd = strstr(request, "\r\n\r\n");

    if (headerEnd != NULL) {
        char *contentLengthHeader = strstr(request, "Content-Length:");
        if (contentLengthHeader != NULL) {
            contentLength = atoi(contentLengthHeader + strlen("Content-Length:"));
        }

        while ((int)(totalBytes - ((headerEnd + 4) - request)) < contentLength &&
               totalBytes < (int)sizeof(request) - 1) {
            bytesRead = recv(clientSocket, request + totalBytes, sizeof(request) - 1 - totalBytes, 0);
            if (bytesRead <= 0) break;
            totalBytes += bytesRead;
            request[totalBytes] = '\0';
        }
    }

    if (sscanf(request, "%15s %255s", method, path) != 2) {
        SOCKET_CLOSE(clientSocket);
        return;
    }

    if (headerEnd != NULL) {
        *headerEnd = '\0';
        bodyStart = headerEnd + 4;
        while (*bodyStart == '\r' || *bodyStart == '\n' || *bodyStart == ' ' || *bodyStart == '\t') {
            bodyStart++;
        }
    }

    if (strcmp(method, "POST") == 0) {
        routeRequest(method, path, bodyStart, response, sizeof(response));
    } else {
        routeRequest(method, path, "", response, sizeof(response));
    }

    send(clientSocket, response, strlen(response), 0);
    SOCKET_CLOSE(clientSocket);
}

static void startServer(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Windows socket startup failed.\n");
        return;
    }
#endif

    const char *portEnv = getenv("PORT");
    int serverPort = portEnv && portEnv[0] != '\0' ? atoi(portEnv) : DEFAULT_SERVER_PORT;
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddress;
    int opt = 1;
    int clientSocket;

    if (serverPort <= 0) {
        serverPort = DEFAULT_SERVER_PORT;
    }

    if (serverSocket < 0) {
        printf("Socket creation failed.\n");
        return;
    }

    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(serverPort);

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        printf("Socket bind failed on port %d.\n", serverPort);
        SOCKET_CLOSE(serverSocket);
        return;
    }

    if (listen(serverSocket, 10) < 0) {
        printf("Socket listen failed.\n");
        SOCKET_CLOSE(serverSocket);
        return;
    }

    printf("ATM backend server is running on http://localhost:%d\n", serverPort);
    printf("Press Ctrl+C to stop the server.\n");

    while (1) {
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket < 0) {
            continue;
        }
        handleClient(clientSocket);
    }
}

int main(int argc, char *argv[]) {
    int choice;

    loadData();

    if (argc > 1 && strcmp(argv[1], "--server") == 0) {
        startServer();
        return 0;
    }

    printf("\n=== SMART BANKING SYSTEM ===\n");
    do {
        printf("\n1. Login\n2. Open new account\n3. Exit\n");
        choice = readInt("Choose an option: ");
        switch (choice) {
            case 1: login(); break;
            case 2: createAccount(); break;
            case 3: printf("Thank you for using Smart Banking System.\n"); break;
            default: printf("Invalid option.\n");
        }
    } while (choice != 3);
    return 0;
}