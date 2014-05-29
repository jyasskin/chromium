// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kErrorUnknown[] = "Unknown";

bool ConvertListValueToStringVector(const base::ListValue& string_list,
                                    std::vector<std::string>* result) {
  for (size_t i = 0; i < string_list.GetSize(); ++i) {
    std::string str;
    if (!string_list.GetString(i, &str))
      return false;
    result->push_back(str);
  }
  return true;
}

bool IsCaCertNssSet(const base::DictionaryValue& properties) {
  std::string ca_cert_nss;
  if (properties.GetStringWithoutPathExpansion(shill::kEapCaCertNssProperty,
                                               &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }

  const base::DictionaryValue* provider = NULL;
  properties.GetDictionaryWithoutPathExpansion(shill::kProviderProperty,
                                               &provider);
  if (!provider)
    return false;
  if (provider->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecCaCertNssProperty, &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }
  if (provider->GetStringWithoutPathExpansion(
          shill::kOpenVPNCaCertNSSProperty, &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }

  return false;
}

}  // namespace

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path),
      connectable_(false),
      prefix_length_(0),
      signal_strength_(0),
      activate_over_non_cellular_networks_(false),
      cellular_out_of_credits_(false),
      has_ca_cert_nss_(false) {
}

NetworkState::~NetworkState() {
}

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  // Keep care that these properties are the same as in |GetProperties|.
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == shill::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == shill::kStateProperty) {
    return GetStringValue(key, value, &connection_state_);
  } else if (key == shill::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == shill::kErrorProperty) {
    if (!GetStringValue(key, value, &error_))
      return false;
    if (ErrorIsValid(error_))
      last_error_ = error_;
    else
      error_.clear();
    return true;
  } else if (key == shill::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == shill::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == shill::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  } else if (key == shill::kEapMethodProperty) {
    return GetStringValue(key, value, &eap_method_);
  } else if (key == shill::kUIDataProperty) {
    scoped_ptr<NetworkUIData> new_ui_data =
        shill_property_util::GetUIDataFromValue(value);
    if (!new_ui_data) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }
    ui_data_ = *new_ui_data;
    return true;
  } else if (key == shill::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &network_technology_);
  } else if (key == shill::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == shill::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == shill::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kActivateOverNonCellularNetworkProperty) {
    return GetBooleanValue(key, value, &activate_over_non_cellular_networks_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(
    const base::DictionaryValue& properties) {
  NET_LOG_DEBUG("InitialPropertiesReceived", path());
  bool changed = false;
  if (!properties.HasKey(shill::kTypeProperty)) {
    NET_LOG_ERROR("NetworkState has no type",
                  shill_property_util::GetNetworkIdFromProperties(properties));
  } else {
    changed |= UpdateName(properties);
  }
  bool had_ca_cert_nss = has_ca_cert_nss_;
  has_ca_cert_nss_ = IsCaCertNssSet(properties);
  changed |= had_ca_cert_nss != has_ca_cert_nss_;
  return changed;
}

void NetworkState::GetStateProperties(base::DictionaryValue* dictionary) const {
  ManagedState::GetStateProperties(dictionary);

  // Properties shared by all types.
  dictionary->SetStringWithoutPathExpansion(shill::kGuidProperty, guid());
  dictionary->SetStringWithoutPathExpansion(shill::kStateProperty,
                                            connection_state());
  dictionary->SetStringWithoutPathExpansion(shill::kSecurityProperty,
                                            security());
  if (!error().empty())
    dictionary->SetStringWithoutPathExpansion(shill::kErrorProperty, error());

  if (!NetworkTypePattern::Wireless().MatchesType(type()))
    return;

  // Wireless properties
  dictionary->SetBooleanWithoutPathExpansion(shill::kConnectableProperty,
                                             connectable());
  dictionary->SetIntegerWithoutPathExpansion(shill::kSignalStrengthProperty,
                                             signal_strength());

  // Wifi properties
  if (NetworkTypePattern::WiFi().MatchesType(type())) {
    dictionary->SetStringWithoutPathExpansion(shill::kEapMethodProperty,
                                              eap_method());
  }

  // Mobile properties
  if (NetworkTypePattern::Mobile().MatchesType(type())) {
    dictionary->SetStringWithoutPathExpansion(
        shill::kNetworkTechnologyProperty,
        network_technology());
    dictionary->SetStringWithoutPathExpansion(shill::kActivationStateProperty,
                                              activation_state());
    dictionary->SetStringWithoutPathExpansion(shill::kRoamingStateProperty,
                                              roaming());
    dictionary->SetBooleanWithoutPathExpansion(shill::kOutOfCreditsProperty,
                                               cellular_out_of_credits());
  }
}

void NetworkState::IPConfigPropertiesChanged(
    const base::DictionaryValue& properties) {
  for (base::DictionaryValue::Iterator iter(properties);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string key = iter.key();
    const base::Value& value = iter.value();

    if (key == shill::kAddressProperty) {
      GetStringValue(key, value, &ip_address_);
    } else if (key == shill::kGatewayProperty) {
      GetStringValue(key, value, &gateway_);
    } else if (key == shill::kNameServersProperty) {
      const base::ListValue* dns_servers;
      if (value.GetAsList(&dns_servers)) {
        dns_servers_.clear();
        ConvertListValueToStringVector(*dns_servers, &dns_servers_);
      }
    } else if (key == shill::kPrefixlenProperty) {
      GetIntegerValue(key, value, &prefix_length_);
    } else if (key == shill::kWebProxyAutoDiscoveryUrlProperty) {
      std::string url_string;
      if (GetStringValue(key, value, &url_string)) {
        if (url_string.empty()) {
          web_proxy_auto_discovery_url_ = GURL();
        } else {
          GURL gurl(url_string);
          if (gurl.is_valid()) {
            web_proxy_auto_discovery_url_ = gurl;
          } else {
            NET_LOG_ERROR("Invalid WebProxyAutoDiscoveryUrl: " + url_string,
                          path());
            web_proxy_auto_discovery_url_ = GURL();
          }
        }
      }
    }
  }
}

bool NetworkState::RequiresActivation() const {
  return (type() == shill::kTypeCellular &&
          activation_state() != shill::kActivationStateActivated &&
          activation_state() != shill::kActivationStateUnknown);
}

bool NetworkState::IsConnectedState() const {
  return StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return StateIsConnecting(connection_state_);
}

bool NetworkState::IsPrivate() const {
  return !profile_path_.empty() &&
      profile_path_ != NetworkProfileHandler::GetSharedProfilePath();
}

std::string NetworkState::GetDnsServersAsString() const {
  std::string result;
  for (size_t i = 0; i < dns_servers_.size(); ++i) {
    if (i != 0)
      result += ",";
    result += dns_servers_[i];
  }
  return result;
}

std::string NetworkState::GetNetmask() const {
  return network_util::PrefixLengthToNetmask(prefix_length_);
}

bool NetworkState::UpdateName(const base::DictionaryValue& properties) {
  std::string updated_name =
      shill_property_util::GetNameFromProperties(path(), properties);
  if (updated_name != name()) {
    set_name(updated_name);
    return true;
  }
  return false;
}

// static
bool NetworkState::StateIsConnected(const std::string& connection_state) {
  return (connection_state == shill::kStateReady ||
          connection_state == shill::kStateOnline ||
          connection_state == shill::kStatePortal);
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == shill::kStateAssociation ||
          connection_state == shill::kStateConfiguration ||
          connection_state == shill::kStateCarrier);
}

// static
bool NetworkState::ErrorIsValid(const std::string& error) {
  // Shill uses "Unknown" to indicate an unset or cleared error state.
  return !error.empty() && error != kErrorUnknown;
}

}  // namespace chromeos
