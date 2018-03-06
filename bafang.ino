#include <stdint.h>

enum class BAFCommand : uint8_t {
    READ      = 0x11,
    WRITE     = 0x16,
};

enum class BAFPasCommand : uint8_t {
    PAS_WALK  = 0x06,
    PAS0      = 0x00,
    PAS1      = 0x01,
    PAS2      = 0x0B,
    PAS3      = 0x0C,
    PAS4      = 0x0D,
    PAS5      = 0x02,
    PAS6      = 0x15,
    PAS7      = 0x16,
    PAS8      = 0x17,
    PAS9      = 0x03,
};

enum class BAFBacklightCommand : uint8_t {
    OFF    = 0xF0,
    ON     = 0xF1,
};

enum ButtonStates {
    BTN_UP,
    BTN_DOWN,
    BTN_PWR,
    BTN_STATE_COUNT
};

struct Color {
    uint8_t r, g, b;
};

// LED Colors
static constexpr Color WRITE    {255, 255, 255};
static constexpr Color RED      {255, 0, 0};
static constexpr Color GREEN    {0, 255, 0};
static constexpr Color BLUE     {0, 0, 255};
static constexpr Color YELLOW   {255, 255, 0};
static constexpr Color MAGENTA  {255, 0, 255};
static constexpr Color CYAN     {0, 255, 255};


// Pin configurations
static constexpr auto PIN_RED = 5;
static constexpr auto PIN_GREEN = 6;
static constexpr auto PIN_BLUE = 9;
static constexpr auto PIN_BTN_PWR = 2;
static constexpr auto PIN_BTN_UP = 3;
static constexpr auto PIN_BTN_DOWN = 4;

static constexpr auto MAX_PAS_LEVEL = 4;

// Color to represent a given PAS level
static constexpr Color PAS2Color[MAX_PAS_LEVEL] {
    RED, GREEN, BLUE, MAGENTA
};

// Helper class template for serializing command packages
template<typename T>
union Serializable {
    T data {};
    uint8_t bytes[sizeof(T)];
    
    const uint8_t *begin() const { return bytes; }
    const uint8_t *end() const { return bytes + sizeof(T); }
};

struct baf_pas_pkg_t {
    BAFCommand      type        = BAFCommand::WRITE;
    const uint8_t   cmd         = 0x08;
    BAFPasCommand   pas         = BAFPasCommand::PAS0;
    uint8_t         checksum    = 0;
} __attribute__((__packed__));
typedef Serializable<baf_pas_pkg_t> baf_pas_pkg;

typedef struct {
    BAFCommand          type        = BAFCommand::WRITE;
    const uint8_t       cmd         = 0x1A;
    BAFBacklightCommand backlight   = BAFBacklightCommand::OFF;
    uint8_t             checksum    = 0;
} __attribute__((__packed__)) baf_light_pkg_t;
typedef Serializable<baf_light_pkg_t> baf_light_pkg;

typedef struct {
    BAFCommand  type        = BAFCommand::WRITE;
    uint8_t     cmd         = 0x1F;
    uint16_t    diameter    = 0;
    uint8_t     checksum    = 0;
} __attribute__((__packed__)) baf_wheel_pkg_t;
typedef Serializable<baf_wheel_pkg_t> baf_wheel_pkg;

typedef struct {
    uint8_t type;
    uint8_t unknown;
} __attribute__((__packed__)) baf_read_byte_pkg_t;
typedef Serializable<baf_read_byte_pkg_t> baf_read_byte_pkg;

static uint8_t  buttonState[BTN_STATE_COUNT] {};         // current state of the button
static uint8_t  lastButtonState[BTN_STATE_COUNT] {};     // previous state of the button
static uint8_t  current_pas_level = 1;
static bool     dirty = false;

void setColor(const Color& c) {
    analogWrite(PIN_RED, c.r);
    analogWrite(PIN_GREEN, c.g);
    analogWrite(PIN_BLUE, c.b);
}

void updatePAS() {
    if (current_pas_level < MAX_PAS_LEVEL) {
        const auto& color = PAS2Color[current_pas_level];
        setColor(color);
    }
}

void sendNewState() {
    
    // Write current PAS level
    baf_pas_pkg pas;
    BAFPasCommand pasLevel;
    switch (current_pas_level) {
        case 0: pasLevel = BAFPasCommand::PAS0; break;
        case 1: pasLevel = BAFPasCommand::PAS1; break;
        case 2: pasLevel = BAFPasCommand::PAS2; break;
        case 3: pasLevel = BAFPasCommand::PAS3; break;
        case 4: pasLevel = BAFPasCommand::PAS4; break;
        default: pasLevel = BAFPasCommand::PAS0; break;
    }
    pas.data.pas = pasLevel;
    pas.data.checksum = static_cast<uint8_t>(pas.data.type) + pas.data.cmd + static_cast<uint8_t>(pas.data.pas);
    Serial.write(pas.bytes, sizeof(pas.data));
    
    delay(1);
    
    baf_light_pkg light;
    Serial.write(light.bytes, sizeof(light.data));
    
    delay(1);
    
    baf_wheel_pkg wheel;
    wheel.data.checksum = static_cast<uint8_t>(wheel.data.type) + wheel.data.cmd + wheel.data.diameter;
    wheel.data.diameter = 0x90C6;
    Serial.write(wheel.bytes, sizeof(wheel.data));
    
    delay(1);
}

void setup() {
    
    // Configure leds
    pinMode(PIN_RED, OUTPUT);
    pinMode(PIN_GREEN, OUTPUT);
    pinMode(PIN_BLUE, OUTPUT);
    
    // Configure buttons
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_PWR, INPUT_PULLUP);
    
    Serial.begin(1200);
#if 0
     while (!Serial) {
     ; // wait for serial port to connect. Needed for native USB port only
     }
#endif
    
    delay(100);
}

void loop() {
    
    // Read current button states
    buttonState[BTN_UP] = digitalRead(PIN_BTN_UP);
    buttonState[BTN_DOWN] = digitalRead(PIN_BTN_DOWN);
    buttonState[BTN_PWR] = digitalRead(PIN_BTN_PWR);
    
    for (int i=0; i < BTN_STATE_COUNT; i++) {
        
        // State changed?
        if (buttonState[i] != lastButtonState[i]) {
            
            if (buttonState[i] == HIGH) {
                // ON -> OFF
                switch (i) {
                    case BTN_UP:
                        current_pas_level++;
                        break;
                    case BTN_DOWN:
                        current_pas_level--;
                        break;
                }
                
                dirty = true;
            } else {
                // ON -> OFF
            }
            
            // Delay a bit to avoid bouncing
            delay(50);
        }

        // Save new state
        lastButtonState[i] = buttonState[i];
    }
    
    if (dirty) {
        
        // Something did change. Update LED and controller
        
        updatePAS();
        sendNewState();
        
        dirty = false;
    }
}