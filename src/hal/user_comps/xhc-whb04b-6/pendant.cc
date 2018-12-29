/*
   Copyright (C) 2017 Raoul Rubien (github.com/rubienr)

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

#include "pendant.h"

// system includes
#include <assert.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <limits>

// 3rd party includes

// local library includes

// local includes
#include "./hal.h"
#include "./xhc-whb04b6.h"
#include "./usb.h"

using std::endl;

namespace XhcWhb04b6 {

// ----------------------------------------------------------------------

const ButtonsCode              KeyCodes::Buttons;
const MetaButtonsCodes         KeyCodes::Meta(Buttons);
const AxisRotaryButtonCodes    KeyCodes::Axis;
const FeedRotaryButtonCodes    KeyCodes::Feed;

// ----------------------------------------------------------------------

KeyEventListener::~KeyEventListener()
{
}

// ----------------------------------------------------------------------

HandwheelStepModeStepSize::HandwheelStepModeStepSize() :
    mSequence{0.001, 0.01, 0.1, 1, 0, 0, 0}
{
}

// ----------------------------------------------------------------------

HandwheelStepModeStepSize::~HandwheelStepModeStepSize()
{
}

// ----------------------------------------------------------------------

float HandwheelStepModeStepSize::getStepSize(PositionNameIndex buttonPosition) const
{
    return mSequence[static_cast<uint8_t>(buttonPosition)];
}

// ----------------------------------------------------------------------

bool HandwheelStepModeStepSize::isPermitted(PositionNameIndex buttonPosition) const
{
    return (getStepSize(buttonPosition) > 0);
}

// ----------------------------------------------------------------------

HandwheelContinuousModeStepSize::HandwheelContinuousModeStepSize() :
    mSequence{2, 5, 10, 30, 60, 100, 0}
{
}

// ----------------------------------------------------------------------

HandwheelContinuousModeStepSize::~HandwheelContinuousModeStepSize()
{
}

// ----------------------------------------------------------------------

float HandwheelContinuousModeStepSize::getStepSize(PositionNameIndex buttonPosition) const
{
    return mSequence[static_cast<uint8_t>(buttonPosition)];
}

// ----------------------------------------------------------------------

bool HandwheelContinuousModeStepSize::isPermitted(PositionNameIndex buttonPosition) const
{
    return (getStepSize(buttonPosition) > 0);
}

// ----------------------------------------------------------------------

HandwheelLeadModeStepSize::HandwheelLeadModeStepSize() :
    mSequence{0, 0, 0, 0, 0, 0, 0.01}
{
}

// ----------------------------------------------------------------------

HandwheelLeadModeStepSize::~HandwheelLeadModeStepSize()
{
}

// ----------------------------------------------------------------------

float HandwheelLeadModeStepSize::getStepSize(PositionNameIndex buttonPosition) const
{
    return mSequence[static_cast<uint8_t>(buttonPosition)];
}

// ----------------------------------------------------------------------

bool HandwheelLeadModeStepSize::isPermitted(PositionNameIndex buttonPosition) const
{
    return (buttonPosition == PositionNameIndex::LEAD);
}

// ----------------------------------------------------------------------

KeyCode::KeyCode(uint8_t code, const char* text, const char* altText) :
    code(code),
    text(text),
    altText(altText)
{
}

// ----------------------------------------------------------------------

KeyCode::KeyCode(const KeyCode& other) :
    code(other.code),
    text(other.text),
    altText(other.altText)
{
}

// ----------------------------------------------------------------------

bool KeyCode::operator==(const KeyCode& other) const
{
    return ((code == other.code) && (text == other.text) && (altText == other.altText));
}

// ----------------------------------------------------------------------

bool KeyCode::operator!=(const KeyCode& other) const
{
    return !(*this == other);
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const KeyCode& data)
{
    std::ios init(NULL);
    init.copyfmt(os);

    os << std::hex << std::setfill('0') << "{code=0x" << std::setw(2) << static_cast<uint16_t>(data.code)
       << " text='" << data.text << "'"
       << " altText='" << data.altText << "'}";

    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

MetaButtonCodes::MetaButtonCodes(const KeyCode& key, const KeyCode& modifier) :
    key(key),
    modifier(modifier)
{
}

// ----------------------------------------------------------------------

MetaButtonCodes::~MetaButtonCodes()
{
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const MetaButtonCodes& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{key=" << data.key << " modifier=" << data.modifier << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

bool MetaButtonCodes::containsKeys(const KeyCode& key, const KeyCode& modifier) const
{
    return (this->key.code == key.code) && (this->modifier.code == modifier.code);
}

// ----------------------------------------------------------------------

bool MetaButtonCodes::operator==(const MetaButtonCodes& other) const
{
    return (key == other.key) && (modifier == other.modifier);
}

// ----------------------------------------------------------------------

bool MetaButtonCodes::operator!=(const MetaButtonCodes& other) const
{
    return !(*this == other);
}

// ----------------------------------------------------------------------

MetaButtonsCodes::MetaButtonsCodes(const ButtonsCode& buttons) :
    reset(buttons.reset, buttons.undefined),
    macro11(buttons.reset, buttons.function),
    stop(buttons.stop, buttons.undefined),
    macro12(buttons.stop, buttons.function),
    start(buttons.start, buttons.undefined),
    macro13(buttons.start, buttons.function),
    feed_plus(buttons.feed_plus, buttons.undefined),
    macro1(buttons.feed_plus, buttons.function),
    feed_minus(buttons.feed_minus, buttons.undefined),
    macro2(buttons.feed_minus, buttons.function),
    spindle_plus(buttons.spindle_plus, buttons.undefined),
    macro3(buttons.spindle_plus, buttons.function),
    spindle_minus(buttons.spindle_minus, buttons.undefined),
    macro4(buttons.spindle_minus, buttons.function),
    machine_home(buttons.machine_home, buttons.undefined),
    macro5(buttons.machine_home, buttons.function),
    safe_z(buttons.safe_z, buttons.undefined),
    macro6(buttons.safe_z, buttons.function),
    workpiece_home(buttons.workpiece_home, buttons.undefined),
    macro7(buttons.workpiece_home, buttons.function),
    spindle_on_off(buttons.spindle_on_off, buttons.undefined),
    macro8(buttons.spindle_on_off, buttons.function),
    function(buttons.function, buttons.undefined),
    probe_z(buttons.probe_z, buttons.undefined),
    macro9(buttons.probe_z, buttons.function),
    macro10(buttons.macro10, buttons.undefined),
    macro14(buttons.macro10, buttons.function),
    manual_pulse_generator(buttons.manual_pulse_generator, buttons.undefined),
    macro15(buttons.manual_pulse_generator, buttons.function),
    step_continuous(buttons.step_continuous, buttons.undefined),
    macro16(buttons.step_continuous, buttons.function),
    undefined(buttons.undefined, buttons.undefined),
    buttons{
        {&reset},
        {&macro11},
        {&stop},
        {&macro12},
        {&start},
        {&macro13},
        {&feed_plus},
        {&macro1},
        {&feed_minus},
        {&macro2},
        {&spindle_plus},
        {&macro3},
        {&spindle_minus},
        {&macro4},
        {&machine_home},
        {&macro5},
        {&safe_z},
        {&macro6},
        {&workpiece_home},
        {&macro7},
        {&spindle_on_off},
        {&macro8},
        {&function},
        {&probe_z},
        {&macro9},
        {&macro10},
        {&macro14},
        {&manual_pulse_generator},
        {&macro15},
        {&step_continuous},
        {&macro16},
        {&undefined}
    }
{
}

// ----------------------------------------------------------------------

const MetaButtonCodes& MetaButtonsCodes::find(const KeyCode& keyCode, const KeyCode& modifierCode) const
{

    std::function<bool(const MetaButtonCodes*)> comparator = [&keyCode, &modifierCode](
        const MetaButtonCodes* metaButton)
    {
        return metaButton->containsKeys(keyCode, modifierCode);
    };

    std::list<const MetaButtonCodes*>::const_iterator button = std::find_if(buttons.begin(), buttons.end(), comparator);

    if (button == buttons.end())
    {
        std::cerr << "failed to find metaButton={ keyCode={" << keyCode << "} modifierCode={" << modifierCode << "}}"
                  << endl;
    }
    assert(button != buttons.end());

    return **button;
}

// ----------------------------------------------------------------------

MetaButtonsCodes::~MetaButtonsCodes()
{
}

// ----------------------------------------------------------------------

Button::Button(const KeyCode& key) :
    mKey(&key)
{
}

// ----------------------------------------------------------------------

Button::~Button()
{
}

// ----------------------------------------------------------------------

Button& Button::operator=(const Button& other)
{
    mKey = other.mKey;
    return *this;
}

// ----------------------------------------------------------------------

AxisRotaryButtonCodes::AxisRotaryButtonCodes() :
    off(0x06, "OFF", ""),
    x(0x11, "X", ""),
    y(0x12, "Y", ""),
    z(0x13, "Z", ""),
    a(0x14, "A", ""),
    b(0x15, "B", ""),
    c(0x16, "C", ""),
    undefined(0x00, "", ""),
    codeMap{
        {off.code,       &off},
        {x.code,         &x},
        {y.code,         &y},
        {z.code,         &z},
        {a.code,         &a},
        {b.code,         &b},
        {c.code,         &c},
        {undefined.code, &undefined}
    }
{
}

// ----------------------------------------------------------------------

FeedRotaryButtonCodes::FeedRotaryButtonCodes() :
    speed_0_001(0x0d, "0.001", "2%"),
    speed_0_01(0x0e, "0.01", "5%"),
    speed_0_1(0x0f, "0.1", "10%"),
    speed_1(0x10, "1", "30%"),
    percent_60(0x1a, "", "60%"),
    percent_100(0x1b, "", "100%"),
    lead(0x1c, "Lead", ""),
    undefined(0x00, "", ""),
    codeMap{
        {speed_0_001.code, &speed_0_001},
        {speed_0_01.code,  &speed_0_01},
        {speed_0_1.code,   &speed_0_1},
        {speed_1.code,     &speed_1},
        {percent_60.code,  &percent_60},
        {percent_100.code, &percent_100},
        {lead.code,        &lead},
        {undefined.code,   &undefined}
    }
{
}

// ----------------------------------------------------------------------

ButtonsCode::ButtonsCode() :
    reset(0x01, "reset", "macro-11"),
    stop(0x02, "stop", "macro-12"),
    start(0x03, "start-pause", "macro-13"),
    feed_plus(0x04, "feed-plus", "macro-1"),
    feed_minus(0x05, "feed-minus", "macro-2"),
    spindle_plus(0x06, "spindle-plus", "macro-3"),
    spindle_minus(0x07, "spindle-minus", "macro-4"),
    machine_home(0x08, "m-home", "macro-5"),
    safe_z(0x09, "safe-z", "macro-6"),
    workpiece_home(0x0a, "w-home", "macro-7"),
    spindle_on_off(0x0b, "s-on-off", "macro-8"),
    function(0x0c, "fn", "<unused>"),
    probe_z(0x0d, "probe-z", "macro-9"),
    macro10(0x10, "macro-10", "macro-14"),
    manual_pulse_generator(0x0e, "mode-continuous", "macro-15"),
    step_continuous(0x0f, "mode-step", "macro-16"),
    undefined(0x00, "", ""),
    codeMap{
        {reset.code,                  &reset},
        {stop.code,                   &stop},
        {start.code,                  &start},
        {feed_plus.code,              &feed_plus},
        {feed_minus.code,             &feed_minus},
        {spindle_plus.code,           &spindle_plus},
        {spindle_minus.code,          &spindle_minus},
        {machine_home.code,           &machine_home},
        {safe_z.code,                 &safe_z},
        {workpiece_home.code,         &workpiece_home},
        {spindle_on_off.code,         &spindle_on_off},
        {function.code,               &function},
        {probe_z.code,                &probe_z},
        {macro10.code,                &macro10},
        {manual_pulse_generator.code, &manual_pulse_generator},
        {step_continuous.code,        &step_continuous},
        {undefined.code,              &undefined}
    }
{
}

// ----------------------------------------------------------------------

const KeyCode& ButtonsCode::getKeyCode(uint8_t keyCode) const
{
    const KeyCode* buttonKeyCode = reinterpret_cast<const KeyCode*>(this);

    while (buttonKeyCode->code != 0)
    {
        if (buttonKeyCode->code == keyCode)
        {
            break;
        }
        buttonKeyCode++;
    }

    assert(nullptr != buttonKeyCode);

    return *buttonKeyCode;
}

// ----------------------------------------------------------------------

const KeyCode& Button::keyCode() const
{
    return *mKey;
}

// ----------------------------------------------------------------------

bool Button::setKeyCode(const KeyCode& keyCode)
{
    bool isNewButton = *mKey != keyCode;
    mKey = &keyCode;
    return isNewButton;
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const Button& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{key=" << data.keyCode() << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

ToggleButton::ToggleButton(const KeyCode& key, const KeyCode& modifier) :
    Button(key),
    mModifier(&modifier)
{
}

// ----------------------------------------------------------------------

ToggleButton::~ToggleButton()
{
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const ToggleButton& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{" << *static_cast<const Button*>(&data) << " modifier=" << data.modifierCode() << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

ToggleButton& ToggleButton::operator=(const ToggleButton& other)
{
    Button::operator=(other);
    mModifier = other.mModifier;
    return *this;
}

// ----------------------------------------------------------------------

const KeyCode& ToggleButton::modifierCode() const
{
    return *mModifier;
}

// ----------------------------------------------------------------------

void ToggleButton::setModifierCode(KeyCode& modifierCode)
{
    mModifier = &modifierCode;
}

// ----------------------------------------------------------------------

bool ToggleButton::containsKeys(const KeyCode& key, const KeyCode& modifier) const
{
    return ((key.code == mKey->code) && (modifier.code == mModifier->code));
}

// ----------------------------------------------------------------------

RotaryButton::RotaryButton(const KeyCode& keyCode) :
    Button(keyCode)
{
}

// ----------------------------------------------------------------------

RotaryButton::~RotaryButton()
{
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const RotaryButton& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{" << *static_cast<const Button*>(&data) << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

RotaryButton& RotaryButton::operator=(const RotaryButton& other)
{
    Button::operator=(other);
    return *this;
}

// ----------------------------------------------------------------------
const HandwheelStepModeStepSize       FeedRotaryButton::mStepStepSizeMapper;
const HandwheelContinuousModeStepSize FeedRotaryButton::mContinuousSizeMapper;
const HandwheelLeadModeStepSize       FeedRotaryButton::mLeadStepSizeMapper;

const std::map<const KeyCode*, HandwheelStepModeStepSize::PositionNameIndex>       FeedRotaryButton::mStepKeycodeLut{
    {&KeyCodes::Feed.speed_0_001, HandwheelStepModeStepSize::PositionNameIndex::RotaryButton0001},
    {&KeyCodes::Feed.speed_0_01,  HandwheelStepModeStepSize::PositionNameIndex::RotaryButton0010},
    {&KeyCodes::Feed.speed_0_1,   HandwheelStepModeStepSize::PositionNameIndex::RotaryButton0100},
    {&KeyCodes::Feed.speed_1,     HandwheelStepModeStepSize::PositionNameIndex::RotaryButton100},
    {&KeyCodes::Feed.percent_60,  HandwheelStepModeStepSize::PositionNameIndex::RotaryButtonUndefined},
    {&KeyCodes::Feed.percent_100, HandwheelStepModeStepSize::PositionNameIndex::RotaryButtonUndefined},
    {&KeyCodes::Feed.lead,        HandwheelStepModeStepSize::PositionNameIndex::RotaryButtonUndefined}
};
const std::map<const KeyCode*, HandwheelContinuousModeStepSize::PositionNameIndex> FeedRotaryButton::mContinuousKeycodeLut{
    {&KeyCodes::Feed.speed_0_001, HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButton2percent},
    {&KeyCodes::Feed.speed_0_01,  HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButton5percent},
    {&KeyCodes::Feed.speed_0_1,   HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButton10percent},
    {&KeyCodes::Feed.speed_1,     HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButton30percent},
    {&KeyCodes::Feed.percent_60,  HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButton60percent},
    {&KeyCodes::Feed.percent_100, HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButton100percent},
    {&KeyCodes::Feed.lead,        HandwheelContinuousModeStepSize::PositionNameIndex::RotaryButtonUndefined}
};
const std::map<const KeyCode*, HandwheelLeadModeStepSize::PositionNameIndex>       FeedRotaryButton::mLeadKeycodeLut{
    {&KeyCodes::Feed.speed_0_001, HandwheelLeadModeStepSize::PositionNameIndex::UNDEFINED},
    {&KeyCodes::Feed.speed_0_01,  HandwheelLeadModeStepSize::PositionNameIndex::UNDEFINED},
    {&KeyCodes::Feed.speed_0_1,   HandwheelLeadModeStepSize::PositionNameIndex::UNDEFINED},
    {&KeyCodes::Feed.speed_1,     HandwheelLeadModeStepSize::PositionNameIndex::UNDEFINED},
    {&KeyCodes::Feed.percent_60,  HandwheelLeadModeStepSize::PositionNameIndex::UNDEFINED},
    {&KeyCodes::Feed.percent_100, HandwheelLeadModeStepSize::PositionNameIndex::UNDEFINED},
    {&KeyCodes::Feed.lead,        HandwheelLeadModeStepSize::PositionNameIndex::LEAD}
};

// ----------------------------------------------------------------------

FeedRotaryButton::FeedRotaryButton(const KeyCode& keyCode,
                                   HandwheelStepmodes::Mode stepMode,
                                   KeyEventListener* listener) :
    RotaryButton(keyCode),
    mStepMode(stepMode),
    mIsPermitted(false),
    mStepSize(0),
    mEventListener(listener)
{
}

// ----------------------------------------------------------------------

FeedRotaryButton::~FeedRotaryButton()
{
}


// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const FeedRotaryButton& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{" << *static_cast<const RotaryButton*>(&data) << " "
       << "isPermitted=" << ((data.isPermitted()) ? "TRUE" : "FALSE") << " "
       << "stepSize=" << data.stepSize() << " "
       << "stepMode=0x" << std::setfill('0') << std::hex << std::setw(2)
       << static_cast<int16_t>(data.stepMode()) << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

FeedRotaryButton& FeedRotaryButton::operator=(const FeedRotaryButton& other)
{
    RotaryButton::operator=(other);
    mStepMode = other.mStepMode;
    return *this;
}

// ----------------------------------------------------------------------

bool FeedRotaryButton::setKeyCode(const KeyCode& keyCode)
{
    bool hasChanged = Button::setKeyCode(keyCode);
    update();
    return hasChanged;
}

// ----------------------------------------------------------------------

void FeedRotaryButton::setStepMode(HandwheelStepmodes::Mode stepMode)
{
    mStepMode = stepMode;
    update();
}

// ----------------------------------------------------------------------

HandwheelStepmodes::Mode FeedRotaryButton::stepMode() const
{
    return mStepMode;
}

// ----------------------------------------------------------------------

float FeedRotaryButton::stepSize() const
{
    return mStepSize;
}

// ----------------------------------------------------------------------

void FeedRotaryButton::update()
{
    if (*mKey == KeyCodes::Feed.undefined)
    {
        mIsPermitted = false;
        return;
    }

    if (*mKey == KeyCodes::Feed.lead)
    {
        mStepSize    = mLeadStepSizeMapper.getStepSize(HandwheelLeadModeStepSize::PositionNameIndex::LEAD);
        mIsPermitted = mLeadStepSizeMapper.isPermitted(HandwheelLeadModeStepSize::PositionNameIndex::LEAD);
    }
    else if (mStepMode == HandwheelStepmodes::Mode::CONTINUOUS)
    {
        auto enumValue = mContinuousKeycodeLut.find(mKey);
        assert(enumValue != mContinuousKeycodeLut.end());
        auto second  = enumValue->second;
        mStepSize    = mContinuousSizeMapper.getStepSize(second);
        mIsPermitted = mContinuousSizeMapper.isPermitted(second);
    }
    else if (mStepMode == HandwheelStepmodes::Mode::STEP)
    {
        auto enumValue = mStepKeycodeLut.find(mKey);
        assert(enumValue != mStepKeycodeLut.end());
        auto second  = enumValue->second;
        mStepSize    = mStepStepSizeMapper.getStepSize(second);
        mIsPermitted = mStepStepSizeMapper.isPermitted(second);
    }
    else
    {
        assert(false);
    }
}

// ----------------------------------------------------------------------

bool FeedRotaryButton::isPermitted() const
{
    return mIsPermitted;
}

// ----------------------------------------------------------------------

AxisRotaryButton::AxisRotaryButton(const KeyCode& keyCode, KeyEventListener* listener) :
    RotaryButton(keyCode),
    mEventListener(listener)
{
}

// ----------------------------------------------------------------------

AxisRotaryButton::~AxisRotaryButton()
{
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const AxisRotaryButton& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{" << *static_cast<const RotaryButton*>(&data)
       << " isPermitted=" << ((data.isPermitted()) ? "TRUE" : "FALSE") << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

AxisRotaryButton& AxisRotaryButton::operator=(const AxisRotaryButton& other)
{
    RotaryButton::operator=(other);
    return *this;
}

// ----------------------------------------------------------------------

bool AxisRotaryButton::isPermitted() const
{
    return (*mKey != KeyCodes::Axis.undefined) && (*mKey != KeyCodes::Axis.off);
}

// ----------------------------------------------------------------------

Handwheel::Handwheel(const FeedRotaryButton& feedButton, KeyEventListener* listener) :
    mCounters(),
    mFeedButton(feedButton),
    mEventListener(listener),
    mWheelCout(&std::cout),
    mPrefix("pndnt ")
{
}

// ----------------------------------------------------------------------

Handwheel::~Handwheel()
{
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const Handwheel& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    os << "{counters=" << data.counters() << "}";
    return os;
}

// ----------------------------------------------------------------------

const HandWheelCounters& Handwheel::counters() const
{
    Handwheel* self = const_cast<Handwheel*>(this);
    return static_cast<const HandWheelCounters&>(self->counters());
}

// ----------------------------------------------------------------------

void Handwheel::setEnabled(bool enabled)
{
    mIsEnabled = enabled;
}
// ----------------------------------------------------------------------

HandWheelCounters& Handwheel::counters()
{
    return mCounters;
}

// ----------------------------------------------------------------------

void Handwheel::enableVerbose(bool enable)
{
    if (enable)
    {
        mWheelCout = &std::cout;
    }
    else
    {
        mWheelCout = &mDevNull;
    }
}

// ----------------------------------------------------------------------

void Handwheel::setMode(HandWheelCounters::CounterNameToIndex activeCounterMode)
{
    mCounters.setActiveCounter(activeCounterMode);
}

// ----------------------------------------------------------------------

void Handwheel::count(int8_t delta)
{
    assert(mEventListener != nullptr);

    if (mIsEnabled)
    {
        mCounters.count(delta);
        mEventListener->onJogDialEvent(mCounters, delta);
    }

    std::ios init(NULL);
    init.copyfmt(*mWheelCout);
    *mWheelCout << mPrefix << "handwheel total counts " << std::setfill(' ') << std::setw(5) << mCounters
                << endl;
    mWheelCout->copyfmt(init);
}

// ----------------------------------------------------------------------

ButtonsState::ButtonsState(KeyEventListener* listener, const ButtonsState* previousState) :
    mPressedButtons(),
    mCurrentMetaButton(&KeyCodes::Meta.undefined),
    mAxisButton(KeyCodes::Axis.undefined, listener),
    mFeedButton(KeyCodes::Feed.undefined, HandwheelStepmodes::Mode::CONTINUOUS, listener),
    mPreviousState(previousState),
    mEventListener(listener)
{
}

// ----------------------------------------------------------------------

ButtonsState::~ButtonsState()
{
}

// ----------------------------------------------------------------------

void ButtonsState::update(const KeyCode& keyCode,
                          const KeyCode& modifierCode,
                          const KeyCode& axisButton,
                          const KeyCode& feedButton)
{
    //! propagate push button events
    const MetaButtonCodes& newButton = KeyCodes::Meta.find(keyCode, modifierCode);
    if (*mCurrentMetaButton != newButton)
    {
        if (*mCurrentMetaButton != KeyCodes::Meta.undefined)
        {
            if (mEventListener != nullptr)
            {
                mEventListener->onButtonReleasedEvent(*mCurrentMetaButton);
            }
        }

        mCurrentMetaButton = &newButton;
        if (*mCurrentMetaButton != KeyCodes::Meta.undefined)
        {
            if (mEventListener != nullptr)
            {
                mEventListener->onButtonPressedEvent(*mCurrentMetaButton);
            }
        }
    }

    //! propagate axis rotary button events
    const KeyCode& oldAxisKeyCode = mAxisButton.keyCode();
    if (mAxisButton.setKeyCode(axisButton))
    {
        mEventListener->onAxisInactiveEvent(oldAxisKeyCode);
        mEventListener->onAxisActiveEvent(mAxisButton.keyCode());
    }

    //! propagate feed rotary button events
    const KeyCode& oldFeedKeyCode = mFeedButton.keyCode();
    if (mFeedButton.setKeyCode(feedButton))
    {
        mEventListener->onFeedInactiveEvent(oldFeedKeyCode);
        mEventListener->onFeedActiveEvent(mFeedButton.keyCode());
    }
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const ButtonsState& data)
{
    std::ios init(NULL);
    init.copyfmt(os);

    os << "{pressed buttons=";
    for (const KeyCode* pb : data.pressedButtons())
    {
        assert(pb != nullptr);
        os << *pb << " ";
    }
    os << " metaButton=" << *data.currentMetaButton()
       << " axisButton=" << data.axisButton()
       << " feedButton=" << data.feedButton() << "}";
    os.copyfmt(init);
    return os;
}

// ----------------------------------------------------------------------

ButtonsState& ButtonsState::operator=(const ButtonsState& other)
{
    mPressedButtons    = other.mPressedButtons;
    mCurrentMetaButton = other.mCurrentMetaButton;
    mAxisButton        = other.mAxisButton;
    mFeedButton        = other.mFeedButton;
    return *this;
}

// ----------------------------------------------------------------------

void ButtonsState::clearPressedButtons()
{
    mPressedButtons.clear();
}

// ----------------------------------------------------------------------

const std::list<const KeyCode*>& ButtonsState::pressedButtons() const
{
    return mPressedButtons;
}

// ----------------------------------------------------------------------

const MetaButtonCodes* ButtonsState::currentMetaButton() const
{
    return mCurrentMetaButton;
}

// ----------------------------------------------------------------------

const AxisRotaryButton& ButtonsState::axisButton() const
{
    return mAxisButton;
}

// ----------------------------------------------------------------------

const FeedRotaryButton& ButtonsState::feedButton() const
{
    return mFeedButton;
}

// ----------------------------------------------------------------------

FeedRotaryButton& ButtonsState::feedButton()
{
    return mFeedButton;
}

// ----------------------------------------------------------------------

Pendant::Pendant(Hal& hal, UsbOutPackageData& displayOutData) :
    mHal(hal),
    mPreviousButtonsState(this),
    mCurrentButtonsState(this, &mPreviousButtonsState),
    mHandWheel(mCurrentButtonsState.feedButton(), this),
    mDisplay(mCurrentButtonsState, hal, displayOutData),
    mPrefix("pndnt "),
    mPendantCout(&std::cout)
{
}

// ----------------------------------------------------------------------

Pendant::~Pendant()
{
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const Pendant& data)
{
    std::ios init(NULL);
    init.copyfmt(os);

    os << "{currentButtonState=" << data.currentButtonsState() << " "
       << "previousButtonState=" << data.previousButtonsState() << " "
       << "handwheel= " << data.handWheel() << "}";
    return os;
}

// ----------------------------------------------------------------------

void Pendant::processEvent(uint8_t keyCode,
                           uint8_t modifierCode,
                           uint8_t rotaryButtonAxisKeyCode,
                           uint8_t rotaryButtonFeedKeyCode,
                           int8_t handWheelStepCount)
{
    shiftButtonState();

    auto key      = KeyCodes::Buttons.codeMap.find(keyCode);
    auto modifier = KeyCodes::Buttons.codeMap.find(modifierCode);
    auto axis     = KeyCodes::Axis.codeMap.find(rotaryButtonAxisKeyCode);
    auto feed     = KeyCodes::Feed.codeMap.find(rotaryButtonFeedKeyCode);

    if (key == KeyCodes::Buttons.codeMap.end())
    {
        *mPendantCout << mPrefix << "failed to interpret key code keyCode={" << keyCode << "}" << endl;
    }
    if (modifier == KeyCodes::Buttons.codeMap.end())
    {
        *mPendantCout << mPrefix << "failed to interpret modifier code keyCode={" << modifierCode << "}" << endl;
    }
    if (axis == KeyCodes::Axis.codeMap.end())
    {
        *mPendantCout << mPrefix << "failed to interpret axis code axisCode={" << modifierCode << "}" << endl;
    }
    if (feed == KeyCodes::Feed.codeMap.end())
    {
        *mPendantCout << mPrefix << "failed to interpret axis code axisCode={" << modifierCode << "}" << endl;
    }

    processEvent(*key->second, *modifier->second, *axis->second, *feed->second, handWheelStepCount);
}

// ----------------------------------------------------------------------

void Pendant::processEvent(const KeyCode& keyCode,
                           const KeyCode& modifierCode,
                           const KeyCode& rotaryButtonAxisKeyCode,
                           const KeyCode& rotaryButtonFeedKeyCode,
                           int8_t handWheelStepCount)
{
    mHal.trySetManualMode(true);
    mHandWheel.setEnabled(mHal.getIsMachineOn());
    mCurrentButtonsState.update(keyCode, modifierCode, rotaryButtonAxisKeyCode, rotaryButtonFeedKeyCode);
    mHandWheel.count(handWheelStepCount);
    mDisplay.updateData();
    mHal.trySetManualMode(false);
}

// ----------------------------------------------------------------------

void Pendant::updateDisplayData()
{
    mDisplay.updateData();
}

// ----------------------------------------------------------------------

void Pendant::clearDisplayData()
{
    mDisplay.clearData();
}

// ----------------------------------------------------------------------

void Pendant::shiftButtonState()
{
    mPreviousButtonsState = mCurrentButtonsState;
    mCurrentButtonsState.clearPressedButtons();
}

// ----------------------------------------------------------------------

void Pendant::enableVerbose(bool enable)
{
    mHandWheel.enableVerbose(enable);
    if (enable)
    {
        mPendantCout = &std::cout;
    }
    else
    {
        mPendantCout = &mDevNull;
    }
}

// ----------------------------------------------------------------------

const ButtonsState& Pendant::currentButtonsState() const
{
    return mCurrentButtonsState;
}

// ----------------------------------------------------------------------

const ButtonsState& Pendant::previousButtonsState() const
{
    return mPreviousButtonsState;
}

// ----------------------------------------------------------------------

const Handwheel& Pendant::handWheel() const
{
    return mHandWheel;
}

// ----------------------------------------------------------------------

Handwheel& Pendant::handWheel()
{
    return const_cast<Handwheel&>(const_cast<const Pendant*>(this)->handWheel());
}

// ----------------------------------------------------------------------

bool Pendant::onButtonPressedEvent(const MetaButtonCodes& metaButton)
{
    *mPendantCout << mPrefix << "button pressed  event metaButton=" << metaButton << endl;
    bool isHandled = false;
    if (metaButton == KeyCodes::Meta.reset)
    {
        mHal.setReset(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.stop)
    {
        mHal.setStop(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.start)
    {
        mHal.setStart(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.feed_plus)
    {
        mHal.setFeedPlus(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.feed_minus)
    {
        mHal.setFeedMinus(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.spindle_plus)
    {
        mHal.setSpindlePlus(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.spindle_minus)
    {
        mHal.setSpindleMinus(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.machine_home)
    {
        mHal.setMachineHome(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.safe_z)
    {
        mHal.setSafeZ(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.workpiece_home)
    {
        mHal.setWorkpieceHome(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.spindle_on_off)
    {
        mHal.toggleSpindleOnOff(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.probe_z)
    {
        mHal.setProbeZ(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro10)
    {
        mHal.setMacro10(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.manual_pulse_generator)
    {
        mCurrentButtonsState.feedButton().setStepMode(HandwheelStepmodes::Mode::CONTINUOUS);
        mHal.setContinuousMode(true);
        dispatchFeedValueToHal();
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.step_continuous)
    {
        mCurrentButtonsState.feedButton().setStepMode(HandwheelStepmodes::Mode::STEP);
        mHal.setStepMode(true);
        dispatchFeedValueToHal();
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro11)
    {
        mHal.setMacro11(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro12)
    {
        mHal.setMacro12(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro13)
    {
        mHal.setMacro13(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro1)
    {
        mHal.setMacro1(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro2)
    {
        mHal.setMacro2(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro3)
    {
        mHal.setMacro3(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro4)
    {
        mHal.setMacro4(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro5)
    {
        mHal.setMacro5(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro6)
    {
        mHal.setMacro6(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro7)
    {
        mHal.setMacro7(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro8)
    {
        mHal.toggleSpindleDirection(true);
        mHal.setMacro8(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro9)
    {
        mHal.setMacro9(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro14)
    {
        mHal.setMacro14(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro15)
    {
        mHal.setMacro15(true);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro16)
    {
        mHal.setMacro16(true);
        isHandled = true;
    }

    mDisplay.onButtonPressedEvent(metaButton);
    return isHandled;
}

// ----------------------------------------------------------------------

bool Pendant::onButtonReleasedEvent(const MetaButtonCodes& metaButton)
{
    *mPendantCout << mPrefix << "button released event metaButton=" << metaButton << endl;
    bool isHandled = false;
    if (metaButton == KeyCodes::Meta.reset)
    {
        mHal.setReset(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.stop)
    {
        mHal.setStop(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.start)
    {
        mHal.setStart(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.feed_plus)
    {
        mHal.setFeedPlus(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.feed_minus)
    {
        mHal.setFeedMinus(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.spindle_plus)
    {
        mHal.setSpindlePlus(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.spindle_minus)
    {
        mHal.setSpindleMinus(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.machine_home)
    {
        mHal.setMachineHome(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.safe_z)
    {
        mHal.setSafeZ(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.workpiece_home)
    {
        mHal.setWorkpieceHome(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.spindle_on_off)
    {
        mHal.toggleSpindleOnOff(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.probe_z)
    {
        mHal.setProbeZ(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro10)
    {
        mHal.setMacro10(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.manual_pulse_generator)
    {
        mHal.setContinuousMode(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.step_continuous)
    {
        mHal.setStepMode(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro11)
    {
        mHal.setMacro11(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro12)
    {
        mHal.setMacro12(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro13)
    {
        mHal.setMacro13(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro1)
    {
        mHal.setMacro1(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro2)
    {
        mHal.setMacro2(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro3)
    {
        mHal.setMacro3(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro4)
    {
        mHal.setMacro4(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro5)
    {
        mHal.setMacro5(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro6)
    {
        mHal.setMacro6(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro7)
    {
        mHal.setMacro7(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro8)
    {
        mHal.toggleSpindleDirection(false);
        mHal.setMacro8(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro9)
    {
        mHal.setMacro9(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro14)
    {
        mHal.setMacro14(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro15)
    {
        mHal.setMacro15(false);
        isHandled = true;
    }
    else if (metaButton == KeyCodes::Meta.macro16)
    {
        mHal.setMacro16(false);
        isHandled = true;
    }

    mDisplay.onButtonReleasedEvent(metaButton);
    return isHandled;
}

// ----------------------------------------------------------------------

void Pendant::onAxisActiveEvent(const KeyCode& axis)
{
    *mPendantCout << mPrefix << "axis   active   event axis=" << axis
                  << " axisButton=" << mCurrentButtonsState.axisButton() << endl;
    dispatchAxisEventToHandwheel(axis, true);
    dispatchAxisEventToHal(axis, true);
    mDisplay.onAxisActiveEvent(axis);
}

// ----------------------------------------------------------------------

void Pendant::onAxisInactiveEvent(const KeyCode& axis)
{
    *mPendantCout << mPrefix << "axis   inactive event axis=" << axis
                  << " axisButton=" << mCurrentButtonsState.axisButton() << endl;
    dispatchAxisEventToHandwheel(axis, false);
    dispatchAxisEventToHal(axis, false);
    mDisplay.onAxisInactiveEvent(axis);
}

// ----------------------------------------------------------------------

void Pendant::onFeedActiveEvent(const KeyCode& feed)
{
    (*mPendantCout) << mPrefix << "feed   active   event feed=" << feed
                    << " feedButton=" << mCurrentButtonsState.feedButton() << endl;

    dispatchFeedEventToHandwheel(feed, true);
    dispatchFeedValueToHal(feed);
    dispatchActiveFeedToHal(feed, true);
    mDisplay.onFeedActiveEvent(feed);
}

// ----------------------------------------------------------------------

void Pendant::dispatchFeedEventToHandwheel(const KeyCode& feed, bool isActive)
{
    if (feed.code == KeyCodes::Feed.lead.code)
    {
        mHandWheel.counters().setLeadValueLimit(
            mHal.getFeedOverrideMinValue() * 100,
            mHal.getFeedOverrideMaxValue() * 100);
        mHandWheel.counters().enableLeadCounter(isActive);
    }
}

// ----------------------------------------------------------------------

void Pendant::dispatchActiveFeedToHal(const KeyCode& feed, bool isActive)
{
    if (feed.code == KeyCodes::Feed.speed_0_001.code)
    {
        mHal.setFeedValueSelected0_001(isActive);
    }
    else if (feed.code == KeyCodes::Feed.speed_0_01.code)
    {
        mHal.setFeedValueSelected0_01(isActive);
    }
    else if (feed.code == KeyCodes::Feed.speed_0_1.code)
    {
        mHal.setFeedValueSelected0_1(isActive);
    }
    else if (feed.code == KeyCodes::Feed.speed_1.code)
    {
        mHal.setFeedValueSelected1_0(isActive);
    }
    else if (feed.code == KeyCodes::Feed.percent_60.code)
    {
        mHal.setFeedValueSelected60(isActive);
    }
    else if (feed.code == KeyCodes::Feed.percent_100.code)
    {
        mHal.setFeedValueSelected100(isActive);
    }
    else if (feed.code == KeyCodes::Feed.lead.code)
    {
        mHal.setFeedValueSelectedLead(isActive);
    }
}

// ----------------------------------------------------------------------

void Pendant::dispatchFeedValueToHal(const KeyCode& keyCode)
{
    // on feed rotary button change and lead is active
    if (keyCode.code == KeyCodes::Feed.lead.code)
    {
        mHal.setFeedOverrideCountEnable(true);
        mHal.setFeedOverrideScale(mCurrentButtonsState.feedButton().stepSize());
        return;
    }
        // on feed rotary button change and lead is inactive
    else
    {
        mHal.setFeedOverrideCountEnable(false);
    }
    dispatchFeedValueToHal();
}

// ----------------------------------------------------------------------

void Pendant::dispatchFeedValueToHal()
{
    // on feed rotary button change
    FeedRotaryButton& feedButton = mCurrentButtonsState.feedButton();
    if (feedButton.isPermitted())
    {
        float axisJogStepSize = 0;
        if (feedButton.stepMode() == HandwheelStepmodes::Mode::STEP)
        {
            mHal.setFeedOverrideCountEnable(false);
            mHal.setStepMode(true);
            mHal.setFeedOverrideScale(0.1);
            mHal.setFeedOverrideDirectValue(false);
            axisJogStepSize = feedButton.stepSize();
        }
        else if (feedButton.stepMode() == HandwheelStepmodes::Mode::CONTINUOUS)
        {
            mHal.setFeedOverrideCountEnable(false);
            mHal.setContinuousMode(true);
            // On velocity mode set feed-override value to absolute percentage value: counts*scale.
            axisJogStepSize          = 2;
            mHal.setFeedOverrideDirectValue(true);
            float feedButtonStepSize = feedButton.stepSize();
            if (feedButtonStepSize >= 10)
            {
                // on velocity >= 10%: set a coarse grained scale,
                // this also affects the Feed+/- increment
                mHal.setFeedOverrideScale(0.1);
                mHal.setFeedOverrideCounts(feedButtonStepSize * 0.1);
            }
            else
            {
                // on velocity < 10%: set a fine grained scale,
                // this also affects the Feed+/- increment
                mHal.setFeedOverrideScale(0.01);
                mHal.setFeedOverrideCounts(feedButtonStepSize * 1);
            }
        }
        else
        {
        }
        mHal.setStepSize(axisJogStepSize);
    }
    else
    {
        mHal.setStepSize(0);
    }
}

// ----------------------------------------------------------------------

void Pendant::onFeedInactiveEvent(const KeyCode& feed)
{
    *mPendantCout << mPrefix << "feed   inactive event feed=" << feed
                  << " feedButton=" << mCurrentButtonsState.feedButton() << endl;
    dispatchFeedEventToHandwheel(feed, false);
    dispatchActiveFeedToHal(feed, false);
    mDisplay.onFeedInactiveEvent(feed);
}

// ----------------------------------------------------------------------

bool Pendant::onJogDialEvent(const HandWheelCounters& counters, int8_t delta)
{

    if (HandWheelCounters::CounterNameToIndex::UNDEFINED != counters.activeCounter() &&
        counters.counts() != 0)
    {
        *mPendantCout << mPrefix << "wheel  event " << counters.counts() << endl;

        if (HandWheelCounters::CounterNameToIndex::LEAD != counters.activeCounter())
        {
            mHandWheel.counters().setLeadValueLimit(
                mHal.getFeedOverrideMinValue() * 100,
                mHal.getFeedOverrideMaxValue() * 100);
        }
        mHal.setJogCounts(counters);
        mDisplay.onJogDialEvent(counters, delta);
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------

void Pendant::dispatchAxisEventToHandwheel(const KeyCode& axis, bool isActive)
{
    if (!isActive)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::UNDEFINED);
    }
    else if (axis.code == KeyCodes::Axis.off.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::UNDEFINED);
    }
    else if (axis.code == KeyCodes::Axis.x.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::AXIS_X);
    }
    else if (axis.code == KeyCodes::Axis.y.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::AXIS_Y);
    }
    else if (axis.code == KeyCodes::Axis.z.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::AXIS_Z);
    }
    else if (axis.code == KeyCodes::Axis.a.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::AXIS_A);
    }
    else if (axis.code == KeyCodes::Axis.b.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::AXIS_B);
    }
    else if (axis.code == KeyCodes::Axis.c.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::AXIS_C);
    }
    else if (axis.code == KeyCodes::Axis.undefined.code)
    {
        mHandWheel.counters().setActiveCounter(HandWheelCounters::CounterNameToIndex::UNDEFINED);
    }
}

// ----------------------------------------------------------------------

void Pendant::dispatchAxisEventToHal(const KeyCode& axis, bool isActive)
{
    if (axis.code == KeyCodes::Axis.off.code)
    {
        mHal.setNoAxisActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.x.code)
    {
        mHal.setAxisXActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.y.code)
    {
        mHal.setAxisYActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.z.code)
    {
        mHal.setAxisZActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.a.code)
    {
        mHal.setAxisAActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.b.code)
    {
        mHal.setAxisBActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.c.code)
    {
        mHal.setAxisCActive(isActive);
    }
    else if (axis.code == KeyCodes::Axis.undefined.code)
    {
        mHal.setNoAxisActive(isActive);
    }
}

// ----------------------------------------------------------------------

Display::Display(const ButtonsState& currentButtonsState, Hal& hal, UsbOutPackageData& displayData) :
    mCurrentButtonsState(currentButtonsState),
    mHal(hal),
    mDisplayData(displayData),
    mAxisPositionMethod(AxisPositionMethod::ABSOLUTE),
    mActiveAxisGroup(AxisGroup::XYZ)
{
}

// ----------------------------------------------------------------------

Display::~Display()
{
}

// ----------------------------------------------------------------------

bool Display::onButtonPressedEvent(const MetaButtonCodes& metaButton)
{
    if (metaButton == KeyCodes::Meta.manual_pulse_generator)
    {
        mDisplayData.displayModeFlags.asBitFields.stepMode =
            static_cast<typename std::underlying_type<DisplayIndicatorStepMode::StepMode>::type>(
                DisplayIndicatorStepMode::StepMode::MANUAL_PULSE_GENERATOR);
        return true;
    }
    else if (metaButton == KeyCodes::Meta.step_continuous)
    {
        mDisplayData.displayModeFlags.asBitFields.stepMode =
            static_cast<typename std::underlying_type<DisplayIndicatorStepMode::StepMode>::type>(
                DisplayIndicatorStepMode::StepMode::STEP);
        return true;
    }
    else if (metaButton == KeyCodes::Meta.macro5)
    {
        mAxisPositionMethod = AxisPositionMethod::ABSOLUTE;
        return true;
    }
    else if (metaButton == KeyCodes::Meta.macro7)
    {
        mAxisPositionMethod = AxisPositionMethod::RELATIVE;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------

bool Display::onButtonReleasedEvent(const MetaButtonCodes& metaButton)
{
    return false;
}

// ----------------------------------------------------------------------

void Display::onAxisActiveEvent(const KeyCode& axis)
{
    if ((axis.code == KeyCodes::Axis.x.code) ||
        (axis.code == KeyCodes::Axis.y.code) ||
        (axis.code == KeyCodes::Axis.z.code))
    {
        mActiveAxisGroup = AxisGroup::XYZ;
    }
    else
    { // a || b || c
        mActiveAxisGroup = AxisGroup::ABC;
    }
}

// ----------------------------------------------------------------------

void Display::onAxisInactiveEvent(const KeyCode& axis)
{
}

// ----------------------------------------------------------------------

void Display::onFeedActiveEvent(const KeyCode& feed)
{
    if (mCurrentButtonsState.feedButton().stepMode() == HandwheelStepmodes::Mode::STEP)
    {
        mDisplayData.displayModeFlags.asBitFields.stepMode =
            static_cast<typename std::underlying_type<DisplayIndicatorStepMode::StepMode>::type>(
                DisplayIndicatorStepMode::StepMode::STEP);
    }
    else if (mCurrentButtonsState.feedButton().stepMode() == HandwheelStepmodes::Mode::CONTINUOUS)
    {
        mDisplayData.displayModeFlags.asBitFields.stepMode =
            static_cast<typename std::underlying_type<DisplayIndicatorStepMode::StepMode>::type>(
                DisplayIndicatorStepMode::StepMode::MANUAL_PULSE_GENERATOR);
    }
}

// ----------------------------------------------------------------------

void Display::onFeedInactiveEvent(const KeyCode& feed)
{
}

// ----------------------------------------------------------------------

bool Display::onJogDialEvent(const HandWheelCounters& counters, int8_t delta)
{
    return false;
}

// ----------------------------------------------------------------------

void Display::updateData()
{
    mDisplayData.displayModeFlags.asBitFields.isReset = !mHal.getIsMachineOn();

    uint32_t spindleSpeed = static_cast<uint32_t>(mHal.getSpindleSpeedAbsRpm());
    uint32_t feedRate     = static_cast<uint32_t>(mHal.getFeedUps() * 60);

    assert(spindleSpeed <= std::numeric_limits<uint16_t>::max());
    assert(feedRate <= std::numeric_limits<uint16_t>::max());

    mDisplayData.spindleSpeed = spindleSpeed;
    mDisplayData.feedRate     = feedRate;

    bool isAbsolutePositionRequest = (mAxisPositionMethod == AxisPositionMethod::ABSOLUTE);
    mDisplayData.displayModeFlags.asBitFields.isRelativeCoordinate = !isAbsolutePositionRequest;
    if (mActiveAxisGroup == AxisGroup::XYZ)
    {
        mDisplayData.row1Coordinate.setCoordinate(static_cast<float>(mHal.getAxisXPosition(isAbsolutePositionRequest)));
        mDisplayData.row2Coordinate.setCoordinate(static_cast<float>(mHal.getAxisYPosition(isAbsolutePositionRequest)));
        mDisplayData.row3Coordinate.setCoordinate(static_cast<float>(mHal.getAxisZPosition(isAbsolutePositionRequest)));
    }
    else
    {
        mDisplayData.row1Coordinate.setCoordinate(static_cast<float>(mHal.getAxisAPosition(isAbsolutePositionRequest)));
        mDisplayData.row2Coordinate.setCoordinate(static_cast<float>(mHal.getAxisBPosition(isAbsolutePositionRequest)));
        mDisplayData.row3Coordinate.setCoordinate(static_cast<float>(mHal.getAxisCPosition(isAbsolutePositionRequest)));
    }
}

void Display::clearData()
{
    mDisplayData.feedRate     = 0;
    mDisplayData.spindleSpeed = 0;
    mDisplayData.displayModeFlags.asBitFields.stepMode =
        static_cast<typename std::underlying_type<DisplayIndicatorStepMode::StepMode>::type>(
            DisplayIndicatorStepMode::StepMode::MANUAL_PULSE_GENERATOR);
    mDisplayData.displayModeFlags.asBitFields.isReset              = true;
    mDisplayData.displayModeFlags.asBitFields.isRelativeCoordinate = false;
    mDisplayData.row1Coordinate.clear();
    mDisplayData.row2Coordinate.clear();
    mDisplayData.row3Coordinate.clear();
}
}
