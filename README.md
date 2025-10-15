# WisconsinShell (Wsh)

**wsh** is a custom Unix-style shell written in C that implements core UNIX concepts 
(fork-exec, pipes, path resolution)

## Features

wsh supports the following core functionalities:

- **Interactive and Batch Modes**: Run wsh as an interactive prompt or execute commands from a script file.
- **Command Execution**: Executes external commands by searching the `PATH` environment variable.
- **Piping**: Chains multiple commands together using the `|` operator, allowing for complex data processing pipelines.
- **Built-in Commands**: A robust set of internal commands that are handled directly by the shell without creating new processes:
  - `exit` - Terminates the shell session
  - `cd [path]` - Changes the current working directory. If no path is given, it changes to the `HOME` directory
  - `path [new_path]` - Displays or modifies the `PATH` environment variable
  - `alias [name = value]` - Creates or lists command aliases. Supports multi-level substitution and detects circular dependencies
  - `unalias <name>` - Removes a previously defined alias
  - `which <command>` - Shows whether a command is a built-in, an alias, or an external executable
  - `history [n]` - Displays the command history or executes the nth command from history

## Getting Started

Follow these instructions to build and run wsh on your local machine.

### Prerequisites

You'll need a C compiler (like `gcc` or `clang`) and `make` installed on your system.

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/saksham-2000/wisconsin-shell
   cd wisconsin-shell
   ```

2. **Compile the source code:**
   ```bash
   make clean
   make
   ```

3. **Run the shell:**

   - **Interactive Mode**: Launch the shell and get a prompt (wsh>)
     ```bash
     ./wsh
     ```

   - **Batch Mode**: Execute a file containing a series of shell commands
     ```bash
     ./wsh <script-file>.sh
     ```

### Usage Examples

Here are some examples of what you can do with wsh:

- **Execute an external command:**
  ```bash
  wsh> ls -l /tmp
  ```

- **Use pipes to combine commands:**
  ```bash
  wsh> ls -l | grep .c | wc -l
  wsh> path | tr 'a-z' 'A-Z'
  wsh> echo Hello World' | rev | tr 'A-Z' 'a-z' | awk '{print $2, $1}
  ```

- **Create and use an alias:**
  ```bash
  wsh> alias ll = 'ls -alF'
  wsh> ll
  ```

- **Modify the PATH:**
  ```bash
  wsh> path /bin:/usr/bin:/usr/local/bin
  ```

- **Check command type:**
  ```bash
  wsh> which cd
  cd: shell built-in command
  wsh> which grep
  grep: /usr/bin/grep
  ```

- **View command history:**
  ```bash
  wsh> history
  1: ls -l
  2: cd /tmp
  3: echo "Hello World"
  ```

## Architecture

The shell's logic is organized into several key components:

- **Main Loop**: Entry point that determines whether to run in interactive or batch mode
- **Parser**: Robust command-line parser that tokenizes input, respects quoted strings, and handles special characters
- **Command Executor**: Implements the fork-exec model for creating child processes
- **Built-in Handler**: Dispatcher that identifies and executes built-in commands without process creation
- **Path Resolution**: Utility functions that search `PATH` directories to locate executables
- **Alias Management**: Hash map-based system for storing and resolving command aliases with circular dependency detection
- **History Management**: Dynamic array for storing and retrieving command history
- **Process Management**: Parent-child process coordination using `fork()`, `exec()`, and `wait()` system calls

### Key System Calls Used

- `fork()` - Create child processes
- `exec()` family - Replace process image with new program
- `wait()` / `waitpid()` - Parent process waits for child completion
- `pipe()` - Create inter-process communication channels
- `dup2()` - Duplicate file descriptors for I/O redirection
- `chdir()` - Change working directory

### Data Structures

The shell leverages custom data structures for efficient operation:
- **HashMap**: For managing aliases with O(1) lookup time
- **Dynamic Array**: For storing command history with automatic resizing

## Technical Highlights

- **Language**: C
- **Memory Management**: Dynamic allocation with comprehensive leak prevention (verified with Valgrind)
- **Error Handling**: Robust error detection with user-friendly messages
- **Process Control**: Efficient parent-child coordination and signal handling
- **Parsing**: Custom tokenizer handling edge cases, quotes, and special characters
- **Alias System**: Multi-level substitution with circular dependency detection
- **Path Resolution**: Efficient command lookup across multiple directories

## Project Structure

```
.
├── src/                    # Source files
│   ├── wsh.c               # Main shell logic    
│   ├── hash_map.c          # Hash map for alias storage│   
│   ├── dynamic_array.c     # Dynamic array for history
│   └── utils.c             #  utility functions
├── include/                # Header files
│   └── wsh.h    
│   ├── hash_map.h            
│   ├── dynamic_array.h     
│   └── utils.h                
├── Makefile                # Build configuration
└── README.md                # Project documentation
```

## Development

### Building from Source

```bash
make clean    # Remove previous builds
make          # Compile the project
make debug    # Compile with debugging symbols
```

### Memory Leak Testing

The shell is designed to be memory-safe with proper allocation and deallocation:

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./wsh
```

### Testing

The shell has been tested against various scenarios including:
- Complex command chains with multiple pipes
- Alias substitution with nested aliases
- Circular alias dependency detection
- Memory leak scenarios and stress tests
- Invalid command and error handling
- Path resolution across different Unix environments
- History navigation and execution

## Future Roadmap

This project is under active development. Here are the planned enhancements:

### Advanced Terminal I/O
- Implement raw/canonical mode switching to capture individual keystrokes
- Add support for arrow keys to navigate through command history
- Implement reverse history search (Ctrl+R)
- Tab completion for commands and file paths

### I/O Redirection
- Input redirection (`<`)
- Output redirection (`>`)
- Append mode (`>>`)
- Error redirection (`2>`, `2>&1`)

### Advanced Shell Features
- **Background Processes**: Execute commands in background using `&`
- **Job Control**: Implement `jobs`, `fg`, `bg` commands for process management
- **Conditional Execution**: Support for `&&` (AND) and `||` (OR) operators
- **Command Substitution**: Enable `$(command)` or backtick syntax
- **Globbing**: Wildcard expansion (`*`, `?`, `[...]`)
- **Environment Variables**: Support for setting, unsetting, and exporting variables
- **Signal Handling**: Proper handling of Ctrl+C, Ctrl+Z, and other signals
- **Shell Scripts**: Enhanced scripting support with conditionals and loops


## Contributing

While this is a personal learning project, feedback and suggestions are always welcome!


## Acknowledgments

Built as an exploration of operating systems fundamentals, leveraging concepts from "Operating Systems: Three Easy Pieces" by Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau, along with practical Unix systems programming experience.

---

**Author**: Saksham  
**GitHub**: [@saksham-2000](https://github.com/saksham-2000)  
**Project Link**: [https://github.com/saksham-2000/wisconsin-shell](https://github.com/saksham-2000/wisconsin-shell)