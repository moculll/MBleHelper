#pragma once
#include <thread>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "MBleHelperErrorCode.h"
class MBleHelper {
public:
	static void init()
	{
		/* if we want to use winrt in qt, this is necessary */
		auto initThread = std::thread([]() {
			winrt::init_apartment(winrt::apartment_type::multi_threaded);
		});
		initThread.detach();
	}

	winrt::Windows::Foundation::IAsyncOperation<int> PairDevice(std::wstring mac);
	winrt::Windows::Foundation::IAsyncOperation<int> UnpairDevice(std::wstring mac);

	winrt::Windows::Foundation::IAsyncOperation<int> Discover() { return DoDiscoveryUncached(device); }
	

	
	/*static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Enumeration::DeviceInformation> FindDeviceByName(std::wstring name, int timeoutSeconds = 5);
	static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Enumeration::DeviceInformation> FindDeviceByPairStatus(bool pairStatus, int timeoutSeconds = 5);
	static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Enumeration::DeviceInformation> FindDeviceByConnectStatus(bool connectStatus, int timeoutSeconds = 5);*/
	

	/*winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService batteryService{ nullptr };
	winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic batteryLevelCharacteristic{ nullptr };*/
private:

	static std::wstring NormalizeMac(std::wstring mac) {
		mac.erase(std::remove(mac.begin(), mac.end(), L':'), mac.end());
		std::transform(mac.begin(), mac.end(), mac.begin(), ::towupper);
		return mac;
	}

	static uint64_t MacToUint64(const std::wstring& mac) {
		std::wstring cleanMac = NormalizeMac(mac);
		if (cleanMac.length() != 12)
			throw std::invalid_argument("Invalid MAC address");

		uint64_t result = 0;
		std::wstringstream ss;
		ss << std::hex << cleanMac;
		ss >> result;
		return result;
	}

	static winrt::Windows::Foundation::IAsyncOperation<int> DoDiscoveryUncached(winrt::Windows::Devices::Enumeration::DeviceInformation device);
	static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Enumeration::DeviceInformation> FindDeviceByMac(std::wstring mac, int timeoutSeconds = 2);
	
	winrt::Windows::Devices::Enumeration::DeviceInformation device{ nullptr };
};