// DAPHandler.cpp
#include "DAPServer.hpp"
#include "NeReLaBasic.hpp" // Include the full definition for the VM
#include "TextIO.hpp"
#include "Commands.hpp" // Needed for to_string
#include "Error.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define closesocket close
#endif

DAPHandler::DAPHandler(NeReLaBasic& vm_instance)
    : vm(vm_instance), server_running(false), listen_socket(-1), client_socket(-1) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        TextIO::print("? DAP Error: WSAStartup failed.\n");
    }
#endif
}

DAPHandler::~DAPHandler() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void DAPHandler::start(int port) {
    if (server_running) {
        return; // Already running
    }
    server_running = true;
    server_thread = std::thread(&DAPHandler::server_loop, this, port);
}

void DAPHandler::stop() {
    server_running = false;
    if (listen_socket != -1) {
        closesocket(listen_socket);
        listen_socket = -1;
    }
    if (client_socket != -1) {
        closesocket(client_socket);
        client_socket = -1;
    }
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

void DAPHandler::server_loop(int port) {
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == -1) {
        TextIO::print("? DAP Error: Cannot create socket.\n");
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        TextIO::print("? DAP Error: Cannot bind socket to port " + std::to_string(port) + ".\n");
        return;
    }

    if (listen(listen_socket, 1) == -1) {
        TextIO::print("? DAP Error: Cannot listen on socket.\n");
        return;
    }

    TextIO::print("DAP Server listening on port " + std::to_string(port) + "\n");

    while (server_running) {
        client_socket = accept(listen_socket, nullptr, nullptr);
        if (client_socket == -1) {
            if (server_running) {
                TextIO::print("? DAP Error: Accept failed.\n");
            }
            continue;
        }

        TextIO::print("DAP Client connected.\n");
        client_session(); // Handle the entire client session in this function
        TextIO::print("DAP Client disconnected.\n");
    }

    closesocket(listen_socket);
    listen_socket = -1;
}

void DAPHandler::client_session() {
    std::string buffer;
    long content_length = -1;

    while (server_running && client_socket != -1) {
        char read_buffer[4096];
        int bytes_read = recv(client_socket, read_buffer, sizeof(read_buffer), 0);

        if (bytes_read <= 0) {
            // Client closed connection or error
            closesocket(client_socket);
            client_socket = -1;
            break;
        }

        buffer.append(read_buffer, bytes_read);
        TextIO::print("DAP RX: Received " + std::to_string(bytes_read) + " bytes. Buffer size is now " + std::to_string(buffer.size()) + ".\n");
        TextIO::print("DAP RX Buffer:\n---\n" + buffer + "\n---\n");
        // Process all complete messages in the buffer
        while (true) {
            size_t header_end = buffer.find("\r\n\r\n");
            if (header_end == std::string::npos) {
                break; // Incomplete header
            }

            std::string headers_str = buffer.substr(0, header_end);
            std::istringstream header_stream(headers_str);
            std::string header_line;
            content_length = -1;

            while (std::getline(header_stream, header_line)) {
                if (header_line.length() > 0 && header_line.back() == '\r') {
                    header_line.pop_back();
                }

                // --- FIX: Case-insensitive search for Content-Length ---
                std::string lower_line = header_line;
                std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                if (lower_line.rfind("content-length:", 0) == 0) {
                    try {
                        // Get the value part from the original, mixed-case string
                        content_length = std::stol(header_line.substr(16));
                    }
                    catch (const std::exception&) {
                        TextIO::print("? DAP Error: Invalid Content-Length value.\n");
                        content_length = -1;
                    }
                }
            }

            if (content_length == -1) {
                TextIO::print("? DAP Warning: Could not find Content-Length in headers. Waiting for more data.\n");
                TextIO::print("Headers received:\n" + headers_str + "\n");
                break;
            }

            TextIO::print("DAP Info: Expecting message body of " + std::to_string(content_length) + " bytes.\n");

            size_t message_start = header_end + 4;
            if (buffer.size() < message_start + content_length) {
                break; // Incomplete message body
            }

            std::string message = buffer.substr(message_start, content_length);
            process_message(message);

            // Remove the processed message from the buffer
            buffer.erase(0, message_start + content_length);
        }
    }
}

