// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/cocoa/system_hotkey_map.h"

#pragma mark - NSDictionary Helper Functions

namespace {

// All 4 following functions return nil if the object doesn't exist, or isn't of
// the right class.
id ObjectForKey(NSDictionary* dict, NSString* key, Class aClass) {
  id object = [dict objectForKey:key];
  if (![object isKindOfClass:aClass])
    return nil;
  return object;
}

NSDictionary* DictionaryForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSDictionary class]);
}

NSArray* ArrayForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSArray class]);
}

NSNumber* NumberForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSNumber class]);
}

}  // namespace

#pragma mark - SystemHotkey

namespace content {

struct SystemHotkey {
  int key_code;
  int modifiers;
};

#pragma mark - SystemHotkeyMap

SystemHotkeyMap::SystemHotkeyMap() {
}
SystemHotkeyMap::~SystemHotkeyMap() {
}

NSDictionary* SystemHotkeyMap::DictionaryFromData(NSData* data) {
  if (!data)
    return nil;

  NSError* error = nil;
  NSPropertyListFormat format;
  NSDictionary* dictionary =
      [NSPropertyListSerialization propertyListWithData:data
                                                options:0
                                                 format:&format
                                                  error:&error];

  if (![dictionary isKindOfClass:[NSDictionary class]])
    return nil;

  return dictionary;
}

bool SystemHotkeyMap::ParseDictionary(NSDictionary* dictionary) {
  system_hotkeys_.clear();

  if (!dictionary)
    return false;

  NSDictionary* hotkey_dictionaries =
      DictionaryForKey(dictionary, @"AppleSymbolicHotKeys");
  if (!hotkey_dictionaries)
    return false;

  for (NSString* hotkey_system_effect in [hotkey_dictionaries allKeys]) {
    if (![hotkey_system_effect isKindOfClass:[NSString class]])
      continue;

    NSDictionary* hotkey_dictionary =
        [hotkey_dictionaries objectForKey:hotkey_system_effect];
    if (![hotkey_dictionary isKindOfClass:[NSDictionary class]])
      continue;

    NSNumber* enabled = NumberForKey(hotkey_dictionary, @"enabled");
    if (!enabled || enabled.boolValue == NO)
      continue;

    NSDictionary* value = DictionaryForKey(hotkey_dictionary, @"value");
    if (!value)
      continue;

    NSArray* parameters = ArrayForKey(value, @"parameters");
    if (!parameters || [parameters count] != 3)
      continue;

    NSNumber* key_code = [parameters objectAtIndex:1];
    if (![key_code isKindOfClass:[NSNumber class]])
      continue;

    NSNumber* modifiers = [parameters objectAtIndex:2];
    if (![modifiers isKindOfClass:[NSNumber class]])
      continue;

    ReserveHotkey(key_code.intValue, modifiers.intValue, hotkey_system_effect);
  }

  return true;
}

bool SystemHotkeyMap::IsEventReserved(NSEvent* event) const {
  NSUInteger modifiers =
      NSShiftKeyMask | NSControlKeyMask | NSCommandKeyMask | NSAlternateKeyMask;
  return IsHotkeyReserved(event.keyCode, event.modifierFlags & modifiers);
}

bool SystemHotkeyMap::IsHotkeyReserved(int key_code, int modifiers) const {
  std::vector<SystemHotkey>::const_iterator it;
  for (it = system_hotkeys_.begin(); it != system_hotkeys_.end(); ++it) {
    if (it->key_code == key_code && it->modifiers == modifiers)
      return true;
  }
  return false;
}

void SystemHotkeyMap::ReserveHotkey(int key_code,
                                    int modifiers,
                                    NSString* system_effect) {
  ReserveHotkey(key_code, modifiers);

  // If a hotkey exists for toggling through the windows of an application, then
  // adding shift to that hotkey toggles through the windows backwards.
  if ([system_effect isEqualToString:@"27"])
    ReserveHotkey(key_code, modifiers | NSShiftKeyMask);
}

void SystemHotkeyMap::ReserveHotkey(int key_code, int modifiers) {
  SystemHotkey hotkey;
  hotkey.key_code = key_code;
  hotkey.modifiers = modifiers;
  system_hotkeys_.push_back(hotkey);
}

}  // namespace content
