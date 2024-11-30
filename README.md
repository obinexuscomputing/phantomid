# PhantomID Project

The PhantomID project is a C-based anonymous account management system that allows you to create, delete, and manage accounts in a tree-like structure. It also includes the ability to send messages between accounts.

## Directory Structure

The project is organized as follows:

```
phantomid/
├── Makefile
├── include/
│   └── phantomid.h
├── src/
│   ├── main.c
│   ├── network.c
│   └── phantomid.c
├── lib/
└── README.md
```

## Building the Project

To build the project, follow these steps:

1. Ensure you have the necessary dependencies installed:
   - OpenSSL library (`libssl-dev`)
   - POSIX Threads library (`libpthread`)

2. Run the following command to build the shared library, static library, and executable:

   ```
   make
   ```

   This will create the following files:
   - `libphantomid.so`: Shared library
   - `libphantomid.a`: Static library
   - `phantomid`: Executable

## Running the PhantomID Daemon

To start the PhantomID daemon, run the following command:

```
./phantomid -p 8888
```

This will start the daemon and listen on port 8888.

## Interacting with the Daemon using `netcat`

You can use `netcat` to interact with the PhantomID daemon. Here are some example commands:

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

## TypeScript NPM Package

The project also includes a TypeScript NPM package for the PhantomID library. The package.json file for the TypeScript package is as follows:

```json
{
  "name": "@obinexuscomputing/phantomid",
  "version": "1.0.0",
  "main": "dist/index.js",
  "types": "dist/index.d.ts",
  "gypfile": true,
  "scripts": {
    "build": "node-gyp rebuild"
  },
  "dependencies": {
    "node-addon-api": "^5.0.0"
  }
}
```

To use the TypeScript package, you can install it using npm:

```
npm install @obinexuscomputing/phantomid
```

And then import the necessary functions from the package:

```typescript
import { phantomInit, phantomCleanup } from '@obinexuscomputing/phantomid';

// Use the bound functions in your code
phantomInit(/* arguments */);
phantomCleanup(/* arguments */);
```