void DAPHandler::process_message(const std::string& message) {
    try {
        nlohmann::json request = nlohmann::json::parse(message);
        if (request["type"] != "request") return;

        std::string command = request["command"];
        TextIO::print("DAP Request: " + command + "\n");

        if (command == "initialize") on_initialize(request);
        else if (command == "launch") on_launch(request);
        else if (command == "setBreakpoints") on_set_breakpoints(request);
        else if (command == "threads") on_threads(request);
        else if (command == "stackTrace") on_stack_trace(request);
        else if (command == "scopes") on_scopes(request);
        else if (command == "variables") on_variables(request);
        else if (command == "continue") on_continue(request);
        else if (command == "next") on_next(request);
        else if (command == "disconnect") on_disconnect(request);

    }
    catch (const nlohmann::json::parse_error& e) {
        TextIO::print("? DAP Error: JSON parse error: " + std::string(e.what()) + "\n");
    }
}

// --- Request Handler Implementations ---

void DAPHandler::on_initialize(const nlohmann::json& request) {
    nlohmann::json capabilities;
    capabilities["supportsConfigurationDoneRequest"] = true;
    capabilities["supportsFunctionBreakpoints"] = false;
    capabilities["supportsConditionalBreakpoints"] = false;
    capabilities["supportsStepInTargetsRequest"] = false;
    capabilities["supportsRestartRequest"] = false;

    send_response(request["seq"], request["command"], capabilities);

    // After responding, send the 'initialized' event
    send_event("initialized", {});
    TextIO::print("DAP: initialized is send.\n");
}

void DAPHandler::on_launch(const nlohmann::json& request) {
    TextIO::print("DAP: on_launch().\n");
    const auto& args = request["arguments"];
    if (!args.contains("program")) {
        TextIO::print("? DAP Error: Launch request is missing 'program' attribute.\n");
        vm.dap_launch_promise.set_value(false); // Signal failure
        send_response(request["seq"], request["command"], {});
        return;
    }

    vm.program_to_debug = args["program"];

    // 1. Load the source code from the file provided by the client
    if (!vm.loadSourceFromFile(vm.program_to_debug)) {
        TextIO::print("? DAP Error: Failed to load source file: " + vm.program_to_debug + "\n");
        vm.dap_launch_promise.set_value(false); // Signal failure
        send_response(request["seq"], request["command"], {});
        return;
    }

    // 2. Compile the loaded source code
    Commands::do_compile(vm);
    if (Error::get() != 0) {
        TextIO::print("? DAP Error: Compilation failed.\n");
        Error::print(); // Show the compilation error
        vm.dap_launch_promise.set_value(false); // Signal failure
        send_response(request["seq"], request["command"], {});
        return;
    }

    // 3. Set the debugger to pause at the very first line of code
    vm.debug_state = NeReLaBasic::DebugState::PAUSED;

    // 4. Send the successful launch response to the client
    send_response(request["seq"], request["command"], {});

    // 5. Unblock the main thread so it can call vm.execute()
    vm.dap_launch_promise.set_value(true);

    // 6. Immediately send a "stopped" event to make the debugger pause at the entry point
    send_stopped_event("entry");
}

void DAPHandler::on_set_breakpoints(const nlohmann::json& request) {
    vm.breakpoints.clear();
    const auto& args = request["arguments"];

    nlohmann::json response_breakpoints = nlohmann::json::array();
    if (args.contains("breakpoints")) {
        for (const auto& bp : args["breakpoints"]) {
            uint16_t line = bp["line"];
            vm.breakpoints[line] = true;

            nlohmann::json response_bp;
            response_bp["verified"] = true;
            response_bp["line"] = line;
            response_breakpoints.push_back(response_bp);
        }
    }

    nlohmann::json body;
    body["breakpoints"] = response_breakpoints;
    send_response(request["seq"], request["command"], body);
}

void DAPHandler::on_threads(const nlohmann::json& request) {
    nlohmann::json body;
    body["threads"] = nlohmann::json::array();

    nlohmann::json main_thread;
    main_thread["id"] = 1;
    main_thread["name"] = "Main Thread";
    body["threads"].push_back(main_thread);

    send_response(request["seq"], request["command"], body);
}

