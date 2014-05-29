// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

// Convert a string ("true", "false") to a boolean.
bool ConvertStringToBoolean(const std::string& string, bool* value) {
  static const char kValueTrue[] = "true";
  static const char kValueFalse[] = "false";

  *value = (string == kValueTrue);
  return *value || (string == kValueFalse);
}

}  // namespace

bool GetConstraintValueAsBoolean(const blink::WebMediaConstraints& constraints,
                                 const std::string& key,
                                 bool* value) {
  return GetMandatoryConstraintValueAsBoolean(constraints, key, value) ||
         GetOptionalConstraintValueAsBoolean(constraints, key, value);
}

bool GetConstraintValueAsInteger(const blink::WebMediaConstraints& constraints,
                                 const std::string& key,
                                 int* value) {
  return GetMandatoryConstraintValueAsInteger(constraints, key, value) ||
         GetOptionalConstraintValueAsInteger(constraints, key, value);
}

bool GetConstraintValueAsString(const blink::WebMediaConstraints& constraints,
                                const std::string& key,
                                std::string* value) {
  blink::WebString value_str;
  base::string16 key_16 = base::UTF8ToUTF16(key);
  if (!constraints.getMandatoryConstraintValue(key_16, value_str) &&
      !constraints.getOptionalConstraintValue(key_16, value_str)) {
    return false;
  }

  *value = value_str.utf8();
  return true;
}

bool GetMandatoryConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const std::string& key,
    bool* value) {
  blink::WebString value_str;
  if (!constraints.getMandatoryConstraintValue(base::UTF8ToUTF16(key),
                                               value_str)) {
    return false;
  }

  return ConvertStringToBoolean(value_str.utf8(), value);
}

bool GetMandatoryConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const std::string& key,
    int* value) {
  blink::WebString value_str;
  if (!constraints.getMandatoryConstraintValue(base::UTF8ToUTF16(key),
                                               value_str)) {
    return false;
  }

  return base::StringToInt(value_str.utf8(), value);
}

bool GetOptionalConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const std::string& key,
    bool* value) {
  blink::WebString value_str;
  if (!constraints.getOptionalConstraintValue(base::UTF8ToUTF16(key),
                                              value_str)) {
    return false;
  }

  return ConvertStringToBoolean(value_str.utf8(), value);
}

bool GetOptionalConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const std::string& key,
    int* value) {
  blink::WebString value_str;
  if (!constraints.getOptionalConstraintValue(base::UTF8ToUTF16(key),
                                              value_str)) {
    return false;
  }

  return base::StringToInt(value_str.utf8(), value);
}

}  // namespace content
