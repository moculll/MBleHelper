
#include <iostream>
#include <windows.h>
#include <objbase.h>
#include "MBleHelper.h"
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth;


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
        /*DebugPrint(L"Device updated: " + update.Id());*/
    });

    watcher.Removed([](DeviceWatcher sender, DeviceInformationUpdate update) {
        /*DebugPrint(L"Device removed: " + update.Id());*/
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

    /*watcher.Stop();*/

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


IAsyncOperation<int> MBleHelper::DoDiscoveryUncached(DeviceInformation device) {

    auto bleDevice = co_await BluetoothLEDevice::FromIdAsync(device.Id());
    if (!bleDevice)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_NOT_FOUND);

    auto servicesResult = co_await bleDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached);
    if (servicesResult.Status() != GattCommunicationStatus::Success)
        co_return static_cast<int>(MBleHelperErrorCode::DEVICE_GATT_FAIL);
    /*wprintf(L"service count: %d\n", servicesResult.Services().Size());*/
    for (auto&& service : servicesResult.Services()) {
       /* wprintf(L"Service UUID: %s\n", winrt::to_hstring(service.Uuid()).c_str());*/

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

    wprintf(L"result: %d", ret);

}
