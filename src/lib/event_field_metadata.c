/*
 * Copyright (c) 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "event_field_metadata.h"
#include <persistence/logging.h>
#include <persistence/event.h>

#define	END_OF_TABLE	{EVENT_TYPE_ALL, 0, 0, 0, 0, 0}

// Paradigms for event argument metadata
#define	NO_UID_CUSTOM_DATA(type, code, arg1, arg2, arg3) \
		{type, code, EVENT_UID_TYPE_NONE, arg1, arg2, arg3}

#define	NO_UID_GENERIC_DATA(type, code) \
		NO_UID_CUSTOM_DATA(type, code, \
				EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER)

#define	DEVICE_UID_WITH_DATA(type, code, arg1, arg2, arg3) \
		{type, code, EVENT_UID_TYPE_DEVICE, arg1, arg2, arg3}

#define	DEVICE_UID_IN_ARG1(type, code) \
		DEVICE_UID_WITH_DATA(type, code, \
				EVENT_ARG_TYPE_DEVICE_UID, EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER)

#define	TWO_DEVICE_UIDS(type, code) \
		DEVICE_UID_WITH_DATA(type, code, \
				EVENT_ARG_TYPE_DEVICE_UID, EVENT_ARG_TYPE_DEVICE_UID, EVENT_ARG_TYPE_OTHER)

#define	NAMESPACE_UID(type, code) \
		{type, code, EVENT_UID_TYPE_NAMESPACE, \
				EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER}

#define	POOL_UID(type, code) \
		{type, code, EVENT_UID_TYPE_POOL, \
				EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER}

// Metadata for all possible events
static const struct event_field_metadata EVENT_FIELD_METADATA_TABLE[] =
{
		// Device Config Events
		DEVICE_UID_IN_ARG1(EVENT_TYPE_CONFIG, EVENT_CODE_CONFIG_GOAL_APPLIED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_CONFIG,
				EVENT_CODE_CONFIG_TOPOLOGY_ADDED_CONFIGURED_DEVICE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_CONFIG, EVENT_CODE_CONFIG_TOPOLOGY_ADDED_NEW_DEVICE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_CONFIG, EVENT_CODE_CONFIG_TOPOLOGY_MISSING_DEVICE),
		TWO_DEVICE_UIDS(EVENT_TYPE_CONFIG, EVENT_CODE_CONFIG_TOPOLOGY_REPLACED_CONFIGURED_DEVICE),
		TWO_DEVICE_UIDS(EVENT_TYPE_CONFIG, EVENT_CODE_CONFIG_TOPOLOGY_REPLACED_NEW_DEVICE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_CONFIG, EVENT_CODE_CONFIG_TOPOLOGY_MOVED_DEVICE),

		// Device Health Events
		DEVICE_UID_IN_ARG1(EVENT_TYPE_HEALTH, EVENT_CODE_HEALTH_HEALTH_STATE_CHANGED),
		NAMESPACE_UID(EVENT_TYPE_HEALTH, EVENT_CODE_HEALTH_NAMESPACE_HEALTH_STATE_CHANGED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_HEALTH, EVENT_CODE_HEALTH_NEW_FWERRORS_FOUND),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_HEALTH, EVENT_CODE_HEALTH_SANITIZE_INPROGRESS),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_HEALTH, EVENT_CODE_HEALTH_SANITIZE_COMPLETE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_HEALTH, EVENT_CODE_HEALTH_SMART_HEALTH),

		// Configuration Change events
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_CONFIG_GOAL_CREATED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_CONFIG_GOAL_DELETED),
		NAMESPACE_UID(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_NAMESPACE_CREATED),
		NAMESPACE_UID(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_NAMESPACE_DELETED),
		NAMESPACE_UID(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_NAMESPACE_MODIFIED),
		DEVICE_UID_WITH_DATA(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_SENSOR_SETTINGS_CHANGE,
				EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_DEVICE_UID, EVENT_ARG_TYPE_OTHER),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_FIRMWARE_UPDATE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_SECURITY_PASSWORD_SET),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_SECURITY_PASSWORD_REMOVED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_SECURITY_SECURE_ERASE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_MGMT, EVENT_CODE_MGMT_SECURITY_FROZEN),

		// Diagnostic Quick Health Check events
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_SUCCESS),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK,
				EVENT_CODE_DIAG_QUICK_NOT_MANAGEABLE_VENDOR_ID),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK,
				EVENT_CODE_DIAG_QUICK_NOT_MANAGEABLE_DEVICE_ID),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK,
				EVENT_CODE_DIAG_QUICK_NOT_MANAGEABLE_FW_API),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_HEALTH),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_MEDIA_TEMP),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_SPARE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_PERCENT_USED),
		// Available events 508-510
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 508),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 509),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 510),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_CORE_TEMP),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 512),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_UNREADABLE_BSR),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_MEDIA_NOT_READY),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_MEDIA_READY_ERROR),

		// Available events 515-518
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 515),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 516),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 517),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 518),

		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_NO_POST_CODE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK,
				EVENT_CODE_DIAG_QUICK_FW_INITIALIZATION_INCOMPLETE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_FW_HIT_ASSERT),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_FW_STALLED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_VIRAL_STATE),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_DRIVER),

		// Available event 525
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 525),

		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_MEDIA_TEMP_THROTTLING),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_MEDIA_TEMP_SHUTDOWN),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_BAD_CORE_TEMP_SHUTDOWN),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_NO_SPARE_DIE),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_UNSAFE_SHUTDOWN),
		// Reserved event 531
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 531),
		// Reserved event 532
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_QUICK, 532),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_AIT_DRAM_NOT_READY),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_MEDIA_DISABLED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_AIT_DRAM_DISABLED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_FW_LOAD_FAILED),

		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_DDRT_IO_INIT_FAILED),

		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_QUICK, EVENT_CODE_DIAG_QUICK_INTERNAL_CPU_ERROR),


		// Diagnostic Platform Config events
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_SUCCESS),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_NO_DIMMS),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_INVALID_NFIT),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_INVALID_PCAT),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_INVALID_PCD),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_INVALID_CURRENT_PCD),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_UNCONFIGURED),
		NO_UID_CUSTOM_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_BROKEN_ISET,
				EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_SERIAL_NUM, EVENT_ARG_TYPE_OTHER),
		NO_UID_CUSTOM_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DUPLICATE_SERIAL_NUMBERS,
				EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_SERIAL_NUM, EVENT_ARG_TYPE_OTHER),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_REBOOT_NEEDED_TO_APPLY_GOAL),

		// Available event 610
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, 610),

		POOL_UID(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_POOL_NEEDS_APP_DIRECT_NAMESPACES),

		// Available event 612
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, 612),

		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_CONFIG_UNBALANCED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMMS_DIFFERENT_SIZES),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_SECURITY_SKUS_MIXED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_MODE_SKUS_MIXED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_DIE_SPARING_SKUS_MIXED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_INIT_FAILED),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_SKU_VIOLATION),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_DIMM_GOAL_SKU_VIOLATION),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG, EVENT_CODE_DIAG_PCONFIG_POOLS_FAILED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_NAMESPACES_FAILED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_NO_BIOS_CONFIG_SUPPORT),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_GOAL_FAILED_CONFIG_ERROR),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_GOAL_FAILED_INSUFFICIENT_RESOURCES),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_GOAL_FAILED_FW_ERROR),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_GOAL_FAILED_UNKNOWN),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_PLATFORM_CONFIG,
				EVENT_CODE_DIAG_PCONFIG_BROKEN_ISET_MOVED),

		// Security Diagnostic events
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_SECURITY, EVENT_CODE_DIAG_SECURITY_SUCCESS),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_SECURITY, EVENT_CODE_DIAG_SECURITY_NO_DIMMS),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_SECURITY, EVENT_CODE_DIAG_SECURITY_INCONSISTENT),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_SECURITY, EVENT_CODE_DIAG_SECURITY_ALL_DISABLED),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_SECURITY, EVENT_CODE_DIAG_SECURITY_ALL_NOTSUPPORTED),

		// FW Diagnostic Events
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_FW_CONSISTENCY, EVENT_CODE_DIAG_FW_SUCCESS),
		NO_UID_GENERIC_DATA(EVENT_TYPE_DIAG_FW_CONSISTENCY, EVENT_CODE_DIAG_FW_NO_DIMMS),
		NO_UID_CUSTOM_DATA(EVENT_TYPE_DIAG_FW_CONSISTENCY, EVENT_CODE_DIAG_FW_INCONSISTENT,
				EVENT_ARG_TYPE_DEVICE_UID_LIST, EVENT_ARG_TYPE_OTHER, EVENT_ARG_TYPE_OTHER),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY,
				EVENT_CODE_DIAG_FW_BAD_TEMP_MEDIA_THRESHOLD),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY,
				EVENT_CODE_DIAG_FW_BAD_TEMP_CONTROLLER_THRESHOLD),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY, EVENT_CODE_DIAG_FW_BAD_SPARE_BLOCK),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY, EVENT_CODE_DIAG_FW_BAD_FW_LOG_LEVEL),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY, EVENT_CODE_DIAG_FW_SYSTEM_TIME_DRIFT),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY,
				EVENT_CODE_DIAG_FW_BAD_POWER_MGMT_POLICY),
		DEVICE_UID_IN_ARG1(EVENT_TYPE_DIAG_FW_CONSISTENCY,
				EVENT_CODE_DIAG_FW_BAD_DIE_SPARING_POLICY),

		END_OF_TABLE
};

NVM_BOOL is_end_of_metadata_table(const struct event_field_metadata *p_table_entry)
{
	COMMON_LOG_ENTRY();
	NVM_BOOL is_end = 0;

	struct event_field_metadata last_table_entry = END_OF_TABLE;
	if (memcmp(p_table_entry, &last_table_entry, sizeof (struct event_field_metadata)) == 0)
	{
		is_end = 1;
	}

	COMMON_LOG_EXIT_RETURN_I(is_end);
	return is_end;
}

NVM_BOOL is_table_entry_for_event(const struct event_field_metadata *p_table_entry,
		const enum event_type type, const NVM_UINT32 code)
{
	COMMON_LOG_ENTRY();
	NVM_BOOL result = (p_table_entry->event_type == type &&
			p_table_entry->event_code == code);

	COMMON_LOG_EXIT_RETURN_I(result);
	return result;
}

struct event_field_metadata get_event_field_metadata(const enum event_type type,
		const NVM_UINT32 code)
{
	COMMON_LOG_ENTRY();

	struct event_field_metadata result = END_OF_TABLE;

	const struct event_field_metadata *p_table_entry = &(EVENT_FIELD_METADATA_TABLE[0]);
	while (!is_end_of_metadata_table(p_table_entry))
	{
		if (is_table_entry_for_event(p_table_entry, type, code))
		{
			result = *p_table_entry;
			break;
		}

		p_table_entry++;
	}

	COMMON_LOG_EXIT();
	return result;
}

NVM_BOOL event_field_metadata_includes_arg_type(const struct event_field_metadata *p_metadata,
		const enum event_arg_type arg_type)
{
	COMMON_LOG_ENTRY();

	NVM_BOOL result = 0;
	if (EVENT_METADATA_HAS_TYPE_IN_ARG((*p_metadata), arg_type, 1) ||
			EVENT_METADATA_HAS_TYPE_IN_ARG((*p_metadata), arg_type, 2) ||
			EVENT_METADATA_HAS_TYPE_IN_ARG((*p_metadata), arg_type, 3))
	{
		result = 1;
	}

	COMMON_LOG_EXIT_RETURN_I(result);
	return result;
}
