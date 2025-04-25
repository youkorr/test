#ifndef WEB_H
#define WEB_H

#include "webdavbox3.h"
#include <string>

namespace esphome {
namespace webdavbox3 {

class WebInterface {
public:
    static std::string get_web_interface_html(WebDAVBox3* instance);
};

}  // namespace webdavbox3
}  // namespace esphome

#endif  // WEB_H
