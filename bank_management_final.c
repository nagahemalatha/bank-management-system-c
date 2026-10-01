/*
    BANK MANAGEMENT SYSTEM - COMPACT VERSION

    Features:
    1. Customer registration and login
    2. Password hashing (educational hash)
    3. Transaction PIN
    4. OTP verification for large transactions
    5. Deposit / Withdraw / Transfer
    6. Bill payment
    7. Daily transfer limit
    8. Transaction history
    9. CSV statement
    10. Savings interest
    11. Loan EMI calculator
    12. FD / RD calculator
    13. Update profile
    14. Change password
    15. Admin dashboard and reports
    16. Account lock / unlock
    17. Audit log

    Compile:
    gcc bank_management_final.c -o bank.exe -lm

    Note:
    The password hash below is for learning/project demonstration.
    Real banking software should use a modern password-hashing algorithm
    such as Argon2, bcrypt or scrypt.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define ACC_FILE "accounts_secure.dat"
#define TXN_FILE "transactions_secure.dat"
#define AUDIT_FILE "audit.log"

#define MIN_BALANCE 500.0
#define DAILY_LIMIT 50000.0
#define OTP_LIMIT 10000.0

typedef struct {
    long no;
    char name[50];
    char username[30];
    unsigned long passwordHash;
    unsigned long passwordSalt;
    unsigned long pinHash;
    char phone[20];
    char type[15];
    double balance;
    int active;
    int locked;
} Account;

typedef struct {
    long id;
    long account;
    long other;
    char type[20];
    char date[25];
    double amount;
    double balance;
} Transaction;

/* ---------- INPUT HELPERS ---------- */

