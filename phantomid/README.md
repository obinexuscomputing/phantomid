# PhantomID C Package

The PhantomID C package is a library for managing an anonymous account management system. It provides a way to create, delete, and manage accounts in a tree-like structure, as well as send messages between accounts.

## API Usage

To use the PhantomID C package, you can follow these steps:

1. Build the library and executable:
   ```
   make
   ```
   This will create the `libphantomid.so` shared library, `libphantomid.a` static library, and the `phantomid` executable.

2. Start the PhantomID daemon:
   ```
   ./phantomid -p 8888
   ```
   This will start the PhantomID daemon and listen on port 8888.

3. Use `netcat` to interact with the daemon:
   - Create a new account (under a parent account):
     ```
     echo "create <parent_id>" | nc localhost 8888
     ```
   - Create a root account:
     ```
     echo "create" | nc localhost 8888
     ```
   - Delete an account:
     ```
     echo "delete <account_id>" | nc localhost 8888
     ```
   - Send a message between accounts:
     ```
     echo "msg <from_id> <to_id> <message>" | nc localhost 8888
     ```
   - List the tree structure:
     ```
     echo "list" | nc localhost 8888
     echo "list bfs" | nc localhost 8888
     echo "list dfs" | nc localhost 8888
     ```
   - Get help:
     ```
     echo "help" | nc localhost 8888
     ```

For more details on the API, please refer to the `phantomid.h` header file.

## Dependencies

The PhantomID C package has the following dependencies:

- OpenSSL library (`libssl-dev`)
- POSIX Threads library (`libpthread`)

Make sure you have these dependencies installed on your system before building and running the project.