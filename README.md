# Cpp_TChat 💬

A lightweight, real-time terminal chat application built in C++.
It features a **Text User Interface (TUI)** using `ncurses`, support for multiple clients, and a threaded client-server architecture.

## 🚀 Features

* **Real-time Messaging:** Low-latency communication using TCP sockets.
* **Text User Interface (TUI):** Clean, scrolling chat history using `ncurses`.
* **Multi-Client Support:** Multiple users can join a single server.
* **Authentication:** Randomly generated session passwords for security.
* **Admin Broadcasting:** Server host can broadcast messages to all connected clients.
* **Threaded Architecture:** Handles sending and receiving simultaneously without blocking.

## 🛠️ Requirements

* **OS:** Linux, macOS, or WSL (Windows Subsystem for Linux).
* **Compiler:** `g++` (GCC) with C++11 support or higher.
* **Libraries:** `ncurses` (for the GUI).

### Installing Dependencies

**Ubuntu / Debian / WSL:**

```bash
sudo apt-get update
sudo apt-get install libncurses5-dev libncursesw5-dev
```

**Arch Linux:**

```bash
sudo pacman -S ncurses
```

**MacOS:**

```bash
brew install ncurses
```

## ⚙️ Build & Run

Clone the repository:

```bash
git clone https://github.com/sethiritham/Cpp_TChat.git
cd Cpp_TChat
```

Compile the code: You must link pthread (for threads) and ncurses (for UI).

```bash
g++ ncurses_chat/main.cpp -o chat_app -pthread -lncurses
```

Run the application:

```bash
./chat_app
```

## 📖 How to Use

When you run the app, you will see a menu:

```plaintext
========== TERMINAL CHAT ==========
1. Run Server
2. Run Client
```

### 1. Hosting a Room (Server)

Select option **1**.

The server will start and generate a **Random Password** (e.g., `48291`).

Share this password with your friends so they can join.

As the admin, you can type messages to broadcast to everyone.

### 2. Joining a Room (Client)

Select option **2**.

Enter your **Username**.

Enter the **Room Password** shown on the Server's screen.

Once connected, the TUI will launch, and you can chat freely!

## 🐞 Troubleshooting

* **"Port Busy" Error:** If you restart the server quickly, you might see a binding error. The code includes `SO_REUSEADDR` to fix this, but if it persists, wait 30 seconds or change the port in `main.cpp`.
* **"Undefined reference to initscr":** This means you forgot to link the library. Make sure you added `-lncurses` to your compile command.
* **Weird Characters (e.g., ^B^B):** Ensure your terminal supports standard ANSI escape codes. The app uses `keypad()` mode to handle scrolling and arrow keys correctly.
