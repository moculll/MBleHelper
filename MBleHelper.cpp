
#include <iostream>
#include <windows.h>
#include <objbase.h>
#include "MBleHelper.h"
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation::Collections;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

IAsyncOperation<int> MBleHelper::PairDevice(std::wstring mac) {
    device = co_await FindDeviceByMac(mac);

    if (!device)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_NOT_FOUND);
    

    if (device.Pairing().IsPaired()) {
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_ALREADY_PAIRED);
    }

    auto pairing = device.Pairing();
    pairing.Custom().PairingRequested([](DeviceInformationCustomPairing const&, DevicePairingRequestedEventArgs const& args) {
        switch (args.PairingKind()) {
        case DevicePairingKinds::ConfirmOnly:
            args.Accept();
            break;
        case DevicePairingKinds::ProvidePin:
            args.Accept(L"1234");
            break;
        case DevicePairingKinds::ConfirmPinMatch:
            args.Accept();
            break;
        default:

            args.Accept();
        }
    });

    auto result = co_await pairing.Custom().PairAsync(
        DevicePairingKinds::ConfirmOnly |
        DevicePairingKinds::ProvidePin |
        DevicePairingKinds::ConfirmPinMatch,
        DevicePairingProtectionLevel::None);

    switch (result.Status()) {
        case DevicePairingResultStatus::Paired:
            co_return static_cast<int>(MBleHelperErrorCode::DEVICE_PAIR_SUCCESS);
        case DevicePairingResultStatus::AlreadyPaired:
            co_return static_cast<int>(MBleHelperErrorCode::DEVICE_ALREADY_PAIRED);
        default:
            break;
    }

    /* users get actual error code after lower 3 bits */
    co_return static_cast<int>(static_cast<int>(MBleHelperErrorCode::UNKNOWN) | (static_cast<int>(result.Status()) << 3));
}

