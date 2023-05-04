#include <mbus.h>

#include <config.h>
#include <BluetoothA2DPSink.h>
#include <BluetoothA2DPSinkQueued.h>
#include <BluetoothA2DPSource.h>
#include <A2DPVolumeControl.h>
#include <BluetoothA2DPCommon.h>
#include <SoundData.h>
#include <BluetoothA2DP.h>

BluetoothA2DPSink a2dp_sink;


#define auxConnectedPin 11 // TODO - read this from the headphone jack - or maybe the illumination pin

#define NUM_TRACKS 19

MBus mBus(10, 20); // TODO - determine pin numbers

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

uint8_t currentTrack = 1; // Current track we are on
uint64_t nextTime = 0; // Next time to send status update
uint64_t timeout = 0; // Timeout value for debouncing messages
bool wasCD = false; // If last command was to change disks
bool ignoreNext = false; // If next input command should be ignored
uint8_t currentCD = 1; // Current CD we are on

void fwOrBw(uint8_t oldTrack, uint8_t newTrack) // to interpret track changes as consecutive forward/backward skips
{
  uint8_t i=0;
  if(oldTrack<newTrack)
  {
    for(i=oldTrack;i<newTrack;i++)
    {
      /*skip forward*/
      a2dp_sink.next();
    }
  }
  else
  {
    for(i=newTrack;i<oldTrack;i++)
    {
      /*skip backwards*/
      a2dp_sink.previous();
    }
  }
}

void loop() {
  uint64_t receivedMessage = 0;
  
  if (a2dp_sink.is_connected()) {
    if (nextTime == 0) {
      mBus.sendChangingDisc(1, 1, MBus::ChangingStatus::kDone);
      mBus.sendDiscInfo(1, NUM_TRACKS, 500);
      mBus.sendPlayingTrack(1, 0);
    } else if (nextTime < millis()) {
      mBus.sendPlayingTrack(currentTrack, (uint16_t)(millis() / 1000)); 
      nextTime = millis() + 500;
    }

    if(mBus.receive(&receivedMessage)) {
      if (ignoreNext) {
        ignoreNext=false;
        if (wasCD) {
          mBus.sendDiscInfo(1, NUM_TRACKS, 500);
          wasCD = false;
        }
      } else if(receivedMessage == Ping) {
        mBus.send(PingOK);//acknowledge Ping
      } else if(receivedMessage == 0x19) {
        mBus.sendChangingDisc(currentCD,currentTrack, MBus::ChangingStatus::kDone); //'still at cd ...'
        delay(50);
        mBus.sendDiscInfo(1, NUM_TRACKS, 500);
        delay(50);
        mBus.sendPlayingTrack(currentTrack,0);
      } else if(receivedMessage == Off) {
        /*do something before shutdown*/
        mBus.send(Wait);//acknowledge
        
        a2dp_sink.stop();
      } else if(receivedMessage == Play) {
        //  never executed?
        mBus.sendPlayingTrack(currentTrack,(uint16_t)(millis()/1000));    
        mBus.sendChangerErrorCode(MBus::ChangerErrorCode::kNormal);
        
        a2dp_sink.play();
      } else if(receivedMessage == Pause) {
        mBus.send(Wait);//acknowledge

        a2dp_sink.pause();
      } else if (receivedMessage == FastFwd) {
        mBus.send(Wait);//acknowledge
        
        a2dp_sink.fast_forward();
      } else if (receivedMessage == FastBwd) {
        mBus.send(Wait);//acknowledge

        a2dp_sink.rewind();
      } else if(receivedMessage >> (4 * 5) == 0x113) {//'please change cd'
        uint64_t test = (receivedMessage >> (4 * 4)) - (uint64_t)0x1130;
        if (test > 0) {//not the same cd as before
          mBus.sendChangingDisc(test, 1, MBus::ChangingStatus::kDone);//'did change'
          delay(50);
          mBus.sendDiscInfo(1, NUM_TRACKS, 500);
          currentCD = test;
          currentTrack = 1;
          wasCD = true;
        } else {
          //same cd, maybe different track
          uint8_t lastTrack = currentTrack;
          //currentTrack=(receivedMessage&((uint64_t)0xF<<(4*2)))>>(4*2);
          currentTrack+=((receivedMessage&((uint64_t)0xF<<(4*3)))>>(4*3))*10;
          
          mBus.sendChangingDisc(currentCD,currentTrack, MBus::ChangingStatus::kDone); //'still at cd...'
          delay(50);
          mBus.sendDiscInfo(1, NUM_TRACKS, 500);
          delay(50);
          mBus.sendPlayingTrack(currentTrack, 0);
          if (timeout < millis()) {//debounce
            if (currentTrack != lastTrack && currentTrack > 1)
            {
              fwOrBw(lastTrack, currentTrack);
            }
            else if (currentTrack == 1)
            {
              wasCD = true;
            }
            else
            {
              /* Error */
            }
            timeout = millis() + 1000;
          }
        }
        ignoreNext = true;
      }
    }
  } else if (digitalRead(auxConnectedPin)) {
    
  } else {
    // Not connected - use internal CD?
  }
}
