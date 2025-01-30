
#pragma once
#include "Usf2EditControllerWebServer.h"
#include <network/choc_HTTPServer.h>
#include <iostream>
#include <fstream>

namespace usf2 {
    class Usf2WebUIServer {
        choc::network::HTTPServer server{};
        std::string base_path{};

    public:
        class Client : public choc::network::HTTPServer::ClientInstance {
            Usf2WebUIServer* owner;
        public:
            Client(Usf2WebUIServer* owner) : owner(owner) {}

            choc::network::HTTPContent getHTTPContent(std::string_view requestedPath) override {
                if (requestedPath == "/")
                    requestedPath = "/index.html";
                auto path = std::format("{}/{}", owner->base_path, requestedPath.data());
                auto fs = std::ifstream(path);
                std::ostringstream ss;
                ss << fs.rdbuf();
                auto s = ss.str();
                choc::network::HTTPContent content{s, choc::network::getMIMETypeFromFilename(requestedPath.data())};
                return content;
            }
            void handleWebSocketMessage(std::string_view message) override {
                // not supported
            }
            void upgradedToWebSocket(std::string_view path) override {
                // not supported
            }
        };

        void initialize(std::string& bundlePath) {
            base_path = bundlePath;
            server.open("127.0.0.1", 58087, 4, [&]() {
                auto ret = std::make_shared<Client>(this);
                return ret;
            }, [&](std::string error) {
                std::cerr << error << std::endl;
            });

            std::cerr << "HTTP Server started at http://localhost:58087" << std::endl;
        }

        ~Usf2WebUIServer() {
            server.close();
        }
    };
}