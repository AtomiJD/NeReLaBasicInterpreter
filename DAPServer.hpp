// DAPHandler.hpp
#pragma once

#include <string>
#include <atomic>

class NeReLaBasic; // Forward declaration

class DAPHandler {
public:
    DAPHandler(NeReLaBasic& vm_instance);
    ~DAPHandler();

    void run_session(); // This stays the same

private:
    NeReLaBasic& vm;
    std::atomic<bool> session_running;
    int client_socket = -1; // This also stays

    void process_command(const std::string& command); // This stays the same
    void client_session(); // This stays the same

    // NEW AND SIMPLIFIED SENDING LOGIC
    void send_message(const std::string& message);

public:
    // This is the public method the interpreter will call
    void send_stopped_message(const std::string& reason, int line);
};