#pragma once

#include "esphome/core/component.h"
#include "esp_http_server.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include "esp_vfs_fat.h"

namespace esphome {
namespace webdavbox3 {

class WebDAVBox3 : public Component {
public:
    void setup() override;
    void loop() override;

    // Permet de définir le chemin racine
    void set_root_path(const std::string& path) { root_path_ = path; }

    // Permet de définir le préfixe d'URL
    void set_url_prefix(const std::string& prefix) { url_prefix_ = prefix; }

private:
    // Membres privés
    httpd_handle_t server_ = NULL;
    std::string root_path_ = "/sdcard/";
    std::string url_prefix_ = "/webdav";  // Préfixe par défaut

    // Méthodes statiques de gestion des requêtes HTTP
    static esp_err_t handle_root(httpd_req_t *req);
    static esp_err_t handle_webdav_list(httpd_req_t *req);
    static esp_err_t handle_webdav_get(httpd_req_t *req);
    static esp_err_t handle_webdav_put(httpd_req_t *req);
    static esp_err_t handle_webdav_delete(httpd_req_t *req);

    // Méthode privée pour configurer le serveur HTTP
    void configure_http_server();
};

} // namespace webdavbox3
} // namespace esphome
