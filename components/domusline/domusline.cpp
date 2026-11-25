#include "domusline.h"
#include <SPI.h>


#define SPI_CLK 250000

namespace esphome {
namespace domusline {

DomusLine::DomusLine() : PollingComponent(200) {}

    void DomusLine::set_cs_pin(uint8_t pin) { CS = pin; }
    void DomusLine::set_ready_pin(uint8_t pin) { readyPin = pin; }

    void DomusLine::setup() 
    {        

        SPI.begin();
        pinMode(CS, OUTPUT);
        digitalWrite(CS, HIGH);
        delay(10);
        digitalWrite(CS, LOW);
        SPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE0));

        delay(500);
        digitalWrite(CS, HIGH);
        
        ESP_LOGD("domusline", "Initializing CC2500");
        init_cc2500();
    }

    void DomusLine::update()
    {
        if(lightIsOn && writeCallbackIsRunning == false)
        {
            // This function will be used to increase or decrease the brightness
            // Only called if the light is on!
            if(actualBrightness > setBrightness)
            {
                cc2500_decreaseBrightness();
                actualBrightness--;
                ESP_LOGD("domusline", "Decrease brightness to %d", actualBrightness);
            }
            else if(actualBrightness < setBrightness)
            {
                cc2500_increaseBrightness();
                actualBrightness++;
                ESP_LOGD("domusline", "Increase brightness to %d", actualBrightness);
            }
        }
    }
  
    light::LightTraits DomusLine::get_traits()
    {
        // return the traits this light supports
        auto traits = light::LightTraits();
        traits.set_supported_color_modes({light::ColorMode::ON_OFF, light::ColorMode::COLD_WARM_WHITE});
        traits.set_min_mireds(175);
        traits.set_max_mireds(333);
        return traits;
    }

    void DomusLine::setup_state(light::LightState *state) 
    {
        // empty for now
        
    }

    void DomusLine::get_current_values(light::LightState *state, uint8_t &currentTemperature, uint8_t &currentBrightness)
    {
        float cold_white, warm_white, brightness;
        // use any of the provided current_values methods
        state->current_values_as_cwww(&cold_white, &warm_white, false);
        state->current_values_as_brightness(&brightness);
        
        currentTemperature = (uint8_t)((255.0*cold_white)/(cold_white + warm_white));
        currentBrightness = (uint8_t)(brightness*10.0 + 0.5);
    }

    void DomusLine::write_state(light::LightState *state)
    {
        writeCallbackIsRunning = true;
        // This will be called by the light to get a new state to be written.
        state->set_gamma_correct(1.0);
        uint8_t currentTemperature, currentBrightness;
        
        get_current_values(state, currentTemperature, currentBrightness);

        bool currentStatus;
        state->current_values_as_binary(&currentStatus);
        ESP_LOGD("domusline", "Light: %d", currentStatus);
        if(lightIsOn != currentStatus)
        {
            if(currentStatus)
            {
                cc2500_on(currentTemperature);
                lastTemperature = currentTemperature;   // We set the temperature here already
                ESP_LOGD("domusline", "Light on");
            }
            else
            {
                cc2500_off();
                ESP_LOGD("domusline", "Light off");
            }
            lightIsOn = currentStatus;
        }

        if(lightIsOn)
        {
            if(currentTemperature != lastTemperature)
            {
                lastTemperature = currentTemperature;
                
                ESP_LOGD("domusline", "Set temperature to %d", currentTemperature);
                cc2500_temperature(currentTemperature);
            }
            else
            {
                ESP_LOGD("domusline", "Dont change temperature - old: %d new: %d", lastTemperature, currentTemperature);
            }
        }


        setBrightness = currentBrightness;
        ESP_LOGD("domusline", "setBrightness: %d", setBrightness);
        
        writeCallbackIsRunning = false;
    }

    void DomusLine::send_spi_package(uint8_t* data, uint8_t length)
    {
        digitalWrite(CS, LOW);
        delayMicroseconds(5);
        while(digitalRead(readyPin) == true)  // Always wait for MISO to be low
        {
            delayMicroseconds(2);
        }

        for(int i=0; i<length; i++)
        {
            SPI.transfer(data[i]);
        }
        delayMicroseconds(5);
        
        digitalWrite(CS, HIGH);
        delayMicroseconds(40);
    }
    
    void DomusLine::send_spi_package_CRC(uint8_t* data, uint8_t length)
    {
        if(length < 12)
        {
            uint8_t data_package[17] = {0x7F, 0x0E, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xFF};

            uint8_t crc = cc2500_crc(data, 11);

            for (std::size_t i = 0; i < length; i++)
            {
                data_package[i + 3] = data[i];
            }
            
            data_package[14] = crc;

            ESP_LOGD("domusline", "Package: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", data_package[0], data_package[1], data_package[2], data_package[3], data_package[4], data_package[5], data_package[6], data_package[7], data_package[8], data_package[9], data_package[10], data_package[11], data_package[12], data_package[13], data_package[14], data_package[15], data_package[16]);


            send_spi_package(data_package, 17);
        }
    }
    
    void DomusLine::wait_send_spi_package(uint8_t* data, uint8_t length)
    {
        digitalWrite(CS, LOW);
        delayMicroseconds(5);
        while(digitalRead(readyPin) == true)
        {
            delayMicroseconds(2);
        }

        for(int i=0; i<length; i++)
        {
            SPI.transfer(data[i]);
        }
        delayMicroseconds(5);
        
        digitalWrite(CS, HIGH);
        delayMicroseconds(40);
    }

    void DomusLine::init_cc2500()
    {
        uint8_t data_00[] = {0x36};
        send_spi_package(data_00, 1);
        digitalWrite(CS, LOW);
        delayMicroseconds(10);
        digitalWrite(CS, HIGH);
        delayMicroseconds(300);

        uint8_t data_00a[] = {0x30};
        send_spi_package(data_00a, 1);
        delayMicroseconds(300);
        uint8_t data_01[] = {0x02, 0x06};
        send_spi_package(data_01, 2);
        uint8_t data_02[] = {0x03, 0x07};
        send_spi_package(data_02, 2);
        uint8_t data_03[] = {0x04, 0xD3};
        send_spi_package(data_03, 2);
        uint8_t data_04[] = {0x05, 0x91};
        send_spi_package(data_04, 2);
        uint8_t data_05[] = {0x0B, 0x09};
        send_spi_package(data_05, 2);
        uint8_t data_06[] = {0x0C, 0x00};
        send_spi_package(data_06, 2);
        uint8_t data_07[] = {0x0D, 0x5D};
        send_spi_package(data_07, 2);
        uint8_t data_08[] = {0x0E, 0x93};
        send_spi_package(data_08, 2);
        uint8_t data_09[] = {0x0F, 0xB1};
        send_spi_package(data_09, 2);
        uint8_t data_10[] = {0x10, 0x2D};
        send_spi_package(data_10, 2);
        uint8_t data_11[] = {0x11, 0x3B};
        send_spi_package(data_11, 2);
        uint8_t data_12[] = {0x12, 0x73};
        send_spi_package(data_12, 2);
        uint8_t data_13[] = {0x13, 0xA2};
        send_spi_package(data_13, 2);
        uint8_t data_14[] = {0x14, 0xF8};
        send_spi_package(data_14, 2);
        uint8_t data_15[] = {0x0A, 0x10};
        send_spi_package(data_15, 2);
        uint8_t data_16[] = {0x15, 0x01};
        send_spi_package(data_16, 2);
        uint8_t data_17[] = {0x21, 0xB6};
        send_spi_package(data_17, 2);
        uint8_t data_18[] = {0x22, 0x10};
        send_spi_package(data_18, 2);
        uint8_t data_19[] = {0x16, 0x07};
        send_spi_package(data_19, 2);
        uint8_t data_20[] = {0x17, 0x30};
        send_spi_package(data_20, 2);
        uint8_t data_21[] = {0x18, 0x18};
        send_spi_package(data_21, 2);
        uint8_t data_22[] = {0x19, 0x1D};
        send_spi_package(data_22, 2);
        uint8_t data_23[] = {0x1A, 0x1C};
        send_spi_package(data_23, 2);
        uint8_t data_24[] = {0x1B, 0xC7};
        send_spi_package(data_24, 2);
        uint8_t data_25[] = {0x1C, 0x00};
        send_spi_package(data_25, 2);
        uint8_t data_26[] = {0x1D, 0xB2};
        send_spi_package(data_26, 2);
        uint8_t data_27[] = {0x23, 0xEA};
        send_spi_package(data_27, 2);
        uint8_t data_28[] = {0x24, 0x0A};
        send_spi_package(data_28, 2);
        uint8_t data_29[] = {0x25, 0x00};
        send_spi_package(data_29, 2);
        uint8_t data_30[] = {0x26, 0x11};
        send_spi_package(data_30, 2);
        uint8_t data_31[] = {0x27, 0x41};
        send_spi_package(data_31, 2);
        uint8_t data_32[] = {0x28, 0x00};
        send_spi_package(data_32, 2);
        uint8_t data_33[] = {0x1E, 0x87};
        send_spi_package(data_33, 2);
        uint8_t data_34[] = {0x1F, 0x6B};
        send_spi_package(data_34, 2);
        uint8_t data_35[] = {0x20, 0xF8};
        send_spi_package(data_35, 2);
        uint8_t data_36[] = {0x29, 0x59};
        send_spi_package(data_36, 2);
        uint8_t data_37[] = {0x2C, 0x88};
        send_spi_package(data_37, 2);
        uint8_t data_38[] = {0x2D, 0x31};
        send_spi_package(data_38, 2);
        uint8_t data_39[] = {0x2E, 0x0B};
        send_spi_package(data_39, 2);
        uint8_t data_40[] = {0x00, 0x29};
        send_spi_package(data_40, 2);
        uint8_t data_41[] = {0x02, 0x06};
        send_spi_package(data_41, 2);
        uint8_t data_42[] = {0x07, 0x04};
        send_spi_package(data_42, 2);
        uint8_t data_43[] = {0x08, 0x05};
        send_spi_package(data_43, 2);
        uint8_t data_44[] = {0x09, 0x01};
        send_spi_package(data_44, 2);
        uint8_t data_45[] = {0x06, 0xFF};
        send_spi_package(data_45, 2);
        uint8_t data_46[] = {0x93, 0xFF};
        send_spi_package(data_46, 2);
        uint8_t data_47[] = {0x7E, 0xFF, 0xFE, 0xBB, 0xA9, 0x7F, 0x6E, 0x97, 0xC5};
        send_spi_package(data_47, 9);
        uint8_t data_48[] = {0x33};
        send_spi_package(data_48, 1);
        delay(3);
        uint8_t data_49[] = {0x34};
        send_spi_package(data_49, 1);
        delay(2);
        uint8_t data_50[] = {0x36};
        send_spi_package(data_50, 1);
        uint8_t data_51[] = {0x3B};
        send_spi_package(data_51, 1);
        uint8_t data_52[] = {0x05, 0x7A, 0xEF, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_spi_package_CRC(data_52, 11);
        uint8_t data_53[] = {0x35};
        send_spi_package(data_53, 1);
        delay(3);
        uint8_t data_54[] = {0x36};
        send_spi_package(data_54, 1);
        uint8_t data_55[] = {0x3B};
        send_spi_package(data_55, 1);
        uint8_t data_56[] = {0x05, 0x7A, 0xEF, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_spi_package_CRC(data_56, 11);
        uint8_t data_57[] = {0x35};
        send_spi_package(data_57, 1);
        delay(3);
        uint8_t data_58[] = {0x36};
        send_spi_package(data_58, 1);
        uint8_t data_59[] = {0x3B};
        send_spi_package(data_59, 1);
        uint8_t data_60[] = {0x05, 0x7A, 0xEF, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_spi_package_CRC(data_60, 11);
        uint8_t data_61[] = {0x35};
        send_spi_package(data_61, 1);
        delay(3);
        uint8_t data_62[] = {0x36};
        send_spi_package(data_62, 1);
        uint8_t data_63[] = {0x3B};
        send_spi_package(data_63, 1);
        uint8_t data_64[] = {0x05, 0x7A, 0xEF, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_spi_package_CRC(data_64, 11);
        uint8_t data_65[] = {0x35};
        send_spi_package(data_65, 1);
        delay(3);
        uint8_t data_66[] = {0x36};
        send_spi_package(data_66, 1);
        uint8_t data_67[] = {0x3B};
        send_spi_package(data_67, 1);
        uint8_t data_68[] = {0x05, 0x7A, 0xEF, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_spi_package_CRC(data_68, 11);
        uint8_t data_69[] = {0x35};
        send_spi_package(data_69, 1);
    }

    void DomusLine::cc2500_on(uint8_t currentTemperature)
    {
        for(int i=0; i<6; i++)
        {
            uint8_t data_00[] = {0x36};
            wait_send_spi_package(data_00, 1);
            uint8_t data_01[] = {0x3B};
            send_spi_package(data_01, 1);
            uint8_t data_02[] = {0x05, 0x7A, 0xEF, 0x01, currentTemperature, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            send_spi_package_CRC(data_02, 11);
            uint8_t data_03[] = {0x35};
            send_spi_package(data_03, 1);
            delay(35);
        }
        
        cc2500_lowPower();
    }

    void DomusLine::cc2500_off()
    {
        for(int i=0; i<6; i++)
        {
            uint8_t data_00[] = {0x36};
            wait_send_spi_package(data_00, 1);
            uint8_t data_01[] = {0x3B};
            send_spi_package(data_01, 1);
            uint8_t data_02[] = {0x05, 0x7A, 0xEF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            send_spi_package_CRC(data_02, 11);
            uint8_t data_03[] = {0x35};
            send_spi_package(data_03, 1);
            delay(35);
        }
        
        cc2500_lowPower();
    }

    void DomusLine::cc2500_increaseBrightness()
    {
        for(int i=0; i<6; i++)
        {
            uint8_t data_00[] = {0x36};
            send_spi_package(data_00, 1);
            uint8_t data_01[] = {0x3B};
            send_spi_package(data_01, 1);
            uint8_t data_02[] = {0x05, 0x7A, 0xEF, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            send_spi_package_CRC(data_02, 11);
            uint8_t data_03[] = {0x35};
            send_spi_package(data_03, 1);
            delay(2);
        }
        uint8_t data_32[] = {0x39};
        send_spi_package(data_32, 1);
    }

    void DomusLine::cc2500_decreaseBrightness()
    {
        for(int i=0; i<5; i++)
        {
            uint8_t data_00[] = {0x36};
            wait_send_spi_package(data_00, 1);
            uint8_t data_01[] = {0x3B};
            send_spi_package(data_01, 1);
            uint8_t data_02[] = {0x05, 0x7A, 0xEF, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            send_spi_package_CRC(data_02, 11);
            uint8_t data_03[] = {0x35};
            send_spi_package(data_03, 1);
            delay(2);
        }
        
        cc2500_lowPower();
    }

    void DomusLine::cc2500_temperature(uint8_t currentTemperature)
    {
        for(int i=0; i<5; i++)
        {
            uint8_t data_00[] = {0x36};
            wait_send_spi_package(data_00, 1);
            uint8_t data_01[] = {0x3B};
            send_spi_package(data_01, 1);
            uint8_t data_02[] = {0x05, 0x7A, 0xEF, 0x50, currentTemperature, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            send_spi_package_CRC(data_02, 11);
            uint8_t data_03[] = {0x35};
            send_spi_package(data_03, 1);
            delay(2);
        }
        
        cc2500_lowPower();
    }

    void DomusLine::cc2500_lowPower()
    {
        for(int i=0; i<5; i++)
        {
            uint8_t data_70[] = {0x39};
            send_spi_package(data_70, 1);
            delay(67);
        }
    }

    uint8_t DomusLine::cc2500_crc(uint8_t* data, std::size_t len)
    {
        uint8_t crc = 0x00;

        for (std::size_t i = 0; i < len; i++)
        {
            crc ^= data[i];
        }

        return crc;
    }


}
}