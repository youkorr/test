#pragma once

#include "esphome/core/component.h"
#include "esp_http_server.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"  // Remplace tcpip_adapter.h

namespace esphome {
namespace webdavbox3 {

class WebDAVBox3 : public Component {
public:
    void setup() override;
    void loop() override;

    void set_root_path(const std::string& path) { root_path_ = path; }
    void set_url_prefix(const std::string& prefix) { url_prefix_ = prefix; }
    void set_port(uint16_t port) { port_ = port; }

private:
    httpd_handle_t server_ = NULL;
    std::string root_path_ = "/sdcard/";
    std::string url_prefix_ = "/webdav";
    uint16_t port_ = 8080;

    static esp_err_t handle_root(httpd_req_t *req);
    static esp_err_t handle_webdav_list(httpd_req_t *req);
    static esp_err_t handle_webdav_get(httpd_req_t *req);
    static esp_err_t handle_webdav_put(httpd_req_t *req);
    static esp_err_t handle_webdav_delete(httpd_req_t *req);

    void configure_http_server();
};
} // namespace webdavbox3
} // namespace esphome

