/*
 * Copyright (c) 2015, Intel Corporation
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
 * This file contains the NVMCLI system related commands.
 */

#include <vector>
#include <string>
#include <string.h>
#include <algorithm>
#include <fstream>

#include <LogEnterExit.h>

#include <server/BaseServerFactory.h>
#include <physical_asset/NVDIMMViewFactory.h>
#include <physical_asset/NVDIMMFactory.h>
#include <erasure/ErasureServiceFactory.h>
#include <mem_config/MemoryResourcesFactory.h>
#include <server/SystemCapabilitiesFactory.h>
#include <intel_cim_framework/Types.h>

#include <intel_cli_framework/CliFrameworkTypes.h>
#include <intel_cli_framework/FeatureBase.h>
#include <intel_cli_framework/CommandSpec.h>
#include <intel_cli_framework/SimpleListResult.h>
#include <intel_cli_framework/PropertyListResult.h>
#include <intel_cli_framework/SyntaxErrorMissingValueResult.h>
#include <intel_cli_framework/SyntaxErrorBadValueResult.h>
#include <intel_cli_framework/NotImplementedErrorResult.h>
#include <intel_cli_framework/Parser.h>
#include <physical_asset/MemoryTopologyViewFactory.h>
#include "CommandParts.h"
#include "WbemToCli_utilities.h"
#include "SystemFeature.h"
#include <exception/NvmExceptionBadTarget.h>
#include <exception/NvmExceptionLibError.h>



#ifdef __WINDOWS__
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

const std::string cli::nvmcli::SystemFeature::Name = "System";

/*
 * Command Specs the System Feature supports
 */
void cli::nvmcli::SystemFeature::getPaths(cli::framework::CommandSpecList &list)
{
	framework::CommandSpec showSystem(SHOW_SYSTEM, TR("Show Host Server"), framework::VERB_SHOW,
			TR("Show basic information about the host server."));
	showSystem.addOption(framework::OPTION_ALL);
	showSystem.addOption(framework::OPTION_DISPLAY);
	showSystem.addTarget(TARGET_SYSTEM_R)
			.helpText(TR("The host server. No filtering is supported on this target."))
			.isValueAccepted(false);

	framework::CommandSpec showDevices(SHOW_DEVICES, TR("Show Device"), framework::VERB_SHOW,
			TR("Show information about one or more " NVM_DIMM_NAME "s."));
	showDevices.addOption(framework::OPTION_DISPLAY)
			.helpText(TR("Filter the returned attributes by explicitly specifying a comma separated "
				"list of attributes."));
	showDevices.addOption(framework::OPTION_ALL);
	showDevices.addTarget(TARGET_DIMM_R)
			.helpText(TR("Restrict output to specific " NVM_DIMM_NAME "s by supplying the dimm target and one "
				"or more comma-separated " NVM_DIMM_NAME " identifiers. The default is to display "
				"all " NVM_DIMM_NAME "s."));
	showDevices.addTarget(TARGET_SOCKET)
			.helpText(TR("Restrict output to the " NVM_DIMM_NAME "s installed on specific sockets by "
				"supplying the socket target and one or more comma-separated socket identifiers. "
				"The default is to display all sockets."));

	framework::CommandSpec modifyDevice(MODIFY_DEVICE, TR("Modify Device"), framework::VERB_SET,
			TR("Change the configurable setting(s) on one or more " NVM_DIMM_NAME "s."));
	modifyDevice.addOption(framework::OPTION_FORCE).helpText(
			TR("Changing " NVM_DIMM_NAME " setting(s) is a potentially destructive operation which requires "
				"confirmation from the user for each " NVM_DIMM_NAME ". This option suppresses the confirmation."));
	modifyDevice.addTarget(TARGET_DIMM.name, true,
			DIMMIDS_STR, false,
			TR("Modify specific " NVM_DIMM_NAME "s by supplying one or more comma-separated " NVM_DIMM_NAME " identifiers. "
			"However, this is not recommended as it may put the system in an undesirable state. "
			"The default is to modify all manageable " NVM_DIMM_NAME "s."));
	modifyDevice.addProperty(FIRSTFASTREFRESH_PROPERTYNAME, true, "0 | 1", true,
			TR("Whether acceleration of the first refresh cycle is enabled."));

	framework::CommandSpec setFwLogging(SET_FW_LOGGING, TR("Set Firmware Logging"), framework::VERB_SET,
			TR("Set the firmware logging level on one or more " NVM_DIMM_NAME "s."));
	setFwLogging.addTarget(TARGET_DIMM.name, true,
			DIMMIDS_STR, false,
			TR("Modify specific " NVM_DIMM_NAME "s by supplying one or more comma-separated " NVM_DIMM_NAME " identifiers. "
			"However, this is not recommended as it may put the system in an undesirable state. "
			"The default is to modify all manageable " NVM_DIMM_NAME "s."));
	setFwLogging.addProperty(FWLOGLEVEL_PROPERTYNAME, true, "Disabled | Error | Warning | Info | Debug", true,
			TR("The firmware logging level."));

	framework::CommandSpec enableDeviceSecurity(ENABLE_DEVICE_SECURITY, TR("Enable Device Security"), framework::VERB_SET,
			TR("Enable security by setting a passphrase on one or more " NVM_DIMM_NAME "s."));
	enableDeviceSecurity.addOption(framework::OPTION_SOURCE)
			.helpText(TR("File path to a local file containing the new passphrase (1-32 characters)."));
	enableDeviceSecurity.addTarget(TARGET_DIMM.name, true,
			DIMMIDS_STR, false,
			TR("Set the passphrase on specific " NVM_DIMM_NAME "s by supplying one or more "
			"comma-separated " NVM_DIMM_NAME " identifiers. However, this is not recommended as it may "
			"put the system in an undesirable state. The default is to set the "
			"passphrase on all manageable " NVM_DIMM_NAME "s."));
	enableDeviceSecurity.addProperty(NEWPASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("The new passphrase (1-32 characters)."));
	enableDeviceSecurity.addProperty(CONFIRMPASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("Confirmation of the new passphrase (1-32 characters and must match NewPassphrase)."));

	framework::CommandSpec changeDevicePassphrase(CHANGE_DEVICE_PASSPHRASE, TR("Change Device Passphrase"), framework::VERB_SET,
			TR("Change the security passphrase on one or more " NVM_DIMM_NAME "s."));
	changeDevicePassphrase.addOption(framework::OPTION_SOURCE)
			.helpText(TR("File path to a local file containing the new passphrase (1-32 characters)."));
	changeDevicePassphrase.addTarget(TARGET_DIMM.name, true,
			DIMMIDS_STR, false,
			TR("Change the passphrase on specific " NVM_DIMM_NAME "s by supplying one or more "
			"comma-separated " NVM_DIMM_NAME " identifiers. However, this is not recommended as it may "
			"put the system in an undesirable state. The default is to change the "
			"passphrase on all manageable " NVM_DIMM_NAME "s."));
	changeDevicePassphrase.addProperty(PASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("The current passphrase (1-32 characters)."));
	changeDevicePassphrase.addProperty(NEWPASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("The new passphrase (1-32 characters)."));
	changeDevicePassphrase.addProperty(CONFIRMPASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("Confirmation of the new passphrase (1-32 characters and must match NewPassphrase)."));

	framework::CommandSpec changeDeviceSecurity(CHANGE_DEVICE_SECURITY, TR("Change Device Security"), framework::VERB_SET,
			TR("Unlock or disable security on one or more " NVM_DIMM_NAME "s by supplying the passphrase "
				"and the desired lock state."));
	changeDeviceSecurity.addOption(framework::OPTION_SOURCE)
			.helpText(TR("File path to a local file containing the new passphrase (1-32 characters)."));
	changeDeviceSecurity.addTarget(TARGET_DIMM.name, true,
			DIMMIDS_STR, false,
			TR("Change the lock state of a specific " NVM_DIMM_NAME "s by supplying one or more "
				"comma-separated " NVM_DIMM_NAME " identifiers. However, this is not recommended as it "
				"may put the system in an undesirable state. The default is to modify all "
				"manageable " NVM_DIMM_NAME "s."));
	changeDeviceSecurity.addProperty(LOCKSTATE_PROPERTYNAME, true, "Unlocked|Disabled", true,
			TR("The desired lock state."));
	changeDeviceSecurity.addProperty(PASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("The current passphrase (1-32 characters)."));


	framework::CommandSpec eraseDeviceData(ERASE_DEVICE_DATA, TR("Erase Device Data"), framework::VERB_DELETE,
			TR("Erase the persistent data on one or more " NVM_DIMM_NAME "s."));
	eraseDeviceData.addOption(framework::OPTION_SOURCE)
			.helpText(TR("File path to a local file containing the new passphrase (1-32 characters)."));
	eraseDeviceData.addTarget(TARGET_DIMM.name, true,
			DIMMIDS_STR, false,
			TR("Erase specific " NVM_DIMM_NAME "s by supplying one or more comma-separated " NVM_DIMM_NAME " identifiers. "
			"However, this is not recommended as it may put the system in an undesirable state. "
			"The default is to erase all manageable " NVM_DIMM_NAME "s."));
	eraseDeviceData.addOption(framework::OPTION_FORCE).helpText(
			TR("Erasing " NVM_DIMM_NAME " data is a destructive operation which requires confirmation from "
			"the user for each " NVM_DIMM_NAME ". This option suppresses the confirmation."));
	eraseDeviceData.addProperty(PASSPHRASE_PROPERTYNAME, true, STRING_PARAM, false,
			TR("The current passphrase (1-32 characters). "
			"Required if security is enabled on the " NVM_DIMM_NAME "."));
// OVERWRITE (3/19/15): Overwrite has been removed however it may come back.
//	eraseDeviceData.addProperty(ERASETYPE_PROPERTYNAME, false,
//			"Crypto|MultiOverwrite|QuickOverwrite", true,
//			TR("The type of erase to be performed. Crypto is default if not specified."));

	framework::CommandSpec showMemoryResources(SHOW_MEMORYRESOURCES, TR("Show Memory Resources"), framework::VERB_SHOW,
			TR("Show the total " NVM_DIMM_NAME " memory resource allocation across the host server."));
	showMemoryResources.addTarget(TARGET_MEMORYRESOURCES_R)
			.helpText(TR("The " NVM_DIMM_NAME " memory resources. No filtering is supported on this target."))
			.isValueAccepted(false);

	framework::CommandSpec showSystemCap(SHOW_SYSTEM_CAPABILITIES, TR("Show System Capabilities"),
			framework::VERB_SHOW, TR("Show the platform supported " NVM_DIMM_NAME " capabilities."));
	showSystemCap.addOption(framework::OPTION_DISPLAY);
	showSystemCap.addOption(framework::OPTION_ALL);
	showSystemCap.addTarget(TARGET_SYSTEM_R)
			.helpText(TR(NVM_DIMM_NAME" platform supported capabilities apply to the entire host server. "
				"No filtering is supported on this target."))
			.isValueAccepted(false);
	showSystemCap.addTarget(TARGET_CAPABILITIES_R)
			.helpText(TR("The platform supported " NVM_DIMM_NAME " capabilities. "
				"No filtering is supported on this target."))
			.isValueAccepted(false);

	cli::framework::CommandSpec showTopology(SHOW_TOPOLOGY, TR("Show Topology"), framework::VERB_SHOW,
			TR("Show the topology of the memory installed in the host server. Use the command Show Device "
					   "to view more detailed information about an " NVM_DIMM_NAME "."));
	showTopology.addOption(framework::OPTION_DISPLAY);
	showTopology.addOption(framework::OPTION_ALL);
	showTopology.addTarget(TARGET_TOPOLOGY_R).isValueAccepted(false);
	showTopology.addTarget(TARGET_DIMM.name, false, DIMMIDS_STR, true,
			TR("Restrict output to specific DIMMs by supplying the dimm target and one or more "
				"comma-separated DIMM identifiers. The default is to display all DIMMs."));
	showTopology.addTarget(TARGET_SOCKET.name, false, SOCKETIDS_STR, true,
			TR("Restrict output to the DIMMs installed on specific sockets by supplying the "
				"socket target and one or more comma-separated socket identifiers. The default is "
				"to display all sockets."));

	list.push_back(showSystem);
	list.push_back(showDevices);
	list.push_back(modifyDevice);
	list.push_back(setFwLogging);
	list.push_back(changeDevicePassphrase);
	list.push_back(enableDeviceSecurity);
	list.push_back(changeDeviceSecurity);
	list.push_back(eraseDeviceData);
	list.push_back(showMemoryResources);
	list.push_back(showSystemCap);
	list.push_back(showTopology);
}

