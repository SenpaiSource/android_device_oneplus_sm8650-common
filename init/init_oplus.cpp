/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <fs_mgr.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace android {
namespace init {
uint32_t InitPropertySet(const std::string& name, const std::string& value);
}  // namespace init
}  // namespace android

using android::base::GetProperty;
using android::base::ParseInt;
using android::fs_mgr::GetKernelCmdline;
using android::init::InitPropertySet;

namespace {
constexpr const char* kCmdlineRegion = "oplus_region";
constexpr const char* kCmdlinePrjName = "oplusboot.prjname";

const std::unordered_map<int, std::string> kRegionMap = {
        {27, "IN"},
        {68, "EU"},
        {151, "CN"},
        {161, "NA"},
        {167, "ROW"},
};
}  // anonymous namespace

/*
 * Only for read-only properties. Properties that can be wrote to more
 * than once should be set in a typical init script (e.g. init.oplus.hw.rc)
 * after the original property has been set.
 */
void vendor_process_bootenv() {
    std::string buf;
    if (!GetKernelCmdline(kCmdlineRegion, &buf)) {
        LOG(ERROR) << kCmdlineRegion << " not found in /proc/cmdline";
        return;
    }

    int region_id;
    if (!ParseInt(buf, &region_id)) {
        LOG(ERROR) << "Region ID [" << buf << "] is invalid";
        return;
    }

    if (region_id != 0) {
        auto it = kRegionMap.find(region_id);
        if (it == kRegionMap.end()) {
            LOG(ERROR) << "Unexpected region ID: " << region_id;
        } else {
            InitPropertySet("ro.boot.hardware.revision", it->second);
        }
    } else {
        if (!GetKernelCmdline(kCmdlinePrjName, &buf)) {
            LOG(ERROR) << kCmdlinePrjName << " not found in /proc/cmdline";
            return;
        }

        int prj_id;
        if (!ParseInt(buf, &prj_id)) {
            LOG(ERROR) << "Project ID [" << buf << "] is invalid";
            return;
        }

        switch (prj_id) {
            case 23926:
            case 23927:
                InitPropertySet("ro.boot.hardware.revision", "CN");
                break;
            case 23976:
            case 23978:
                InitPropertySet("ro.boot.hardware.revision", "GLO");
                break;
            default:
                LOG(ERROR) << "Unexpected project ID: " << prj_id;
                break;
        }
    }
}

void OverrideProperty(const char* name, const char* value) {
    size_t valuelen = strlen(value);

    prop_info* pi = (prop_info*)__system_property_find(name);
    if (pi != nullptr) {
        __system_property_update(pi, value, valuelen);
    } else {
        __system_property_add(name, strlen(name), value, valuelen);
    }
}

void SetRegionalProperties(const std::string& model, const std::string& name,
                           const std::string& fingerprint) {
    const std::vector<std::string> model_props = {
        "ro.product.model",
        "ro.product.odm.model",
        "ro.product.product.model",
        "ro.product.system.model",
        "ro.product.system_ext.model",
        "ro.product.vendor.model",
    };
    for (const auto& prop : model_props) {
        OverrideProperty(prop.c_str(), model.c_str());
    }

    if (!name.empty()) {
        const std::vector<std::string> name_props = {
            "ro.product.name",
            "ro.product.odm.name",
            "ro.product.product.name",
            "ro.product.system.name",
            "ro.product.system_ext.name",
            "ro.product.vendor.name",
        };
        for (const auto& prop : name_props) {
            OverrideProperty(prop.c_str(), name.c_str());
        }
    }

    if (!fingerprint.empty()) {
        const std::vector<std::string> fp_props = {
            "ro.build.fingerprint",
            "ro.bootimage.build.fingerprint",
            "ro.odm.build.fingerprint",
            "ro.product.build.fingerprint",
            "ro.system.build.fingerprint",
            "ro.system_ext.build.fingerprint",
            "ro.vendor.build.fingerprint",
        };
        for (const auto& prop : fp_props) {
            OverrideProperty(prop.c_str(), fingerprint.c_str());
        }
    }
}

void vendor_load_properties() {
    auto fingerprint = GetProperty("ro.vendor.build.fingerprint", "");
    auto hw_revision = GetProperty("ro.boot.hardware.revision", "");
    std::string prj_buf;
    int prj_id = 0;
    if (GetKernelCmdline(kCmdlinePrjName, &prj_buf)) {
        ParseInt(prj_buf, &prj_id);
    } else {
        auto prj_prop = GetProperty("ro.boot.prjname", "");
        if (!prj_prop.empty()) {
            ParseInt(prj_prop, &prj_id);
        }
    }

    if (prj_id == 24211 || prj_id == 23926 || prj_id == 23927 || prj_id == 23976 || prj_id == 23978) {
        // OnePlus Nord 4 (avalon) / Ace 3V (audi)
        if (hw_revision == "EU") {
            std::string eu_fp = fingerprint;
            auto pos = eu_fp.find("CPH2661IN");
            if (pos != std::string::npos) {
                eu_fp.replace(pos, 9, "CPH2663EEA");
            } else if ((pos = eu_fp.find("CPH2661")) != std::string::npos) {
                eu_fp.replace(pos, 7, "CPH2663EEA");
            }
            SetRegionalProperties("CPH2663", "CPH2663EEA", eu_fp);
        } else if (hw_revision == "ROW" || hw_revision == "GLO") {
            std::string row_fp = fingerprint;
            auto pos = row_fp.find("CPH2661IN");
            if (pos != std::string::npos) {
                row_fp.replace(pos, 9, "CPH2663");
            } else if ((pos = row_fp.find("CPH2661")) != std::string::npos) {
                row_fp.replace(pos, 7, "CPH2663");
            }
            SetRegionalProperties("CPH2663", "CPH2663", row_fp);
        } else if (hw_revision == "CN") {
            std::string cn_fp = fingerprint;
            auto pos = cn_fp.find("CPH2661IN");
            if (pos != std::string::npos) {
                cn_fp.replace(pos, 9, "PJF110");
            }
            SetRegionalProperties("PJF110", "PJF110", cn_fp);
        } else {
            // IN or default
            if (!fingerprint.empty()) {
                InitPropertySet("ro.build.fingerprint", fingerprint);
            }
        }
    } else {
        // Other devices (e.g. OnePlus 12 / corvette)
        if (hw_revision == "EU") {
            std::string eu_fp = fingerprint;
            auto pos = eu_fp.find("CPH2573");
            if (pos != std::string::npos) {
                eu_fp.replace(pos, 7, "CPH2581EEA");
            }
            SetRegionalProperties("CPH2581", "CPH2581EEA", eu_fp);
        } else if (hw_revision == "NA") {
            std::string na_fp = fingerprint;
            auto pos = na_fp.find("CPH2573");
            if (pos != std::string::npos) {
                na_fp.replace(pos, 7, "CPH2583");
            }
            SetRegionalProperties("CPH2583", "CPH2583", na_fp);
        } else if (hw_revision == "CN") {
            std::string cn_fp = fingerprint;
            auto pos = cn_fp.find("CPH2573");
            if (pos != std::string::npos) {
                cn_fp.replace(pos, 7, "PJD110");
            }
            SetRegionalProperties("PJD110", "PJD110", cn_fp);
        } else {
            if (!fingerprint.empty()) {
                InitPropertySet("ro.build.fingerprint", fingerprint);
            }
        }
    }
}