IAsyncOperation<DeviceInformation> MBleHelper::FindDeviceByMac(std::wstring mac, int timeoutSeconds) {
    auto normalizedMac = NormalizeMac(mac);

    auto foundDevice = std::make_shared<DeviceInformation>(nullptr);
    auto deviceFound = std::make_shared<bool>(false);

    DeviceWatcher watcher = DeviceInformation::CreateWatcher(
        BluetoothLEDevice::GetDeviceSelectorFromBluetoothAddress(MacToUint64(mac)),
        nullptr,
        DeviceInformationKind::AssociationEndpoint);

    watcher.Added([=](DeviceWatcher sender, DeviceInformation device) {
        try {

            auto ble = BluetoothLEDevice::FromIdAsync(device.Id()).get();
            if (ble) {
                std::wstringstream ss;
                ss << std::uppercase << std::hex << ble.BluetoothAddress();
                std::wstring devAddr = ss.str();


                if (devAddr.ends_with(normalizedMac)) {
                    *foundDevice = device;
                    *deviceFound = true;
                    sender.Stop();
                }
            }
        }
        catch (const std::exception& e) {
            (void)e;
         
        }
        catch (...) {
           
        }
    });

    watcher.Updated([](DeviceWatcher sender, DeviceInformationUpdate update) {
        
    });

    watcher.Removed([](DeviceWatcher sender, DeviceInformationUpdate update) {
 
    });

    watcher.EnumerationCompleted([](DeviceWatcher sender, IInspectable const&) {
        
    });

    watcher.Stopped([](DeviceWatcher sender, IInspectable const&) {
        
    });

    watcher.Start();

    auto startTime = std::chrono::steady_clock::now();
    while (!*deviceFound &&
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count() < timeoutSeconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (*deviceFound) {

        co_return *foundDevice;
    }

    co_return nullptr;

}

IAsyncOperation<int> MBleHelper::UnpairDevice(std::wstring mac) {

    auto device = co_await FindDeviceByMac(mac);
    auto result = co_await device.Pairing().UnpairAsync();

    co_return static_cast<int>(static_cast<int>(MBleHelperErrorCode::UNKNOWN) | (static_cast<int>(result.Status()) << 3));
}

bool MBleHelper::registerCharNotify(const std::wstring &uuid, CharacteristicNotifyType func)
{
    auto characteristic = characteristicMap.find(uuid);
    if (characteristic != characteristicMap.end()) {
        auto& control = characteristic->second;
        if (!control.chr)
            return false;
        control.callback = func;
        auto status = control.chr.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

        if (status != GattCommunicationStatus::Success) {
            wprintf(L"open cccd failed.\r\n");
        }

        control.chr.ValueChanged([&](GattCharacteristic const& sender, GattValueChangedEventArgs const& args) {

            using namespace Windows::Storage::Streams;

            auto reader = DataReader::FromBuffer(args.CharacteristicValue());
            uint32_t length = reader.UnconsumedBufferLength();

            std::vector<uint8_t> vec;
            vec.reserve(length);

            std::wstringstream ss;
            ss << L"Notification received: ";

            for (uint32_t i = 0; i < length; ++i) {
                uint8_t byte = reader.ReadByte();
                vec.push_back(byte);
                ss << L"0x"
                    << std::hex << std::uppercase
                    << std::setw(2) << std::setfill(L'0')
                    << static_cast<int>(byte)
                    << L" ";
            }

            if (func) {
                func(std::move(vec));
            }

            wprintf(ss.str().c_str());
            
        });
        return true;


    }
    return false;


}

bool MBleHelper::writeChar(const std::wstring &uuid, std::vector<uint8_t>& data)
{
    auto characteristic = characteristicMap.find(NormalizeUuidString(uuid));
    if (characteristic != characteristicMap.end()) {
        auto& control = characteristic->second;
        if (!control.chr)
            return false;
        DataWriter writer;
        for (auto byte : data) {
            writer.WriteByte(byte);
        }

        auto result = control.chr.WriteValueAsync(writer.DetachBuffer()).get();
        if (result == GattCommunicationStatus::Success) {
            return true;
        }
    }
    return false;
}


void MBleHelper::initChar(const std::wstring &serviceUuid, const std::wstring &uuid)
{
    auto characteristic = characteristicMap.find(uuid);
    if (characteristic != characteristicMap.end())
        return;

    auto bleDevice = BluetoothLEDevice::FromIdAsync(device.Id()).get();
    if (!bleDevice)
        return; 


    for (auto&& service : services.Services()) {
        if (winrt::to_hstring(service.Uuid()) == NormalizeUuidString(serviceUuid)) {
            
            auto characteristicsResult = service.GetCharacteristicsAsync(BluetoothCacheMode::Cached).get();
            wprintf(L"service UUID: %s, chr num: %d\n", winrt::to_hstring(service.Uuid()).c_str(), characteristicsResult.Characteristics().Size());
            
            for (auto&& characteristic : characteristicsResult.Characteristics()) {
                wprintf(L"characteristic UUID: %s\n", winrt::to_hstring(characteristic.Uuid()).c_str());
                if (winrt::to_hstring(characteristic.Uuid()) == NormalizeUuidString(uuid)) {
                    CharacteristicControl chr;
                    chr.chr = characteristic;
                    characteristicMap.emplace(NormalizeUuidString(uuid), chr);
                    wprintf(L"find char.\r\n");
                    return;
                }
               
            }

        }
    }
}


IAsyncOperation<int> MBleHelper::DoDiscoveryUncached() {

    auto bleDevice = co_await BluetoothLEDevice::FromIdAsync(device.Id());
    if (!bleDevice)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_NOT_FOUND);

    services = co_await bleDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached);
    if (services.Status() != GattCommunicationStatus::Success)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_GATT_FAIL);
    /*wprintf(L"service count: %d\n", servicesResult.Services().Size());*/
    for (auto&& service : services.Services()) {
        wprintf(L"Service UUID: %s\n", winrt::to_hstring(service.Uuid()).c_str());

        auto characteristicsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
        /*if (winrt::to_hstring(service.Uuid()) == L"{}") {
        }*/
    }
    co_return static_cast<int>(MBleHelperErrorCode::SUCCESS);
}

IAsyncOperation<int> MBleHelper::DoDiscoveryUncached(DeviceInformation device) {

    auto bleDevice = co_await BluetoothLEDevice::FromIdAsync(device.Id());
    if (!bleDevice)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_NOT_FOUND);

    auto servicesResult = co_await bleDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached);
    if (servicesResult.Status() != GattCommunicationStatus::Success)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_GATT_FAIL);
    /*wprintf(L"service count: %d\n", servicesResult.Services().Size());*/
    for (auto&& service : servicesResult.Services()) {
        wprintf(L"Service UUID: %s\n", winrt::to_hstring(service.Uuid()).c_str());

        auto characteristicsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
        /*if (winrt::to_hstring(service.Uuid()) == L"{}") {
        }*/
    }
    co_return static_cast<int>(MBleHelperErrorCode::SUCCESS);
}


int main()
{
    MBleHelper::init();
    std::unique_ptr<MBleHelper> bleHelper = std::make_unique<MBleHelper>();

    wchar_t macWStr[32] = { 0 };
    wscanf_s(L"%31ls", macWStr, (unsigned)_countof(macWStr));

    std::wstring mac(macWStr);


    bleHelper->UnpairDevice(mac).get();

    bleHelper->PairDevice(mac).get();

    auto ret = bleHelper->Discover().get();
    bleHelper->initChar(L"{}", L"{}");

    bleHelper->initChar(L"{}", L"{}");

    bleHelper->registerCharNotify(L"{}", [](std::vector<uint8_t>&& data) {
        wprintf(L"get notify, length: %lld", data.size());
    });

    std::vector<uint8_t> data = { 0x2c, 0x06, 0x01 };

    for (int i = 0; i < 10; i++) {
        bleHelper->writeChar(L"", data);
    }
    

    Sleep(1000000);
}