// Constructor, just calls super class
cli::nvmcli::SystemFeature::SystemFeature() : cli::framework::FeatureBase(),
	m_pTopologyProvider(NULL)
{
	setTopologyProvider(new wbem::physical_asset::MemoryTopologyViewFactory());
}

cli::nvmcli::SystemFeature::~SystemFeature()
{
	// Clean up dynamically-allocated member providers

	if (m_pTopologyProvider)
	{
		delete m_pTopologyProvider;
		m_pTopologyProvider = NULL;
	}
}

void cli::nvmcli::SystemFeature::setTopologyProvider(wbem::physical_asset::MemoryTopologyViewFactory *pTopologyProvider)
{
	if (m_pTopologyProvider)
	{
		delete m_pTopologyProvider;
	}
	m_pTopologyProvider = pTopologyProvider;
}

/*
 * Get all the BaseServer Instances from the wbem base server factory.
 */
cli::framework::ResultBase *cli::nvmcli::SystemFeature::run(
		const int &commandId, const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);
	framework::ResultBase *pResult = NULL;

	switch (commandId)
	{
	case SHOW_SYSTEM:
		pResult = showSystem(parsedCommand);
		break;
	case SHOW_DEVICES:
		pResult = showDimms(parsedCommand);
		break;
	case CHANGE_DEVICE_PASSPHRASE:
		pResult = changeDevicePassphrase(parsedCommand);
		break;
	case ENABLE_DEVICE_SECURITY:
		pResult = enableDeviceSecurity(parsedCommand);
		break;
	case CHANGE_DEVICE_SECURITY:
		pResult = changeDeviceSecurity(parsedCommand);
		break;
	case ERASE_DEVICE_DATA:
		pResult = eraseDeviceData(parsedCommand);
		break;
	case SHOW_MEMORYRESOURCES:
		pResult = showMemoryResources(parsedCommand);
		break;
	case SHOW_SYSTEM_CAPABILITIES:
		pResult = showSystemCapabilities(parsedCommand);
		break;
	case SHOW_TOPOLOGY:
		pResult = showTopology(parsedCommand);
		break;
	case MODIFY_DEVICE:
		pResult = modifyDevice(parsedCommand);
		break;
	case SET_FW_LOGGING:
		pResult = setFwLogging(parsedCommand);
		break;
	default:
		pResult = new framework::NotImplementedErrorResult(commandId, Name);
		break;
	}

	return pResult;
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::showSystem(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);
	framework::ResultBase *pResult = NULL;
	try
	{
		// get the host server info from wbem
		wbem::server::BaseServerFactory serverProvider;

		// populate the default attributes
		wbem::framework::attribute_names_t defaultAttributes;
		defaultAttributes.push_back(wbem::NAME_KEY);
		defaultAttributes.push_back(wbem::OSNAME_KEY);
		defaultAttributes.push_back(wbem::OSVERSION_KEY);
		defaultAttributes.push_back(wbem::MIXEDSKU_KEY);
		defaultAttributes.push_back(wbem::SKUVIOLATION_KEY);

		// populate the all option attributes
		wbem::framework::attribute_names_t allAttributes(defaultAttributes);
		allAttributes.push_back(wbem::LOGLEVEL_KEY);

		// retrieve the actual based on the user input
		wbem::framework::attribute_names_t display_attributes =
				GetAttributeNames(parsedCommand.options, defaultAttributes, allAttributes);

		wbem::framework::attribute_names_t wbem_attributes(display_attributes);
		wbem_attributes.push_back(wbem::OPERATIONALSTATUS_KEY);
		FilterAttributeNames(wbem_attributes, wbem::MIXEDSKU_KEY);
		FilterAttributeNames(wbem_attributes, wbem::SKUVIOLATION_KEY);

		// get the server information
		wbem::framework::instances_t *pInstances = serverProvider.getInstances(wbem_attributes);
		if (pInstances)
		{
			// convert operational status to sku violation and mixed sku
			convertSystemOpStatusToSku(*(pInstances->begin()));

			cli::framework::PropertyListResult *pListResult =
					NvmInstanceToPropertyListResult(*(pInstances->begin()), display_attributes);
			pListResult->setName("Host");
			pResult = pListResult;
			delete pInstances;
		}
		// failures will throw an exception, this would prevent a crash due to an unexpected error
		else
		{
			if (NULL != pResult)
			{
				delete pResult;
			}
			pResult = new framework::ErrorResult(framework::ErrorResult::ERRORCODE_UNKNOWN,
				TRS(nvmcli::UNKNOWN_ERROR_STR));
		}
	}
	catch (wbem::framework::Exception &e)
	{
		if (NULL != pResult)
		{
			delete pResult;
		}
		pResult = NvmExceptionToResult(e);
	}
	return pResult;
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::showDimms(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);
	framework::ResultBase *pResult = NULL;
	try
	{
		wbem::physical_asset::NVDIMMViewFactory dimmProvider;

		// define default display attributes
		wbem::framework::attribute_names_t defaultAttributes;
		defaultAttributes.push_back(wbem::DIMMID_KEY);
		defaultAttributes.push_back(wbem::CAPACITY_KEY);
		defaultAttributes.push_back(wbem::FWVERSION_KEY);
		defaultAttributes.push_back(wbem::LOCKSTATE_KEY);
		defaultAttributes.push_back(wbem::HEALTHSTATE_KEY);
		defaultAttributes.push_back(wbem::ACTIONREQUIRED_KEY);

		wbem::framework::attribute_names_t allAttributes(defaultAttributes);
		allAttributes.push_back(wbem::DIMMGUID_KEY);
		allAttributes.push_back(wbem::DIMMHANDLE_KEY);
		allAttributes.push_back(wbem::PHYSICALID_KEY);
		allAttributes.push_back(wbem::MANAGEABILITYSTATE_KEY);
		allAttributes.push_back(wbem::SKUVIOLATION_KEY);
		allAttributes.push_back(wbem::FWAPIVERSION_KEY);
		allAttributes.push_back(wbem::INTERFACEFORMATCODE_KEY);
		allAttributes.push_back(wbem::SOCKETID_KEY);
		allAttributes.push_back(wbem::MEMCONTROLLERID_KEY);
		allAttributes.push_back(wbem::MEMORYTYPE_KEY);
		allAttributes.push_back(wbem::MANUFACTURER_KEY);
		allAttributes.push_back(wbem::MANUFACTURERID_KEY);
		allAttributes.push_back(wbem::MODEL_KEY);
		allAttributes.push_back(wbem::VENDORID_KEY);
		allAttributes.push_back(wbem::DEVICEID_KEY);
		allAttributes.push_back(wbem::REVISIONID_KEY);
		allAttributes.push_back(wbem::SERIALNUMBER_KEY);
		allAttributes.push_back(wbem::ISNEW_KEY);
		allAttributes.push_back(wbem::FORMFACTOR_KEY);
		allAttributes.push_back(wbem::VOLATILECAPACITY_KEY);
		allAttributes.push_back(wbem::PERSISTENTCAPACITY_KEY);
		allAttributes.push_back(wbem::UNCONFIGUREDCAPACITY_KEY);
		allAttributes.push_back(wbem::INACCESSIBLECAPACITY_KEY);
		allAttributes.push_back(wbem::RESERVEDCAPACITY_KEY);
		allAttributes.push_back(wbem::PARTNUMBER_KEY);
		allAttributes.push_back(wbem::DEVICELOCATOR_KEY);
		allAttributes.push_back(wbem::BANKLABEL_KEY);
		allAttributes.push_back(wbem::CHANNEL_KEY);
		allAttributes.push_back(wbem::CHANNELPOS_KEY);
		allAttributes.push_back(wbem::DATAWIDTH_KEY);
		allAttributes.push_back(wbem::TOTALWIDTH_KEY);
		allAttributes.push_back(wbem::SPEED_KEY);
		allAttributes.push_back(wbem::FWLOGLEVEL_KEY);
		allAttributes.push_back(wbem::POWERMANAGEMENTENABLED_KEY);
		allAttributes.push_back(wbem::POWERLIMIT_KEY);
		allAttributes.push_back(wbem::PEAKPOWERBUDGET_KEY);
		allAttributes.push_back(wbem::AVGPOWERBUDGET_KEY);
		allAttributes.push_back(wbem::DIESPARINGENABLED_KEY);
		allAttributes.push_back(wbem::DIESPARINGLEVEL_KEY);
		allAttributes.push_back(wbem::LASTSHUTDOWNSTATUS_KEY);
		allAttributes.push_back(wbem::DIESPARESUSED_KEY);
		allAttributes.push_back(wbem::FIRSTFASTREFRESH_KEY);
		allAttributes.push_back(wbem::ACTIONREQUIREDEVENTS_KEY);
		allAttributes.push_back(wbem::LASTSHUTDOWNTIME_KEY);
		allAttributes.push_back(wbem::SECURITYCAPABILITIES_KEY);
		allAttributes.push_back(wbem::DIESPARINGCAPABLE_KEY);
		allAttributes.push_back(wbem::MEMORYMODESSUPPORTED_KEY);

		// get the desired attributes
		wbem::framework::attribute_names_t attributes =
				GetAttributeNames(parsedCommand.options, defaultAttributes, allAttributes);

		// make sure we have the id in our display
		// this would cover the case the user asks for specific display attributes, but they
		// don't include the handle
		if (!wbem::framework_interface::NvmInstanceFactory::containsAttribute(wbem::DIMMID_KEY, attributes))
		{
			attributes.insert(attributes.begin(), wbem::DIMMID_KEY);
		}

		// create the display filters
		wbem::framework::attribute_names_t requestedAttributes = attributes;
		cli::nvmcli::filters_t filters;
		generateDimmFilter(parsedCommand, requestedAttributes, filters);
		generateSocketFilter(parsedCommand, requestedAttributes, filters);

		// get all instances
		wbem::framework::instances_t *pInstances = dimmProvider.getInstances(requestedAttributes);
		if (pInstances)
		{
			// VendorID and DeviceID are displayed as hex without the '0x' prefix
			for (size_t i = 0; i < pInstances->size(); i++)
			{
				wbem::framework::Attribute vendorId;
				if ((*pInstances)[i].getAttribute(wbem::VENDORID_KEY, vendorId) == wbem::framework::SUCCESS)
				{
					std::stringstream vendorIdStream;
					vendorIdStream << std::hex << vendorId.uintValue();
					wbem::framework::Attribute vendorIdStr(vendorIdStream.str(), false);
					(*pInstances)[i].setAttribute(wbem::VENDORID_KEY, vendorIdStr);
				}

				wbem::framework::Attribute deviceId;
				if ((*pInstances)[i].getAttribute(wbem::DEVICEID_KEY, deviceId) == wbem::framework::SUCCESS)
				{
					std::stringstream deviceIdStream;
					deviceIdStream << std::hex << deviceId.uintValue();
					wbem::framework::Attribute deviceIdStr(deviceIdStream.str(), false);
					(*pInstances)[i].setAttribute(wbem::DEVICEID_KEY, deviceIdStr);
				}

				// FirstFastRefresh is displayed as 0 for disabled and 1 for enabled
				wbem::framework::Attribute firstFastRefresh;
				if ((*pInstances)[i].getAttribute(wbem::FIRSTFASTREFRESH_KEY, firstFastRefresh) == wbem::framework::SUCCESS)
				{
					std::stringstream firstFastRefreshStream;
					firstFastRefreshStream << firstFastRefresh.uintValue();
					wbem::framework::Attribute firstFastRefreshStr(firstFastRefreshStream.str(), false);
					(*pInstances)[i].setAttribute(wbem::FIRSTFASTREFRESH_KEY, firstFastRefreshStr);
				}

				wbem::framework::Attribute arEventAttr;
				if ((*pInstances)[i].getAttribute(wbem::ACTIONREQUIREDEVENTS_KEY, arEventAttr) == wbem::framework::SUCCESS)
				{
					wbem::framework::STR_LIST arEventList = arEventAttr.strListValue();
					if (!arEventList.size())
					{
						arEventList.push_back(wbem::NA);
						(*pInstances)[i].setAttribute(wbem::ACTIONREQUIREDEVENTS_KEY,
										wbem::framework::Attribute(arEventList, false));
					}
				}

				convertSecurityCapabilities((*pInstances)[i]);
				
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::CAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::VOLATILECAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::PERSISTENTCAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::UNCONFIGUREDCAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::INACCESSIBLECAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::RESERVEDCAPACITY_KEY);

				updateLastShutDownStatus((*pInstances)[i]);
				updateLastShutDownTime((*pInstances)[i]);
			}

			// format the return data, or throw invalid parameter exception
			pResult = NvmInstanceToObjectListResult(*pInstances, "Dimm",
					wbem::DIMMID_KEY, attributes, filters);

			// Set layout to table unless the -all or -display option is present
			if (!framework::parsedCommandContains(parsedCommand, framework::OPTION_DISPLAY) &&
				!framework::parsedCommandContains(parsedCommand, framework::OPTION_ALL))
			{
				pResult->setOutputType(framework::ResultBase::OUTPUT_TEXTTABLE);
			}
		}
		// failures will throw an exception, this would prevent a crash due to an unexpected error
		else
		{
			pResult = new framework::ErrorResult(framework::ErrorResult::ERRORCODE_UNKNOWN,
				TRS(nvmcli::UNKNOWN_ERROR_STR));
		}
	}
	catch (wbem::framework::Exception &e)
	{
		if (NULL != pResult)
		{
			delete pResult;
		}
		pResult = NvmExceptionToResult(e);
	}

	return pResult;
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::modifyDevice(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	std::string basePrefix;
	std::vector<std::string> dimms;
	framework::ResultBase *pResults = NULL;

	std::string firstFastRefresh = framework::Parser::getPropertyValue(parsedCommand, FIRSTFASTREFRESH_PROPERTYNAME);

	if ((!cli::framework::stringsIEqual(firstFastRefresh, ZERO_PROPERTYVALUE)) &&
			(!cli::framework::stringsIEqual(firstFastRefresh, ONE_PROPERTYVALUE)))
	{
		pResults = new framework::SyntaxErrorBadValueResult(
							framework::TOKENTYPE_PROPERTY,
							FIRSTFASTREFRESH_PROPERTYNAME.c_str(),
							firstFastRefresh);
	}
	else
	{
		basePrefix = TRS(MODIFYDEVICE_MSG);
		pResults = cli::nvmcli::getDimms(parsedCommand, dimms);
	}

	if (pResults == NULL)
	{
		wbem::physical_asset::NVDIMMFactory dimmFactory;

		wbem::framework::attributes_t attributes;
		if (cli::framework::stringsIEqual(firstFastRefresh, ZERO_PROPERTYVALUE))

		{
			wbem::framework::Attribute attr(false, false);
			attributes[wbem::FIRSTFASTREFRESH_KEY] = attr;
		}
		else
		{
			wbem::framework::Attribute attr(true, false);
			attributes[wbem::FIRSTFASTREFRESH_KEY] = attr;
		}

		framework::SimpleListResult *pListResults = new framework::SimpleListResult();

		std::vector<std::string>::iterator iter = dimms.begin();
		for (; iter != dimms.end(); iter++)
		{
			std::string dimmStr = wbem::physical_asset::NVDIMMViewFactory::guidToDimmIdStr((*iter));
			std::string prefix = cli::framework::ResultBase::stringFromArgList((basePrefix + " %s").c_str(),
					dimmStr.c_str());
			prefix += ": ";

			try
			{
				bool forceOption = parsedCommand.options.find(framework::OPTION_FORCE.name)
						!= parsedCommand.options.end();

				// if user didn't specify the force option, prompt them to continue
				std::string prompt = framework::ResultBase::stringFromArgList(
						MODIFY_DEV_PROMPT.c_str(), dimmStr.c_str());
				if (!forceOption && !promptUserYesOrNo(prompt))
				{
					pListResults->insert(prefix + cli::framework::UNCHANGED_MSG);
				}
				else
				{
					wbem::framework::ObjectPath path;
					dimmFactory.createPathFromGuid(*iter, path);
					dimmFactory.modifyInstance(path, attributes);

					pListResults->insert(prefix + TRS(cli::framework::SUCCESS_MSG));
				}
			}
			catch (wbem::framework::Exception &e)
			{
				pListResults->insert(prefix + e.what());
				SetResultErrorCodeFromException(*pListResults, e);
			}
		}
		pResults = pListResults;
	}

	return pResults;
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::setFwLogging(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	std::string basePrefix;
	std::vector<std::string> dimms;
	framework::ResultBase *pResults = NULL;

	std::string logLevelStr = framework::Parser::getPropertyValue(parsedCommand, FWLOGLEVEL_PROPERTYNAME);

	if ((!cli::framework::stringsIEqual(logLevelStr, DISABLED_PROPERTYVALUE)) &&
			(!cli::framework::stringsIEqual(logLevelStr, ERROR_PROPERTYVALUE)) &&
			(!cli::framework::stringsIEqual(logLevelStr, WARNING_PROPERTYVALUE)) &&
			(!cli::framework::stringsIEqual(logLevelStr, INFO_PROPERTYVALUE)) &&
			(!cli::framework::stringsIEqual(logLevelStr, DEBUG_PROPERTYVALUE)))
	{
		pResults = new framework::SyntaxErrorBadValueResult(
							framework::TOKENTYPE_PROPERTY,
							FWLOGLEVEL_PROPERTYNAME.c_str(),
							logLevelStr);
	}
	else
	{
		basePrefix = TRS(SETFWLOGGING_MSG);
		pResults = cli::nvmcli::getDimms(parsedCommand, dimms);
	}

	if (pResults == NULL)
	{
		wbem::physical_asset::NVDIMMFactory dimmFactory;

		enum fw_log_level logLevel = logLevelStringToEnum(logLevelStr);

		wbem::framework::attributes_t attributes;
		wbem::framework::Attribute attr(logLevel, false);
		attributes[wbem::FWLOGLEVEL_KEY] = attr;

		framework::SimpleListResult *pListResults = new framework::SimpleListResult();

		std::vector<std::string>::iterator iter = dimms.begin();
		for (; iter != dimms.end(); iter++)
		{
			std::string dimmStr = wbem::physical_asset::NVDIMMViewFactory::guidToDimmIdStr((*iter));
			std::string prefix = cli::framework::ResultBase::stringFromArgList((basePrefix + " %s").c_str(),
					dimmStr.c_str());
			prefix += ": ";

			try
			{
				wbem::framework::ObjectPath path;
				dimmFactory.createPathFromGuid(*iter, path);
				dimmFactory.modifyInstance(path, attributes);

				pListResults->insert(prefix + TRS(cli::framework::SUCCESS_MSG));
			}
			catch (wbem::framework::Exception &e)
			{
				pListResults->insert(prefix + e.what());
				SetResultErrorCodeFromException(*pListResults, e);
			}
		}
		pResults = pListResults;
	}
	return pResults;
}


enum return_code cli::nvmcli::SystemFeature::setFirstPassphrase(std::string *pPassphrase,
		std::string newValue)
{
	enum return_code rc = NVM_SUCCESS;
	if (pPassphrase != NULL)
	{
		if (!(*pPassphrase).empty())
		{
			// shouldn't have password twice in the file
			rc = NVM_ERR_BADFILE;
		}
		else
		{
			*pPassphrase = newValue;
		}
	}
	return rc;
}

enum return_code cli::nvmcli::SystemFeature::getPassphrasesFromString(
		std::string lineStr, std::string *pPassphrase, std::string *pNewPassphrase)
{
	enum return_code rc = NVM_SUCCESS;

	size_t pos = -1;
	// expect the file to contain 'passphrase=xxxx' and/or 'newpassphrase=xxxx'
	pos = lineStr.find("=");

	if (pos != std::string::npos)
	{
		std::string property = lineStr.substr(0, pos);
		std::transform(property.begin(), property.end(), property.begin(), ::tolower);

		if (pos > sizeof (property) + 1)
		{
			std::string value = lineStr.substr(pos + 1);
			std::string passphraseLowercase = PASSPHRASE_PROPERTYNAME;
			std::transform(passphraseLowercase.begin(), passphraseLowercase.end(),
					passphraseLowercase.begin(), ::tolower);
			std::string newPassphraseLowercase = NEWPASSPHRASE_PROPERTYNAME;
			std::transform(newPassphraseLowercase.begin(), newPassphraseLowercase.end(),
					newPassphraseLowercase.begin(), ::tolower);
			if (property.compare(passphraseLowercase) == 0)
			{
				// set passphrase property from file
				rc = setFirstPassphrase(pPassphrase, value);
			}
			else if (property.compare(newPassphraseLowercase) == 0)
			{
				rc = setFirstPassphrase(pNewPassphrase, value);
			} // no need for else - just ignore and it'll fail with invalid password
		}
	}
	return rc;
}

/*
 * helper method to pull passphrase from the specified file, returns pResults if failed
 */
cli::framework::ResultBase *cli::nvmcli::SystemFeature::readPassphrases(std::string passphraseFile,
		std::string *pPassphrase, std::string *pNewPassphrase, std::string prefix)
{
	enum return_code rc = NVM_SUCCESS;

	std::string line;
	cli::framework::ResultBase *pResults = NULL;

	std::ifstream readfile(passphraseFile.c_str());
	if (readfile.fail())
	{
		rc = NVM_ERR_BADFILE;
	}
	else
	{
		getline(readfile, line);
		// accept caps or lower case ascii indication
		std::transform(line.begin(), line.end(), line.begin(), ::tolower);
		if (0 != line.compare("#ascii"))
		{
			readfile.close();
			rc = NVM_ERR_BADFILE;
		}
		else
		{
			std::string line;

			enum return_code rc;
			while (getline(readfile, line) && (!pResults))
			{

				if ((rc = getPassphrasesFromString(line, pPassphrase, pNewPassphrase))
						!= NVM_SUCCESS)
				{
					break;
				}
			}
		}
	}

	if (rc != NVM_SUCCESS)
	{
		NVM_ERROR_DESCRIPTION errStr;
		nvm_get_error(rc, errStr, NVM_ERROR_LEN);
		pResults = new framework::ErrorResult(framework::ResultBase::ERRORCODE_UNKNOWN, errStr, prefix);
	}

	return pResults;
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::enableDeviceSecurity(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	framework::ResultBase *pResults = NULL;
	std::string basePrefix = TRS(SETPASSPHRASE_MSG);
	std::vector<std::string> dimms;

	std::string newPassphrase;
	std::string confirmPassphrase;

	// if user provided a password file as an option, read passwords from the file
	framework::StringMap::const_iterator source = parsedCommand.options.find(framework::OPTION_SOURCE.name);
	if (source != parsedCommand.options.end() && !source->second.empty())
	{
		// get path
		std::string passphraseFile = source->second;

		// password file option existed, so expect to use files for retrieving
		// all passwords
		pResults = readPassphrases(passphraseFile.c_str(), NULL, &newPassphrase, basePrefix);
		if (!pResults)
		{
			if (newPassphrase.empty())
			{
				pResults = new framework::SyntaxErrorMissingValueResult(
						framework::TOKENTYPE_PROPERTY,
						NEWPASSPHRASE_PROPERTYNAME.c_str());
			}
			else
			{
				confirmPassphrase = newPassphrase;
			}
		}
	}
	else
	{
		// file name source was not specified for passwords, expect caller to have provided them
		// on the command line
		newPassphrase = framework::Parser::getPropertyValue(parsedCommand, NEWPASSPHRASE_PROPERTYNAME);
		if (newPassphrase.empty())
		{
			// caller didn't give the new password, so prompt for it
			newPassphrase = promptUserHiddenString(TRS(NEW_PASSPHRASE_PROMPT));
		}

		confirmPassphrase = framework::Parser::getPropertyValue(parsedCommand, CONFIRMPASSPHRASE_PROPERTYNAME);
		if (confirmPassphrase.empty())
		{
			confirmPassphrase = promptUserHiddenString(TRS(CONFIRM_NEW_PASSPHRASE_PROMPT));
		}
	}

	// make sure confirm matches new
	if ((pResults == NULL) && (newPassphrase.compare(confirmPassphrase) != 0))
	{
		pResults = new framework::ErrorResult(
				ERRORCODE_SECURITY_PASSPHRASEMISSMATCH,
				TRS(ERRORMSG_SECURITY_PASSPHRASEMISSMATCH),
				basePrefix);
	}

	if (pResults == NULL)
	{
		pResults = cli::nvmcli::getDimms(parsedCommand, dimms);
	}

	if (pResults == NULL)
	{
		framework::SimpleListResult *pListResults = new framework::SimpleListResult();
		pResults = pListResults;
		wbem::physical_asset::NVDIMMFactory dimmProvider;

		for (std::vector<std::string>::const_iterator dimmIter = dimms.begin();
				dimmIter != dimms.end(); dimmIter++)
		{
			std::string prefix = cli::framework::ResultBase::stringFromArgList((basePrefix + " %s").c_str(),
					wbem::physical_asset::NVDIMMViewFactory::guidToDimmIdStr((*dimmIter)).c_str());
			prefix += ": ";
			try
			{
				dimmProvider.setPassphrase((*dimmIter), newPassphrase, "");
				pListResults->insert(prefix + TRS(cli::framework::SUCCESS_MSG));
			}
			catch (wbem::framework::Exception &e)
			{
				cli::framework::ErrorResult *eResult = NvmExceptionToResult(e);
				if (eResult)
				{
					pListResults->insert(prefix + eResult->outputText());
					pListResults->setErrorCode(eResult->getErrorCode());
					delete eResult;
				}
				break; // don't continue on failure
			}
		}
	}

	return pResults;
}

/*
 * Change passphrase on one or more dimms
 */
cli::framework::ResultBase *cli::nvmcli::SystemFeature::changeDevicePassphrase(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	cli::framework::ResultBase *pResults = NULL;
	std::string basePrefix = TRS(CHANGEPASSPHRASE_MSG);
	std::vector<std::string> dimms;

	std::string passphrase;
	std::string newPassphrase;
	std::string confirmPassphrase;

	// if user provided a password file as an option, read passwords from the file
	framework::StringMap::const_iterator source = parsedCommand.options.find(framework::OPTION_SOURCE.name);
	if (source != parsedCommand.options.end() && !source->second.empty())
	{
		// get path
		std::string passphraseFile = source->second;

		// password file option existed, so expect to use files for retrieving
		// all passwords
		pResults = readPassphrases(passphraseFile.c_str(), &passphrase,
				&newPassphrase, basePrefix);
		if (!pResults)
		{
			if (passphrase.empty())
			{
				pResults = new framework::SyntaxErrorMissingValueResult(
						framework::TOKENTYPE_PROPERTY,
						PASSPHRASE_PROPERTYNAME.c_str());
			}
			else if (newPassphrase.empty())
			{
				pResults = new framework::SyntaxErrorMissingValueResult(
						framework::TOKENTYPE_PROPERTY,
						NEWPASSPHRASE_PROPERTYNAME.c_str());
			}
			else
			{
				confirmPassphrase = newPassphrase;
			}
		}
	}
	else
	{
		// file name source was not specified for passwords, expect caller to have provided them
		// on the command line

		passphrase = framework::Parser::getPropertyValue(parsedCommand, PASSPHRASE_PROPERTYNAME);
		if (passphrase.empty())
		{
			// caller didn't give the passphrase, so prompt for it
			passphrase = promptUserHiddenString(TRS(PASSPHRASE_PROMPT));
		}

		newPassphrase = framework::Parser::getPropertyValue(parsedCommand, NEWPASSPHRASE_PROPERTYNAME);
		if (newPassphrase.empty())
		{
			// caller didn't give the new passphrase, so prompt for it
			newPassphrase = promptUserHiddenString(TRS(NEW_PASSPHRASE_PROMPT));
		}

		confirmPassphrase = framework::Parser::getPropertyValue(parsedCommand, CONFIRMPASSPHRASE_PROPERTYNAME);
		if (confirmPassphrase.empty())
		{
			// caller didn't give the confirmation passphrase, so prompt for it
			confirmPassphrase = promptUserHiddenString(TRS(CONFIRM_NEW_PASSPHRASE_PROMPT));
		}
	}

	if (!pResults)
	{
		// make sure confirm matches new
		if (newPassphrase.compare(confirmPassphrase) != 0)
		{
			pResults = new framework::ErrorResult(
					ERRORCODE_SECURITY_PASSPHRASEMISSMATCH,
					TRS(ERRORMSG_SECURITY_PASSPHRASEMISSMATCH),
					basePrefix);
		}
		else
		{
			pResults = cli::nvmcli::getDimms(parsedCommand, dimms);
		}

		if (pResults == NULL)
		{
			framework::SimpleListResult *pListResults = new framework::SimpleListResult();
			pResults = pListResults;
			wbem::physical_asset::NVDIMMFactory dimmProvider;

			for (std::vector<std::string>::const_iterator dimmIter = dimms.begin();
					dimmIter != dimms.end(); dimmIter++)
			{
				std::string prefix = cli::framework::ResultBase::stringFromArgList((basePrefix + " %s").c_str(),
						wbem::physical_asset::NVDIMMViewFactory::guidToDimmIdStr((*dimmIter)).c_str());
				prefix += ": ";

				try
				{
					dimmProvider.setPassphrase((*dimmIter), newPassphrase, passphrase);
					pListResults->insert(prefix + TRS(cli::framework::SUCCESS_MSG));
				}
				catch (wbem::framework::Exception &e)
				{
					cli::framework::ErrorResult *eResult = NvmExceptionToResult(e);
					if (eResult)
					{
						pListResults->insert(prefix + eResult->outputText());
						pListResults->setErrorCode(eResult->getErrorCode());
						delete eResult;
					}
					break; // don't continue on failure
				}
			}
		}
	}
	return pResults;
}

/*
 * Unlock all devices in list
 */
cli::framework::ResultBase *cli::nvmcli::SystemFeature::changeDeviceSecurity(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	std::string basePrefix = "";
	std::vector<std::string> dimms;
	framework::ResultBase *pResults = NULL;

	std::string newLockState = framework::Parser::getPropertyValue(parsedCommand, LOCKSTATE_PROPERTYNAME);
	std::string passphrase;

	// if user provided a password file as an option, read passwords from the file
	framework::StringMap::const_iterator source = parsedCommand.options.find(framework::OPTION_SOURCE.name);
	if (source != parsedCommand.options.end() && !source->second.empty())
	{
		// get path
		std::string passphraseFile = source->second;

		// password file option existed, so expect to use files for retrieving
		// all passwords
		pResults = readPassphrases(passphraseFile.c_str(), &passphrase, NULL, basePrefix);
		if (!pResults)
		{
			if (passphrase.empty())
			{
				pResults = new framework::SyntaxErrorMissingValueResult(
						framework::TOKENTYPE_PROPERTY,
						PASSPHRASE_PROPERTYNAME.c_str());
			}
		}
	}
	else
	{
		// file name source was not specified for passwords, expect caller to have provided them
		// on the command line

		passphrase = framework::Parser::getPropertyValue(parsedCommand, PASSPHRASE_PROPERTYNAME);
		if (passphrase.empty())
		{
			// caller didn't give the password, so prompt for it
			passphrase = promptUserHiddenString(TRS(PASSPHRASE_PROMPT));

		}
	}

	if (!pResults)
	{
		if (passphrase.empty())
		{
			pResults = new framework::SyntaxErrorMissingValueResult(
				framework::TOKENTYPE_PROPERTY,
				PASSPHRASE_PROPERTYNAME.c_str());
		}
		else if (newLockState.empty())
		{
			pResults = new framework::SyntaxErrorMissingValueResult(
					framework::TOKENTYPE_PROPERTY,
					LOCKSTATE_PROPERTYNAME.c_str());
		}
		else if (!cli::framework::stringsIEqual(newLockState, UNLOCKED_PROPERTYVALUE) &&
				!cli::framework::stringsIEqual(newLockState, DISABLED_PROPERTYVALUE))
		{
			pResults = new framework::SyntaxErrorBadValueResult(
					framework::TOKENTYPE_PROPERTY,
					LOCKSTATE_PROPERTYNAME.c_str(),
					newLockState);
		}
		else
		{
			if (cli::framework::stringsIEqual(newLockState, cli::nvmcli::UNLOCKED_PROPERTYVALUE))
			{
				basePrefix = TRS(UNLOCK_MSG);
			}
			else
			{
				basePrefix = TRS(REMOVEPASSPHRASE_MSG);
			}

			pResults = cli::nvmcli::getDimms(parsedCommand, dimms);
		}
	}

	if (pResults == NULL)
	{
		wbem::physical_asset::NVDIMMFactory dimmProvider;
		framework::SimpleListResult *pListResults = new framework::SimpleListResult();
		pResults = pListResults;
		for (std::vector<std::string>::const_iterator dimmIter = dimms.begin();
				dimmIter != dimms.end(); dimmIter++)
		{
			std::string prefix = cli::framework::ResultBase::stringFromArgList((basePrefix + " %s").c_str(),
					wbem::physical_asset::NVDIMMViewFactory::guidToDimmIdStr((*dimmIter)).c_str());
			prefix += ": ";
			try
			{
				// Based on the logic above, if the newLockState is not unlocked then it must be disabled
				if (cli::framework::stringsIEqual(newLockState, UNLOCKED_PROPERTYVALUE))
				{
					dimmProvider.unlock((*dimmIter), passphrase);
				}
				else
				{
					dimmProvider.removePassphrase((*dimmIter), passphrase);
				}

				pListResults->insert(prefix + TRS(cli::framework::SUCCESS_MSG));
			}
			catch (wbem::exception::NvmExceptionLibError &e)
			{
				// security is disabled and the device is already unlocked so this isn't really an error
				if (e.getLibError() == NVM_ERR_SECURITYDISABLED)
				{
					pListResults->insert(prefix + TRS(UNLOCK_ALREADYDISABLED_MSG));
				}
				else
				{
					cli::framework::ErrorResult *eResult = NvmExceptionToResult(e);
					if (eResult)
					{
						pListResults->insert(prefix + eResult->outputText());
						pListResults->setErrorCode(eResult->getErrorCode());
						delete eResult;
					}
					break; // don't continue on failure
				}
			}
			catch (wbem::framework::Exception &e)
			{
				cli::framework::ErrorResult *eResult = NvmExceptionToResult(e);
				pListResults->insert(prefix + eResult->outputText());
				pListResults->setErrorCode(eResult->getErrorCode());
				break; // don't continue on failure
			}
		}
	}
	return pResults;
}

/*
 * erase all devices in list
 */
cli::framework::ResultBase *cli::nvmcli::SystemFeature::eraseDeviceData(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	std::string basePrefix = TRS(ERASEDEVICEDATA_MSG);
	std::vector<std::string> dimms;
	framework::ResultBase *pResults = NULL;

	std::string passphrase;

	// if user provided a password file as an option, read passwords from the file
	framework::StringMap::const_iterator source = parsedCommand.options.find(framework::OPTION_SOURCE.name);
	if (source != parsedCommand.options.end() && !source->second.empty())
	{
		// get path
		std::string passphraseFile = source->second;

		// password file option existed, so expect to use files for retrieving
		// all passwords
		pResults = readPassphrases(passphraseFile.c_str(), &passphrase, NULL, basePrefix);
		if (!pResults)
		{
			if (passphrase.empty())
			{
				pResults = new framework::SyntaxErrorMissingValueResult(
						framework::TOKENTYPE_PROPERTY,
						PASSPHRASE_PROPERTYNAME.c_str());
			}
		}
	}
	else
	{
		// file name source was not specified for passwords, expect caller to have provided them
		// on the command line

		passphrase = framework::Parser::getPropertyValue(parsedCommand, PASSPHRASE_PROPERTYNAME);
		if (passphrase.empty())
		{
			// caller didn't give the password, so prompt for it
			passphrase = promptUserHiddenString(TRS(PASSPHRASE_PROMPT));
		}
	}

	if (pResults == NULL)
	{
		pResults = cli::nvmcli::getDimms(parsedCommand, dimms);
	}

	if (!pResults)
	{
		wbem::erasure::eraseType eraseType = wbem::erasure::ERASETYPE_CRYPTO_ERASE;
		bool eraseTypeExists = false;
		std::string eraseTypeString = framework::Parser::getPropertyValue(parsedCommand,
				ERASETYPE_PROPERTYNAME, &eraseTypeExists);

		if (eraseTypeExists)
		{
			if (cli::framework::stringsIEqual(eraseTypeString, ERASETYPE_PROPERTY_CRYPTO))
			{
				eraseType = wbem::erasure::ERASETYPE_CRYPTO_ERASE;
			}
			else if (cli::framework::stringsIEqual(eraseTypeString, ERASETYPE_PROPERTYNAME_MULTIOVERWRITE))
			{
				eraseType = wbem::erasure::ERASETYPE_MULTI_OVERWRITE;
			}
			else if (cli::framework::stringsIEqual(eraseTypeString, ERASETYPE_PROPERTY_QUICKOVERWRITE))
			{
				eraseType = wbem::erasure::ERASETYPE_QUICK_OVERWRITE;
			}
			else
			{
				pResults = new cli::framework::SyntaxErrorBadValueResult(
						framework::TOKENTYPE_PROPERTY, ERASETYPE_PROPERTYNAME, eraseTypeString);
			}
		}

		if (pResults == NULL)
		{
			wbem::erasure::ErasureServiceFactory erasureProvider;
			framework::SimpleListResult *pListResults = new framework::SimpleListResult();
			pResults = pListResults;

			for (std::vector<std::string>::const_iterator dimmIter = dimms.begin();
					dimmIter != dimms.end(); dimmIter++)
			{
				std::string dimmStr = wbem::physical_asset::NVDIMMViewFactory::guidToDimmIdStr((*dimmIter));
				std::string prefix = cli::framework::ResultBase::stringFromArgList((basePrefix + " %s").c_str(),
						dimmStr.c_str());
				prefix += ": ";
				try
				{
					bool forceOption = parsedCommand.options.find(framework::OPTION_FORCE.name)
							!= parsedCommand.options.end();

					// if user didn't specify the force option, prompt them to continue
					std::string prompt = framework::ResultBase::stringFromArgList(
							ERASE_DEV_PROMPT.c_str(), dimmStr.c_str());
					if (!forceOption && !promptUserYesOrNo(prompt))
					{
						pListResults->insert(prefix + cli::framework::UNCHANGED_MSG);
					}
					else
					{
						erasureProvider.eraseDevice((*dimmIter), passphrase,
							eraseType);

						pListResults->insert(prefix + TRS(cli::framework::SUCCESS_MSG));
					}
				}
				catch (wbem::framework::Exception &e)
				{
					cli::framework::ErrorResult *eResult = NvmExceptionToResult(e);
					if (eResult)
					{
						pListResults->insert(prefix + eResult->outputText());
						pListResults->setErrorCode(eResult->getErrorCode());
						delete eResult;
					}
					break; // don't continue on failure
				}
			}
		}
	}
	return pResults;
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::showMemoryResources(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	framework::ResultBase *pResult = NULL;
	try
	{
		// define default display attributes
		wbem::framework::attribute_names_t defaultAttributes;
		defaultAttributes.push_back(wbem::CAPACITY_KEY);
		defaultAttributes.push_back(wbem::VOLATILECAPACITY_KEY);
		defaultAttributes.push_back(wbem::PERSISTENTCAPACITY_KEY);
		defaultAttributes.push_back(wbem::UNCONFIGUREDCAPACITY_KEY);
		defaultAttributes.push_back(wbem::INACCESSIBLECAPACITY_KEY);
		defaultAttributes.push_back(wbem::RESERVEDCAPACITY_KEY);

		// read the options
		wbem::framework::attribute_names_t attributes = GetAttributeNames(parsedCommand.options, defaultAttributes);

		// get memory resources capacities from wbem
		wbem::mem_config::MemoryResourcesFactory capacityProvider;

		wbem::framework::instances_t *pInstances = capacityProvider.getInstances(attributes);
		if (pInstances)
		{
			cli::framework::PropertyListResult *pListResult;
			if (pInstances->size() > 0)
			{
				cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::CAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::VOLATILECAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::PERSISTENTCAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::UNCONFIGUREDCAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::INACCESSIBLECAPACITY_KEY);
				cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::RESERVEDCAPACITY_KEY);
				pListResult = NvmInstanceToPropertyListResult(*(pInstances->begin()), attributes);
			}
			else
			{
				pListResult = new framework::PropertyListResult();
			}

			pListResult->setName("MemoryResources");
			pResult = pListResult;
			delete pInstances;
		}
		else
		{
			pResult = new framework::ErrorResult(framework::ErrorResult::ERRORCODE_UNKNOWN,
				TRS(nvmcli::UNKNOWN_ERROR_STR));
		}
	}
	catch (wbem::framework::Exception &e)
	{
		if (NULL != pResult)
		{
			delete pResult;
		}
	pResult = NvmExceptionToResult(e);
}
	return pResult;
}


cli::framework::ResultBase *cli::nvmcli::SystemFeature::showSystemCapabilities(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	framework::ResultBase *pResult = NULL;
	wbem::server::SystemCapabilitiesFactory factory;
	wbem::framework::attribute_names_t defaultAttributes;
	defaultAttributes.push_back(wbem::PLATFORMCONFIGSUPPORTED_KEY);
	defaultAttributes.push_back(wbem::ALIGNMENT_KEY);
	defaultAttributes.push_back(wbem::CURRENTVOLATILEMEMORYMODE_KEY);
	defaultAttributes.push_back(wbem::CURRENTPERSISTENTMEMORYMODE_KEY);

	wbem::framework::attribute_names_t allAttributes(defaultAttributes);
	allAttributes.push_back(wbem::MEMORYMODESSUPPORTED_KEY);
	allAttributes.push_back(wbem::SUPPORTEDPERSISTENTSETTINGS_KEY);
	allAttributes.push_back(wbem::RECOMMENDEDPERSISTENTSETTINGS_KEY);
	allAttributes.push_back(wbem::MAXNAMESPACES_KEY);
	allAttributes.push_back(wbem::MINNAMESPACESIZE_KEY);
	allAttributes.push_back(wbem::BLOCKSIZES_KEY);
	allAttributes.push_back(wbem::PERSISTENTMEMORYMIRRORSUPPORT_KEY);
	allAttributes.push_back(wbem::DIMMSPARESUPPORT_KEY);
	allAttributes.push_back(wbem::PERSISTENTMEMORYMIGRATIONSUPPORT_KEY);
	allAttributes.push_back(wbem::RENAMENAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::ENABLENAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::DISABLENAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::GROWAPPDIRECTNAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::SHRINKAPPDIRECTNAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::GROWSTORAGENAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::SHRINKSTORAGENAMESPACESUPPORT_KEY);
	allAttributes.push_back(wbem::INITIATESCRUBSUPPORT_KEY);

	wbem::framework::attribute_names_t attributes =
			GetAttributeNames(parsedCommand.options, defaultAttributes, allAttributes);

	try
	{
		wbem::framework::instances_t *pInstances = factory.getInstances(attributes);
		if (pInstances->size() != 1)
		{
			pResult = new framework::ErrorResult(framework::ErrorResult::ERRORCODE_UNKNOWN,
					TRS(nvmcli::UNKNOWN_ERROR_STR));
		}
		else
		{
			// convert capacities to formatted sizes
			cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::ALIGNMENT_KEY);
			cli::nvmcli::convertCapacityAttribute((*pInstances)[0], wbem::MINNAMESPACESIZE_KEY);
			pResult = NvmInstanceToPropertyListResult((*pInstances)[0], attributes, "SystemCapabilities");
		}
	}
	catch (wbem::framework::Exception &e)
	{
		if (NULL != pResult)
		{
			delete pResult;
		}
		pResult = NvmExceptionToResult(e);
	}
	return pResult;
}

