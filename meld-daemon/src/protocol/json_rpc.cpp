#include "json_rpc.hpp"
#include <iostream>
#include <sstream>

namespace meld::lsp::protocol {

std::optional<Message> JsonRpc::parse(const std::string& raw) {
    try {
        auto j = json::parse(raw);
        Message msg;

        if (j.contains("method")) {
            msg.method = j["method"].get<std::string>();
            if (j.contains("params")) {
                msg.params = j["params"];
            }
            if (j.contains("id")) {
                msg.id = j["id"].get<int>();
                msg.type = MessageType::Request;
            } else {
                msg.type = MessageType::Notification;
            }
        } else if (j.contains("result")) {
            msg.type = MessageType::Response;
            msg.result = j["result"];
            if (j.contains("id")) {
                msg.id = j["id"].get<int>();
            }
        } else if (j.contains("error")) {
            msg.type = MessageType::Error;
            if (j.contains("id")) {
                msg.id = j["id"].get<int>();
            }
            auto& err = j["error"];
            msg.error_code = err.value("code", 0);
            msg.error_message = err.value("message", std::string("Unknown error"));
        } else {
            return std::nullopt;
        }

        return msg;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

std::string JsonRpc::make_response(int id, const json& result) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    return response.dump();
}

std::string JsonRpc::make_error(int id, int code, const std::string& message) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}}
    };
    return response.dump();
}

std::string JsonRpc::make_notification(const std::string& method, const json& params) {
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
    return notification.dump();
}

std::optional<std::string> JsonRpc::read_message(std::istream& input) {
    std::string header_line;
    int content_length = -1;

    // Read headers until empty line
    while (std::getline(input, header_line)) {
        // Remove trailing \r if present
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }
        if (header_line.empty()) {
            break;
        }
        if (header_line.find("Content-Length:") == 0) {
            content_length = std::stoi(header_line.substr(15));
        }
    }

    if (content_length < 0) {
        return std::nullopt;
    }

    std::string body(content_length, '\0');
    input.read(body.data(), content_length);

    if (input.gcount() != content_length) {
        return std::nullopt;
    }

    return body;
}

void JsonRpc::write_message(std::ostream& output, const std::string& body) {
    output << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    output.flush();
}

void MessageRouter::register_request(const std::string& method, RequestHandler handler) {
    request_handlers_[method] = std::move(handler);
}

void MessageRouter::register_notification(const std::string& method, NotificationHandler handler) {
    notification_handlers_[method] = std::move(handler);
}

std::optional<std::string> MessageRouter::dispatch(const Message& msg) {
    if (msg.type == MessageType::Request && msg.id.has_value()) {
        auto it = request_handlers_.find(msg.method);
        if (it != request_handlers_.end()) {
            try {
                json result = it->second(msg.params);
                return JsonRpc::make_response(*msg.id, result);
            } catch (const std::exception& e) {
                return JsonRpc::make_error(*msg.id, -32603, e.what());
            }
        }
        return JsonRpc::make_error(*msg.id, -32601, "Method not found: " + msg.method);
    }

    if (msg.type == MessageType::Notification) {
        auto it = notification_handlers_.find(msg.method);
        if (it != notification_handlers_.end()) {
            it->second(msg.params);
        }
        // Notifications don't produce responses
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace meld::lsp::protocol
