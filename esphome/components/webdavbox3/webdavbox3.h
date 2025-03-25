#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include <esp_http_server.h>

namespace esphome {
namespace webdavbox3 {

class WebDAVBox3 : public Component {
public:
    void setup() override;
    void loop() override;

    void set_root_path(const std::string& path) {
        root_path_ = path.c_str();
    }

private:
    // Serveur web asynchrone
    AsyncWebServer server_{80};
    
    // Chemin racine pour les opérations WebDAV
    String root_path_{"/sdcard/"};

    // Méthodes internes de gestion WebDAV
    void handle_webdav_get_(AsyncWebServerRequest *request);
    void handle_webdav_put_(AsyncWebServerRequest *request);
    void handle_webdav_delete_(AsyncWebServerRequest *request);
    void handle_webdav_list_(AsyncWebServerRequest *request);
    void handle_root_(AsyncWebServerRequest *request);
};

} // namespace webdavbox3
} // namespace esphome