/*
 * create filters for job ID
 */
void cli::nvmcli::SystemFeature::generateJobFilter(
		const cli::framework::ParsedCommand &parsedCommand,
		wbem::framework::attribute_names_t &attributes,
		cli::nvmcli::filters_t &filters)
{
	std::vector<std::string> jobTargets =
			cli::framework::Parser::getTargetValues(parsedCommand,
					cli::nvmcli::TARGET_JOB.name);
	if (!jobTargets.empty())
	{
		struct instanceFilter jobFilter;
		jobFilter.attributeName = wbem::INSTANCEID_KEY;

		for (std::vector<std::string>::const_iterator jobTargetIter = jobTargets.begin();
			 jobTargetIter != jobTargets.end(); jobTargetIter++)
		{
			std::string target = (*jobTargetIter);
			jobFilter.attributeValues.push_back(target);
		}

		if (!jobFilter.attributeValues.empty())
		{
			filters.push_back(jobFilter);
			// make sure we have the JOB INSTANCEID filter attribute
			if (!wbem::framework_interface::NvmInstanceFactory::containsAttribute(wbem::INSTANCEID_KEY, attributes))
			{
				attributes.insert(attributes.begin(), wbem::INSTANCEID_KEY);
			}
		}
	}
}

cli::framework::ResultBase *cli::nvmcli::SystemFeature::showTopology(
		const framework::ParsedCommand &parsedCommand)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);
	framework::ResultBase *pResult = NULL;
	wbem::framework::instances_t *pInstances = NULL;
	try
	{
		// define default display attributes
		wbem::framework::attribute_names_t defaultAttributes;
		defaultAttributes.push_back(wbem::MEMORYTYPE_KEY);
		defaultAttributes.push_back(wbem::CAPACITY_KEY);
		defaultAttributes.push_back(wbem::DIMMID_KEY);
		defaultAttributes.push_back(wbem::PHYSICALID_KEY);
		defaultAttributes.push_back(wbem::DEVICELOCATOR_KEY);

		wbem::framework::attribute_names_t allAttributes = defaultAttributes;
		allAttributes.push_back(wbem::SOCKETID_KEY);
		allAttributes.push_back(wbem::MEMCONTROLLERID_KEY);
		allAttributes.push_back(wbem::CHANNEL_KEY);
		allAttributes.push_back(wbem::CHANNELPOS_KEY);
		allAttributes.push_back(wbem::NODECONTROLLERID_KEY);
		allAttributes.push_back(wbem::BANKLABEL_KEY);

		// update the display attributes based on what the user passed in
		wbem::framework::attribute_names_t displayAttributes =
				GetAttributeNames(parsedCommand.options, defaultAttributes, allAttributes);

		// include dimmid and physicalid in display when the user asks for specific display attributes
		if (!wbem::framework_interface::NvmInstanceFactory::containsAttribute(
				wbem::PHYSICALID_KEY, displayAttributes))
		{
			displayAttributes.insert(displayAttributes.begin(), wbem::PHYSICALID_KEY);
		}
		if (!wbem::framework_interface::NvmInstanceFactory::containsAttribute(
				wbem::DIMMID_KEY, displayAttributes))
		{
			displayAttributes.insert(displayAttributes.begin(), wbem::DIMMID_KEY);
		}

		cli::nvmcli::filters_t filters;
		generateFilterForAttributeWithTargetValues(parsedCommand, nvmcli::TARGET_DIMM.name,
		                                           wbem::DIMMID_KEY, filters);
		generateSocketFilter(parsedCommand, displayAttributes, filters);

		// get all instances
		pInstances = m_pTopologyProvider->getInstances(displayAttributes);
		if (pInstances)
		{
			for (size_t i = 0; i < pInstances->size(); i++)
			{
				cli::nvmcli::convertCapacityAttribute((*pInstances)[i], wbem::CAPACITY_KEY);
			}

			// format the return data
			pResult = NvmInstanceToObjectListResult(*pInstances, "DimmTopology",
			                                        wbem::DIMMID_KEY, displayAttributes, filters);
			delete pInstances;

			// Set layout to table unless the -all or -display option is present
			if (!framework::parsedCommandContains(parsedCommand, framework::OPTION_DISPLAY) &&
			    !framework::parsedCommandContains(parsedCommand, framework::OPTION_ALL))
			{
				pResult->setOutputType(framework::ResultBase::OUTPUT_TEXTTABLE);
			}
		}
		// failures will throw an exception, this would prevent a crash due to an unexpected error
		else
		{
			pResult = new framework::ErrorResult(framework::ErrorResult::ERRORCODE_UNKNOWN,
				TRS(nvmcli::UNKNOWN_ERROR_STR));
		}
	}
	catch (wbem::framework::Exception &e)
	{
		if (pInstances)
		{
			delete pInstances;
		}

		if (pResult)
		{
			delete pResult;
		}
		pResult = NvmExceptionToResult(e);
	}

	return pResult;
}

