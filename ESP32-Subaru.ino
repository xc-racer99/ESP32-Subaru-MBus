#include <config.h>
#include <BluetoothA2DPSink.h>
#include <BluetoothA2DPSinkQueued.h>
#include <BluetoothA2DPSource.h>
#include <A2DPVolumeControl.h>
#include <BluetoothA2DPCommon.h>
#include <SoundData.h>
#include <BluetoothA2DP.h>

BluetoothA2DPSink a2dp_sink;

void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  Serial.printf("AVRC metadata rsp: attribute id 0x%x, %s\n", data1, data2);
}

void setup() {
    i2s_pin_config_t my_pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    a2dp_sink.set_pin_config(my_pin_config);

    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    
    a2dp_sink.start("Subaru");
}

void loop() {
  // put your main code here, to run repeatedly:

}