void clearInput(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void text(char *message, char *value, int size) {
    printf("%s", message);
    fgets(value, size, stdin);
    value[strcspn(value, "\n")] = '\0';
}

double amount(char *message) {
    double x;

    while (1) {
        printf("%s", message);

        if (scanf("%lf", &x) == 1) {
            clearInput();
            return x;
        }

        clearInput();
        printf("Enter a valid amount.\n");
    }
}

long number(char *message) {
    long x;

    while (1) {
        printf("%s", message);

        if (scanf("%ld", &x) == 1) {
            clearInput();
            return x;
        }

        clearInput();
        printf("Enter a valid number.\n");
    }
}

void pauseScreen(void) {
    char x[5];

    printf("\nPress Enter to continue...");
    fgets(x, sizeof(x), stdin);
}

/* ---------- PASSWORD HASH ---------- */

/*
   Simple educational hash.
   It does NOT provide production-level password security.
*/
unsigned long hashPassword(const char *password, unsigned long salt) {
    unsigned long hash = 2166136261u ^ salt;
    int c;

    while ((c = (unsigned char)*password++)) {
        hash ^= (unsigned long)c + salt;
        hash *= 16777619u;
        hash ^= hash >> 13;
    }

    hash ^= salt * 2654435761u;
    hash ^= hash >> 16;
    return hash;
}

unsigned long makeSalt(void) {
    unsigned long a = (unsigned long)rand();
    unsigned long b = (unsigned long)rand();
    return ((unsigned long)time(NULL) << 16) ^ (a << 8) ^ b ^ 0xA5A5A5A5u;
}

int strongPassword(const char *p) {
    int upper=0, lower=0, digit=0, special=0;
    size_t i;

    if (strlen(p) < 8)
        return 0;

    for (i=0; p[i]; i++) {
        if (p[i] >= 'A' && p[i] <= 'Z') upper=1;
        else if (p[i] >= 'a' && p[i] <= 'z') lower=1;
        else if (p[i] >= '0' && p[i] <= '9') digit=1;
        else special=1;
    }

    return upper && lower && digit && special;
}

int validPIN(const char *pin) {
    size_t i, n=strlen(pin);
    if (n < 4 || n > 6) return 0;
    for (i=0; i<n; i++)
        if (pin[i] < '0' || pin[i] > '9') return 0;
    return 1;
}

/* ---------- DATE / LOG ---------- */

void dateNow(char *date) {
    time_t now = time(NULL);
    strftime(date, 25, "%d-%m-%Y %H:%M:%S", localtime(&now));
}

void audit(const char *user, const char *action) {
    FILE *f = fopen(AUDIT_FILE, "a");
    char date[25];

    if (!f)
        return;

    dateNow(date);
    fprintf(f, "%s | %s | %s\n", date, user, action);
    fclose(f);
}

/* ---------- ACCOUNT FILE ---------- */

long nextAccount(void) {
    FILE *f;
    long n = 100001;

    f = fopen("next_account.dat", "r");

    if (f) {
        fscanf(f, "%ld", &n);
        fclose(f);
    }

    f = fopen("next_account.dat", "w");

    if (f) {
        fprintf(f, "%ld", n + 1);
        fclose(f);
    }

    return n;
}

int findAccount(long no, Account *account) {
    FILE *f = fopen(ACC_FILE, "rb");

    if (!f)
        return 0;

    while (fread(account, sizeof(Account), 1, f)) {
        if (account->no == no) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

int findUser(const char *username, Account *account) {
    FILE *f = fopen(ACC_FILE, "rb");

    if (!f)
        return 0;

    while (fread(account, sizeof(Account), 1, f)) {
        if (strcmp(account->username, username) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

int saveAccount(Account *account) {
    FILE *f = fopen(ACC_FILE, "ab");

    if (!f)
        return 0;

    fwrite(account, sizeof(Account), 1, f);
    fclose(f);

    return 1;
}

int updateAccount(Account *updated) {
    FILE *f = fopen(ACC_FILE, "r+b");
    Account account;

    if (!f)
        return 0;

    while (fread(&account, sizeof(Account), 1, f)) {
        if (account.no == updated->no) {
            fseek(f, -(long)sizeof(Account), SEEK_CUR);
            fwrite(updated, sizeof(Account), 1, f);
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

/* ---------- TRANSACTIONS ---------- */

long nextTransactionID(void) {
    static long id = 1000;
    return ++id;
}

void addTransaction(Account *account,
                    const char *type,
                    double money,
                    long other) {
    FILE *f = fopen(TXN_FILE, "ab");
    Transaction t;

    if (!f)
        return;

    t.id = nextTransactionID();
    t.account = account->no;
    t.other = other;
    strcpy(t.type, type);
    t.amount = money;
    t.balance = account->balance;
    dateNow(t.date);

    fwrite(&t, sizeof(Transaction), 1, f);
    fclose(f);
}

/* ---------- OTP ---------- */

/*
   OTP is simulated by displaying it on the screen.
   A real application would send it by SMS/email.
*/
int verifyOTP(void) {
    int otp = 100000 + rand() % 900000;
    int entered, attempts;

    printf("\n--------------------------------\n");
    printf("OTP (SIMULATION): %06d\n", otp);
    printf("OTP is valid for this verification only.\n");
    printf("--------------------------------\n");

    for (attempts=1; attempts<=3; attempts++) {
        entered = (int)number("Enter OTP: ");
        if (entered == otp) {
            printf("OTP verified successfully.\n");
            return 1;
        }
        printf("Wrong OTP. Attempts left: %d\n", 3-attempts);
    }

    printf("OTP verification failed.\n");
    return 0;
}

int checkPIN(Account *account);

int verifyTransactionSecurity(Account *account, double money) {
    if (!checkPIN(account))
        return 0;

    if (money >= OTP_LIMIT) {
        printf("Large transaction detected.\n");

        if (!verifyOTP())
            return 0;
    }

    return 1;
}

/* ---------- CREATE ACCOUNT ---------- */

void registerAccount(void) {
    Account a;
    int choice;
    char password[50];

    memset(&a, 0, sizeof(a));

    printf("\n========== CREATE ACCOUNT ==========\n");

    text("Name: ", a.name, sizeof(a.name));
    text("Username: ", a.username, sizeof(a.username));

    if (findUser(a.username, &a)) {
        printf("Username already exists.\n");
        return;
    }

    do {
        text("Password (8+ chars, upper/lower/digit/special): ", password, sizeof(password));
        if (!strongPassword(password))
            printf("Weak password. Example: Bank@1234\n");
    } while (!strongPassword(password));

    a.passwordSalt = makeSalt();
    a.passwordHash = hashPassword(password, a.passwordSalt);

    do {
        text("Transaction PIN (4-6 digits): ", password, sizeof(password));
        if (!validPIN(password))
            printf("PIN must contain only 4-6 digits.\n");
    } while (!validPIN(password));
    a.pinHash = hashPassword(password, a.passwordSalt ^ 0x13579BDFu);
    text("Phone: ", a.phone, sizeof(a.phone));

    printf("\n1. Savings\n");
    printf("2. Current\n");

    choice = (int)number("Account Type: ");

    if (choice == 1)
        strcpy(a.type, "Savings");
    else if (choice == 2)
        strcpy(a.type, "Current");
    else {
        printf("Invalid account type.\n");
        return;
    }

    a.no = nextAccount();
    a.balance = 0;
    a.active = 1;
    a.locked = 0;

    if (saveAccount(&a)) {
        printf("\nAccount created successfully!\n");
        printf("Account Number: %ld\n", a.no);
        audit(a.username, "Account created");
    } else {
        printf("Could not create account.\n");
    }
}

/* ---------- LOGIN ---------- */

int login(Account *account) {
    char username[30];
    char password[50];
    int attempts = 0;

    printf("\n========== CUSTOMER LOGIN ==========\n");

    text("Username: ", username, sizeof(username));

    if (!findUser(username, account)) {
        printf("User not found.\n");
        return 0;
    }

    if (!account->active) {
        printf("Account is inactive.\n");
        return 0;
    }

    if (account->locked) {
        printf("Account is locked. Contact admin.\n");
        return 0;
    }

    while (attempts < 3) {
        text("Password: ", password, sizeof(password));

        if (hashPassword(password, account->passwordSalt) == account->passwordHash) {
            printf("Login successful.\n");
            audit(account->username, "Customer login");
            return 1;
        }

        attempts++;
        printf("Wrong password. Attempts left: %d\n",
               3 - attempts);
    }

    account->locked = 1;
    updateAccount(account);

    printf("Account locked after 3 failed attempts.\n");
    audit(account->username, "Account locked");

    return 0;
}

/* ---------- PIN ---------- */

int checkPIN(Account *account) {
    char pin[20];

    text("Transaction PIN: ", pin, sizeof(pin));

    if (hashPassword(pin, account->passwordSalt ^ 0x13579BDFu) == account->pinHash)
        return 1;

    printf("Incorrect transaction PIN.\n");
    return 0;
}

/* ---------- ACCOUNT DETAILS ---------- */

void showBalance(Account *account) {
    printf("\n========== ACCOUNT DETAILS ==========\n");
    printf("Account Number : %ld\n", account->no);
    printf("Name           : %s\n", account->name);
    printf("Username       : %s\n", account->username);
    printf("Phone          : %s\n", account->phone);
    printf("Account Type   : %s\n", account->type);
    printf("Balance        : Rs. %.2lf\n", account->balance);
}

/* ---------- DEPOSIT ---------- */

void deposit(Account *account) {
    double money = amount("Deposit Amount: ");

    if (money <= 0) {
        printf("Invalid amount.\n");
        return;
    }

    account->balance += money;

    if (updateAccount(account)) {
        addTransaction(account, "DEPOSIT", money, 0);
        audit(account->username, "Deposit");
        printf("Deposit successful.\n");
        printf("Balance: Rs. %.2lf\n", account->balance);
    }
}

/* ---------- WITHDRAW ---------- */

void withdraw(Account *account) {
    double money = amount("Withdrawal Amount: ");

    if (money <= 0 || money > account->balance) {
        printf("Invalid amount or insufficient balance.\n");
        return;
    }

    if (strcmp(account->type, "Savings") == 0 &&
        account->balance - money < MIN_BALANCE) {
        printf("Minimum savings balance is Rs. %.2lf\n",
               MIN_BALANCE);
        return;
    }

    if (!verifyTransactionSecurity(account, money))
        return;

    account->balance -= money;

    updateAccount(account);
    addTransaction(account, "WITHDRAW", money, 0);
    audit(account->username, "Withdrawal");

    printf("Withdrawal successful.\n");
    printf("Balance: Rs. %.2lf\n", account->balance);
}

/* ---------- DAILY TRANSFER ---------- */

double todayTransfers(long accountNo) {
    FILE *f = fopen(TXN_FILE, "rb");
    Transaction t;
    char today[25];
    double total = 0;

    if (!f)
        return 0;

    dateNow(today);

    while (fread(&t, sizeof(t), 1, f)) {
        if (t.account == accountNo &&
            strcmp(t.type, "TRANSFER") == 0 &&
            strncmp(t.date, today, 10) == 0) {
            total += t.amount;
        }
    }

    fclose(f);
    return total;
}

/* ---------- TRANSFER ---------- */

void transfer(Account *sender) {
    Account receiver;
    long receiverNo;
    double money;
    double sentToday;

    receiverNo = number("Recipient Account Number: ");

    if (receiverNo == sender->no ||
        !findAccount(receiverNo, &receiver) ||
        !receiver.active) {
        printf("Invalid recipient account.\n");
        return;
    }

    money = amount("Transfer Amount: ");

    if (money <= 0 || money > sender->balance) {
        printf("Invalid amount or insufficient balance.\n");
        return;
    }

    if (sender->balance - money < MIN_BALANCE &&
        strcmp(sender->type, "Savings") == 0) {
        printf("Minimum savings balance would be violated.\n");
        return;
    }

    sentToday = todayTransfers(sender->no);

    if (sentToday + money > DAILY_LIMIT) {
        printf("Daily transfer limit of Rs. %.2lf exceeded.\n",
               DAILY_LIMIT);
        return;
    }

    if (!verifyTransactionSecurity(sender, money))
        return;

    sender->balance -= money;
    receiver.balance += money;

    if (!updateAccount(sender) || !updateAccount(&receiver)) {
        printf("Transfer could not be completed.\n");
        return;
    }

    addTransaction(sender, "TRANSFER", money, receiver.no);
    audit(sender->username, "Fund transfer");

    printf("Transfer successful!\n");
    printf("New Balance: Rs. %.2lf\n", sender->balance);
}

/* ---------- BILL PAYMENT ---------- */

void billPayment(Account *account) {
    int choice;
    double money;
    char bill[20];

    printf("\n========== BILL PAYMENT ==========\n");
    printf("1. Electricity\n");
    printf("2. Water\n");
    printf("3. Mobile\n");
    printf("4. Internet\n");

    choice = (int)number("Select Bill: ");

    if (choice == 1)
        strcpy(bill, "ELECTRICITY");
    else if (choice == 2)
        strcpy(bill, "WATER");
    else if (choice == 3)
        strcpy(bill, "MOBILE");
    else if (choice == 4)
        strcpy(bill, "INTERNET");
    else {
        printf("Invalid bill type.\n");
        return;
    }

    money = amount("Bill Amount: ");

    if (money <= 0 || money > account->balance) {
        printf("Invalid amount or insufficient balance.\n");
        return;
    }

    if (strcmp(account->type, "Savings") == 0 &&
        account->balance - money < MIN_BALANCE) {
        printf("Minimum savings balance would be violated.\n");
        return;
    }

    if (!verifyTransactionSecurity(account, money))
        return;

    account->balance -= money;
    updateAccount(account);
    addTransaction(account, bill, money, 0);
    audit(account->username, "Bill payment");

    printf("%s bill paid successfully.\n", bill);
    printf("Balance: Rs. %.2lf\n", account->balance);
}

/* ---------- TRANSACTION HISTORY ---------- */

void history(Account *account) {
    FILE *f = fopen(TXN_FILE, "rb");
    Transaction t;
    int found = 0;

    printf("\n========== TRANSACTION HISTORY ==========\n");

    if (!f) {
        printf("No transactions found.\n");
        return;
    }

    while (fread(&t, sizeof(t), 1, f)) {
        if (t.account == account->no) {
            printf("ID:%ld | %-12s | Rs.%.2lf | "
                   "Balance:Rs.%.2lf | %s\n",
                   t.id, t.type, t.amount,
                   t.balance, t.date);
            found = 1;
        }
    }

    fclose(f);

    if (!found)
        printf("No transactions found.\n");
}

/* ---------- CSV EXPORT ---------- */

void exportCSV(Account *account) {
    FILE *f = fopen(TXN_FILE, "rb");
    FILE *out;
    Transaction t;
    char filename[60];

    sprintf(filename, "statement_%ld.csv", account->no);

    out = fopen(filename, "w");

    if (!out) {
        printf("Could not create CSV file.\n");
        if (f)
            fclose(f);
        return;
    }

    fprintf(out,
            "ID,Type,Amount,Balance,OtherAccount,Date\n");

    if (f) {
        while (fread(&t, sizeof(t), 1, f)) {
            if (t.account == account->no) {
                fprintf(out, "%ld,%s,%.2lf,%.2lf,%ld,%s\n",
                        t.id, t.type, t.amount,
                        t.balance, t.other, t.date);
            }
        }

        fclose(f);
    }

    fclose(out);

    printf("CSV statement created: %s\n", filename);
}

/* ---------- INTEREST ---------- */

void interest(Account *account) {
    double interestAmount;

    if (strcmp(account->type, "Savings") != 0) {
        printf("Interest calculation is for Savings accounts.\n");
        return;
    }

    interestAmount = account->balance * 4.0 / 100;

    printf("Current Balance: Rs. %.2lf\n",
           account->balance);
    printf("Annual Interest Rate: 4%%\n");
    printf("Estimated Annual Interest: Rs. %.2lf\n",
           interestAmount);
}

/* ---------- EMI CALCULATOR ---------- */

double calculateEMI(double principal,
                    double annualRate,
                    int months) {
    double monthlyRate = annualRate / 12 / 100;

    if (monthlyRate == 0)
        return principal / months;

    return principal * monthlyRate *
           pow(1 + monthlyRate, months) /
           (pow(1 + monthlyRate, months) - 1);
}

void loanCalculator(void) {
    double principal;
    double rate;
    double emiValue;
    int months;

    printf("\n========== LOAN EMI CALCULATOR ==========\n");

    principal = amount("Loan Amount: ");
    rate = amount("Annual Interest Rate (%): ");
    months = (int)number("Tenure (months): ");

    if (principal <= 0 || rate < 0 || months <= 0) {
        printf("Invalid loan details.\n");
        return;
    }

    emiValue = calculateEMI(principal, rate, months);

    printf("\nMonthly EMI     : Rs. %.2lf\n", emiValue);
    printf("Total Repayment : Rs. %.2lf\n",
           emiValue * months);
}

/* ---------- FD / RD ---------- */

void depositCalculator(void) {
    int choice;

    printf("\n========== FD / RD CALCULATOR ==========\n");
    printf("1. Fixed Deposit\n");
    printf("2. Recurring Deposit\n");

    choice = (int)number("Select: ");

    if (choice == 1) {
        double principal = amount("FD Amount: ");
        double rate = amount("Annual Rate (%): ");
        int months = (int)number("Duration (months): ");

        if (principal <= 0 || rate < 0 || months <= 0) {
            printf("Invalid details.\n");
            return;
        }

        printf("Estimated Maturity: Rs. %.2lf\n",
               principal +
               principal * rate * months / 12 / 100);
    }
    else if (choice == 2) {
        double monthly = amount("Monthly RD Amount: ");
        double rate = amount("Annual Rate (%): ");
        int months = (int)number("Duration (months): ");

        if (monthly <= 0 || rate < 0 || months <= 0) {
            printf("Invalid details.\n");
            return;
        }

        printf("Estimated Maturity: Rs. %.2lf\n",
               monthly * months +
               monthly * months * rate * months / 24 / 100);
    }
    else {
        printf("Invalid choice.\n");
    }
}

/* ---------- PROFILE UPDATE ---------- */

void updateProfile(Account *account) {
    char name[50];
    char phone[20];

    printf("\n========== UPDATE PROFILE ==========\n");

    text("New Name: ", name, sizeof(name));
    text("New Phone: ", phone, sizeof(phone));

    if (strlen(name) == 0 || strlen(phone) == 0) {
        printf("Name and phone cannot be empty.\n");
        return;
    }

    strcpy(account->name, name);
    strcpy(account->phone, phone);

    if (updateAccount(account)) {
        printf("Profile updated successfully.\n");
        audit(account->username, "Profile updated");
    }
}

/* ---------- CHANGE PASSWORD ---------- */

void changePassword(Account *account) {
    char oldPassword[50];
    char newPassword[50];
    char confirmPassword[50];

    printf("\n========== CHANGE PASSWORD ==========\n");

    text("Old Password: ", oldPassword, sizeof(oldPassword));

    if (hashPassword(oldPassword, account->passwordSalt) != account->passwordHash) {
        printf("Old password is incorrect.\n");
        return;
    }

    text("New Password: ", newPassword, sizeof(newPassword));
    text("Confirm Password: ",
         confirmPassword, sizeof(confirmPassword));

    if (!strongPassword(newPassword)) {
        printf("Password must be 8+ characters with upper, lower, digit and special character.\n");
        return;
    }

    if (strcmp(newPassword, confirmPassword) != 0) {
        printf("Passwords do not match.\n");
        return;
    }

    account->passwordSalt = makeSalt();
    account->passwordHash = hashPassword(newPassword, account->passwordSalt);
    updateAccount(account);

    printf("Password changed successfully.\n");
    audit(account->username, "Password changed");
}

/* ---------- CUSTOMER MENU ---------- */

void customerMenu(Account *account) {
    int choice;

    while (1) {
        printf("\n========================================\n");
        printf("          CUSTOMER DASHBOARD\n");
        printf("========================================\n");

        printf("Welcome: %s | A/C: %ld\n",
               account->name, account->no);
        printf("Balance: Rs. %.2lf\n\n",
               account->balance);

        printf("1. Account Details\n");
        printf("2. Deposit\n");
        printf("3. Withdraw\n");
        printf("4. Fund Transfer\n");
        printf("5. Bill Payment\n");
        printf("6. Transaction History\n");
        printf("7. Export CSV Statement\n");
        printf("8. Savings Interest\n");
        printf("9. Loan EMI Calculator\n");
        printf("10. FD / RD Calculator\n");
        printf("11. Update Profile\n");
        printf("12. Change Password\n");
        printf("13. Logout\n");

        choice = (int)number("Select: ");

        switch (choice) {
            case 1:
                showBalance(account);
                break;

            case 2:
                deposit(account);
                break;

            case 3:
                withdraw(account);
                break;

            case 4:
                transfer(account);
                break;

            case 5:
                billPayment(account);
                break;

            case 6:
                history(account);
                break;

            case 7:
                exportCSV(account);
                break;

            case 8:
                interest(account);
                break;

            case 9:
                loanCalculator();
                break;

            case 10:
                depositCalculator();
                break;

            case 11:
                updateProfile(account);
                break;

            case 12:
                changePassword(account);
                break;

            case 13:
                audit(account->username, "Customer logout");
                return;

            default:
                printf("Invalid choice.\n");
        }

        pauseScreen();
    }
}

/* ---------- ADMIN ---------- */

void showCustomers(void) {
    FILE *f = fopen(ACC_FILE, "rb");
    Account a;

    if (!f) {
        printf("No customers found.\n");
        return;
    }

    printf("\n========== CUSTOMERS ==========\n");

    while (fread(&a, sizeof(a), 1, f)) {
        printf("A/C:%ld | %-20s | %s | Rs.%.2lf | %s%s\n",
               a.no,
               a.name,
               a.type,
               a.balance,
               a.active ? "ACTIVE" : "INACTIVE",
               a.locked ? " | LOCKED" : "");
    }

    fclose(f);
}

void searchCustomer(void) {
    Account a;
    long no = number("Account Number: ");

    if (findAccount(no, &a)) {
        showBalance(&a);
    }
    else {
        printf("Customer not found.\n");
    }
}

void lockUnlock(void) {
    Account a;
    int choice;
    long no = number("Account Number: ");

    if (!findAccount(no, &a)) {
        printf("Account not found.\n");
        return;
    }

    printf("1. Lock\n");
    printf("2. Unlock\n");

    choice = (int)number("Select: ");

    if (choice == 1)
        a.locked = 1;
    else if (choice == 2)
        a.locked = 0;
    else {
        printf("Invalid choice.\n");
        return;
    }

    updateAccount(&a);
    audit("admin", "Account status changed");

    printf("Account status updated.\n");
}

/* ---------- ADMIN REPORT ---------- */

void adminReport(void) {
    FILE *f;
    FILE *t;
    Account a;
    Transaction tx;

    int customers = 0;
    int active = 0;
    int locked = 0;
    int transactions = 0;
    double totalBalance = 0;

    f = fopen(ACC_FILE, "rb");

    if (f) {
        while (fread(&a, sizeof(a), 1, f)) {
            customers++;
            if (a.active)
                active++;
            if (a.locked)
                locked++;

            totalBalance += a.balance;
        }

        fclose(f);
    }

    t = fopen(TXN_FILE, "rb");

    if (t) {
        while (fread(&tx, sizeof(tx), 1, t))
            transactions++;

        fclose(t);
    }

    printf("\n========== ADMIN REPORT ==========\n");
    printf("Total Customers   : %d\n", customers);
    printf("Active Accounts   : %d\n", active);
    printf("Locked Accounts   : %d\n", locked);
    printf("Total Bank Balance: Rs. %.2lf\n", totalBalance);
    printf("Total Transactions: %d\n", transactions);
}

/* ---------- AUDIT VIEW ---------- */

void viewAuditLog(void) {
    FILE *f = fopen(AUDIT_FILE, "r");
    char line[200];

    printf("\n========== AUDIT LOG ==========\n");

    if (!f) {
        printf("No audit records.\n");
        return;
    }

    while (fgets(line, sizeof(line), f))
        printf("%s", line);

    fclose(f);
}

/* ---------- ADMIN LOGIN ---------- */

int adminLogin(void) {
    char username[30];
    char password[50];

    printf("\n========== ADMIN LOGIN ==========\n");

    text("Username: ", username, sizeof(username));
    text("Password: ", password, sizeof(password));

    if (strcmp(username, "admin") == 0 &&
        hashPassword(password, 0xABCDEF01u) ==
        hashPassword("admin123", 0xABCDEF01u)) {
        printf("Admin login successful.\n");
        audit("admin", "Admin login");
        return 1;
    }

    printf("Invalid admin login.\n");
    return 0;
}

/* ---------- ADMIN MENU ---------- */

void adminMenu(void) {
    int choice;

    while (1) {
        printf("\n========================================\n");
        printf("             ADMIN DASHBOARD\n");
        printf("========================================\n");

        printf("1. View Customers\n");
        printf("2. Search Customer\n");
        printf("3. Lock / Unlock Account\n");
        printf("4. Admin Reports\n");
        printf("5. View Audit Log\n");
        printf("6. Logout\n");

        choice = (int)number("Select: ");

        switch (choice) {
            case 1:
                showCustomers();
                break;

            case 2:
                searchCustomer();
                break;

            case 3:
                lockUnlock();
                break;

            case 4:
                adminReport();
                break;

            case 5:
                viewAuditLog();
                break;

            case 6:
                return;

            default:
                printf("Invalid choice.\n");
        }

        pauseScreen();
    }
}

/* ---------- MAIN ---------- */

int main(void) {
    int choice;
    Account user;

    srand((unsigned int)(time(NULL) ^ (unsigned int)clock()));

    while (1) {
        printf("\n========================================\n");
        printf("       BANK MANAGEMENT SYSTEM\n");
        printf("========================================\n");

        printf("1. Create Account\n");
        printf("2. Customer Login\n");
        printf("3. Admin Login\n");
        printf("4. Exit\n");

        choice = (int)number("Select: ");

        switch (choice) {
            case 1:
                registerAccount();
                pauseScreen();
                break;

            case 2:
                if (login(&user))
                    customerMenu(&user);
                pauseScreen();
                break;

            case 3:
                if (adminLogin())
                    adminMenu();
                pauseScreen();
                break;

            case 4:
                printf("Thank you for using the Bank Management System.\n");
                return 0;

            default:
                printf("Invalid choice.\n");
        }
    }
}