void cli::nvmcli::SystemFeature::updateLastShutDownStatus(
		wbem::framework::Instance& instance)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	wbem::framework::Attribute a;
	if (wbem::framework::SUCCESS ==
			instance.getAttribute(wbem::LASTSHUTDOWNSTATUS_KEY, a))
	{
		std::string newValue = "";
		for (size_t i = 0; i < a.uint16ListValue().size(); i++)
		{
			if (!newValue.empty())
			{
				newValue += ", ";
			}
			switch(a.uint16ListValue()[i])
			{
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_UKNOWN:
				newValue += "Unknown";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_FW_FLUSH_COMPLETE:
				newValue += "FW Flush Complete";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_PM_ADR_COMMAND:
				newValue += "PM ADR Command";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_PM_S3:
				newValue += "PM S3";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_PM_S5:
				newValue += "PM S5";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_DDRT_POWER_FAIL:
				newValue += "DDRT Power Fail Command";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_PMIC_12V_POWER_FAIL:
				newValue += "PMIC 12V Power Fail";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_PM_WARM_RESET:
				newValue += "PM Warm Reset";
				break;
			case wbem::physical_asset::DEVICE_LAST_SHUTDOWN_STATUS_THERMAL_SHUTDOWN:
				newValue += "Thermal Shutdown";
				break;
			}
		}
		instance.setAttribute(wbem::LASTSHUTDOWNSTATUS_KEY,
				wbem::framework::Attribute(newValue, false));
	}

}

