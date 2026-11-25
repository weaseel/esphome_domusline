#include "domusline.h"
#include <SPI.h>


constexpr uint32_t SPI_CLK = 250000;

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
        initialize_cc2500();
    }

    void DomusLine::update()
    {
        if(lightIsOn && writeCallbackIsRunning == false)
        {
            // This function will be used to increase or decrease the brightness
            // Only called if the light is on!
            if(actualBrightness > setBrightness)
            {
                decrease_light_brightness();
                actualBrightness--;
                ESP_LOGD("domusline", "Decrease brightness to %d", actualBrightness);
            }
            else if(actualBrightness < setBrightness)
            {
                increase_light_brightness();
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
                turn_light_on(currentTemperature);
                lastTemperature = currentTemperature;   // We set the temperature here already
                ESP_LOGD("domusline", "Light on");
            }
            else
            {
                turn_light_off();
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
                set_light_color_temperature(currentTemperature);
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

    void DomusLine::transfer_cc2500_bytes(const uint8_t *data, uint8_t length)
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
    
    void DomusLine::send_cc2500_strobe(Cc2500Strobe strobe)
    {
        const uint8_t value = static_cast<uint8_t>(strobe);
        transfer_cc2500_bytes(&value, 1);
    }

    void DomusLine::write_cc2500_register(Cc2500Register reg, uint8_t value)
    {
        const uint8_t transaction[] = {static_cast<uint8_t>(reg), value};
        transfer_cc2500_bytes(transaction, sizeof(transaction));
    }

    uint8_t DomusLine::read_cc2500_register(Cc2500Register reg)
    {
        digitalWrite(CS, LOW);
        delayMicroseconds(5);
        while(digitalRead(readyPin) == HIGH)
        {
            delayMicroseconds(2);
        }

        SPI.transfer(static_cast<uint8_t>(reg) | 0x80);
        const uint8_t value = SPI.transfer(0xFF);
        delayMicroseconds(5);
        digitalWrite(CS, HIGH);
        delayMicroseconds(40);
        return value;
    }

    void DomusLine::write_cc2500_burst(Cc2500Register address, const uint8_t *data, uint8_t length)
    {
        if (length > 16)
        {
            ESP_LOGE("domusline", "CC2500 burst write too long: %u", length);
            return;
        }

        uint8_t transaction[17];
        transaction[0] = static_cast<uint8_t>(address) | 0x40;
        for (uint8_t i = 0; i < length; i++)
            transaction[i + 1] = data[i];
        transfer_cc2500_bytes(transaction, length + 1);
    }

    void DomusLine::initialize_cc2500()
    {
        send_cc2500_strobe(Cc2500Strobe::SIDLE);
        digitalWrite(CS, LOW);
        delayMicroseconds(10);
        digitalWrite(CS, HIGH);
        delayMicroseconds(300);

        send_cc2500_strobe(Cc2500Strobe::SRES);
        delayMicroseconds(300);

        write_cc2500_register(Cc2500Register::IOCFG0, 0x06);
        write_cc2500_register(Cc2500Register::FIFOTHR, 0x07);
        write_cc2500_register(Cc2500Register::SYNC1, 0xD3);
        write_cc2500_register(Cc2500Register::SYNC0, 0x91);
        write_cc2500_register(Cc2500Register::FSCTRL1, 0x09);
        write_cc2500_register(Cc2500Register::FSCTRL0, 0x00);
        write_cc2500_register(Cc2500Register::FREQ2, 0x5D);
        write_cc2500_register(Cc2500Register::FREQ1, 0x93);
        write_cc2500_register(Cc2500Register::FREQ0, 0xB1);
        write_cc2500_register(Cc2500Register::MDMCFG4, 0x2D);
        write_cc2500_register(Cc2500Register::MDMCFG3, 0x3B);
        write_cc2500_register(Cc2500Register::MDMCFG2, 0x73);
        write_cc2500_register(Cc2500Register::MDMCFG1, 0xA2);
        write_cc2500_register(Cc2500Register::MDMCFG0, 0xF8);
        write_cc2500_register(Cc2500Register::CHANNR, 0x10);
        write_cc2500_register(Cc2500Register::DEVIATN, 0x01);
        write_cc2500_register(Cc2500Register::FREND1, 0xB6);
        write_cc2500_register(Cc2500Register::FREND0, 0x10);
        write_cc2500_register(Cc2500Register::MCSM2, 0x07);
        write_cc2500_register(Cc2500Register::MCSM1, 0x30);
        write_cc2500_register(Cc2500Register::MCSM0, 0x18);
        write_cc2500_register(Cc2500Register::FOCCFG, 0x1D);
        write_cc2500_register(Cc2500Register::BSCFG, 0x1C);
        write_cc2500_register(Cc2500Register::AGCCTRL2, 0xC7);
        write_cc2500_register(Cc2500Register::AGCCTRL1, 0x00);
        write_cc2500_register(Cc2500Register::AGCCTRL0, 0xB2);
        write_cc2500_register(Cc2500Register::FSCAL3, 0xEA);
        write_cc2500_register(Cc2500Register::FSCAL2, 0x0A);
        write_cc2500_register(Cc2500Register::FSCAL1, 0x00);
        write_cc2500_register(Cc2500Register::FSCAL0, 0x11);
        write_cc2500_register(Cc2500Register::RCCTRL1, 0x41);
        write_cc2500_register(Cc2500Register::RCCTRL0, 0x00);
        write_cc2500_register(Cc2500Register::WOREVT1, 0x87);
        write_cc2500_register(Cc2500Register::WOREVT0, 0x6B);
        write_cc2500_register(Cc2500Register::WORCTRL, 0xF8);
        write_cc2500_register(Cc2500Register::FSTEST, 0x59);
        write_cc2500_register(Cc2500Register::TEST2, 0x88);
        write_cc2500_register(Cc2500Register::TEST1, 0x31);
        write_cc2500_register(Cc2500Register::TEST0, 0x0B);
        write_cc2500_register(Cc2500Register::IOCFG2, 0x29);
        write_cc2500_register(Cc2500Register::IOCFG0, 0x06);
        write_cc2500_register(Cc2500Register::PKTCTRL1, 0x04);
        write_cc2500_register(Cc2500Register::PKTCTRL0, 0x05);
        write_cc2500_register(Cc2500Register::ADDR, 0x01);
        write_cc2500_register(Cc2500Register::PKTLEN, 0xFF);

        (void) read_cc2500_register(Cc2500Register::MDMCFG1);

        const uint8_t pa_table[] = {0xFF, 0xFE, 0xBB, 0xA9, 0x7F, 0x6E, 0x97, 0xC5};
        write_cc2500_burst(Cc2500Register::PATABLE, pa_table, sizeof(pa_table));

        send_cc2500_strobe(Cc2500Strobe::SCAL);
        delay(3);
        send_cc2500_strobe(Cc2500Strobe::SRX);
        delay(2);

        for (uint8_t attempt = 0; attempt < 5; attempt++)
        {
            send_cc2500_strobe(Cc2500Strobe::SIDLE);
            send_cc2500_strobe(Cc2500Strobe::SFTX);
            send_domusline_command(DomusLineCommand::SET_COLOR_TEMPERATURE, 0x2C);
            send_cc2500_strobe(Cc2500Strobe::STX);
            delay(3);
        }
    }

    void DomusLine::send_domusline_command(DomusLineCommand command, uint8_t argument)
    {
        const uint8_t payload[] = {0x05, 0x7A, 0xEF, static_cast<uint8_t>(command), argument,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t fifo_packet[] = {0x0E, 0x68, 0x05, 0x7A, 0xEF, static_cast<uint8_t>(command), argument,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xFF};
        fifo_packet[13] = calculate_domusline_xor_checksum(payload, sizeof(payload));

        ESP_LOGD("domusline", "Queue Domus Line command 0x%02X, argument 0x%02X",
                 static_cast<uint8_t>(command), argument);
        write_cc2500_burst(Cc2500Register::TX_FIFO, fifo_packet, sizeof(fifo_packet));
    }

    void DomusLine::send_domusline_command_repeatedly(DomusLineCommand command, uint8_t argument,
                                                      uint8_t repeat_count, uint16_t repeat_delay_ms)
    {
        for (uint8_t attempt = 0; attempt < repeat_count; attempt++)
        {
            send_cc2500_strobe(Cc2500Strobe::SIDLE);
            send_cc2500_strobe(Cc2500Strobe::SFTX);
            send_domusline_command(command, argument);
            send_cc2500_strobe(Cc2500Strobe::STX);
            delay(repeat_delay_ms);
        }
    }

    void DomusLine::turn_light_on(uint8_t currentTemperature)
    {
        send_domusline_command_repeatedly(DomusLineCommand::LIGHT_ON, currentTemperature, 6, 35);
        enter_cc2500_power_down();
    }

    void DomusLine::turn_light_off()
    {
        send_domusline_command_repeatedly(DomusLineCommand::LIGHT_OFF, 0, 6, 35);
        enter_cc2500_power_down();
    }

    void DomusLine::increase_light_brightness()
    {
        send_domusline_command_repeatedly(DomusLineCommand::BRIGHTNESS_UP, 0, 6, 2);
        send_cc2500_strobe(Cc2500Strobe::SPWD);
    }

    void DomusLine::decrease_light_brightness()
    {
        send_domusline_command_repeatedly(DomusLineCommand::BRIGHTNESS_DOWN, 0, 5, 2);
        enter_cc2500_power_down();
    }

    void DomusLine::set_light_color_temperature(uint8_t currentTemperature)
    {
        send_domusline_command_repeatedly(DomusLineCommand::SET_COLOR_TEMPERATURE, currentTemperature, 5, 2);
        enter_cc2500_power_down();
    }

    void DomusLine::enter_cc2500_power_down()
    {
        for (uint8_t attempt = 0; attempt < 5; attempt++)
        {
            send_cc2500_strobe(Cc2500Strobe::SPWD);
            delay(67);
        }
    }

    uint8_t DomusLine::calculate_domusline_xor_checksum(const uint8_t *data, std::size_t length)
    {
        uint8_t checksum = 0;
        for (std::size_t i = 0; i < length; i++)
            checksum ^= data[i];
        return checksum;
    }


}
}