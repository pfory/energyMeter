--Init  
base = "/flat/EnergyMeter/esp09/"
deviceID = "ESP8266 PowerMeter "..node.chipid()

pulseTotal        = 0
pulseLength       = 0
pulseOld          = 0
lastSend          = tmr.now()
pulseDuration     = 0
pulseDurationMin  = 70000
pulseDurationMax  = 100000
sendingData       = false
sendDelay         = 20000000 --in us

heartBeat = node.bootreason() + 10
val = rtcmem.read32(41, 1) -- Read the values in slots 41
if (val==20) then
  heartBeat=val
  rtcmem.write32(41, 0)
end
uart.write(0,"Boot reason:")
print(heartBeat)

Broker="192.168.1.52"
--Broker="88.146.202.186"  

pinLed = 3
gpio.mode(pinLed,gpio.OUTPUT)  
gpio.write(pinLed,gpio.LOW)  

versionSW         = 0.71
versionSWString   = "EnergyMeter v" 
print(versionSWString .. versionSW)

pin = 4
pulse1 = 0
du=0
gpio.mode(pin,gpio.INT)

function pinPulse(level)
  if gpio.read(pin) == gpio.LOW then
  --if level == gpio.HIGH then --nabezna hrana
    --tmr.delay(10000)
    pulseDuration = tmr.now()
    gpio.write(pinLed,gpio.HIGH)  
    print("nabezna")
  else 
    if (tmr.now() - pulseDuration) > pulseDurationMin and (tmr.now() - pulseDuration) < pulseDurationMax then
      pulseTotal=pulseTotal+1

      if pulseTotal % 100 == 0 then
        print("Save pulseTotal:"..pulseTotal)
        file.open("config.ini", "w+")
        file.write(string.format("%u", pulseTotal))
        file.write("\n\r")
        file.close()
      end
      rtcmem.write32(40, pulseTotal) --save pulsetotal to memory on address 40
      print("Heap:"..node.heap())

      gpio.write(pinLed,gpio.LOW) 
      if (tmr.now()<pulseOld) then --timer overloaded
        pulseLength = (math.pow (2, 31) - pulseOld + tmr.now()) / 1000
      else
        pulseLength = (tmr.now() - pulseOld)/1000
      end
      pulseOld = tmr.now()
      print("dobezna delka:"..pulseLength)
      if (sendingData==false) then
        sendData()
      end
    end
  end
end

function sendHB() 
  m:publish(base.."VersionSW",   versionSW,0,0)  
  m:publish(base.."HeartBeat",   string.format("%.0f",heartBeat),0,0)
  if heartBeat==0 then heartBeat=1
  else heartBeat=0
  end
end

function sendData()
  sendingData = true
  if (tmr.now() - lastSend) > sendDelay or lastSend > tmr.now() then
    lastSend = tmr.now()
    print("I am sending pulse to OpenHab:"..pulseTotal)
    --pulseTotal = pulseTotal + 1
    --pulseLength = math.random(100,1000000)
    
    m:publish(base.."Pulse",string.format("%.0f",pulseTotal),0,0)  
    m:publish(base.."pulseLength", string.format("%.0f",pulseLength),0,0)  

    print("Data sent")

  end 
  sendHB()
  sendingData = false
end

function mqtt_sub()  
  m:subscribe(base.."com",0, function(conn)   
    print("Mqtt Subscribed to OpenHAB feed for device "..deviceID)  
  end)  
end  


function reconnect()
  print ("Waiting for Wifi")
  if wifi.sta.status() == 5 and wifi.sta.getip() ~= nil then 
    print ("Wifi Up!")
    tmr.stop(1) 
    m:connect(Broker, 31883, 0, 1, function(conn) 
      print(wifi.sta.getip())
      print("Mqtt Connected to:" .. Broker) 
      mqtt_sub() --run the subscription function 
    end)
  end
end


m = mqtt.Client(deviceID, 180, "datel", "hanka12")  
m:lwt("/lwt", deviceID, 0, 0)  
m:on("offline", function(con)   
  print ("Mqtt Reconnecting...")   
  tmr.alarm(1, 10000, 1, function()  
    reconnect()
  end)
end)  


 -- on publish message receive event  
m:on("message", function(conn, topic, data)   
  print("Received:" .. topic .. ":" .. data) 
  if topic == base.."com" then
    if data == "ON" then
      print("Restarting ESP, bye.")
      rtcmem.write32(41, 20) --save bootreason 20 to memory on address 41
      node.restart()
    end
  end
end)  

function readConfig() 
  print ("Read config file... ")
  file.open("config.ini", "r")
  s = file.readline()
  if (s==nil) then
    print("empty file")
    pulseTotal=0
  else
    pulseTotal = s
    pulseTotal=pulseTotal+0
  end
  file.close()  
  print("PulseTotal from file:"..pulseTotal)
  val = rtcmem.read32(40, 1) -- Read the values in slots 41
  print("PulseTotal from EEPROM:"..val)
  if (val - pulseTotal<100) then
    pulseTotal = val
    print("Correct pulseTotal from EEPROM:"..pulseTotal)
  end
  rtcmem.write32(40, pulseTotal) --save pulsetotal to memory on address 40
end

readConfig()

m:connect(Broker, 1883, 0, 1, function(conn) 
  mqtt_sub() --run the subscription function 
  sendHB()
  print(wifi.sta.getip())
  print("Mqtt Connected to:" .. Broker.." - "..base) 
  gpio.trig(pin, "both", pinPulse)
end) 