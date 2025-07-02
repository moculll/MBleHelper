#pragma once
#include <windows.h>
#include <thread>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <functional>
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

	winrt::Windows::Foundation::IAsyncOperation<int> Discover() { return DoDiscoveryUncached(); }
	
	using CharacteristicNotifyType = std::function<void(std::vector<uint8_t>&& data)>;

	bool registerCharNotify(const std::wstring &uuid, CharacteristicNotifyType func);

	bool writeChar(const std::wstring &uuid, std::vector<uint8_t>& data);

	void initChar(const std::wstring &serviceUuid, const std::wstring &uuid);


	
private:


	std::wstring ToLower(const std::wstring& str) {
		std::wstring lower = str;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
		return lower;
	}

	std::wstring NormalizeUuidString(const std::wstring& uuid) {
		if (!uuid.empty() && uuid.front() == L'{' && uuid.back() == L'}') {
			return ToLower(uuid);
		}
		else {
			return L"{" + ToLower(uuid) + L"}";
		}
	}


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
	winrt::Windows::Foundation::IAsyncOperation<int> DoDiscoveryUncached();
	static winrt::Windows::Foundation::IAsyncOperation<int> DoDiscoveryUncached(winrt::Windows::Devices::Enumeration::DeviceInformation device);
	static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Enumeration::DeviceInformation> FindDeviceByMac(std::wstring mac, int timeoutSeconds = 2);
	


	struct CharacteristicControl {
		CharacteristicControl() : chr(nullptr) {}
		winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic chr;
		CharacteristicNotifyType callback;
	};
	std::map<std::wstring, CharacteristicControl> characteristicMap;

	winrt::Windows::Devices::Enumeration::DeviceInformation device{ nullptr };
	winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceServicesResult services{ nullptr };


};