void DAPHandler::on_stack_trace(const nlohmann::json& request) {
    nlohmann::json body;
    body["stackFrames"] = nlohmann::json::array();

    // APL-style, reverse iteration is more natural for call stacks
    for (int i = vm.call_stack.size() - 1; i >= 0; --i) {
        const auto& frame = vm.call_stack[i];
        nlohmann::json dap_frame;
        dap_frame["id"] = i; // Frame ID
        dap_frame["name"] = "Function"; // You should store function names in StackFrame
        dap_frame["line"] = frame.return_pcode; // Placeholder, you need accurate line info
        dap_frame["column"] = 1;
        body["stackFrames"].push_back(dap_frame);
    }

    // Add the current execution point as the top frame (frame 0)
    nlohmann::json current_frame;
    current_frame["id"] = vm.call_stack.size();
    current_frame["name"] = "[Global Scope]";
    current_frame["line"] = vm.runtime_current_line;
    current_frame["column"] = 1;
    body["stackFrames"].push_back(current_frame);

    send_response(request["seq"], request["command"], body);
}

void DAPHandler::on_scopes(const nlohmann::json& request) {
    int frame_id = request["arguments"]["frameId"];

    nlohmann::json body;
    body["scopes"] = nlohmann::json::array();

    nlohmann::json local_scope;
    local_scope["name"] = "Locals";
    local_scope["variablesReference"] = frame_id + 1; // Use frameId+1 as a unique ref for locals
    local_scope["expensive"] = false;
    body["scopes"].push_back(local_scope);

    nlohmann::json global_scope;
    global_scope["name"] = "Globals";
    global_scope["variablesReference"] = 9999; // A fixed large number for globals
    global_scope["expensive"] = false;
    body["scopes"].push_back(global_scope);

    send_response(request["seq"], request["command"], body);
}

void DAPHandler::on_variables(const nlohmann::json& request) {
    int ref = request["arguments"]["variablesReference"];

    nlohmann::json body;
    body["variables"] = nlohmann::json::array();

    auto add_var = [&](const std::string& name, const BasicValue& value) {
        nlohmann::json var;
        var["name"] = name;
        var["value"] = to_string(value);
        var["type"] = "Variant"; // You could add more detailed type info
        var["variablesReference"] = 0; // 0 means not expandable
        body["variables"].push_back(var);
        };

    if (ref == 9999) { // Global scope
        for (const auto& pair : vm.variables) {
            add_var(pair.first, pair.second);
        }
    }
    else { // Local scope
        int frame_id = ref - 1;
        if (frame_id >= 0 && frame_id < vm.call_stack.size()) {
            for (const auto& pair : vm.call_stack[frame_id].local_variables) {
                add_var(pair.first, pair.second);
            }
        }
    }

    send_response(request["seq"], request["command"], body);
}

void DAPHandler::on_continue(const nlohmann::json& request) {
    vm.resume_from_debugger();
    send_response(request["seq"], request["command"], {});
}

void DAPHandler::on_next(const nlohmann::json& request) {
    vm.step_over();
    send_response(request["seq"], request["command"], {});
}

void DAPHandler::on_disconnect(const nlohmann::json& request) {
    send_response(request["seq"], request["command"], {});
    stop(); // Shut down the server
}

// --- Event Sending ---

void DAPHandler::send_stopped_event(const std::string& reason) {
    nlohmann::json body;
    body["reason"] = reason;
    body["threadId"] = 1;
    body["allThreadsStopped"] = true;
    send_event("stopped", body);
}

void DAPHandler::send_message(const std::string& message) {
    if (client_socket != -1 && server_running.load()) {
        // Ensure the message ends with a single newline character.
        std::string payload = message + "\n";
        send(client_socket, payload.c_str(), payload.length(), 0);
    }
}

// IMPLEMENT the new public helper for sending the specific "stopped" message.
void DAPHandler::send_stopped_message(const std::string& reason, int line) {
    // Format the message exactly as your client code expects:
    // "stopped <reason> <line>"
    std::string msg = "stopped " + reason + " " + std::to_string(line);
    send_message(msg);
}
