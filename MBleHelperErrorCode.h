#pragma once

enum class MBleHelperErrorCode {
	SUCCESS,
	DEVICE_NOT_FOUND,
	DEVICE_ALREADY_PAIRED,
	DEVICE_PAIR_SUCCESS,
	DEVICE_GATT_FAIL,
	UNKNOWN,
	END = (1 << 31),
};