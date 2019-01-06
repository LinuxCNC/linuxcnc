/*
   Copyright (C) 2017 Raoul Rubien (github.com/rubienr).

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */

#include "./usb.h"

// system includes
#include <assert.h>
#include <iostream>
#include <iomanip>
#include <libusb.h>
#include <unistd.h>

// 3rd party includes

// local library includes
#include <rtapi_math.h>

// local includes
#include "./hal.h"
#include "./xhc-whb04b6.h"

using std::endl;

namespace XhcWhb04b6 {


// ----------------------------------------------------------------------

//! This callback function passes received libusb transfer to the object
//! interested in the data.
void usbInputResponseCallback(struct libusb_transfer* transfer)
{
    assert(transfer->user_data != nullptr);
    UsbRawInputListener* receiver = reinterpret_cast<UsbRawInputListener*>(transfer->user_data);
    receiver->onUsbDataReceived(transfer);
}

// ----------------------------------------------------------------------

const ConstantUsbPackages Usb::ConstantPackages;


// ----------------------------------------------------------------------

UsbOutPackageBlock::UsbOutPackageBlock() :
    asBlock()
{
    assert(sizeof(UsbOutPackageBlockBuffer) == sizeof(UsbOutPackageBlockFields));
}

// ----------------------------------------------------------------------

UsbInPackageBuffer::UsbInPackageBuffer() :
    asBuffer{0}
{
    assert(sizeof(asFields) == sizeof(asBuffer));
}

// ----------------------------------------------------------------------

UsbEmptyPackage::UsbEmptyPackage() :
    UsbInPackage(0x04, 0xff, 0, 0, 0, 0, 0, 0xff)
{
}

// ----------------------------------------------------------------------

bool UsbEmptyPackage::operator==(const UsbInPackage& other) const
{
    // equality constraints: 0x4 0x? 0x0 0x0 0x0 0x0 0x0 0x?
    if ((header == other.header) &&
        // (notAvailable2 == other.notAvailable2) &&
        (buttonKeyCode1 == other.buttonKeyCode1) &&
        (buttonKeyCode2 == other.buttonKeyCode2) &&
        (rotaryButtonFeedKeyCode == other.rotaryButtonFeedKeyCode) &&
        (rotaryButtonAxisKeyCode == other.rotaryButtonAxisKeyCode) &&
        (stepCount == other.stepCount)
        // && (crc == other.crc)
        )
    {
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------

bool UsbEmptyPackage::operator!=(const UsbInPackage& other) const
{
    return !((*this) == other);
}

// ----------------------------------------------------------------------

UsbSleepPackage::UsbSleepPackage() :
    UsbInPackage(0x04, 0xff, 0xff, 0xff, 0xff, 0xff, -127, 0xff)
{
}

// ----------------------------------------------------------------------

bool UsbSleepPackage::operator==(const UsbInPackage& other) const
{
    // equality constraints: 0x4 0x? 0x? 0x? 0x? 0x? 0x? 0x?
    if ((header == other.header)
        // && (notAvailable2 == other.notAvailable2)
        // && (buttonKeyCode1 == other.buttonKeyCode1)
        // && (buttonKeyCode2 == other.buttonKeyCode2)
        // && (rotaryButtonFeedKeyCode == other.rotaryButtonFeedKeyCode)
        // && (rotaryButtonAxisKeyCode == other.rotaryButtonAxisKeyCode)
        // && (stepCount == other.stepCount)
        // && (crc == other.crc)
        )
    {
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------

bool UsbSleepPackage::operator!=(const UsbInPackage& other) const
{
    return !((*this) == other);
}

// ----------------------------------------------------------------------

ConstantUsbPackages::ConstantUsbPackages() :
    sleepPackage(),
    emptyPackage()
{
}

// ----------------------------------------------------------------------

uint16_t Usb::getUsbVendorId() const
{
    return usbVendorId;
}

// ----------------------------------------------------------------------

uint16_t Usb::getUsbProductId() const
{
    return usbProductId;
}

// ----------------------------------------------------------------------


const bool Usb::isDeviceOpen() const
{
    return deviceHandle != nullptr;
}

// ----------------------------------------------------------------------

libusb_context** Usb::getContextReference()
{
    return &context;
}

// ----------------------------------------------------------------------

libusb_context* Usb::getContext()
{
    return context;
}

// ----------------------------------------------------------------------

void Usb::setContext(libusb_context* context)
{
    this->context = context;
}

// ----------------------------------------------------------------------

libusb_device_handle* Usb::getDeviceHandle()
{
    return deviceHandle;
}

// ----------------------------------------------------------------------

void Usb::setDeviceHandle(libusb_device_handle* deviceHandle)
{
    this->deviceHandle = deviceHandle;
}

// ----------------------------------------------------------------------

bool Usb::isWaitForPendantBeforeHalEnabled() const
{
    return isWaitWithTimeout;
}

// ----------------------------------------------------------------------

bool Usb::getDoReconnect() const
{
    return mDoReconnect;
}

// ----------------------------------------------------------------------

void Usb::setDoReconnect(bool doReconnect)
{
    this->mDoReconnect = doReconnect;
}

// ----------------------------------------------------------------------

Usb::Usb(const char* name, OnUsbInputPackageListener& onDataReceivedCallback, Hal& hal) :
    sleepState(),
    inputPackageBuffer(),
    outputPackageBuffer(),
    mDataHandler(onDataReceivedCallback),
    mRawDataCallback(usbInputResponseCallback),
    mHal(hal),
    inTransfer(libusb_alloc_transfer(0)),
    outTransfer(libusb_alloc_transfer(0)),
    verboseTxOut(&devNull),
    verboseRxOut(&devNull),
    verboseInitOut(&devNull),
    mName(name)
{
    gettimeofday(&sleepState.mLastWakeupTimestamp, nullptr);
}

// ----------------------------------------------------------------------

void Usb::sendDisplayData()
{
    outputPackageBuffer.asBlocks.init(&outputPackageData);

    if (mIsSimulationMode)
    {
        *verboseTxOut << "out   0x" << outputPackageBuffer.asBlocks << endl <<
                      std::dec << "out   size " << sizeof(outputPackageBuffer.asBlockArray) << "B " << outputPackageData
                      << endl;
    }

    for (size_t idx = 0; idx < (sizeof(outputPackageBuffer.asBlockArray) / sizeof(UsbOutPackageBlockFields)); idx++)
    {
        UsbOutPackageBlock& block = outputPackageBuffer.asBlockArray[idx];
        size_t blockSize = sizeof(UsbOutPackageBlock);
        // see also
        // http://www.beyondlogic.org/usbnutshell/usb6.shtml
        // http://libusb.sourceforge.net/api-1.0/group__desc.html
        // http://libusb.sourceforge.net/api-1.0/group__misc.html
        int    r         = libusb_control_transfer(deviceHandle,
            // send to hid descriptor: bmRequestType == LIBUSB_DT_HID == 0x21 == (interface | endpoint)
                                                   LIBUSB_DT_HID,
            // bRequest == LIBUSB_REQUEST_SET_CONFIGURATION == 0x09 == set configuration
                                                   LIBUSB_REQUEST_SET_CONFIGURATION,
            // wValue: if bRequest == LIBUSB_REQUEST_SET_CONFIGURATION the configuration value
                                                   0x0306,
            // wIndex, device interface number
                                                   0x00,
            // data to transmit
                                                   block.asBuffer.asBytes,
            // wLength, data length
                                                   blockSize,
            // transfer timeout[ms]
                                                   0);

        if (r < 0)
        {
            std::cerr << "transmission failed, try to reconnect ..." << endl;
            setDoReconnect(true);
            return;
        }
    }
}

// ----------------------------------------------------------------------

void UsbOutPackageAxisCoordinate::setCoordinate(const float& coordinate)
{
    float coordinateAbs = rtapi_fabs(coordinate);
    if (coordinate == coordinateAbs)
    {
        coordinateSign = 0;
    }
    else
    {
        coordinateSign = 1;
    }

    uint32_t scaledCoordinate = static_cast<uint32_t>(rtapi_rint(coordinateAbs * 10000.0));
    integerValue  = static_cast<uint16_t>(scaledCoordinate / 10000);
    fractionValue = static_cast<uint16_t>(scaledCoordinate % 10000);
}

// ----------------------------------------------------------------------

void UsbOutPackageAxisCoordinate::clear()
{
    integerValue   = 0;
    fractionValue  = 0;
    coordinateSign = 0;
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const UsbOutPackageAxisCoordinate& coordinate)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << ((coordinate.coordinateSign == 1) ? "-" : "+") << std::setfill('0')
       << std::setw(4) << static_cast<unsigned short>(coordinate.integerValue) << "."
       << std::setw(4) << static_cast<unsigned short>(coordinate.fractionValue);

    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

UsbOutPackageBlockFields::UsbOutPackageBlockFields() :
    reportId(0x06),
    __padding0(0),
    __padding1(0),
    __padding2(0),
    __padding3(0),
    __padding4(0),
    __padding5(0),
    __padding6(0)
{
}

// ----------------------------------------------------------------------

void UsbOutPackageBlockFields::init(const void* data)
{
    reportId   = 0x06;
    __padding0 = reinterpret_cast<const uint8_t*>(data)[0];
    __padding1 = reinterpret_cast<const uint8_t*>(data)[1];
    __padding2 = reinterpret_cast<const uint8_t*>(data)[2];
    __padding3 = reinterpret_cast<const uint8_t*>(data)[3];
    __padding4 = reinterpret_cast<const uint8_t*>(data)[4];
    __padding5 = reinterpret_cast<const uint8_t*>(data)[5];
    __padding6 = reinterpret_cast<const uint8_t*>(data)[6];
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const UsbOutPackageBlockFields& block)
{
    std::ios init(NULL);
    init.copyfmt(os);

    os << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned short>(block.reportId) << std::setw(2)
       << static_cast<unsigned short>(block.__padding0) << std::setw(2) << static_cast<unsigned short>(block.__padding1)
       << std::setw(2) << static_cast<unsigned short>(block.__padding2) << std::setw(2)
       << static_cast<unsigned short>(block.__padding3) << std::setw(2) << static_cast<unsigned short>(block.__padding4)
       << std::setw(2) << static_cast<unsigned short>(block.__padding5) << std::setw(2)
       << static_cast<unsigned short>(block.__padding6);

    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

UsbOutPackageBlocks::UsbOutPackageBlocks() :
    block0(),
    block1(),
    block2()
{
}

// ----------------------------------------------------------------------

void UsbOutPackageBlocks::init(const UsbOutPackageData* data)
{
    const uint8_t* d = reinterpret_cast<const uint8_t*>(data);
    block0.init(d += 0);
    block1.init(d += 7);
    block2.init(d + 7);
}

// ----------------------------------------------------------------------


std::ostream& operator<<(std::ostream& os, const UsbOutPackageBlocks& blocks)
{
    return os << blocks.block0 << " " << blocks.block1 << " " << blocks.block2;
}

// ----------------------------------------------------------------------

UsbOutPackageData::UsbOutPackageData()
{
    clear();
}

// ----------------------------------------------------------------------

void UsbOutPackageData::clear()
{
    header = 0xfdfe;
    //! \sa WhbContext::printCrcDebug(const UsbInPackage&, const UsbOutPackageData&)
    seed   = 0xfe;
    displayModeFlags.asByte = 0;

    row1Coordinate.clear();
    row2Coordinate.clear();
    row3Coordinate.clear();

    feedRate     = 0;
    spindleSpeed = 0;
    padding      = 0;
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const UsbOutPackageData& data)
{
    std::ios init(NULL);
    init.copyfmt(os);

    bool enableMultiLine = false;
    if (enableMultiLine)
    {
        os << std::hex << std::setfill('0') << "header       0x" << std::setw(2) << data.header << endl
           << "day of month   0x"
           << std::setw(2)
           << static_cast<unsigned short>(data.seed) << endl << "status 0x" << std::setw(2)
           << static_cast<unsigned short>(data.displayModeFlags.asByte) << endl << std::dec << "coordinate1  "
           << data.row1Coordinate << endl << "coordinate2  " << data.row2Coordinate << endl << "coordinate3  "
           << data.row3Coordinate << endl << "feed rate        " << data.feedRate << endl << "spindle rps      "
           << data.spindleSpeed;
    }
    else
    {
        os << std::hex << std::setfill('0') << "hdr 0x" << std::setw(4) << data.header << " dom 0x" << std::setw(2)
           << static_cast<unsigned short>(data.seed) << " status 0x" << std::setw(2)
           << static_cast<unsigned short>(data.displayModeFlags.asByte) << std::dec << " coord1 "
           << data.row1Coordinate << " coord2 " << data.row2Coordinate << " coord3 "
           << data.row3Coordinate << " feed " << std::setw(4) << data.feedRate << " spindle rps "
           << std::setw(5) << data.spindleSpeed;
    }
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

UsbOutPackageBuffer::UsbOutPackageBuffer() :
    asBlocks()
{
    if (false)
    {
        std::cout << "sizeof usb data " << sizeof(UsbOutPackageData) << endl
                  << " blocks count   " << sizeof(UsbOutPackageBlocks) / sizeof(UsbOutPackageBlockFields) << endl
                  << " sizeof block   " << sizeof(UsbOutPackageBlockFields) << endl
                  << " sizeof blocks  " << sizeof(UsbOutPackageBlocks) << endl
                  << " sizeof array   " << sizeof(asBlockArray) << endl
                  << " sizeof package " << sizeof(UsbOutPackageData) << endl;
    }
    assert(sizeof(UsbOutPackageBlocks) == sizeof(asBlockArray));
    size_t blocksCount = sizeof(UsbOutPackageBlocks) / sizeof(UsbOutPackageBlockFields);
    assert((sizeof(UsbOutPackageData) + blocksCount) == sizeof(UsbOutPackageBlocks));
}

// ----------------------------------------------------------------------

UsbInPackage::UsbInPackage() :
    header(0),
    randomByte(0),
    buttonKeyCode1(0),
    buttonKeyCode2(0),
    rotaryButtonFeedKeyCode(0),
    rotaryButtonAxisKeyCode(0),
    stepCount(0),
    crc(0)
{
}

// ----------------------------------------------------------------------

UsbInPackage::UsbInPackage(const uint8_t notAvailable1, const uint8_t notAvailable2, const uint8_t buttonKeyCode1,
                           const uint8_t buttonKeyCode2, const uint8_t rotaryButtonFeedKeyCode,
                           const uint8_t rotaryButtonAxisKeyCode, const int8_t stepCount, const uint8_t crc) :
    header(notAvailable1),
    randomByte(notAvailable2),
    buttonKeyCode1(buttonKeyCode1),
    buttonKeyCode2(buttonKeyCode2),
    rotaryButtonFeedKeyCode(rotaryButtonFeedKeyCode),
    rotaryButtonAxisKeyCode(rotaryButtonAxisKeyCode),
    stepCount(stepCount),
    crc(crc)
{
}

// ----------------------------------------------------------------------

void Usb::setSimulationMode(bool isSimulationMode)
{
    mIsSimulationMode = isSimulationMode;
}

// ----------------------------------------------------------------------

void Usb::setIsRunning(bool enableRunning)
{
    mIsRunning = enableRunning;
}

// ----------------------------------------------------------------------

void Usb::requestTermination()
{
    mIsRunning = false;
}

// ----------------------------------------------------------------------

bool Usb::setupAsyncTransfer()
{
    assert(inTransfer != nullptr);
    libusb_fill_bulk_transfer(inTransfer, deviceHandle,
                              (0x1 | LIBUSB_ENDPOINT_IN), inputPackageBuffer.asBuffer,
                              sizeof(inputPackageBuffer.asBuffer), mRawDataCallback,
        //! pass this object as callback data
                              static_cast<void*>(this),
        //! timeout[ms]
                              750);
    int r = libusb_submit_transfer(inTransfer);
    assert(0 == r);
    return (0 == r);
}

// ----------------------------------------------------------------------

void Usb::onUsbDataReceived(struct libusb_transfer* transfer)
{
    assert(mHal.isInitialized());

    int      expectedPackageSize = static_cast<int>(sizeof(UsbInPackage));
    std::ios init(NULL);
    init.copyfmt(*verboseTxOut);
    switch (transfer->status)
    {
        case (LIBUSB_TRANSFER_COMPLETED):
            // sleep mode was previously detected, drop current package
            if (sleepState.mDropNextInPackage)
            {
                if (Usb::ConstantPackages.sleepPackage != inputPackageBuffer.asFields)
                {
                    *verboseTxOut << "expected sleep package starting with " << std::hex << std::setfill('0')
                                  << std::setw(2)
                                  << static_cast<unsigned short>(Usb::ConstantPackages.sleepPackage.header)
                                  << " but got " << std::hex << std::setfill('0') << std::setw(2)
                                  << static_cast<unsigned short>(inputPackageBuffer.asFields.header) << endl;
                    verboseTxOut->copyfmt(init);
                }

                sleepState.mDropNextInPackage = false;
                goto ___TRUNCATE_PACKAGE;
            }

            if (transfer->actual_length == expectedPackageSize)
            {
                //! detect pendant going to sleep:
                //! when powering off pedant sends two packages
                //! 1st: \ref UsbEmptyPackage
                //! 2nd: \ref UsbSleepPackage
                if (Usb::ConstantPackages.emptyPackage == inputPackageBuffer.asFields)
                {
                    sleepState.mDropNextInPackage = true;
                    mHal.setIsPendantSleeping(true);
                    if (mIsSimulationMode)
                    {
                        struct timeval now;
                        gettimeofday(&now, nullptr);
                        *verboseTxOut << "event going to sleep: device was idle for "
                                      << (now.tv_sec - sleepState.mLastWakeupTimestamp.tv_sec) << " seconds" << endl;
                    }
                }
                    // on regular package
                else
                {
                    if (mHal.getIsPendantSleeping())
                    {
                        mHal.setIsPendantSleeping(false);
                        if (mIsSimulationMode)
                        {
                            struct timeval now;
                            gettimeofday(&now, nullptr);
                            *verboseTxOut << "woke up: device was sleeping for "
                                          << (now.tv_sec - sleepState.mLastWakeupTimestamp.tv_sec) << " seconds"
                                          << endl;
                        }
                        gettimeofday(&sleepState.mLastWakeupTimestamp, nullptr);
                    }
                }
                // pass structured transfer to usb data handler
                mDataHandler.onInputDataReceived(inputPackageBuffer.asFields);
            }
            else
            {
                std::cerr << "received unexpected package size: expected=" << (transfer->actual_length) << ", current="
                          << expectedPackageSize << endl;
            }

            if (mIsRunning)
            {
                setupAsyncTransfer();
            }

            break;

        ___TRUNCATE_PACKAGE:
        case (LIBUSB_TRANSFER_TIMED_OUT):
            if (mIsRunning)
            {
                setupAsyncTransfer();
            }
            break;

        case (LIBUSB_TRANSFER_CANCELLED):
            break;

        case (LIBUSB_TRANSFER_STALL):
        case (LIBUSB_TRANSFER_NO_DEVICE):
        case (LIBUSB_TRANSFER_OVERFLOW):
        case (LIBUSB_TRANSFER_ERROR):
            std::cerr << "transfer error: " << transfer->status << endl;
            requestTermination();
            break;

        default:
            std::cerr << "unknown transfer status: " << transfer->status << endl;
            requestTermination();
            break;
    }
    //libusb_free_transfer(transfer);
}

// ----------------------------------------------------------------------

Usb::~Usb()
{
}

// ----------------------------------------------------------------------

void Usb::enableVerboseTx(bool enable)
{
    if (enable)
    {
        verboseTxOut = &std::cout;
    }
    else
    {
        verboseTxOut = &devNull;
    }
}

// ----------------------------------------------------------------------

void Usb::enableVerboseRx(bool enable)
{
    if (enable)
    {
        verboseRxOut = &std::cout;
    }
    else
    {
        verboseRxOut = &devNull;
    }
}

// ----------------------------------------------------------------------

void Usb::enableVerboseInit(bool enable)
{
    if (enable)
    {
        verboseInitOut = &std::cout;
    }
    else
    {
        verboseInitOut = &devNull;
    }
}

// ----------------------------------------------------------------------

bool Usb::init()
{
    if (getDoReconnect())
    {
        int pauseSecs = 3;
        *verboseInitOut << "init  pausing " << pauseSecs << "s, waiting for device to be gone ...";
        while ((pauseSecs--) >= 0)
        {
            *verboseInitOut << "." << std::flush;
            sleep(1);
        }
        setDoReconnect(false);
        *verboseInitOut << " done" << endl;
    }

    *verboseInitOut << "init  usb context ...";
    int r = libusb_init(&context);
    if (r != 0)
    {
        std::cerr << endl << "failed to initialize usb context" << endl;
        return false;
    }
    *verboseInitOut << " ok" << endl;

    libusb_log_level logLevel = LIBUSB_LOG_LEVEL_INFO;
    //logLevel = LIBUSB_LOG_LEVEL_DEBUG;
    libusb_set_debug(context, logLevel);

    std::ios init(NULL);
    init.copyfmt(*verboseInitOut);
    if (isWaitWithTimeout)
    {
        *verboseInitOut << "init  waiting maximum " << static_cast<unsigned short>(mWaitSecs) << "s for device "
                        << mName << " vendorId=0x" << std::hex << std::setfill('0') << std::setw(2) << usbVendorId
                        << " productId=0x" << std::setw(2) << usbProductId << " ...";
    }
    else
    {
        *verboseInitOut << "init  not waiting for device " << mName
                        << " vendorId=0x" << std::hex << std::setfill('0') << std::setw(2) << usbVendorId
                        << " productId=0x" << std::setw(2) << usbProductId << std::dec
                        << ", will continue in " << static_cast<unsigned short>(mWaitSecs) << "s ...";
    }
    verboseInitOut->copyfmt(init);

    do
    {
        libusb_device** devicesReference;
        ssize_t devicesCount = libusb_get_device_list(context, &devicesReference);
        if (devicesCount < 0)
        {
            std::cerr << endl << "failed to get device list" << endl;
            return false;
        }

        deviceHandle = libusb_open_device_with_vid_pid(context, usbVendorId, usbProductId);
        libusb_free_device_list(devicesReference, 1);
        *verboseInitOut << "." << std::flush;
        if (!isDeviceOpen())
        {
            *verboseInitOut << "." << std::flush;
            if (isWaitWithTimeout)
            {
                *verboseInitOut << "." << std::flush;
                if ((mWaitSecs--) <= 0)
                {
                    std::cerr << endl << "timeout exceeded, exiting" << endl;
                    return false;
                }
            }
            sleep(1);
        }
    } while (!isDeviceOpen() && mIsRunning);
    *verboseInitOut << " ok" << endl
                    << "init  " << mName << " device found" << endl;

    if (isDeviceOpen())
    {
        *verboseInitOut << "init  detaching active kernel driver ...";
        if (libusb_kernel_driver_active(deviceHandle, 0) == 1)
        {
            int r = libusb_detach_kernel_driver(deviceHandle, 0);
            assert(0 == r);
            *verboseInitOut << " ok" << endl;
        }
        else
        {
            *verboseInitOut << " already detached" << endl;
        }
        *verboseInitOut << "init  claiming interface ...";
        int r = libusb_claim_interface(deviceHandle, 0);
        if (r != 0)
        {
            std::cerr << endl << "failed to claim interface" << endl;
            return false;
        }
        *verboseInitOut << " ok" << endl;
    }
    return true;
}

// ----------------------------------------------------------------------

void Usb::setWaitWithTimeout(uint8_t waitSecs)
{
    mWaitSecs         = waitSecs;
    if (mWaitSecs > 0)
    {
        isWaitWithTimeout = true;
        return;
    }
    isWaitWithTimeout = false;
}

// ----------------------------------------------------------------------

UsbOutPackageData& Usb::getOutputPackageData()
{
    return outputPackageData;
}

// ----------------------------------------------------------------------

SleepDetect::SleepDetect() :
    mDropNextInPackage(false),
    mLastWakeupTimestamp()
{
}

// ----------------------------------------------------------------------

UsbRawInputListener::~UsbRawInputListener()
{
}

// ----------------------------------------------------------------------

OnUsbInputPackageListener::~OnUsbInputPackageListener()
{
}
}
