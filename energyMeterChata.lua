--Init  
base = "/home/EnergyMeter/esp02/"
deviceID = "ESP8266 PowerMeter "..node.chipid()

pulseTotal        = 0
pulseLength       = 0
pulseOld          = 0
heartBeat         = node.bootreason() + 10
lastSend          = tmr.now()
pulseDuration     = 0

heartBeat = node.bootreason() + 10
print("Boot reason:")
print(heartBeat)

wifi.setmode(wifi.STATION)
wifi.sta.config("Datlovo","Nu6kMABmseYwbCoJ7LyG")
wifi.sta.autoconnect(1)

Broker="88.146.202.186"  

pinLed = 3
gpio.mode(pinLed,gpio.OUTPUT)  
gpio.write(pinLed,gpio.LOW)  

versionSW         = 0.45
versionSWString   = "EnergyMeter chata v" 
print(versionSWString .. versionSW)

pin = 4
pulse1 = 0
du=0
gpio.mode(pin,gpio.INT)

function pinPulse(level)
  if gpio.read(pin) == gpio.HIGH then
  --if level == gpio.HIGH then --nabezna hrana
    --tmr.delay(10000)
    pulseDuration = tmr.now()
    gpio.write(pinLed,gpio.HIGH)  
    print("nabezna")
  else 
    --if (tmr.now() - pulseDuration) > 70000 and (tmr.now() - pulseDuration) < 100000 then
      pulseTotal=pulseTotal+1
      gpio.write(pinLed,gpio.LOW) 
      if (tmr.now()<pulseOld) then --timer overloaded
        pulseLength = (math.pow (2, 31) - pulseOld + tmr.now()) / 1000
      else
        pulseLength = (tmr.now() - pulseOld)/1000
      end
      pulseOld = tmr.now()
      print("dobezna")
      print(pulseLength)
      sendData()
    --end
  end
end


function sendData()
  if (tmr.now() - lastSend) > 5000000 or lastSend > tmr.now() then
    lastSend = tmr.now()
    print("I am sending pulse to OpenHab")
    print(pulseTotal)
    --pulseTotal = pulseTotal + 1
    --pulseLength = math.random(100,1000000)
    m:publish(base.."Pulse",pulseTotal,0,0)  
    m:publish(base.."pulseLength", pulseLength,0,0)  
    m:publish(base.."VersionSW",   versionSW,0,0)  
    m:publish(base.."HeartBeat",   heartBeat,0,0)

    if pulseTotal % 100 == 0 then
      file.open("config.ini", "w+")
      --file.write('pulseCount=')
      file.write(string.format("%u", pulseTotal))
      file.write("\n\r")
      file.close()
    end
    if heartBeat==0 then heartBeat=1
    else heartBeat=0
    end
  end 
end

function mqtt_sub()  
  m:subscribe(base,0, function(conn)   
    print("Mqtt Subscribed to OpenHAB feed for device "..deviceID)  
  end)  
end  


function reconnect()
  print ("Waiting for Wifi")
  if wifi.sta.status() == 5 and wifi.sta.getip() ~= nil then 
    print ("Wifi Up!")
    tmr.stop(1) 
    m:connect(Broker, 31883, 0, function(conn) 
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
end)  

tmr.alarm(0, 1000, 1, function() 
  print ("Connecting to Wifi... ")
  if wifi.sta.status() == 5 and wifi.sta.getip() ~= nil then 
    print ("Wifi connected")
    tmr.stop(0) 
    m:connect(Broker, 31883, 0, function(conn) 
      mqtt_sub() --run the subscription function 
      print(wifi.sta.getip())
      print("Mqtt Connected to:" .. Broker.." - "..base) 
      gpio.trig(pin, "both", pinPulse)
      print ("Read config file... ")
      file.open("config.ini", "r")
      s = file.readline()
      if (s==nil) then
        print("empty file")
        pulseTotal=0
      else
        pulseTotal = s
      end
      print(pulseTotal)
      file.close()  
      m:publish(base.."VersionSW",   versionSW,0,0)  
      m:publish(base.."HeartBeat",   heartBeat,0,0)
    end) 
  end
end)