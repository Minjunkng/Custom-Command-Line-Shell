# Custom-Command-Line-Shell

A custom, robust command-line shell written in C, featuring basic command execution, variable management, process control, and a suite of useful built-in commands, including file utilities and basic networking capabilities. Created as an assignment for CSC209 for the University of Toronto.

---

## Features

**mysh** provides a powerful command-line interface with the following key features:

* **Command Execution:** Executes external programs using the system's `PATH`.
* **Built-in Commands:** Includes implementations for common shell commands.
* **Piping:** Supports piping output from one command to the input of another.
* **Variable Management:** Allows for the assignment and retrieval of custom shell variables (e.g., `VAR=value`).
* **Process/Job Control:** Basic functionality for managing running processes, including listing and killing jobs.
* **Input/Output Handling:** Robust tokenizing and I/O helpers with error display.
* **Networking Utilities:** Includes built-in commands to start a simple chat server or client.

---

## Building and Installation

This project is built using `make` and requires a standard C compiler (like GCC).

### Prerequisites

* `gcc`
* `make`

### Build Instructions

To compile the `mysh` executable, simply run `make` in the root directory:

```bash
make
