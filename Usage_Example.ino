/**
 * @file example_basic_negotiation.ino
 * @brief Basic USB-C Power Delivery Negotiation Example
 * 
 * This example demonstrates how to use the FUSB302B library for basic
 * USB-C Power Delivery negotiation. It will:
 * 
 * 1. Initialize the FUSB302B controller
 * 2. Detect device attachment
 * 3. Negotiate power delivery (20V, 3A in this example)
 * 4. Recognize the connected device type
 * 
 * Hardware Requirements:
 * - FUSB302B USB-C PD Controller connected via I2C
 * - Interrupt pin connected to GPIO 6
 * - Proper USB-C connector with CC1/CC2 lines
 * 
 * @author Your Name
 * @date 2024
 */

#include "FUSB302B.h"

// Configuration
const int DESIRED_VOLTAGE = 20;  // Volts
const int DESIRED_CURRENT = 3;   // Amps
const int INTERRUPT_PIN = 6;     // GPIO pin for FUSB302B interrupt

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    Serial1.begin(115200);
    Serial.println("USB-C Power Delivery Example Starting...");
    
    // Initialize I2C
    Wire.begin();
    
    // Setup interrupt pin
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
    gpio_set_irq_enabled_with_callback(INTERRUPT_PIN, 
                                      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                      true, &InterruptFlagger);
    
    // Give system time to stabilize
    delay(150);
    
    // Initialize FUSB302B
    reset_fusb();
    
    Serial.println("FUSB302B initialized. Waiting for device connection...");
}

void setup1() {
    // Core 1 setup handled by library
}

void loop() {
    // Main processing happens in loop1() on core 1
    delay(100);
    
    // Optional: Print status periodically
    static unsigned long last_status = 0;
    if (millis() - last_status > 5000) {
        last_status = millis();
        
        if (IS_DEVICE_ATTACHED()) {
            Serial.println("Device connected and negotiated successfully!");
            Serial.print("Device type: ");
            switch(dev_type) {
                case DEVICE_TYPE_CHARGER:
                    Serial.println("Charger");
                    break;
                case DEVICE_TYPE_MONITOR:
                    Serial.println("Monitor");
                    break;
                case DEVICE_TYPE_TABLET:
                    Serial.println("Tablet");
                    break;
                case DEVICE_TYPE_LAPTOP:
                    Serial.println("Laptop");
                    break;
                default:
                    Serial.println("Unknown");
                    break;
            }
            
            // Print available power options
            Serial.println("Available power options:");
            for (int i = 0; i < 5; i++) {
                if (volt_options[i] != -1) {
                    Serial.print("  ");
                    Serial.print(volt_options[i]);
                    Serial.print("V @ ");
                    Serial.print(amp_options[i] * 10); // Convert from 10mA units
                    Serial.println("mA");
                }
            }
        } else {
            Serial.println("No device connected");
        }
    }
}

void loop1() {
    // Main power delivery processing
    if (int_flag) {
        check_interrupt(); // Process attach/detach events
        
        if (IS_NEW_ATTACHMENT()) {
            Serial.println("New device detected!");
            
            // Determine CC line orientation
            orient_cc();
            
            // Enable transmission on the correct CC line
            enable_tx_cc(cc_line, true);
            
            // Recognize device type (using PD 3.0 extended capabilities)
            recog_dev(5, 0.5); // Start with safe 5V negotiation
            
            // Reset and reinitialize for main power negotiation
            reset_fusb();
            enable_tx_cc(cc_line, true);
            
            // Negotiate desired power
            if (pd_init(DESIRED_VOLTAGE, DESIRED_CURRENT)) {
                Serial.print("Successfully negotiated ");
                Serial.print(DESIRED_VOLTAGE);
                Serial.print("V at ");
                Serial.print(DESIRED_CURRENT);
                Serial.println("A");
            } else {
                Serial.println("Power negotiation failed, using default");
            }
            
            Serial.println("=== Power Delivery Setup Complete ===");
            
            // Optional: Renegotiate to different power after delay
            // delay(5000);
            // if (reneg_pd(12, 2)) {
            //     Serial.println("Renegotiated to 12V @ 2A");
            // }
            
        } else if (!attached) {
            // Device disconnected
            reset_fusb();
            Serial.println("Device disconnected");
        }
        
        int_flag = false;
    }
}

// Optional: Add custom functions for your application

/**
 * @brief Check if specific voltage is available
 * @param desired_voltage Voltage to check for
 * @return true if voltage is available
 */
bool isVoltageAvailable(int desired_voltage) {
    for (int i = 0; i < 5; i++) {
        if (volt_options[i] == desired_voltage) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Get maximum available power
 * @return Maximum power in watts, 0 if no options available
 */
int getMaxAvailablePower() {
    int max_power = 0;
    for (int i = 0; i < 5; i++) {
        if (volt_options[i] != -1) {
            int power = volt_options[i] * (amp_options[i] * 10) / 1000; // Convert to watts
            if (power > max_power) {
                max_power = power;
            }
        }
    }
    return max_power;
}

/**
 * @brief Request optimal power based on device type
 * @return true if successful
 */
bool requestOptimalPower() {
    // Adjust power request based on detected device type
    int target_voltage = 5;
    int target_current = 1;
    
    switch(dev_type) {
        case DEVICE_TYPE_LAPTOP:
            target_voltage = 20;
            target_current = 3;
            break;
        case DEVICE_TYPE_TABLET:
            target_voltage = 12;
            target_current = 2;
            break;
        case DEVICE_TYPE_CHARGER:
            target_voltage = 9;
            target_current = 2;
            break;
        default:
            target_voltage = 5;
            target_current = 1;
            break;
    }
    
    return sel_src_cap(target_voltage, target_current);
} 