#include <MBus.h>

#include <config.h>
#include <BluetoothA2DPSink.h>
#include <BluetoothA2DPSinkQueued.h>
#include <BluetoothA2DPSource.h>
#include <A2DPVolumeControl.h>
#include <BluetoothA2DPCommon.h>
#include <SoundData.h>
#include <BluetoothA2DP.h>

BluetoothA2DPSink a2dp_sink;

MBus mBus(21);

void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  Serial.printf("AVRC metadata rsp: attribute id 0x%x, %s\n", data1, data2);
}

void setup() {    
  static const i2s_config_t i2s_config = {
       .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
       .sample_rate = 41000,
       .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
       .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
       .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB),
       .intr_alloc_flags = 0, // default interrupt priority
       .dma_buf_count = 8,
       .dma_buf_len = 64,
       .use_apll = false,
       .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
  };
  a2dp_sink.set_i2s_config(i2s_config);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.start("Subaru");

  pinMode(21, (OUTPUT_OPEN_DRAIN | INPUT));
  digitalWrite(21, HIGH);

  Serial.begin(115200);

  Serial.println("Loop finished");
}

void loop()
{
  uint64_t receivedMessage = 0;  

  if(mBus.receive(&receivedMessage))
  {
    Serial.println(receivedMessage, HEX);
    
    if (receivedMessage == 0x68)
    {
      mBus.send(0xe8);//acknowledge Ping
      delay(7);
      mBus.send(0x69); // ???
      delay(7);
      mBus.send(0xEF00000); // Wait
      delay(7);
      mBus.sendChangedCD(1, 1);
      //mBus.send(0xEB910000008); // sendChangedCD
      //delay(7);
      //mBus.send(0xED100000000000C00); // Available discs, disk 1 only // TODO - is this necessary?
      delay(7);
      mBus.sendCDStatus(1);
      //mBus.send(0xEC1000000000); // sendCDStatus
      delay(7);
      mBus.sendPlayingTrack(1, 110);
      //mBus.send(0xE90000100000008); // sendPlayingTrack
      Serial.println("Sending ping");
    } else if (receivedMessage == 0x611012) {
      
    }
  }
}