void cli::nvmcli::SystemFeature::convertSecurityCapabilities(wbem::framework::Instance &wbemInstance)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	wbem::framework::Attribute securityAttr;
	if (wbemInstance.getAttribute(wbem::SECURITYCAPABILITIES_KEY, securityAttr) ==
			wbem::framework::SUCCESS)
	{
		std::string securityStr;
		wbem::framework::UINT16_LIST securityList = securityAttr.uint16ListValue();
		wbem::framework::UINT16_LIST::const_iterator iter = securityList.begin();
		for (; iter != securityList.end(); iter++) {
			NVM_UINT16 mode = *iter;
			switch (mode) {
				case wbem::physical_asset::SECURITY_PASSPHRASE:
					securityStr += ", " + ENCRYPTION_STR;
					break;
				case wbem::physical_asset::SECURITY_ERASE:
					securityStr += ", " + ERASE_STR;
					break;
				default:
					break;
			}
		}
		if (!securityStr.empty())
		{
			securityStr = securityStr.erase(0, 2);
		}
		wbem::framework::Attribute newSecurityAttr(securityStr, false);
		wbemInstance.setAttribute(wbem::SECURITYCAPABILITIES_KEY, newSecurityAttr);
	}
}

void cli::nvmcli::SystemFeature::updateLastShutDownTime(
		wbem::framework::Instance& instance)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	wbem::framework::Attribute a;
	if (wbem::framework::SUCCESS ==
			instance.getAttribute(wbem::LASTSHUTDOWNTIME_KEY, a))
	{
		time_t time = (time_t) (a.uint64Value());
		std::string timeStr = ctime(&time);
		std::size_t nullpos = timeStr.find("\n");
		if (nullpos != std::string::npos)
		{
			timeStr.erase(nullpos, 1);
		}
		wbem::framework::Attribute sdTimeAttr(timeStr, false);
		instance.setAttribute(wbem::LASTSHUTDOWNTIME_KEY, sdTimeAttr);
	}
}

