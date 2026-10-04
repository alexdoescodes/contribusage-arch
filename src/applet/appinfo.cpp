#include "appinfo.h"

#include "core/version.h"

QString AppInfo::version() const {
    return contribusage::version();
}
