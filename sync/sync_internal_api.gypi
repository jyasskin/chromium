# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include_dirs': [
    '..',
  ],
  'defines': [
    'SYNC_IMPLEMENTATION',
  ],
  'dependencies': [
    '../base/base.gyp:base',
    '../net/net.gyp:net',
    '../third_party/zlib/zlib.gyp:zlib',
    '../url/url.gyp:url_lib',
  ],
  'sources': [
    'internal_api/attachments/fake_attachment_store.cc',
    'internal_api/attachments/fake_attachment_uploader.cc',
    'internal_api/base_node.cc',
    'internal_api/base_transaction.cc',
    'internal_api/change_record.cc',
    'internal_api/change_reorder_buffer.cc',
    'internal_api/change_reorder_buffer.h',
    'internal_api/debug_info_event_listener.cc',
    'internal_api/debug_info_event_listener.h',
    'internal_api/delete_journal.cc',
    'internal_api/events/commit_request_event.cc',
    'internal_api/events/commit_response_event.cc',
    'internal_api/events/configure_get_updates_request_event.cc',
    'internal_api/events/get_updates_response_event.cc',
    'internal_api/events/normal_get_updates_request_event.cc',
    'internal_api/events/poll_get_updates_request_event.cc',
    'internal_api/events/protocol_event.cc',
    'internal_api/http_bridge.cc',
    'internal_api/http_bridge_network_resources.cc',
    'internal_api/internal_components_factory_impl.cc',
    'internal_api/js_mutation_event_observer.cc',
    'internal_api/js_mutation_event_observer.h',
    'internal_api/js_sync_encryption_handler_observer.cc',
    'internal_api/js_sync_encryption_handler_observer.h',
    'internal_api/js_sync_manager_observer.cc',
    'internal_api/js_sync_manager_observer.h',
    'internal_api/protocol_event_buffer.cc',
    'internal_api/protocol_event_buffer.h',
    'internal_api/public/attachments/fake_attachment_store.h',
    'internal_api/public/attachments/fake_attachment_uploader.h',
    'internal_api/public/base/ack_handle.cc',
    'internal_api/public/base/ack_handle.h',
    'internal_api/public/base/attachment_id_proto.cc',
    'internal_api/public/base/attachment_id_proto.h',
    'internal_api/public/base/cancelation_observer.cc',
    'internal_api/public/base/cancelation_observer.h',
    'internal_api/public/base/cancelation_signal.cc',
    'internal_api/public/base/cancelation_signal.h',
    'internal_api/public/base/enum_set.h',
    'internal_api/public/base/enum_set.h',
    'internal_api/public/base/invalidation.cc',
    'internal_api/public/base/invalidation.h',
    'internal_api/public/base/model_type.h',
    'internal_api/public/base/node_ordinal.cc',
    'internal_api/public/base/node_ordinal.h',
    'internal_api/public/base/ordinal.h',
    'internal_api/public/base/progress_marker_map.cc',
    'internal_api/public/base/progress_marker_map.h',
    'internal_api/public/base/unique_position.cc',
    'internal_api/public/base/unique_position.h',
    'internal_api/public/base_node.h',
    'internal_api/public/base_transaction.h',
    'internal_api/public/change_record.h',
    'internal_api/public/configure_reason.h',
    'internal_api/public/data_type_association_stats.cc',
    'internal_api/public/data_type_association_stats.h',
    'internal_api/public/data_type_debug_info_listener.cc',
    'internal_api/public/data_type_debug_info_listener.h',
    'internal_api/public/delete_journal.h',
    'internal_api/public/engine/model_safe_worker.cc',
    'internal_api/public/engine/model_safe_worker.h',
    'internal_api/public/engine/passive_model_worker.cc',
    'internal_api/public/engine/passive_model_worker.h',
    'internal_api/public/engine/polling_constants.cc',
    'internal_api/public/engine/polling_constants.h',
    'internal_api/public/engine/sync_status.cc',
    'internal_api/public/engine/sync_status.h',
    'internal_api/public/events/commit_request_event.h',
    'internal_api/public/events/commit_response_event.h',
    'internal_api/public/events/configure_get_updates_request_event.h',
    'internal_api/public/events/get_updates_response_event.h',
    'internal_api/public/events/normal_get_updates_request_event.h',
    'internal_api/public/events/poll_get_updates_request_event.h',
    'internal_api/public/events/protocol_event.h',
    'internal_api/public/http_bridge.h',
    'internal_api/public/http_bridge_network_resources.h',
    'internal_api/public/http_post_provider_factory.h',
    'internal_api/public/http_post_provider_interface.h',
    'internal_api/public/internal_components_factory.h',
    'internal_api/public/internal_components_factory_impl.h',
    'internal_api/public/network_resources.h',
    'internal_api/public/read_node.h',
    'internal_api/public/read_transaction.h',
    'internal_api/public/sessions/commit_counters.cc',
    'internal_api/public/sessions/commit_counters.h',
    'internal_api/public/sessions/model_neutral_state.cc',
    'internal_api/public/sessions/model_neutral_state.h',
    'internal_api/public/sessions/status_counters.cc',
    'internal_api/public/sessions/status_counters.h',
    'internal_api/public/sessions/sync_session_snapshot.cc',
    'internal_api/public/sessions/sync_session_snapshot.h',
    'internal_api/public/sessions/type_debug_info_observer.cc',
    'internal_api/public/sessions/type_debug_info_observer.h',
    'internal_api/public/sessions/update_counters.cc',
    'internal_api/public/sessions/update_counters.h',
    'internal_api/public/sync_auth_provider.h',
    'internal_api/public/sync_core_proxy.h',
    'internal_api/public/sync_encryption_handler.cc',
    'internal_api/public/sync_encryption_handler.h',
    'internal_api/public/sync_manager.cc',
    'internal_api/public/sync_manager.h',
    'internal_api/public/sync_manager_factory.h',
    'internal_api/public/user_share.h',
    'internal_api/public/util/experiments.h',
    'internal_api/public/util/immutable.h',
    'internal_api/public/util/report_unrecoverable_error_function.h',
    'internal_api/public/util/sync_string_conversions.cc',
    'internal_api/public/util/sync_string_conversions.h',
    'internal_api/public/util/syncer_error.cc',
    'internal_api/public/util/syncer_error.h',
    'internal_api/public/util/unrecoverable_error_handler.h',
    'internal_api/public/util/unrecoverable_error_info.cc',
    'internal_api/public/util/unrecoverable_error_info.h',
    'internal_api/public/util/weak_handle.cc',
    'internal_api/public/util/weak_handle.h',
    'internal_api/public/write_node.h',
    'internal_api/public/write_transaction.h',
    'internal_api/read_node.cc',
    'internal_api/read_transaction.cc',
    'internal_api/sync_backup_manager.cc',
    'internal_api/sync_backup_manager.h',
    'internal_api/sync_core.cc',
    'internal_api/sync_core.h',
    'internal_api/sync_core_proxy.cc',
    'internal_api/sync_core_proxy_impl.cc',
    'internal_api/sync_core_proxy_impl.h',
    'internal_api/sync_encryption_handler_impl.cc',
    'internal_api/sync_encryption_handler_impl.h',
    'internal_api/sync_manager_factory.cc',
    'internal_api/sync_manager_impl.cc',
    'internal_api/sync_manager_impl.h',
    'internal_api/sync_rollback_manager.cc',
    'internal_api/sync_rollback_manager.h',
    'internal_api/sync_rollback_manager_base.cc',
    'internal_api/sync_rollback_manager_base.h',
    'internal_api/syncapi_internal.cc',
    'internal_api/syncapi_internal.h',
    'internal_api/syncapi_server_connection_manager.cc',
    'internal_api/syncapi_server_connection_manager.h',
    'internal_api/user_share.cc',
    'internal_api/write_node.cc',
    'internal_api/write_transaction.cc',
  ],
}