enum fw_log_level cli::nvmcli::SystemFeature::logLevelStringToEnum(std::string logLevel)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	enum fw_log_level level;
	if (cli::framework::stringsIEqual(logLevel, DISABLED_PROPERTYVALUE))
	{
		level = FW_LOG_LEVEL_DISABLED;
	}
	else if (cli::framework::stringsIEqual(logLevel, ERROR_PROPERTYVALUE))
	{
		level = FW_LOG_LEVEL_ERROR;
	}
	else if (cli::framework::stringsIEqual(logLevel, WARNING_PROPERTYVALUE))
	{
		level = FW_LOG_LEVEL_WARN;
	}
	else if (cli::framework::stringsIEqual(logLevel, INFO_PROPERTYVALUE))
	{
		level = FW_LOG_LEVEL_INFO;
	}
	else if (cli::framework::stringsIEqual(logLevel, DEBUG_PROPERTYVALUE))
	{
		level = FW_LOG_LEVEL_DEBUG;
	}
	else
	{
		level = FW_LOG_LEVEL_UNKNOWN;
	}
	return level;
}

/*
 * Helper to convert operational status to sku violation and mixed sku attributes
 */
void cli::nvmcli::SystemFeature::convertSystemOpStatusToSku(wbem::framework::Instance& instance)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	bool mixedSku = false;
	bool skuViolation = false;

	wbem::framework::Attribute opStatus;
	if (instance.getAttribute(wbem::OPERATIONALSTATUS_KEY, opStatus) ==
			wbem::framework::SUCCESS)
	{
		for (size_t i = 0; i < opStatus.uint16ListValue().size(); i++)
		{
			if (opStatus.uint16ListValue()[i] == wbem::server::BASESERVER_OPSTATUS_MIXEDSKU)
			{
				mixedSku = true;
			}
			else if (opStatus.uint16ListValue()[i] == wbem::server::BASESERVER_OPSTATUS_SKUVIOLATION)
			{
				skuViolation = true;
			}
		}
	}

	// add mixed sku attribute
	wbem::framework::Attribute mixedSkuAttr(mixedSku, false);
	instance.setAttribute(wbem::MIXEDSKU_KEY, mixedSkuAttr);

	// add sku violation attribute
	wbem::framework::Attribute skuViolationAttr(skuViolation, false);
	instance.setAttribute(wbem::SKUVIOLATION_KEY, skuViolationAttr);
}
