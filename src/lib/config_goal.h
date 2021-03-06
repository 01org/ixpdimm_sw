/*
 * Copyright (c) 2015 2016, Intel Corporation
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

/*
 * This file contains common functions related to converting between platform config data
 * and a config goal.
 */

#ifndef	CONFIG_GOAL_H_
#define	CONFIG_GOAL_H_

#include "export_api.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include "platform_config_data.h"

/*
 * Convert a platform config data input table to a config goal structure.
 */
int config_input_table_to_config_goal(const NVM_UID device_uid,
		unsigned char *p_table,
		const NVM_UINT32 ext_table_offset,
		const NVM_UINT32 table_length,
		struct config_goal *p_config_goal);

enum config_goal_status get_config_goal_status_from_platform_config_data(
		struct platform_config_data *p_config);

NVM_API void config_goal_to_partition_ext_table(const struct config_goal *p_goal,
    struct partition_size_change_extension_table *p_table,
    const struct device_discovery *p_discovery);

NVM_API int validate_config_goal(const struct config_goal *p_goal,
    const struct nvm_capabilities *p_capabilities,
    const struct device_discovery *p_discovery);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_GOAL_H_ */
